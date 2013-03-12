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

#include <QUrl>

#include "QDjangoHttpController.h"
#include "QDjangoHttpRequest.h"
#include "QDjangoHttpResponse.h"
#include "QDjangoUrlResolver.h"

#include "QXmppPasswordChecker.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"

#include "mod_auth_proxy.h"

ProxyResponse::ProxyResponse(QXmppPasswordReply *reply, bool isDigest)
    : m_isDigest(isDigest),
    m_isReady(false),
    m_reply(reply)
{
    bool check;
    Q_UNUSED(check);

    setHeader("Content-Type", "text/plain");

    reply->setParent(this);
    check = connect(reply, SIGNAL(finished()),
                    this, SLOT(onReply()));
    Q_ASSERT(check);
}

bool ProxyResponse::isReady() const
{
    return m_isReady;
}

void ProxyResponse::onReply()
{
    const QXmppPasswordReply::Error error = m_reply->error();
    if (error == QXmppPasswordReply::NoError) {
        if (m_isDigest)
            setBody(m_reply->digest().toHex());
    } else if (error == QXmppPasswordReply::AuthorizationError) {
        setStatusCode(QDjangoHttpResponse::NotFound);
    } else {
        setStatusCode(QDjangoHttpResponse::InternalServerError);
    }
    m_reply->deleteLater();

    m_isReady = true;
    emit ready();
}

class XmppAuthProxyPrivate
{
public:
    QString url;
    QString user;
    QString password;
};

XmppAuthProxy::XmppAuthProxy()
 : d(new XmppAuthProxyPrivate)
{
}

XmppAuthProxy::~XmppAuthProxy()
{
    delete d;
}

QString XmppAuthProxy::url() const
{
    return d->url;
}

void XmppAuthProxy::setUrl(const QString &url)
{
    d->url = url;
}

QString XmppAuthProxy::user() const
{
    return d->user;
}

void XmppAuthProxy::setUser(const QString &user)
{
    d->user = user;
}

QString XmppAuthProxy::password() const
{
    return d->password;
}

void XmppAuthProxy::setPassword(const QString &password)
{
    d->password = password;
}

QDjangoHttpResponse *XmppAuthProxy::respondToRequest(const QDjangoHttpRequest &request)
{
    // we need a password checker!
    QXmppPasswordChecker *checker = server()->passwordChecker();
    if (!checker)
        return serveInternalServerError(request);

    // POST is the current interface
    if (request.method() != "POST")
        return serveBadRequest(request);

    // check server credentials
    if (!d->user.isEmpty() && !d->password.isEmpty()) {
        QString username;
        QString password;
        if (!getBasicAuth(request, username, password) ||
            username != d->user ||
            password != d->password)
        {
            return serveAuthorizationRequired(request);
        }
    }

    // check username and domain were provided
    QUrl url;
    url.setEncodedQuery(request.body());
    if (!url.hasQueryItem("username") || !url.hasQueryItem("password"))
        return serveBadRequest(request);

    // check password
    QXmppPasswordRequest pwdRequest;
    pwdRequest.setDomain(server()->domain());
    pwdRequest.setUsername(url.queryItemValue("username"));
    pwdRequest.setPassword(url.queryItemValue("password"));

    QXmppPasswordReply *reply = checker->checkPassword(pwdRequest);
    return new ProxyResponse(reply, false);
}

bool XmppAuthProxy::start()
{
    // add HTTP interface
    QDjangoUrlResolver *urls = server()->findChild<QDjangoUrlResolver*>();
    if (!d->url.isEmpty() && urls) {
        QString rx(d->url);
        if (rx.startsWith("/"))
            rx.remove(0, 1);
        urls->set(QRegExp("^" + rx + "$"), this, "respondToRequest");
    }

    return true;
}

void XmppAuthProxy::stop()
{
}

// PLUGIN

class XmppAuthProxyPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("auth_proxy"))
            return new XmppAuthProxy;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("auth_proxy");
    };
};

Q_EXPORT_PLUGIN2(auth_proxy, XmppAuthProxyPlugin)

