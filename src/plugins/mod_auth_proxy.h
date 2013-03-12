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

#ifndef XMPP_SERVER_AUTH_PROXY_H
#define XMPP_SERVER_AUTH_PROXY_H

#include <QStringList>

#include "QDjangoHttpController.h"
#include "QDjangoHttpResponse.h"

#include "QXmppServerExtension.h"

class QXmppPasswordReply;
class XmppAuthProxyPrivate;

class ProxyResponse : public QDjangoHttpResponse
{
    Q_OBJECT

public:
    ProxyResponse(QXmppPasswordReply *reply, bool isDigest);
    bool isReady() const;

private slots:
    void onReply();

private:
    bool m_isDigest;
    bool m_isReady;
    QXmppPasswordReply *m_reply;
};

class XmppAuthProxy : public QXmppServerExtension, public QDjangoHttpController
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "auth_proxy");
    Q_PROPERTY(QString url READ url WRITE setUrl);
    Q_PROPERTY(QString user READ user WRITE setUser);
    Q_PROPERTY(QString password READ password WRITE setPassword);

public:
    XmppAuthProxy();
    ~XmppAuthProxy();

    QString url() const;
    void setUrl(const QString &url);

    QString user() const;
    void setUser(const QString &user);

    QString password() const;
    void setPassword(const QString &password);

    // extension
    bool start();
    void stop();

private slots:
    QDjangoHttpResponse *respondToRequest(const QDjangoHttpRequest &request);

private:
    XmppAuthProxyPrivate * const d;
};

#endif
