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

#include <QBuffer>
#include <QDateTime>
#include <QDomElement>
#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "QDjangoHttpRequest.h"
#include "QDjangoHttpResponse.h"
#include "QDjangoQuerySet.h"
#include "QDjangoUrlResolver.h"

#include "QXmppConstants.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStream.h"
#include "QXmppUtils.h"

#include "QXmppDiagnosticIq.h"

#include "mod_diag.h"

#define FAST_RETRY 60
#define SLOW_RETRY 3600

QString Diagnostic::jid() const
{
    return m_jid;
}

void Diagnostic::setJid(const QString &jid)
{
    m_jid = jid;
}

QDateTime Diagnostic::queueStamp() const
{
    return m_queueStamp;
}

void Diagnostic::setQueueStamp(const QDateTime &stamp)
{
    m_queueStamp = stamp;
}

QDateTime Diagnostic::requestStamp() const
{
    return m_requestStamp;
}

void Diagnostic::setRequestStamp(const QDateTime &stamp)
{
    m_requestStamp = stamp;
}

QString Diagnostic::response() const
{
    return m_response;
}

void Diagnostic::setResponse(const QString &response)
{
    m_response = response;
}

QDateTime Diagnostic::responseStamp() const
{
    return m_responseStamp;
}

void Diagnostic::setResponseStamp(const QDateTime &stamp)
{
    m_responseStamp = stamp;
}

class XmppServerDiagPrivate
{
public:
    QString jid;
    bool httpAdminEnabled;
};

XmppServerDiag::XmppServerDiag()
    : d(new XmppServerDiagPrivate)
{
    QDjango::registerModel<Diagnostic>();
    QDjango::createTables();

    d->httpAdminEnabled = false;
}

XmppServerDiag::~XmppServerDiag()
{
    delete d;
}

QStringList XmppServerDiag::discoveryItems() const
{
    return QStringList() << d->jid;
}

QDjangoHttpResponse *XmppServerDiag::serveNodeDetail(const QDjangoHttpRequest &request, const QString &bareJid)
{
    QDjangoHttpResponse *response = new QDjangoHttpResponse;
    Diagnostic *diag = QDjangoQuerySet<Diagnostic>().get(
        QDjangoWhere("jid", QDjangoWhere::Equals, bareJid));
    if (request.method() == "GET")
    {
        // get a cache entry
        if (diag)
        {
            QDateTime modified = diag->queueStamp();
            if (diag->requestStamp() > modified)
                modified = diag->requestStamp();
            if (diag->responseStamp() > modified)
                modified = diag->responseStamp();

            response->setHeader("Content-Type", "application/xml; charset=utf-8");
            if (modified.isValid())
                response->setHeader("Last-Modified", httpDateTime(modified));
            response->setHeader("Expires", "0");
            response->setStatusCode(QDjangoHttpResponse::OK);
            QString body = QString("<node jid=\"%1\"").arg(diag->jid());
            if (diag->queueStamp().isValid())
                body += QString(" queueStamp=\"%2\"").arg(httpDateTime(diag->queueStamp()));
            if (diag->requestStamp().isValid())
                body += QString(" requestStamp=\"%2\"").arg(httpDateTime(diag->requestStamp()));
            if (diag->responseStamp().isValid())
                body += QString(" responseStamp=\"%2\"").arg(httpDateTime(diag->responseStamp()));
            body += ">";
            if (diag->responseStamp().isValid())
                body += diag->response();
            body += "</node>";
            response->setBody(body.toUtf8());
        } else {
            response->setStatusCode(QDjangoHttpResponse::NotFound);
        }
    }
    else if (request.method() == "POST")
    {
        // refresh a cache entry
        if (!diag)
        {
            diag = new Diagnostic;
            diag->setJid(bareJid);
        }
        diag->setQueueStamp(QDateTime::currentDateTime());
        if (diag->save())
        {
            refreshNode(diag, FAST_RETRY);
            response->setStatusCode(QDjangoHttpResponse::OK);
        } else
            response->setStatusCode(QDjangoHttpResponse::InternalServerError);
    }
    else if (request.method() == "DELETE")
    {
        // remove an entry from cache
        if (!diag)
            response->setStatusCode(QDjangoHttpResponse::NotFound);
        else if (diag->remove())
            response->setStatusCode(QDjangoHttpResponse::OK);
        else
            response->setStatusCode(QDjangoHttpResponse::InternalServerError);
    }
    else
    {
        // unknown method
        response->setStatusCode(QDjangoHttpResponse::MethodNotAllowed);
    }
    if (diag)
        delete diag;
    return response;
}

QDjangoHttpResponse *XmppServerDiag::serveNodeList(const QDjangoHttpRequest &request)
{
    QString body = QString("<nodes domain=\"%1\">").arg(server()->domain());
    QDjangoQuerySet<Diagnostic> diags;
    diags = diags.orderBy(QStringList() << "jid");
    Diagnostic diag;
    for (int i = 0; i < diags.size(); ++i)
    {
        if (!diags.at(i, &diag))
            continue;
        body += QString("<node jid=\"%1\"").arg(diag.jid());
        if (diag.queueStamp().isValid())
            body += QString(" queueStamp=\"%2\"").arg(httpDateTime(diag.queueStamp()));
        if (diag.requestStamp().isValid())
            body += QString(" requestStamp=\"%2\"").arg(httpDateTime(diag.requestStamp()));
        if (diag.responseStamp().isValid())
            body += QString(" responseStamp=\"%2\"").arg(httpDateTime(diag.responseStamp()));
        body += "/>";
    }
    body += "</nodes>";

    QDjangoHttpResponse *response = new QDjangoHttpResponse;
    response->setHeader("Content-Type", "application/xml; charset=utf-8");
    response->setBody(body.toUtf8());
    return response;
}

QDjangoHttpResponse* XmppServerDiag::serveSpeed(const QDjangoHttpRequest &request)
{
    QDjangoHttpResponse *response = new QDjangoHttpResponse;
    if (request.method() == "GET") {
        response->setHeader("Content-Type", "application/octet-stream");
        response->setBody(QByteArray(1024 * 1024, '0'));
    }
    response->setHeader("Access-Control-Allow-Origin", "*");
    return response;
}

bool XmppServerDiag::handleStanza(const QDomElement &stanza)
{
    if (stanza.attribute("to") != d->jid)
        return false;

    if (stanza.tagName() == "iq" && QXmppDiscoveryIq::isDiscoveryIq(stanza))
    {
        QXmppDiscoveryIq discoIq;
        discoIq.parse(stanza);

        if (discoIq.type() == QXmppIq::Get)
        {
            QXmppDiscoveryIq responseIq;
            responseIq.setFrom(discoIq.to());
            responseIq.setTo(discoIq.from());
            responseIq.setId(discoIq.id());
            responseIq.setType(QXmppIq::Result);
            responseIq.setQueryType(discoIq.queryType());

            if (discoIq.queryType() == QXmppDiscoveryIq::InfoQuery)
            {
                QStringList features = QStringList() << ns_disco_info << ns_disco_items << ns_diagnostics;
                QList<QXmppDiscoveryIq::Identity> identities;

                QXmppDiscoveryIq::Identity identity;
                identity.setCategory("diagnostics");
                identity.setType("server");
                identity.setName("Diagnostics server");
                identities.append(identity);

                responseIq.setFeatures(features);
                responseIq.setIdentities(identities);
            }
            server()->sendPacket(responseIq);

            // check whether we need to run diagnostics on this peer
            Diagnostic *diag = QDjangoQuerySet<Diagnostic>().get(
                    QDjangoWhere("jid", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(discoIq.from())));
            if (diag)
            {
                refreshNode(diag, FAST_RETRY);
                delete diag;
            }
            return true;
        }
    }
    else if (stanza.tagName() == "iq" && QXmppDiagnosticIq::isDiagnosticIq(stanza))
    {
        QXmppDiagnosticIq iq;
        iq.parse(stanza);

        if (iq.type() == QXmppIq::Result)
        {
            Diagnostic *diag = QDjangoQuerySet<Diagnostic>().get(
                    QDjangoWhere("jid", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(iq.from())));
            if (diag)
            {
                debug("Diagnostics response from " + diag->jid());
                QBuffer buffer;
                buffer.open(QIODevice::WriteOnly);
                QXmlStreamWriter writer(&buffer);
                iq.toXml(&writer);
                diag->setResponse(QString::fromUtf8(buffer.data()));
                diag->setResponseStamp(QDateTime::currentDateTime());
                diag->save();
                delete diag;
            }
        }
    }
 
    return true;
}

bool XmppServerDiag::httpAdminEnabled() const
{
    return d->httpAdminEnabled;
}

void XmppServerDiag::setHttpAdminEnabled(bool enabled)
{
    d->httpAdminEnabled = enabled;
}

QString XmppServerDiag::jid() const
{
    return d->jid;
}

void XmppServerDiag::setJid(const QString &jid)
{
    d->jid = jid;
}

void XmppServerDiag::refreshNode(Diagnostic *diag, int retrySeconds)
{
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-retrySeconds);
    if ((diag->responseStamp().isValid() && diag->responseStamp() > diag->queueStamp()) ||
        (diag->requestStamp().isValid() && diag->requestStamp() > cutoff))
        return;

    QXmppDiagnosticIq iq;
    iq.setType(QXmppIq::Get);
    iq.setFrom(d->jid);
    iq.setTo(diag->jid() + "/wiLink");
    debug("Diagnostics request to " + iq.to());
    server()->sendPacket(iq);

    diag->setRequestStamp(QDateTime::currentDateTime());
    diag->save();
}

bool XmppServerDiag::start()
{
    // determine jid
    if (d->jid.isEmpty())
        d->jid = "diagnostics." + server()->domain();

    // add HTTP interface
    QDjangoUrlResolver *urls = server()->findChild<QDjangoUrlResolver*>();
    if (urls) {
        urls->set(QRegExp("^speed/$"), this, "serveSpeed");
        if (d->httpAdminEnabled) {
            urls->set(QRegExp("^diagnostics/nodes/$"), this, "serveNodeList");
            urls->set(QRegExp("^diagnostics/nodes/(.+)$"), this, "serveNodeDetail");
        }
        return true;
    }

    return true;
}

// PLUGIN

class XmppServerDiagPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("diag"))
            return new XmppServerDiag;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("diag");
    };
};

Q_EXPORT_PLUGIN2(diag, XmppServerDiagPlugin)

