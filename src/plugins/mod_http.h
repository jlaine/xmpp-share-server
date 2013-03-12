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

#ifndef XMPP_SERVER_HTTP_H
#define XMPP_SERVER_HTTP_H

#include "QXmppServerExtension.h"

class QDjangoHttpRequest;
class QDjangoHttpResponse;
class XmppServerHttpPrivate;

class XmppServerHttp : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "http");
    Q_PROPERTY(QString host READ host WRITE setHost);
    Q_PROPERTY(quint16 port READ port WRITE setPort);
    Q_PROPERTY(QString staticRoot READ staticRoot WRITE setStaticRoot);
    Q_PROPERTY(QString staticUrl READ staticUrl WRITE setStaticUrl);

public:
    XmppServerHttp();
    ~XmppServerHttp();

    QString host() const;
    void setHost(const QString &host);

    quint16 port() const;
    void setPort(quint16 port);

    QString staticRoot() const;
    void setStaticRoot(const QString &staticRoot);

    QString staticUrl() const;
    void setStaticUrl(const QString &staticUrl);

    bool start();
    void stop();

private slots:
    QDjangoHttpResponse *_q_serveStatic(const QDjangoHttpRequest &request, const QString &path);
    void _q_requestFinished(QDjangoHttpRequest *request, QDjangoHttpResponse *response);

private:
    XmppServerHttpPrivate * const d;
};

#endif
