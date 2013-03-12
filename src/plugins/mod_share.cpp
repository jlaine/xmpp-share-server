/*
 * xmpp-share-server
 * Copyright (C) 2010-2013 Wifirst
 * See AUTHORS file for a full list of contributors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QCoreApplication>
#include <QDateTime>
#include <QDomElement>
#include <QMap>
#include <QMutexLocker>
#include <QSettings>
#include <QTimer>

#include "QXmppConstants.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppPresence.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppShareIq.h"
#include "QXmppStream.h"
#include "QXmppUtils.h"

#include "mod_share.h"

#define SEARCH_MAX_SECONDS  10

class Peer
{
public:
    Peer() : bytes(0), files(0), explored(false) {};
    QString jid;
    QString name;
    qint64 bytes;
    qint64 files;
    bool explored;
    QDateTime exploreNext;
    QString exploreTag;
};

class Search
{
public:
    QXmppShareSearchIq responseIq;
    QStringList pending;
    QDateTime timeout;
};

/** Sort peers by decreasing share size.
 */
static bool peerLessThan(const Peer &peer1, const Peer &peer2)
{
    if (peer1.bytes > peer2.bytes)
        return true;
    else if (peer1.bytes == peer2.bytes)
        return QString::compare(peer1.name, peer2.name, Qt::CaseInsensitive) < 0;
    else
        return false;
}

static void totals(const QXmppShareItem *item, qint64 &bytes, qint64 &files)
{
    for (int i = 0; i < item->size(); ++i)
    {
        const QXmppShareItem *child = item->child(i);
        if (child->type() == QXmppShareItem::FileItem)
        {
            bytes += child->fileSize();
            files++;
        } else {
            totals(child, bytes, files);
        }
    }
}

class XmppServerSharePrivate
{
public:
    QMutex mutex;
    QSettings *statistics;
    void broadcastList(QXmppServer *server);
    QXmppShareSearchIq browseIq() const;

    QStringList allowedDomains;
    qint64 availableBytes;
    qint64 availableFiles;
    bool forceProxy;
    QString jid;
    QString redirectDomain;
    QString redirectServer;

    QMap<QString, Peer> peers;
    QMap<QString, Search*> searches;
    QTimer *exploreTimer;
};

void XmppServerSharePrivate::broadcastList(QXmppServer *server)
{
    QXmppShareSearchIq responseIq = browseIq();
    responseIq.setFrom(jid);
    responseIq.setType(QXmppIq::Set);
    foreach (const QString &peer, peers.keys()) {
        responseIq.setTo(peer);
        server->sendPacket(responseIq);
    }
}

QXmppShareSearchIq XmppServerSharePrivate::browseIq() const
{
    QXmppShareSearchIq responseIq;
    responseIq.collection().setLocations(QXmppShareLocation(jid));

    QList<Peer> sortedPeers = peers.values();
    qSort(sortedPeers.begin(), sortedPeers.end(), peerLessThan);
    foreach (const Peer &peer, sortedPeers)
    {
        QXmppShareItem collection(QXmppShareItem::CollectionItem);
        collection.setFileSize(peer.bytes);
        collection.setName(peer.name);
        collection.setLocations(QXmppShareLocation(peer.jid));
        responseIq.collection().appendChild(collection);
    }
    return responseIq;
}


XmppServerShare::XmppServerShare()
    : d(new XmppServerSharePrivate)
{
    d->availableBytes = 0;
    d->availableFiles = 0;
    d->forceProxy = false;
    d->statistics = 0;

    // a check will be done every minute of what peers
    // need to be explore
    d->exploreTimer = new QTimer(this);
    d->exploreTimer->setInterval(60000);

    bool check = connect(d->exploreTimer, SIGNAL(timeout()),
        this, SLOT(explorePeers()));
    Q_ASSERT(check);
    Q_UNUSED(check);
}

XmppServerShare::~XmppServerShare()
{
    delete d;
}

void XmppServerShare::checkTimeout()
{
    const QDateTime currentTime = QDateTime::currentDateTime();
    const QStringList keys = d->searches.keys();
    foreach (const QString &tag, keys)
    {
        Search *search = d->searches.value(tag);
        if (search->timeout >= currentTime)
        {
            warning(QString("Search timeout %1 for %2").arg(search->responseIq.tag(), search->responseIq.to()));
            server()->sendPacket(search->responseIq);
            d->searches.remove(tag);
            delete search;
        }
    }
}

QStringList XmppServerShare::discoveryItems() const
{
    return QStringList() << d->jid;
}

bool XmppServerShare::handleStanza(const QDomElement &element)
{
    if (element.attribute("to") != d->jid)
        return false;

    // FIXME: make locking more granular
    QMutexLocker(&d->mutex);
    if (element.tagName() == "iq" && QXmppDiscoveryIq::isDiscoveryIq(element))
    {
        QXmppDiscoveryIq discoIq;
        discoIq.parse(element);

        if (discoIq.type() == QXmppIq::Get)
        {
            QXmppDiscoveryIq responseIq;
            responseIq.setFrom(discoIq.to());
            responseIq.setTo(discoIq.from());
            responseIq.setId(discoIq.id());
            responseIq.setType(QXmppIq::Result);
            responseIq.setQueryType(discoIq.queryType());

            if (discoIq.queryType() == QXmppDiscoveryIq::InfoQuery)
            {
                QStringList features = QStringList() << ns_disco_info << ns_disco_items << ns_shares;
                QList<QXmppDiscoveryIq::Identity> identities;

                QXmppDiscoveryIq::Identity identity;
                identity.setCategory("store");
                identity.setType("file");
                identity.setName("File sharing server");
                identities.append(identity);

                responseIq.setFeatures(features);
                responseIq.setIdentities(identities);

            }
            server()->sendPacket(responseIq);
            return true;
        }
    }
    else if (element.tagName() == "iq" && QXmppShareSearchIq::isShareSearchIq(element))
    {
        QXmppShareSearchIq iq;
        iq.parse(element);

        handleShareSearchIq(iq);
        return true;
    }
    else if (element.tagName() == "presence")
    {
        QXmppPresence presence;
        presence.parse(element);

        return handlePresence(presence);
    }

    return false;
}

/// Returns the XMPP domains which are allowed to use the share server.
///

QStringList XmppServerShare::allowedDomains() const
{
    return d->allowedDomains;
}
 
/// Sets the XMPP domains which are allowed to use the share server.
///
/// If not defined, defaults to the server's domain.
///
/// \param allowedDomains

void XmppServerShare::setAllowedDomains(const QStringList &allowedDomains)
{
    d->allowedDomains = allowedDomains;
}

bool XmppServerShare::forceProxy() const
{
    return d->forceProxy;
}

void XmppServerShare::setForceProxy(bool force)
{
    d->forceProxy = force;
}

QString XmppServerShare::jid() const
{
    return d->jid;
}

void XmppServerShare::setJid(const QString &jid)
{
    d->jid = jid;
}

QString XmppServerShare::redirectDomain() const
{
    return d->redirectDomain;
}

void XmppServerShare::setRedirectDomain(const QString &domain)
{
    d->redirectDomain = domain;
}

QString XmppServerShare::redirectServer() const
{
    return d->redirectServer;
}

void XmppServerShare::setRedirectServer(const QString &server)
{
    d->redirectServer = server;
}

void XmppServerShare::explorePeers()
{
    foreach (const QString &recipient, d->peers.keys())
    {
        if (d->peers[recipient].exploreNext > QDateTime::currentDateTime())
            continue;

        // explore
        QXmppShareSearchIq iq;
        iq.setFrom(d->jid);
        iq.setTo(recipient);
        iq.setDepth(0);
        d->peers[recipient].exploreTag = iq.tag();
        server()->sendPacket(iq);

        // schedule next explore in 3 hours
        d->peers[recipient].exploreNext = QDateTime::currentDateTime().addSecs(3 * 3600);
    }
}

void XmppServerShare::handleShareSearchIq(const QXmppShareSearchIq &shareIq)
{
    if (!d->peers.contains(shareIq.from()))
        return;

    // handle a search request
    if (shareIq.type() == QXmppIq::Get)
    {
        // browse peers
        if (shareIq.search().isEmpty() && shareIq.depth() == 1)
        {
            debug(QString("Browse request %1 from %2").arg(shareIq.tag(), shareIq.from()));
            QXmppShareSearchIq responseIq = d->browseIq();
            responseIq.setId(shareIq.id());
            responseIq.setFrom(shareIq.to());
            responseIq.setTo(shareIq.from());
            responseIq.setType(QXmppIq::Result);
            responseIq.setTag(shareIq.tag());
            debug(QString("Browse response %1 for %2").arg(responseIq.tag(), responseIq.to()));
            server()->sendPacket(responseIq);
            return;
        }

        // forward search to peers
        debug(QString("Search request %1 for %2").arg(shareIq.tag(), shareIq.from()));
        const QString tag = QXmppUtils::generateStanzaHash();

        // record search
        Search *search = new Search;
        search->responseIq.collection().setLocations(QXmppShareLocation(d->jid));
        search->responseIq.setId(shareIq.id());
        search->responseIq.setFrom(shareIq.to());
        search->responseIq.setTo(shareIq.from());
        search->responseIq.setType(QXmppIq::Result);
        search->responseIq.setTag(shareIq.tag());
        search->timeout = QDateTime::currentDateTime().addSecs(SEARCH_MAX_SECONDS);
        d->searches.insert(tag, search);

        // relay search to peers
        foreach (const QString &peer, d->peers.keys())
        {
            QXmppShareSearchIq iq;
            iq.setFrom(d->jid);
            iq.setTo(peer);
            iq.setType(QXmppIq::Get);
            iq.setTag(tag);
            if (shareIq.depth())
                iq.setDepth(qMax(1, shareIq.depth() - 1));
            iq.setSearch(shareIq.search());
            search->pending.append(iq.id());
            server()->sendPacket(iq);
        }

        // watch request timeout
        QTimer::singleShot(SEARCH_MAX_SECONDS * 1000, this, SLOT(checkTimeout()));
    }

    // handle a search result
    else if (shareIq.type() == QXmppIq::Error || shareIq.type() == QXmppIq::Result)
    {
        const QString tag = shareIq.tag();

        // exploration result
        Peer &peer = d->peers[shareIq.from()];
        if (peer.exploreTag == tag)
        {
            d->availableBytes -= peer.bytes;
            d->availableFiles -= peer.files;

            // update statistics
            peer.explored = true;
            peer.bytes = 0;
            peer.files = 0;
            totals(&shareIq.collection(), peer.bytes, peer.files);
            info(QString("Explore result from %1 %2 files %3 bytes").arg(
                shareIq.from(), QString::number(peer.files), QString::number(peer.bytes)));

            d->availableBytes += peer.bytes;
            d->availableFiles += peer.files;
            setGauge("share.file.size", d->availableBytes);
            setGauge("share.file.count", d->availableFiles);

            // broadcast list of peers
            d->broadcastList(server());
            return;
        }

        Search *search = d->searches.value(tag);
        if (!search || !search->pending.contains(shareIq.id()))
            return;
        search->pending.removeAll(shareIq.id());

        // store results
        if (shareIq.type() != QXmppIq::Error && shareIq.collection().size())
        {
            QXmppShareItem &collection = search->responseIq.collection();
            int row = collection.size();
            for (int i = 0; i < collection.size(); ++i) {
                const QString jid = collection.child(i)->locations().first().jid();
                if (peerLessThan(peer, d->peers[jid])) {
                    row = i;
                    break;
                }
            }
            QXmppShareItem *item = collection.insertChild(row, shareIq.collection());
            item->setName(peer.name);
        }

        // check whether the search is finished
        if (search->pending.isEmpty())
        {
            debug(QString("Search response %1 for %2").arg(search->responseIq.tag(), search->responseIq.to()));
            server()->sendPacket(search->responseIq);
            d->searches.remove(tag);
            delete search;
        }
    }
}

bool XmppServerShare::handlePresence(const QXmppPresence &presence)
{
    const QString from = presence.from();
    if (from.isEmpty())
    {
        warning("Got a presence without a sender!");
        return true;
    }

    if (presence.type() == QXmppPresence::Available)
    {
        // find shares extension
        QXmppElement shareExtension;
        foreach (const QXmppElement &extension, presence.extensions())
        {
            if (extension.attribute("xmlns") == ns_shares)
            {
                shareExtension = extension;
                break;
            }
        }
        if (shareExtension.attribute("xmlns") != ns_shares)
            return true;

        // prepare reply
        QXmppPresence response;
        response.setFrom(presence.to());
        response.setTo(presence.from());
        response.setVCardUpdateType(QXmppPresence::VCardUpdateNone);

        QXmppElement x;
        x.setTagName("x");
        x.setAttribute("xmlns", ns_shares);

        // only accept allowed domains
        if (!d->allowedDomains.contains(QXmppUtils::jidToDomain(from)))
        {
            warning("Refused to register " + from);
            QXmppStanza::Error error(QXmppStanza::Error::Cancel, QXmppStanza::Error::Forbidden);
            response.setType(QXmppPresence::Error);
            response.setError(error);
            response.setExtensions(QXmppElementList() << x);
            server()->sendPacket(response);
            return true;
        }

        // redirect
        if (!d->redirectDomain.isEmpty())
        {
            QXmppElement domainElement;
            domainElement.setTagName("domain");
            domainElement.setValue(d->redirectDomain);
            x.appendChild(domainElement);

            QXmppElement serverElement;
            serverElement.setTagName("server");
            serverElement.setValue(d->redirectServer);
            x.appendChild(serverElement);

            //info(QString("Redirecting share user %1 to %2").arg(from, d->redirectDomain));
            QXmppStanza::Error error(QXmppStanza::Error::Modify, QXmppStanza::Error::Redirect);
            response.setType(QXmppPresence::Error);
            response.setError(error);
            response.setExtensions(QXmppElementList() << x);
            server()->sendPacket(response);
            return true;
        }

        // Determine nickname
        QXmppElement nickNameElement = shareExtension.firstChildElement("nickName");
        QString nickName;
        if (nickNameElement.isNull())
            nickName = QXmppUtils::jidToUser(from);
        else
            nickName = nickNameElement.value();

        bool changed = false;
        if (!d->peers.contains(from))
        {
            // register with server
            d->peers[from].jid = from;
            d->peers[from].name = nickName;
            // explore the peer in a minute to give it time to start up
            d->peers[from].exploreNext = QDateTime::currentDateTime().addSecs(59);

            info(QString("Registered share user %1 as %2").arg(from, d->peers[from].name));
            setGauge("share.participant.count", d->peers.size());
            changed = true;
        }

        // reply
        if (d->forceProxy)
        {
            QXmppElement force;
            force.setTagName("force-proxy");
            force.setValue("1");
            x.appendChild(force);
        }
        response.setExtensions(QXmppElementList() << x);
        server()->sendPacket(response);

        // broadcast list of peers
        if (changed)
            d->broadcastList(server());
    }
    else if (presence.type() == QXmppPresence::Unavailable)
    {
        if (d->peers.contains(from))
        {
            d->availableBytes -= d->peers[from].bytes;
            d->availableFiles -= d->peers[from].files;
            d->peers.remove(from);

            // update statistics
            info(QString("Unregistered share user %1").arg(from));
            setGauge("share.file.size", d->availableBytes);
            setGauge("share.file.count", d->availableFiles);
            setGauge("share.participant.count", d->peers.size());

            // broadcast list of peers
            d->broadcastList(server());
        }
    }

    // return false to enable directed presence handling
    return false;
}

bool XmppServerShare::start()
{
    // determine allowed domains
    if (d->allowedDomains.isEmpty())
        d->allowedDomains << server()->domain();

    // determine jid
    if (d->jid.isEmpty())
        d->jid = "shares." + server()->domain();

    // start periodic exploration of peers
    d->exploreTimer->start();
    return true;
}

void XmppServerShare::stop()
{
    d->exploreTimer->stop();
}

// PLUGIN

class XmppServerSharePlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("share"))
            return new XmppServerShare;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("share");
    };
};

Q_EXPORT_PLUGIN2(share, XmppServerSharePlugin)

