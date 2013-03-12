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
#include <QDomDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>

#include "QDjangoQuerySet.h"

#include "QXmppServer.h"
#include "QXmppServerPlugin.h"

#include "mod_vcard.h"
#include "mod_wifirst.h"

XmppServerWifirst::XmppServerWifirst()
{
    bool check;
    Q_UNUSED(check);

    m_network = new QNetworkAccessManager(this);
    check = connect(m_network, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
                    this, SLOT(authenticationRequired(QNetworkReply*,QAuthenticator*)));
    Q_ASSERT(check);

    m_timer = new QTimer(this);
    m_timer->setInterval(120000);
    m_timer->setSingleShot(true);
    check = connect(m_timer, SIGNAL(timeout()),
                    this, SLOT(feedQueue()));
    Q_ASSERT(check);
}

void XmppServerWifirst::authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    authenticator->setUser(m_user);
    authenticator->setPassword(m_password);
}

void XmppServerWifirst::batchFailed(const QString &error)
{
    warning(QString("Failed processing jobs %1: %2").arg(m_batchUrl.toString(), error));
    updateCounter("wifirst.vcard.fail");
    m_batchUrl.clear();
    m_vcardJobs.clear();

    // schedule next run
    m_timer->start();
}

/** Queue vcard and roster jobs.
 */
void XmppServerWifirst::feedQueue()
{
    // fetch next vcard batch
    m_batchUrl = m_url;
    m_batchUrl.addQueryItem("type", "vcard");

    info(QString("Fetching jobs from %1").arg(m_batchUrl.toString()));
    QNetworkReply *reply = m_network->get(QNetworkRequest(m_batchUrl));
    connect(reply, SIGNAL(finished()), this, SLOT(handleBatch()));
}

/** Handle a list of jobs received from the server.
 */
void XmppServerWifirst::handleBatch()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        batchFailed("Could not retrieve job list");
        return;
    }

    // follow redirect
    QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirectUrl.isValid()) {
        m_batchUrl = reply->url().resolved(redirectUrl);
        debug(QString("Following redirect to %1").arg(m_batchUrl.toString()));
        QNetworkReply *reply = m_network->get(QNetworkRequest(m_batchUrl));
        connect(reply, SIGNAL(finished()), this, SLOT(handleBatch()));
        return;
    }

    // parse jobs
    QDomDocument doc;
    if (!doc.setContent(reply)) {
        m_batchUrl.clear();
        processQueue();
        return;
    }
    QMap<QString, QUrl> vcardJobs;
    QDomElement jobElement = doc.documentElement().firstChildElement("wilink_job");
    while (!jobElement.isNull()) {
        const QString type = jobElement.firstChildElement("type").text();
        const QString action = jobElement.firstChildElement("action").text();
        if (type == "vcard") {
            const QString jid = jobElement.firstChildElement("jid").text();
            QUrl url = QUrl(jobElement.firstChildElement("url").text());
            if (action != "update" || jid.isEmpty() || !url.isValid()) {
                warning("Received an invalid vcard job");
                return;
            }
            vcardJobs[jid] = reply->url().resolved(url);
        } else {
            warning(QString("Received an invalid job type '%1'").arg(type));
            return;
        }
        jobElement = jobElement.nextSiblingElement("wilink_job");
    }

    // store jobs and start processing them
    m_vcardJobs = vcardJobs;
    processQueue();
}

void XmppServerWifirst::handleVCard()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    const QString jid = reply->property("jid").toString();
    if (reply->error() != QNetworkReply::NoError) {
        batchFailed(QString("Could not retrieve vcard for %1 from %2").arg(jid, reply->url().toString()));
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(reply)) {
        batchFailed(QString("Could not parse vcard for %1 from %2").arg(jid, reply->url().toString()));
        return;
    }

    QDomElement card = doc.documentElement();

    // validate nickname
    const QString nickName = card.firstChildElement("nickname").text();
    if (nickName.isEmpty()) {
        batchFailed(QString("vcard for %1 has an invalid nickname").arg(jid));
        return;
    }

    // validate profile URL
    QUrl profileUrl = QUrl(card.firstChildElement("profile-url").text());
    if (profileUrl.isValid())
        profileUrl = reply->url().resolved(profileUrl);

    // validate image URL
    QUrl imageUrl = QUrl(card.firstChildElement("avatar-url").text());
    if (imageUrl.isValid()) {
        imageUrl = reply->url().resolved(imageUrl);

        debug(QString("Fetching vcard image for %1 from %2").arg(jid, imageUrl.toString()));
        reply = m_network->get(QNetworkRequest(imageUrl));
        reply->setProperty("jid", jid);
        reply->setProperty("nickname", nickName);
        reply->setProperty("url", profileUrl);
        connect(reply, SIGNAL(finished()), this, SLOT(handleVCardImage()));
    } else {
        VCard card;
        card.setJid(jid);
        card.setNickName(nickName);
        if (profileUrl.isValid())
            card.setUrl(profileUrl.toString());

        if (!card.save()) {
            batchFailed(QString("Could not save vcard for %1").arg(card.jid()));
            return;
        }
        info(QString("Updated vcard for %1").arg(card.jid()));
        updateCounter("wifirst.vcard.update");

        // process next job
        processQueue();
    }
}

void XmppServerWifirst::handleVCardImage()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    const QString jid = reply->property("jid").toString();
    const QString nickName = reply->property("nickname").toString();
    const QUrl url = reply->property("url").toUrl();

    VCard card;
    card.setJid(jid);
    card.setNickName(nickName);
    if (url.isValid())
        card.setUrl(url.toString());

    // skip image if it fails
    if (reply->error() == QNetworkReply::NoError) {
        card.setPhoto(reply->readAll());
        card.setPhotoType(QString::fromAscii(reply->rawHeader("Content-Type")));
    } else {
        warning(QString("Could not retrieve vcard image for %1 from %2").arg(jid, reply->url().toString()));
    }
    if (!card.save()) {
        batchFailed(QString("Could not save vcard for %1").arg(card.jid()));
        return;
    }
    info(QString("Updated vcard for %1").arg(card.jid()));
    updateCounter("wifirst.vcard.update");

    // process next job
    processQueue();
}

void XmppServerWifirst::processQueue()
{
    if (!m_vcardJobs.isEmpty()) {
        // process next vcard job
        const QString jid = m_vcardJobs.keys().first();
        const QUrl url = m_vcardJobs.take(jid);
        debug(QString("Fetching vcard for %1 from %2").arg(jid, url.toString()));
        QNetworkRequest req(url);
        req.setRawHeader("Accept", "application/xml");
        QNetworkReply *reply = m_network->get(req);
        reply->setProperty("jid", jid);
        connect(reply, SIGNAL(finished()), this, SLOT(handleVCard()));
        return;
    }

    // finished
    info(QString("Finished processing jobs %1").arg(m_batchUrl.toString()));
    QNetworkReply *reply = m_network->post(QNetworkRequest(m_batchUrl), "_method=delete");
    connect(reply, SIGNAL(finished()), reply, SLOT(deleteLater()));
    m_batchUrl.clear();

    // schedule next run
    m_timer->start();
}

bool XmppServerWifirst::start()
{
    if (m_url.isValid() && !m_user.isEmpty() && !m_password.isEmpty())
        m_timer->start();
    return true;
}

void XmppServerWifirst::stop()
{
    m_timer->stop();
}

QString XmppServerWifirst::password() const
{
    return m_password;
}

void XmppServerWifirst::setPassword(const QString &password)
{
    m_password = password;
}

QString XmppServerWifirst::url() const
{
    return m_url.toString();
}

void XmppServerWifirst::setUrl(const QString &url)
{
    m_url = url;
}

QString XmppServerWifirst::user() const
{
    return m_user;
}

void XmppServerWifirst::setUser(const QString &user)
{
    m_user = user;
}

// PLUGIN

class XmppServerWifirstPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("wifirst"))
            return new XmppServerWifirst;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("wifirst");
    };
};

Q_EXPORT_PLUGIN2(wifirst, XmppServerWifirstPlugin)

