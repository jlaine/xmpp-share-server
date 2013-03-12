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
#include <QStringList>

#include "QDjangoQuerySet.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppUtils.h"

#include "mod_privacy.h"
#include "mod_roster.h"

static const char *ns_privacy = "jabber:iq:privacy";

class XmppServerPrivacyPrivate
{
public:
    bool enabled;
};

XmppServerPrivacy::XmppServerPrivacy()
    : d(new XmppServerPrivacyPrivate)
{
    d->enabled = false;
}

XmppServerPrivacy::~XmppServerPrivacy()
{
    delete d;
}

bool XmppServerPrivacy::enabled() const
{
    return d->enabled;
}

void XmppServerPrivacy::setEnabled(bool enabled)
{
    d->enabled = enabled;
}

QStringList XmppServerPrivacy::discoveryFeatures() const
{
    return QStringList() << ns_privacy;
}

int XmppServerPrivacy::extensionPriority() const
{
    // make sure this handles messages before mod_archive
    return 1;
}

bool XmppServerPrivacy::handleStanza(const QDomElement &element)
{
    if (!d->enabled
        || element.tagName() != "message"
        || element.attribute("type") == "error"
        || element.attribute("type") == "groupchat"
        || element.attribute("type") == "headline")
        return false;

    const QString from = element.attribute("from");
    const QString to = element.attribute("to");
    const QString domain = server()->domain();
    if (QXmppUtils::jidToDomain(to) == domain && QXmppUtils::jidToBareJid(to) != domain) {
        QDjangoQuerySet<Contact> qs;
        qs = qs.filter(QDjangoWhere("user", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(to)));
        qs = qs.filter(QDjangoWhere("jid", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(from)));
        qs = qs.filter(QDjangoWhere("subscription", QDjangoWhere::Equals, QXmppRosterIq::Item::From) || QDjangoWhere("subscription", QDjangoWhere::Equals, QXmppRosterIq::Item::Both));
        if (qs.count() == 0) {
            warning("Dropping message from " + from + " to " + to);
            return true;
        }
    }
    return false;
}

// PLUGIN

class XmppServerPrivacyPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("privacy"))
            return new XmppServerPrivacy;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("privacy");
    };
};

Q_EXPORT_PLUGIN2(privacy, XmppServerPrivacyPlugin)

