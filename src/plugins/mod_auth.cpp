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

#include <QAuthenticator>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

#include "QDjangoQuerySet.h"

#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppUtils.h"

#include "mod_auth.h"

enum Backend {
    FileBackend,
    HttpBackend,
    SqlBackend,
};

class Cache
{
public:
    Cache();
    ~Cache();

    QString get(const QString &key) const;
    bool set(const QString &key, const QString &value, int expiration = 0);

private:
    QHash<QString, QPair<QString, QDateTime> > m_values;
};

Cache::Cache()
{
}

Cache::~Cache()
{
}

QString Cache::get(const QString &key) const
{
    if (m_values.contains(key)) {
        QPair<QString, QDateTime> entry = m_values.value(key);
        if (entry.second > QDateTime::currentDateTime()) {
            qDebug("Cache hit for '%s'", qPrintable(key));
            return entry.first;
        }
    }

    qDebug("Cache miss for '%s'", qPrintable(key));
    return QString();
}

bool Cache::set(const QString &key, const QString &value, int expiration)
{
    qDebug("Cache set for '%s'", qPrintable(key));

    if (key.isEmpty()) {
        return false;
    } else {
        QPair<QString, QDateTime> entry;
        entry.first = value;
        if (expiration > 0)
            entry.second = QDateTime::currentDateTime().addSecs(expiration);
        m_values.insert(key, entry);
        return true;
    }
}

HttpPasswordReply::HttpPasswordReply(QNetworkReply *reply)
    : m_networkReply(reply)
{
    connect(m_networkReply, SIGNAL(finished()),
            this, SLOT(onFinished()));
}

void HttpPasswordReply::onFinished()
{
    if (m_networkReply->error() == QNetworkReply::NoError) {
        setDigest(QByteArray::fromHex(m_networkReply->readAll()));
    } else if (m_networkReply->error() == QNetworkReply::ContentNotFoundError) {
        setError(QXmppPasswordReply::AuthorizationError);
    } else {
        setError(QXmppPasswordReply::TemporaryError);
    }
    m_networkReply->deleteLater();
    finish();
}

class XmppPasswordCheckerPrivate
{
public:
    Backend backend;
    Cache cache;
    QNetworkAccessManager *network;
    QUrl url;
    QString user;
    QString password;
    QString searchBase;
    QString searchPattern;
    QSettings *settings;
    QSqlDatabase sqlDatabase;
    QString sqlGetQuery;
};

XmppPasswordChecker::XmppPasswordChecker()
    : d(new XmppPasswordCheckerPrivate)
{
    d->network = 0;
    d->url = QUrl("https://www.wifirst.net/http-auth/");
    d->settings = 0;
}

XmppPasswordChecker::~XmppPasswordChecker()
{
    delete d;
}

static QString hashPassword(const QString &username, const QString &domain, const QString &password)
{
    const QByteArray input = QString("%1:%2:%3").arg(username, domain, password).toUtf8();
    return QCryptographicHash::hash(input, QCryptographicHash::Md5).toHex();
}

QXmppPasswordReply* XmppPasswordChecker::checkPassword(const QXmppPasswordRequest &request)
{
    const QDateTime now = QDateTime::currentDateTime();

    QXmppPasswordReply *reply = 0;

    // check cache
    QString cachedPassword = d->cache.get(QString("%1@%2").arg(request.username(), request.domain()));
    if (!cachedPassword.isNull() &&
        (hashPassword(request.username(), request.domain(), request.password()) == cachedPassword))
    {
        reply = new QXmppPasswordReply;
        reply->finishLater();
        return reply;
    }

    // perform authentication check
    if (d->backend == HttpBackend) {

        QNetworkRequest networkRequest(d->url.toString());
        networkRequest.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
        networkRequest.setRawHeader("User-Agent", QString(qApp->applicationName() + "/" + qApp->applicationVersion()).toAscii());
        QUrl url;
        url.addQueryItem("domain", request.domain());
        url.addQueryItem("username", request.username());
        url.addQueryItem("password", request.password());
        QNetworkReply *networkReply = d->network->post(networkRequest, url.encodedQuery());

        // reply
        reply = new HttpPasswordReply(networkReply);
    } else {

        // query base implementation
        reply = QXmppPasswordChecker::checkPassword(request);
    }

    // schedule cache update
    reply->setProperty("__cache_key", QString("%1@%2").arg(request.username(), request.domain()));
    reply->setProperty("__cache_value", hashPassword(request.username(), request.domain(), request.password()));
    connect(reply, SIGNAL(finished()), this, SLOT(onPasswordReply()));
    return reply;
}

void XmppPasswordChecker::onDigestAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    if (!d->user.isEmpty() && !d->password.isEmpty())
    {
        authenticator->setUser(d->user);
        authenticator->setPassword(d->password);
    }
}

void XmppPasswordChecker::onDigestReply()
{
    QXmppPasswordReply *reply = qobject_cast<QXmppPasswordReply*>(sender());
    if (!reply)
        return;

    // update cache
    if (reply->error() == QXmppPasswordReply::NoError) {
        const QString key = reply->property("__cache_key").toString();
        const QString value = reply->digest().toHex();
        d->cache.set(key, value, 24*3600);
    }
}

void XmppPasswordChecker::onPasswordReply()
{
    QXmppPasswordReply *reply = qobject_cast<QXmppPasswordReply*>(sender());
    if (!reply)
        return;

    // update cache
    if (reply->error() == QXmppPasswordReply::NoError) {
        const QString key = reply->property("__cache_key").toString();
        const QString value = reply->property("__cache_value").toString();
        d->cache.set(key, value, 3600);
    }
}

QXmppPasswordReply *XmppPasswordChecker::getDigest(const QXmppPasswordRequest &request)
{
    const QDateTime now = QDateTime::currentDateTime();
    QXmppPasswordReply *reply = 0;

    // check cache
    QString cachedPassword = d->cache.get(QString("%1@%2").arg(request.username(), request.domain()));
    if (!cachedPassword.isNull()) {
        reply = new QXmppPasswordReply;
        reply->setDigest(QByteArray::fromHex(cachedPassword.toAscii()));
        reply->finishLater();
        return reply;
    }

    if (d->backend == HttpBackend && d->url.isValid()) {
        QNetworkRequest networkRequest(d->url.toString());
        networkRequest.setRawHeader("User-Agent", QString(qApp->applicationName() + "/" + qApp->applicationVersion()).toAscii());
        QUrl url;
        url.addQueryItem("username", request.username());
        url.addQueryItem("domain", request.domain());
        QNetworkReply *networkReply = d->network->post(networkRequest, url.encodedQuery());

        // reply
        reply = new HttpPasswordReply(networkReply);

    } else {

        // query base implementation
        reply = QXmppPasswordChecker::getDigest(request);
    }
    Q_ASSERT(reply);

    // schedule cache update
    reply->setProperty("__cache_key", QString("%1@%2").arg(request.username(), request.domain()));
    connect(reply, SIGNAL(finished()), this, SLOT(onDigestReply()));
    return reply;
}

QXmppPasswordReply::Error XmppPasswordChecker::getPassword(const QXmppPasswordRequest &request, QString &password)
{
    if (d->backend == FileBackend) {
        const QString key = request.domain() + "/" + request.username();
        if (d->settings->status() != QSettings::NoError)
            return QXmppPasswordReply::TemporaryError;
        if (!d->settings->contains(key))
            return QXmppPasswordReply::AuthorizationError;
        password = d->settings->value(key).toString();
        return QXmppPasswordReply::NoError;
    }
    else if (d->backend == SqlBackend) {
        const QString jid = request.username() + "@" + request.domain();
        QSqlQuery query(d->sqlDatabase);
        query.prepare(d->sqlGetQuery);
        query.addBindValue(jid);
        if (!query.exec()) {
            warning(QString("Could not retrieve password from SQL for %1: %2").arg(jid, query.lastError().text()));
            return QXmppPasswordReply::TemporaryError;
        }
        if (!query.next() || query.value(0).toString() != jid)
            return QXmppPasswordReply::AuthorizationError;
        password = query.value(1).toString();
        return QXmppPasswordReply::NoError;
    }
    return QXmppPasswordReply::TemporaryError;
}

bool XmppPasswordChecker::hasGetPassword() const
{
    return d->backend == FileBackend || d->backend == SqlBackend; 
}

bool XmppPasswordChecker::start()
{
    const QString scheme = d->url.scheme();
    if (scheme == "file") {
        d->backend = FileBackend;
        d->settings = new QSettings(d->url.toLocalFile(), QSettings::IniFormat, this);
        if (d->settings->status() != QSettings::NoError) {
            warning("Cannot open password file");
            return false;
        }
    }
    else if (scheme == "http" || scheme == "https") {
        d->backend = HttpBackend;
        d->network = new QNetworkAccessManager(this);
        connect(d->network, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
                this, SLOT(onDigestAuthenticationRequired(QNetworkReply*,QAuthenticator*)));
    }
    else if (scheme == "mysql") {
        if (!d->url.hasQueryItem("jid_field") || !d->url.hasQueryItem("password_field")) {
            warning("SQL auth URL requires jid_field and password_field");
            return false;
        }

        d->backend = SqlBackend;
        d->sqlDatabase = QSqlDatabase::addDatabase("QMYSQL", "_auth_connection");
        d->sqlDatabase.setConnectOptions("MYSQL_OPT_RECONNECT=1");
        d->sqlDatabase.setDatabaseName(d->url.path().mid(1));
        d->sqlDatabase.setHostName(d->url.host());
        d->sqlDatabase.setUserName(d->user);
        d->sqlDatabase.setPassword(d->password);
        if (!d->sqlDatabase.open()) {
            warning("Cannot open SQL database");
            return false;
        }
        d->sqlGetQuery = QString("SELECT %1, %2 FROM %3 WHERE %4 = ?").arg(
            d->url.queryItemValue("jid_field"),
            d->url.queryItemValue("password_field"),
            d->url.queryItemValue("table"),
            d->url.queryItemValue("jid_field"));
    }
    else {
        warning("Unsupported authentication URL " + d->url.toString());
        return false;
    }

    server()->setPasswordChecker(this);
    return true;
}

void XmppPasswordChecker::stop()
{
    server()->setPasswordChecker(0);
}

QString XmppPasswordChecker::user() const
{
    return d->user;
}

void XmppPasswordChecker::setUser(const QString &user)
{
    d->user = user;
}

QString XmppPasswordChecker::password() const
{
    return d->password;
}

void XmppPasswordChecker::setPassword(const QString &password)
{
    d->password = password;
}

QString XmppPasswordChecker::searchBase() const
{
    return d->searchBase;
}

void XmppPasswordChecker::setSearchBase(const QString &searchBase)
{
    d->searchBase = searchBase;
}

QString XmppPasswordChecker::searchPattern() const
{
    return d->searchPattern;
}

void XmppPasswordChecker::setSearchPattern(const QString &searchPattern)
{
    d->searchPattern = searchPattern;
}

/** Returns the URL for the password source.
 */
QString XmppPasswordChecker::url() const
{
    return d->url.toString();
}

/** Sets the URL for the password source.
 *
 * Examples:
 *  file:///etc/xmpp-users.ini
 *  http://www.example.com/auth
 *  mysql://mysql.example.com/some_database?table=some_table&password_field=some_pwd_field&jid_field=some_jid_field
 *
 * @param url
 */
void XmppPasswordChecker::setUrl(const QString &url)
{
    d->url = url;
}

// PLUGIN

class XmppServerAuthPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("auth"))
            return new XmppPasswordChecker;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("auth");
    };
};

Q_EXPORT_PLUGIN2(auth, XmppServerAuthPlugin)

