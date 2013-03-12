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
#include "QXmppPingIq.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStream.h"

#include "mod_ping.h"

QStringList XmppServerPing::discoveryFeatures() const
{
    return QStringList() << ns_ping;
}

bool XmppServerPing::handleStanza(const QDomElement &element)
{
    if (element.attribute("to") != server()->domain())
        return false;

    // XEP-0199: XMPP Ping
    if (element.tagName() == "iq" && QXmppPingIq::isPingIq(element))
    {
        QXmppPingIq request;
        request.parse(element);

        QXmppIq response(QXmppIq::Result);
        response.setId(request.id());
        response.setFrom(request.to());
        response.setTo(request.from());
        server()->sendPacket(response);
        return true;
    }

    return false;
}

// PLUGIN

class XmppServerPingPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("ping"))
            return new XmppServerPing;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("ping");
    };
};

Q_EXPORT_PLUGIN2(ping, XmppServerPingPlugin)

