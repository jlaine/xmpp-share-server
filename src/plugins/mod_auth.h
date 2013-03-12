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

#ifndef XMPP_SERVER_AUTH_H
#define XMPP_SERVER_AUTH_H

#include <QDateTime>
#include <QStringList>

#include "QDjangoModel.h"

#include "QXmppPasswordChecker.h"
#include "QXmppServerExtension.h"

class QAuthenticator;
class QNetworkReply;
class XmppPasswordCheckerPrivate;

class HttpPasswordReply : public QXmppPasswordReply
{
    Q_OBJECT

public:
    HttpPasswordReply(QNetworkReply *reply);

private slots:
    void onFinished();

private:
    QNetworkReply *m_networkReply;
};

class XmppPasswordChecker : public QXmppServerExtension, QXmppPasswordChecker
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "auth");
    Q_PROPERTY(QString url READ url WRITE setUrl);
    Q_PROPERTY(QString user READ user WRITE setUser);
    Q_PROPERTY(QString password READ password WRITE setPassword);
    Q_PROPERTY(QString searchBase READ searchBase WRITE setSearchBase);
    Q_PROPERTY(QString searchPattern READ searchPattern WRITE setSearchPattern);

public:
    XmppPasswordChecker();
    ~XmppPasswordChecker();

    QString url() const;
    void setUrl(const QString &url);

    QString user() const;
    void setUser(const QString &user);

    QString password() const;
    void setPassword(const QString &password);

    QString searchBase() const;
    void setSearchBase(const QString &searchBase);
 
    QString searchPattern() const;
    void setSearchPattern(const QString &searchPattern);

    // password checker
    QXmppPasswordReply *checkPassword(const QXmppPasswordRequest &request);
    QXmppPasswordReply *getDigest(const QXmppPasswordRequest &request);
    QXmppPasswordReply::Error getPassword(const QXmppPasswordRequest &request, QString &password);
    bool hasGetPassword() const;

    // extension
    bool start();
    void stop();

private slots:
    void onDigestAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
    void onDigestReply();
    void onPasswordReply();

private:
    XmppPasswordCheckerPrivate * const d;
};

#endif
