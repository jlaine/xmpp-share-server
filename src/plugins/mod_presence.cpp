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

#include <QDomElement>

#include "QXmppConstants.h"
#include "QXmppPresence.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStream.h"
#include "QXmppUtils.h"

#include "mod_presence.h"

class XmppServerPresencePrivate
{
public:
    XmppServerPresencePrivate(XmppServerPresence *qq);

    QSet<QString> collectSubscribers(const QString &jid);
    QSet<QString> collectSubscriptions(const QString &jid);

    QHash<QString, QHash<QString, QXmppPresence> > presences;
    QHash<QString, QSet<QString> > subscribers;

private:
    XmppServerPresence *q;
};

XmppServerPresencePrivate::XmppServerPresencePrivate(XmppServerPresence *qq)
    : q(qq)
{
}

/// Collect subscribers from the extensions.
///
/// \param jid

QSet<QString> XmppServerPresencePrivate::collectSubscribers(const QString &jid)
{
    QSet<QString> recipients;
    foreach (QXmppServerExtension *extension, q->server()->extensions())
        recipients += extension->presenceSubscribers(jid);
    return recipients;
}

/// Collect subscriptions from the extensions.
///
/// \param jid

QSet<QString> XmppServerPresencePrivate::collectSubscriptions(const QString &jid)
{
    QSet<QString> recipients;
    foreach (QXmppServerExtension *extension, q->server()->extensions())
        recipients += extension->presenceSubscriptions(jid);
    return recipients;
}

XmppServerPresence::XmppServerPresence()
{
    d = new XmppServerPresencePrivate(this);
}

XmppServerPresence::~XmppServerPresence()
{
    delete d;
}

/// Returns the list of available resources for the given local JID.
///
/// \param bareJid

QList<QXmppPresence> XmppServerPresence::availablePresences(const QString &bareJid) const
{
    return d->presences.value(bareJid).values();
}

int XmppServerPresence::extensionPriority() const
{
    // FIXME: until we can handle presence errors, we need to
    // keep this extension last.
    return -1000;
}

bool XmppServerPresence::handleStanza(const QDomElement &element)
{
    if (element.tagName() != QLatin1String("presence"))
        return false;

    const QString domain = server()->domain();
    const QString from = element.attribute("from");
    const QString type = element.attribute("type");
    const QString to = element.attribute("to");

    if (to == domain) {
        // presence to the local domain

        // we only want available or unavailable presences from local users
        if ((!type.isEmpty() && type != QLatin1String("unavailable"))
            || (QXmppUtils::jidToDomain(from) != domain))
            return true;

        const QString bareFrom = QXmppUtils::jidToBareJid(from);
        bool isInitial = false;

        if (type.isEmpty()) {
            QXmppPresence presence;
            presence.parse(element);

            // record the presence for future use
            isInitial = !d->presences.value(bareFrom).contains(from);
            d->presences[bareFrom][from] = presence;
        } else {
            d->presences[bareFrom].remove(from);
            if (d->presences[bareFrom].isEmpty())
                d->presences.remove(bareFrom);
        }

        // broadcast it to subscribers
        foreach (const QString &subscriber, d->collectSubscribers(from)) {
            // avoid loop
            if (subscriber == to)
                continue;
            QDomElement changed = element.cloneNode(true).toElement();
            changed.setAttribute("to", subscriber);
            server()->handleElement(changed);
        }

        // get presences from subscriptions
        if (isInitial) {
            foreach (const QString &subscription, d->collectSubscriptions(from)) {
                if (QXmppUtils::jidToDomain(subscription) != domain) {
                    QXmppPresence probe;
                    probe.setType(QXmppPresence::Probe);
                    probe.setFrom(from);
                    probe.setTo(subscription);
                    server()->sendPacket(probe);
                } else {
                    QXmppPresence push;
                    foreach (push, availablePresences(subscription)) {
                        push.setTo(from);
                        server()->sendPacket(push);
                    }
                }
            }
        }

        // the presence was for us, stop here
        return true;
    } else {
        // directed presence
        if ((type.isEmpty() || type == QLatin1String("unavailable")) && QXmppUtils::jidToDomain(from) == domain) {
            // available or unavailable presence from local user
            if (type.isEmpty())
                d->subscribers[from].insert(to);
            else {
                d->subscribers[from].remove(to);
                if (d->subscribers[from].isEmpty())
                    d->subscribers.remove(from);
            }
        } else if (type == QLatin1String("error") && QXmppUtils::jidToDomain(to) == domain) {
            // error presence to a local user
            d->subscribers[to].remove(from);
            if (d->subscribers[to].isEmpty())
                d->subscribers.remove(to);
        }

        // the presence was not for us
        return false;
    }
}

QSet<QString> XmppServerPresence::presenceSubscribers(const QString &jid)
{
    return d->subscribers.value(jid);
}

XmppServerPresence* XmppServerPresence::instance(QXmppServer *server)
{
    foreach (QXmppServerExtension *extension, server->extensions()) {
        XmppServerPresence *presenceExtension = qobject_cast<XmppServerPresence*>(extension);
        if (presenceExtension)
            return presenceExtension;
    }
    return 0;
}

bool XmppServerPresence::start()
{
    bool check;
    Q_UNUSED(check);

    check = connect(server(), SIGNAL(clientDisconnected(QString)),
                    this, SLOT(_q_clientDisconnected(QString)));
    Q_ASSERT(check);

    return true;
}

void XmppServerPresence::stop()
{
    disconnect(server(), SIGNAL(clientDisconnected(QString)),
               this, SLOT(_q_clientDisconnected(QString)));
}

void XmppServerPresence::_q_clientDisconnected(const QString &jid)
{
    Q_ASSERT(!jid.isEmpty());

    // check the user exited cleanly
    const bool hadPresence = d->presences.value(QXmppUtils::jidToBareJid(jid)).contains(jid);
    if (hadPresence) {
        // the client had sent an initial available presence but did
        // not sent an unavailable presence, synthesize it
        QDomDocument doc;
        QDomElement presence = doc.createElement("presence");
        presence.setAttribute("from", jid);
        presence.setAttribute("type", "unavailable");
        presence.setAttribute("to", server()->domain());
        server()->handleElement(presence);
    } else {
        // synthesize unavailable presence to directed presence receivers
        foreach (const QString &recipient, presenceSubscribers(jid)) {
            QDomDocument doc;
            QDomElement presence = doc.createElement("presence");
            presence.setAttribute("from", jid);
            presence.setAttribute("type", "unavailable");
            presence.setAttribute("to", recipient);
            server()->handleElement(presence);
        }
    }
}

// PLUGIN

class XmppServerPresencePlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("presence"))
            return new XmppServerPresence;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("presence");
    };
};

Q_EXPORT_PLUGIN2(presence, XmppServerPresencePlugin)

