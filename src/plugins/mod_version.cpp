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
#include "QXmppVersionIq.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStream.h"

#include "mod_version.h"

QStringList XmppServerVersion::discoveryFeatures() const
{
    return QStringList() << ns_version;
}

bool XmppServerVersion::handleStanza(const QDomElement &element)
{
    if (element.attribute("to") != server()->domain())
        return false;

    // XEP-0092: Software Version
    if(QXmppVersionIq::isVersionIq(element))
    {
        QXmppVersionIq versionIq;
        versionIq.parse(element);

        if (versionIq.type() == QXmppIq::Get)
        {
            QXmppVersionIq responseIq;
            responseIq.setType(QXmppIq::Result);
            responseIq.setId(versionIq.id());
            responseIq.setTo(versionIq.from());
            responseIq.setName(qApp->applicationName());
            responseIq.setVersion(qApp->applicationVersion());
            server()->sendPacket(responseIq);
        }
        return true;
    }

    return false;
}

// PLUGIN

class XmppServerVersionPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("version"))
            return new XmppServerVersion;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("version");
    };
};

Q_EXPORT_PLUGIN2(version, XmppServerVersionPlugin)

