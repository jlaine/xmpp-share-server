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
#include <QCryptographicHash>
#include <QDomElement>
#include <QHostInfo>
#include <QSettings>
#include <QTimer>

#include "QXmppByteStreamIq.h"
#include "QXmppConfiguration.h"
#include "QXmppConstants.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppPingIq.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppSocks.h"
#include "QXmppStream.h"
#include "QXmppUtils.h"

#include "mod_proxy65.h"

const int blockSize = 16384;

static QHostAddress lookupHost(const QString &name)
{
    QHostInfo hostInfo = QHostInfo::fromName(name);
    foreach (const QHostAddress &address, hostInfo.addresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            return address;
        }
    }
    return QHostAddress();
}

static QString streamHash(const QString &sid, const QString &initiatorJid, const QString &targetJid)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    QString str = sid + initiatorJid + targetJid;
    hash.addData(str.toAscii());
    return hash.result().toHex();
}

QTcpSocketPair::QTcpSocketPair(const QString &hash, QObject *parent)
    : QXmppLoggable(parent),
    key(hash),
    transfer(0),
    target(0),
    source(0)
{
}

bool QTcpSocketPair::activate()
{
    if (!source || !target) {
        warning("Both source and target sockets are needed to activate " + key);
        return false;
    }
    time.start();
    connect(target, SIGNAL(bytesWritten(qint64)), this, SLOT(sendData()));
    connect(source, SIGNAL(readyRead()), this, SLOT(sendData()));
    return true;
}

void QTcpSocketPair::addSocket(QTcpSocket *socket)
{
    if (source)
    {
        warning("Unexpected connection for " + key);
        socket->deleteLater();
        return;
    }

    if (target)
    {
        debug(QString("Opened source connection for %1 %2:%3").arg(
            key,
            socket->peerAddress().toString(),
            QString::number(socket->peerPort())));
        source = socket;
        source->setReadBufferSize(4 * blockSize);
        connect(source, SIGNAL(disconnected()), this, SLOT(disconnected()));
    }
    else
    {
        debug(QString("Opened target connection for %1 %2:%3").arg(
            key,
            socket->peerAddress().toString(),
            QString::number(socket->peerPort())));
        target = socket;
        connect(target, SIGNAL(disconnected()), this, SLOT(disconnected()));
    }
    socket->setParent(this);
}

void QTcpSocketPair::disconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (target == socket)
    {
        debug("Closed target connection for " + key);
        emit finished();
    } else if (source == socket) {
        debug("Closed source connection for " + key);
        if (!target || !target->isOpen())
            emit finished();
    }
}

void QTcpSocketPair::sendData()
{
    // don't saturate the outgoing socket
    if (target->bytesToWrite() >= 2 * blockSize)
        return;

    // check for completion
    if (!source->isOpen())
    {
        if (!target->bytesToWrite())
            target->close();
        return;
    }

    char buffer[blockSize];
    qint64 length = source->read(buffer, blockSize);
    if (length < 0)
    {
        if (!target->bytesToWrite())
            target->close();
        return;
    }
    if (length > 0)
    {
        target->write(buffer, length);
        transfer += length;
    }
}

class XmppServerProxy65Private
{
public:
    // configuration
    QStringList allowedDomains;
    QString jid;
    QHostAddress hostAddress;
    QString hostName;
    quint16 port;

    // state
    QHash<QString, QTcpSocketPair*> pairs;
    QXmppSocksServer *server;
};

XmppServerProxy65::XmppServerProxy65()
    : d(new XmppServerProxy65Private)
{
    bool check;
    Q_UNUSED(check);

    d->port = 7777;
    d->server = new QXmppSocksServer(this);

    check = connect(d->server, SIGNAL(newConnection(QTcpSocket*,QString,quint16)),
                    this, SLOT(slotSocketConnected(QTcpSocket*,QString,quint16)));
    Q_ASSERT(check);
}

XmppServerProxy65::~XmppServerProxy65()
{
    delete d;
}

/// Returns the XMPP domains which are allowed to use the proxy.
///

QStringList XmppServerProxy65::allowedDomains() const
{
    return d->allowedDomains;
}

/// Sets the XMPP domains which are allowed to use the proxy.
///
/// If not defined, defaults to the server's domain.
///
/// \param allowedDomains

void XmppServerProxy65::setAllowedDomains(const QStringList &allowedDomains)
{
    d->allowedDomains = allowedDomains;
}

/// Returns the proxy server's JID.
///

QString XmppServerProxy65::jid() const
{
    return d->jid;
}

/// Set the proxy server's JID.
///
/// \param jid

void XmppServerProxy65::setJid(const QString &jid)
{
    d->jid = jid;
}

/// Returns the host which is advertised to clients for SOCKS5 connections.
///

QString XmppServerProxy65::host() const
{
    return d->hostName;
}

/// Sets the host which is advertised to clients for SOCKS5 connections.
///
/// If not defined, defaults to the server's domain.
///
/// \param host

void XmppServerProxy65::setHost(const QString &host)
{
    d->hostName = host;
}

/// Returns the port on which to listen for SOCKS5 connections.
///

quint16 XmppServerProxy65::port() const
{
    return d->port;
}

/// Sets the port on which to listen for SOCKS5 connections.
///
/// If not defined, defaults to 7777.
///
/// \param port

void XmppServerProxy65::setPort(quint16 port)
{
    d->port = port;
}

QStringList XmppServerProxy65::discoveryItems() const
{
    return QStringList() << d->jid;
}

bool XmppServerProxy65::handleStanza(const QDomElement &element)
{
    if (element.attribute("to") != d->jid)
        return false;

    if (element.tagName() == "iq" && QXmppDiscoveryIq::isDiscoveryIq(element))
    {
        QXmppDiscoveryIq discoIq;
        discoIq.parse(element);

        if (discoIq.type() == QXmppIq::Get)
        {
            QXmppDiscoveryIq responseIq;
            responseIq.setTo(discoIq.from());
            responseIq.setFrom(discoIq.to());
            responseIq.setId(discoIq.id());
            responseIq.setType(QXmppIq::Result);
            responseIq.setQueryType(discoIq.queryType());

            if (discoIq.queryType() == QXmppDiscoveryIq::InfoQuery)
            {
                QStringList features = QStringList() << ns_disco_info << ns_disco_items << ns_bytestreams;

                QList<QXmppDiscoveryIq::Identity> identities;
                QXmppDiscoveryIq::Identity identity;
                identity.setCategory("proxy");
                identity.setType("bytestreams");
                identity.setName("SOCKS5 Bytestreams");
                identities.append(identity);

                responseIq.setFeatures(features);
                responseIq.setIdentities(identities);
            }

            server()->sendPacket(responseIq);
            return true;
        }
    }
    else if (element.tagName() == "iq" && QXmppByteStreamIq::isByteStreamIq(element))
    {
        QXmppByteStreamIq bsIq;
        bsIq.parse(element);

        if (bsIq.type() == QXmppIq::Get)
        {
            // request for the proxy's network address
            if (d->allowedDomains.contains(QXmppUtils::jidToDomain(bsIq.from()))) {
                QXmppByteStreamIq responseIq;
                responseIq.setType(QXmppIq::Result);
                responseIq.setTo(bsIq.from());
                responseIq.setFrom(bsIq.to());
                responseIq.setId(bsIq.id());

                QList<QXmppByteStreamIq::StreamHost> streamHosts;

                QXmppByteStreamIq::StreamHost streamHost;
                streamHost.setJid(d->jid);
                streamHost.setHost(d->hostAddress.toString());
                streamHost.setPort(d->port);
                streamHosts.append(streamHost);

                responseIq.setStreamHosts(streamHosts);
                server()->sendPacket(responseIq);
            } else {
                QXmppIq responseIq;
                responseIq.setType(QXmppIq::Error);
                responseIq.setTo(bsIq.from());
                responseIq.setFrom(bsIq.to());
                responseIq.setId(bsIq.id());
                responseIq.setError(QXmppStanza::Error(QXmppStanza::Error::Auth, QXmppStanza::Error::Forbidden));
                server()->sendPacket(responseIq);
            }
        }
        else if (bsIq.type() == QXmppIq::Set)
        {
            QString hash = streamHash(bsIq.sid(), bsIq.from(), bsIq.activate());
            QTcpSocketPair *pair = d->pairs.value(hash);

            QXmppIq responseIq;
            responseIq.setTo(bsIq.from());
            responseIq.setFrom(bsIq.to());
            responseIq.setId(bsIq.id());

            if (pair &&
                d->allowedDomains.contains(QXmppUtils::jidToDomain(bsIq.from())))
            {
                if (pair->activate()) {
                    info(QString("Activated connection %1 by %2").arg(hash, bsIq.from()));
                    responseIq.setType(QXmppIq::Result);
                } else {
                    warning(QString("Failed to activate connection %1 by %2").arg(hash, bsIq.from()));
                    responseIq.setType(QXmppIq::Error);
                }
            } else {
                warning(QString("Not activating connection %1 by %2").arg(hash, bsIq.from()));
                responseIq.setType(QXmppIq::Error);
            }
            server()->sendPacket(responseIq);
        }
        return true;
    }
    return false;
}

bool XmppServerProxy65::start()
{
    // determine allowed domains
    if (d->allowedDomains.isEmpty())
        d->allowedDomains << server()->domain();

    // determine jid
    if (d->jid.isEmpty())
        d->jid = "proxy." + server()->domain();

    // determine advertised address
    if (d->hostName.isEmpty())
        d->hostName = server()->domain();
    d->hostAddress = lookupHost(d->hostName);
    if (d->hostAddress.isNull()) {
        warning(QString("Could not lookup host %1").arg(d->hostName));
        return false;
    }

    // start listening
    if (!d->server->listen(d->port))
        return false;

    return true;
}

void XmppServerProxy65::stop()
{
    // refuse incoming connections
    d->server->close();

    // close socket pairs
    foreach (QTcpSocketPair *pair, d->pairs)
        delete pair;
    d->pairs.clear();
}

void XmppServerProxy65::slotSocketConnected(QTcpSocket *socket, const QString &hostName, quint16 port)
{
    Q_UNUSED(port);

    QTcpSocketPair *pair = d->pairs.value(hostName);
    if (!pair)
    {
        bool check;
        Q_UNUSED(check);

        pair = new QTcpSocketPair(hostName, this);
        check = connect(pair, SIGNAL(finished()),
                        this, SLOT(slotPairFinished()));
        Q_ASSERT(check);
        d->pairs.insert(hostName, pair);
    }
    pair->addSocket(socket);
}

void XmppServerProxy65::slotPairFinished()
{
    QTcpSocketPair *pair = qobject_cast<QTcpSocketPair*>(sender());
    if (!pair)
        return;

    info(QString("Data transfered for %1 %2").arg(pair->key, QString::number(pair->transfer)));

    // update totals
    updateCounter("proxy65.bytes", pair->transfer);
    updateCounter("proxy65.transfers");

    // remove socket pair
    d->pairs.remove(pair->key);
    pair->deleteLater();
}

// PLUGIN

class XmppServerProxy65Plugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("proxy65"))
            return new XmppServerProxy65;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("proxy65");
    };
};

Q_EXPORT_PLUGIN2(proxy65, XmppServerProxy65Plugin)

