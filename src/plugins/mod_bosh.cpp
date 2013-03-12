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

#include <QDomDocument>
#include <QStringList>
#include <QTimer>
#include <QXmlStreamWriter>

#include "QDjangoHttpController.h"
#include "QDjangoHttpRequest.h"
#include "QDjangoHttpResponse.h"
#include "QDjangoUrlResolver.h"

#include "QXmppIncomingBosh.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"

#include "mod_bosh.h"

static QDjangoHttpResponse *filterResponse(QDjangoHttpResponse *response) {
    response->setHeader("Access-Control-Allow-Origin", "*");
    response->setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->setHeader("Access-Control-Allow-Headers", "Content-Type");
    return response;
}

BoshResponse::BoshResponse()
    : m_ready(false)
{
}

bool BoshResponse::isReady() const
{
    return m_ready;
}

void BoshResponse::setReady(bool isReady)
{
    if (isReady == m_ready)
        return;

    m_ready = isReady;
    if (m_ready)
        emit ready();
}

class XmppServerBoshPrivate
{
public:
    QMap<QString, QXmppIncomingBosh*> sessions;
    QMap<QXmppIncomingBosh*, BoshResponse*> responses;
};

XmppServerBosh::XmppServerBosh()
    : d(new XmppServerBoshPrivate)
{
}

XmppServerBosh::~XmppServerBosh()
{
    delete d;
}

QDjangoHttpResponse *XmppServerBosh::respondToRequest(const QDjangoHttpRequest &request)
{
    if (request.method() == "GET") {
        QDjangoHttpResponse *response = new QDjangoHttpResponse;
        response->setHeader("Content-Type", "text/html; charset=utf-8");
        response->setBody("<html><body><a href=\"http://www.xmpp.org/extensions/xep-0124.html\">XEP-0124</a> - BOSH</body></html>");
        return filterResponse(response);
    } else if (request.method() == "OPTIONS") {
        return filterResponse(new QDjangoHttpResponse);
    } else if (request.method() != "POST") {
        return filterResponse(QDjangoHttpController::serveBadRequest(request));
    }

    // parse XML
    QDomDocument doc;
    doc.setContent(request.body(), true);
    QDomElement body = doc.documentElement();
    if (body.tagName() != "body" || body.attribute("rid").isEmpty())
        return filterResponse(QDjangoHttpController::serveBadRequest(request));

    // check the session ID is correct
    const QString sid = body.attribute("sid");
    if (!sid.isEmpty() && !d->sessions.contains(sid))
        return filterResponse(QDjangoHttpController::serveNotFound(request));

    QXmppIncomingBosh *stream = 0;
    if (sid.isEmpty()) {
        // create a new session
        stream = new QXmppIncomingBosh(server()->domain(), server());
        connect(stream, SIGNAL(disconnected()), this, SLOT(streamDisconnected()));
        connect(stream, SIGNAL(readyRead()), this, SLOT(writeData()));
        d->sessions[stream->id()] = stream;

        server()->addIncomingClient(stream);
        stream->handleStream(body);
    } else {
        // request for an existing session
        if (d->responses.contains(stream)) {
            BoshResponse *old = d->responses.take(stream);
            old->setReady(true);
        }

        stream = d->sessions[sid];
        bool handled = false;

        if (body.attribute("restart") == "true") {
            stream->handleStream(body);
            handled = true;
        }

        QDomElement stanza = body.firstChildElement();
        while (!stanza.isNull()) {
            stream->handleStanza(stanza);
            handled = true;
            stanza = stanza.nextSiblingElement();
        }

        // if we did not feed anything to the stream, pass it an empty
        // element to reset the idle timer
        if (!handled)
            stream->handleStanza(QDomElement());
    }

    // generate HTTP response
    BoshResponse *response = new BoshResponse;
    response->setHeader("Content-Type", "application/xml; charset=utf-8");
    response->setReady(stream->hasPendingData());
    response->setBody(stream->readPendingData());
    if (!response->isReady()) {
        connect(response, SIGNAL(destroyed(QObject*)), this, SLOT(responseDestroyed(QObject*)));
        connect(response, SIGNAL(expired()), this, SLOT(responseExpired()));
        QTimer::singleShot(stream->wait() * 1000, response, SIGNAL(expired()));
        d->responses[stream] = response;
    }
    return filterResponse(response);
}

void XmppServerBosh::responseDestroyed(QObject *obj)
{
    QXmppIncomingBosh *stream = d->responses.key(static_cast<BoshResponse*>(obj));
    d->responses.remove(stream);
}

void XmppServerBosh::responseExpired()
{
    BoshResponse *response = qobject_cast<BoshResponse*>(sender());
    if (!response)
        return;
    response->setReady(true);
    d->responses.remove(d->responses.key(response));
}

void XmppServerBosh::streamDisconnected()
{
    QXmppIncomingBosh *stream = qobject_cast<QXmppIncomingBosh*>(sender());
    if (!stream)
        return;
    d->sessions.remove(stream->id());
}

void XmppServerBosh::writeData()
{
    QXmppIncomingBosh *stream = qobject_cast<QXmppIncomingBosh*>(sender());
    if (!stream)
        return;

    BoshResponse *response = d->responses.value(stream);
    if (!response)
        return;

    response->setBody(stream->readPendingData());
    response->setReady(true);
    d->responses.remove(stream);
}

bool XmppServerBosh::start()
{
    QDjangoUrlResolver *urls = server()->findChild<QDjangoUrlResolver*>();
    if (urls) {
        urls->set(QRegExp("^http-bind/$"), this, "respondToRequest");
        return true;
    }
    return false;
}

// PLUGIN

class XmppServerBoshPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("bosh"))
            return new XmppServerBosh;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("bosh");
    };
};

Q_EXPORT_PLUGIN2(bosh, XmppServerBoshPlugin)

