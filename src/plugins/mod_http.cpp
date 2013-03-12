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

#include <QDir>
#include <QFileInfo>
#include <QRegExp>
#include <QStringList>

#include "QDjangoHttpController.h"
#include "QDjangoHttpRequest.h"
#include "QDjangoHttpResponse.h"
#include "QDjangoHttpServer.h"
#include "QDjangoUrlResolver.h"

#include "QXmppServer.h"
#include "QXmppServerPlugin.h"

#include "mod_http.h"

class XmppServerHttpPrivate
{
public:
    QDjangoHttpServer *httpServer;

    // config
    QString host;
    quint16 port;
    QString staticRoot;
    QString staticUrl;
};

XmppServerHttp::XmppServerHttp()
    : d(new XmppServerHttpPrivate)
{
    bool check;
    Q_UNUSED(check);

    d->staticRoot = "/var/lib/xmpp-share-server/public";
    d->staticUrl = "/static/";
    d->httpServer = new QDjangoHttpServer(this);

    d->host = "0.0.0.0";
    d->port = 5280;

    check = connect(d->httpServer, SIGNAL(requestFinished(QDjangoHttpRequest*,QDjangoHttpResponse*)),
                    this, SLOT(_q_requestFinished(QDjangoHttpRequest*,QDjangoHttpResponse*)));
    Q_ASSERT(check);
}

XmppServerHttp::~XmppServerHttp()
{
    delete d;
}

QString XmppServerHttp::host() const
{
    return d->host;
}

void XmppServerHttp::setHost(const QString &host)
{
    d->host = host;
}

quint16 XmppServerHttp::port() const
{
    return d->port;
}

void XmppServerHttp::setPort(quint16 port)
{
    d->port = port;
}

QString XmppServerHttp::staticRoot() const
{
    return d->staticRoot;
}

void XmppServerHttp::setStaticRoot(const QString &staticRoot)
{
    d->staticRoot = staticRoot;
}

QString XmppServerHttp::staticUrl() const
{
    return d->staticUrl;
}

void XmppServerHttp::setStaticUrl(const QString &staticUrl)
{
    d->staticUrl = staticUrl;
}

bool XmppServerHttp::start()
{
    if (!d->httpServer->listen(QHostAddress(d->host), d->port))
        return false;

    if (!d->staticRoot.isEmpty() && !d->staticUrl.isEmpty()) {
        QString rx(d->staticUrl);
        while (rx.startsWith('/'))
            rx.remove(0, 1);
        if (!rx.endsWith('/'))
            rx.append('/');

        d->httpServer->urls()->set(QRegExp("^" + rx + "(.+)$"), this, "_q_serveStatic");
    }
    return true;
}

void XmppServerHttp::stop()
{
    d->httpServer->close();
}

void XmppServerHttp::_q_requestFinished(QDjangoHttpRequest *request, QDjangoHttpResponse *response)
{
    const QString referer = request->meta("HTTP_REFERER");
    const QString userAgent = request->meta("HTTP_USER_AGENT");

    info(QString("HTTP request \"%1 %2 HTTP/%3\" %4 %5 \"%6\" \"%7\"").arg(
        request->method(),
        request->path(),
        QLatin1String("1.1"),
        QString::number(response->statusCode()),
        QString::number(response->body().size()),
        referer.isEmpty() ? QLatin1String("-") : referer,
        userAgent.isEmpty() ? QLatin1String("-") : userAgent));
}

QDjangoHttpResponse *XmppServerHttp::_q_serveStatic(const QDjangoHttpRequest &request, const QString &path)
{
    if (!path.contains("..")) {
        const QString filePath = QDir(d->staticRoot).filePath(path);
        QDjangoHttpResponse *response = QDjangoHttpController::serveStatic(request, filePath);
        response->setHeader("Access-Control-Allow-Origin", "*");
        return response;
    }
    return QDjangoHttpController::serveNotFound(request);
}

// PLUGIN

class XmppServerHttpPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("http"))
            return new XmppServerHttp;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("http");
    };
};

Q_EXPORT_PLUGIN2(http, XmppServerHttpPlugin)

