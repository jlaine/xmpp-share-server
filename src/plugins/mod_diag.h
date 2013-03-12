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

#ifndef DIAG_SERVER_H
#define DIAG_SERVER_H

#include "QDjangoHttpController.h"
#include "QDjangoModel.h"

#include "QXmppServerExtension.h"

class QDomElement;
class XmppServerDiagPrivate;

class Diagnostic : public QDjangoModel
{
    Q_OBJECT
    Q_PROPERTY(QString jid READ jid WRITE setJid)
    Q_PROPERTY(QDateTime queueStamp READ queueStamp WRITE setQueueStamp)
    Q_PROPERTY(QDateTime requestStamp READ requestStamp WRITE setRequestStamp)
    Q_PROPERTY(QString response READ response WRITE setResponse)
    Q_PROPERTY(QDateTime responseStamp READ responseStamp WRITE setResponseStamp)

    Q_CLASSINFO("jid", "max_length=255 primary_key=true");

public:
    QString jid() const;
    void setJid(const QString &jid);

    QDateTime queueStamp() const;
    void setQueueStamp(const QDateTime &stamp);

    QDateTime requestStamp() const;
    void setRequestStamp(const QDateTime &stamp);

    QString response() const;
    void setResponse(const QString &response);

    QDateTime responseStamp() const;
    void setResponseStamp(const QDateTime &stamp);

private:
    QString m_jid;
    QDateTime m_queueStamp;
    QDateTime m_requestStamp;
    QString m_response;
    QDateTime m_responseStamp;
};

class XmppServerDiag : public QXmppServerExtension, public QDjangoHttpController
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "diag");
    Q_PROPERTY(bool httpAdminEnabled READ httpAdminEnabled WRITE setHttpAdminEnabled);
    Q_PROPERTY(QString jid READ jid WRITE setJid);

public:
    XmppServerDiag();
    ~XmppServerDiag();

    bool httpAdminEnabled() const;
    void setHttpAdminEnabled(bool enabled);

    QString jid() const;
    void setJid(const QString &jid);

    /// \cond
    QStringList discoveryItems() const;
    bool handleStanza(const QDomElement &element);
    bool start();
    /// \endcond

private slots:
    QDjangoHttpResponse* serveNodeDetail(const QDjangoHttpRequest &request, const QString &bareJid);
    QDjangoHttpResponse* serveNodeList(const QDjangoHttpRequest &request);
    QDjangoHttpResponse* serveSpeed(const QDjangoHttpRequest &request);

private:
    void refreshNode(Diagnostic *diag, int retrySeconds);

    XmppServerDiagPrivate * const d;
};

#endif
