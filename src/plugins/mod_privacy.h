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

#ifndef XMPP_SERVER_PRIVACY_H
#define XMPP_SERVER_PRIVACY_H

#include "QXmppServerExtension.h"

class XmppServerPrivacyPrivate;

/// \brief QXmppServer extension for privacy lists.
///

class XmppServerPrivacy : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "privacy");
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled)

public:
    XmppServerPrivacy();
    ~XmppServerPrivacy();

    bool enabled() const;
    void setEnabled(bool enabled);

    QStringList discoveryFeatures() const;
    int extensionPriority() const;
    bool handleStanza(const QDomElement &element);

private:
    friend class XmppServerPrivacyPrivate;
    XmppServerPrivacyPrivate *d;
};

#endif
