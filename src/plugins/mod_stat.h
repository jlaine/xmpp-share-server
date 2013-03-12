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

#ifndef XMPP_SERVER_STATS_H
#define XMPP_SERVER_STATS_H

#include "QXmppServerExtension.h"

class QDjangoHttpRequest;
class QDjangoHttpResponse;
class XmppServerStatPrivate;

/// \brief QXmppServer extension for statistics.
///

class XmppServerStat : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "stat");

public:
    /// \cond
    bool start();
    void stop();
    /// \endcond

private slots:
    QDjangoHttpResponse *serveClients(const QDjangoHttpRequest &request);
    QDjangoHttpResponse *serveRequests(const QDjangoHttpRequest &request);
    QDjangoHttpResponse *serveStatistics(const QDjangoHttpRequest &request, const QString &key);
};

#endif
