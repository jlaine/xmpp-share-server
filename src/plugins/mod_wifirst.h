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

#ifndef XMPP_SERVER_WIFIRST_H
#define XMPP_SERVER_WIFIRST_H

#include <QUrl>

#include "QXmppServerExtension.h"

class QAuthenticator;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class XmppServerWifirst : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "wifirst");
    Q_PROPERTY(QString url READ url WRITE setUrl)
    Q_PROPERTY(QString user READ user WRITE setUser)
    Q_PROPERTY(QString password READ password WRITE setPassword)

public:
    XmppServerWifirst();
    bool start();
    void stop();

    QString url() const;
    void setUrl(const QString &url);

    QString user() const;
    void setUser(const QString &user);

    QString password() const;
    void setPassword(const QString &password);

private slots:
    void authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
    void feedQueue();
    void handleBatch();
    void handleVCard();
    void handleVCardImage();
    void processQueue();

private:
    void batchFailed(const QString &error);
    QNetworkAccessManager *m_network;
    QTimer *m_timer;

    // config
    QUrl m_url;
    QString m_user;
    QString m_password;

    // state
    QUrl m_batchUrl;
    QMap<QString, QUrl> m_vcardJobs;
};

#endif
