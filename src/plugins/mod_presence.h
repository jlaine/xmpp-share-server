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

#ifndef XMPP_SERVER_PRESENCE_H
#define XMPP_SERVER_PRESENCE_H

#include "QXmppServerExtension.h"

class QXmppPresence;
class XmppServerPresencePrivate;

/// \brief QXmppServer extension for presence handling.
///

class XmppServerPresence : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "presence");

public:
    XmppServerPresence();
    ~XmppServerPresence();

    QList<QXmppPresence> availablePresences(const QString &bareJid) const;
    int extensionPriority() const;
    bool handleStanza(const QDomElement &element);
    QSet<QString> presenceSubscribers(const QString &jid);
    bool start();
    void stop();

    static XmppServerPresence* instance(QXmppServer *server);

private slots:
    void _q_clientDisconnected(const QString &jid);

private:
    friend class XmppServerPresencePrivate;
    XmppServerPresencePrivate *d;
};

#endif
