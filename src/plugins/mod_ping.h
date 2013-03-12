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

#ifndef XMPP_SERVER_PING_H
#define XMPP_SERVER_PING_H

#include "QXmppServerExtension.h"

/// \brief QXmppServer extension for XEP-0199: XMPP Ping.
///

class XmppServerPing : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "ping");

public:
    QStringList discoveryFeatures() const;
    bool handleStanza(const QDomElement &element);
};

#endif
