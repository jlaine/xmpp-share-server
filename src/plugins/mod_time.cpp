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
#include "QXmppEntityTimeIq.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStream.h"

#include "mod_time.h"

QStringList XmppServerTime::discoveryFeatures() const
{
    return QStringList() << ns_entity_time;
}

bool XmppServerTime::handleStanza(const QDomElement &element)
{
    if (element.attribute("to") != server()->domain())
        return false;

    // XEP-0202: Entity Time
    if(QXmppEntityTimeIq::isEntityTimeIq(element))
    {
        QXmppEntityTimeIq timeIq;
        timeIq.parse(element);

        if (timeIq.type() == QXmppIq::Get)
        {
            QXmppEntityTimeIq responseIq;
            responseIq.setType(QXmppIq::Result);
            responseIq.setId(timeIq.id());
            responseIq.setTo(timeIq.from());

            QDateTime currentTime = QDateTime::currentDateTime();
            QDateTime utc = currentTime.toUTC();
            responseIq.setUtc(utc);

            currentTime.setTimeSpec(Qt::UTC);
            responseIq.setTzo(utc.secsTo(currentTime));

            server()->sendPacket(responseIq);
        }
        return true;
    }

    return false;
}

// PLUGIN

class XmppServerTimePlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("time"))
            return new XmppServerTime;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("time");
    };
};

Q_EXPORT_PLUGIN2(time, XmppServerTimePlugin)

