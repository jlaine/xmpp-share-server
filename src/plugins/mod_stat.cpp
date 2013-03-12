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
#include <QSslSocket>

#include "QDjangoHttpController.h"
#include "QDjangoHttpRequest.h"
#include "QDjangoHttpResponse.h"
#include "QDjangoQuerySet.h"
#include "QDjangoUrlResolver.h"

#include "QXmppConstants.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppIncomingClient.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppUtils.h"

#include "mod_roster.h"
#include "mod_stat.h"

/// Serve list of clients.
///
QDjangoHttpResponse *XmppServerStat::serveClients(const QDjangoHttpRequest &request)
{
    QByteArray body;

    QXmlStreamWriter writer(&body);
    writer.writeStartDocument("1.0");
    writer.writeStartElement("clients");
    foreach(QXmppIncomingClient *client, server()->findChildren<QXmppIncomingClient*>()) {
        writer.writeStartElement("client");
        writer.writeAttribute("jid", client->jid());
        QSslSocket *socket = client->findChild<QSslSocket*>();
        if (socket) {
            writer.writeAttribute("remoteAddress", socket->peerAddress().toString());
            writer.writeAttribute("remotePort", QString::number(socket->peerPort()));
        }
        writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndDocument();

    QDjangoHttpResponse *response = new QDjangoHttpResponse;
    response->setHeader("Content-Type", "application/xml");
    response->setBody(body);
    return response;
}

/// Serve list of subscription requests.
///
QDjangoHttpResponse *XmppServerStat::serveRequests(const QDjangoHttpRequest &request)
{
    QByteArray body;

    QDjangoQuerySet<Contact> qs;
    qs = qs.filter(QDjangoWhere("ask", QDjangoWhere::Equals, 1));
    qs = qs.filter(QDjangoWhere("subscription", QDjangoWhere::Equals, 0));

    QXmlStreamWriter writer(&body);
    writer.writeStartDocument("1.0");
    writer.writeStartElement("requests");
    foreach(const Contact &contact, qs) {
        writer.writeStartElement("request");
        writer.writeAttribute("id", contact.pk().toString());
        writer.writeAttribute("to", contact.user());
        writer.writeAttribute("from", contact.jid());
        writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndDocument();

    QDjangoHttpResponse *response = new QDjangoHttpResponse;
    response->setHeader("Content-Type", "application/xml");
    response->setBody(body);
    return response;
}

/// Serve statistics over HTTP.
///
QDjangoHttpResponse *XmppServerStat::serveStatistics(const QDjangoHttpRequest &request, const QString &key)
{
    QVariantMap map = server()->statistics();
    if (map.contains(key)) {
        QDjangoHttpResponse *response = new QDjangoHttpResponse;
        response->setHeader("Content-Type", "text/plain");
        response->setBody(QByteArray::number(map.value(key).toInt()));
        return response;
    }
    return QDjangoHttpController::serveNotFound(request);
}

bool XmppServerStat::start()
{
    // add HTTP interface
    QDjangoUrlResolver *urls = server()->findChild<QDjangoUrlResolver*>();
    if (urls) {
        urls->set(QRegExp("^clients/$"), this, "serveClients");
        urls->set(QRegExp("^requests/$"), this, "serveRequests");
        urls->set(QRegExp("^stats/(.+)$"), this, "serveStatistics");
    }

    return true;
}

void XmppServerStat::stop()
{
}

// PLUGIN

class XmppServerStatPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("stats"))
            return new XmppServerStat;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("stats");
    };
};

Q_EXPORT_PLUGIN2(stat, XmppServerStatPlugin)

