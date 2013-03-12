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
#include <QDomElement>

#include "QXmppConstants.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStream.h"

#include "mod_disco.h"

QStringList XmppServerDiscovery::items() const
{
    return m_discoveryItems;
}

void XmppServerDiscovery::setItems(const QStringList &items)
{
    m_discoveryItems = items;
}

QStringList XmppServerDiscovery::discoveryFeatures() const
{
    return QStringList() << ns_disco_info << ns_disco_items << ns_rsm;
}

QStringList XmppServerDiscovery::discoveryItems() const
{
    return m_discoveryItems;
}

bool XmppServerDiscovery::handleStanza(const QDomElement &element)
{
    if (element.attribute("to") != server()->domain())
        return false;

    // XEP-0030: Service Discovery
    const QString type = element.attribute("type");
    if (element.tagName() == "iq" && QXmppDiscoveryIq::isDiscoveryIq(element) && type == "get")
    {
        QXmppDiscoveryIq request;
        request.parse(element);

        QXmppDiscoveryIq response;
        response.setType(QXmppIq::Result);
        response.setId(request.id());
        response.setFrom(request.to());
        response.setTo(request.from());
        response.setQueryType(request.queryType());

        if (request.queryType() == QXmppDiscoveryIq::ItemsQuery)
        {
            QList<QXmppDiscoveryIq::Item> items;
            foreach (QXmppServerExtension *extension, server()->extensions())
            {
                foreach (const QString &jid, extension->discoveryItems())
                {
                    QXmppDiscoveryIq::Item item;
                    item.setJid(jid);
                    items.append(item);
                }
            }
            response.setItems(items);
        } else {
            // identities
            QList<QXmppDiscoveryIq::Identity> identities;
            QXmppDiscoveryIq::Identity identity;
            identity.setCategory("server");
            identity.setType("im");
            identity.setName(qApp->applicationName());
            identities.append(identity);
            response.setIdentities(identities);

            // features
            QStringList features;
            foreach (QXmppServerExtension *extension, server()->extensions())
                features += extension->discoveryFeatures();
            response.setFeatures(features);
        }
        server()->sendPacket(response);
        return true;
    }
    return false;
}

// PLUGIN

class XmppServerDiscoveryPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("disco"))
            return new XmppServerDiscovery;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("disco");
    };
};

Q_EXPORT_PLUGIN2(disco, XmppServerDiscoveryPlugin)

