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

#include <cstdlib>
#include <csignal>

#include <QCoreApplication>
#include <QDomElement>
#include <QSettings>
#include <QSqlDatabase>
#include <QTemporaryFile>
#include <QTimer>

#include "QDjango.h"

#include "QXmppDiscoveryIq.h"
#include "QXmppLogger.h"
#include "QXmppShareDatabase.h"
#include "QXmppShareManager.h"
#include "QXmppTransferManager.h"
#include "QXmppUtils.h"

#include "client.h"
#include "config.h"

ShareClient::ShareClient()
    : m_statistics(0)
{
    bool check;
    Q_UNUSED(check);

    // setup database
    m_temporaryFile = new QTemporaryFile(this);
    check = m_temporaryFile->open();
    Q_ASSERT(check);
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(m_temporaryFile->fileName());
    check = database.open();
    Q_ASSERT(check);
    QDjango::setDatabase(database);

    // setup extensions
    m_db = new QXmppShareDatabase(this);
    addExtension(new QXmppShareManager(this, m_db));
    m_transferManager = findExtension<QXmppTransferManager>();

    // connect signals
    check = connect(this, SIGNAL(connected()),
                    this, SLOT(slotConnected()));
    Q_ASSERT(check);

    check = connect(this, SIGNAL(disconnected()),
                    this, SLOT(slotDisconnected()));
    Q_ASSERT(check);

    check = connect(this, SIGNAL(discoveryIqReceived(QXmppDiscoveryIq)),
                    this, SLOT(slotDiscoveryIqReceived(QXmppDiscoveryIq)));
    Q_ASSERT(check);

    check = connect(this, SIGNAL(presenceReceived(QXmppPresence)),
                    this, SLOT(slotPresenceReceived(QXmppPresence)));
    Q_ASSERT(check);

    check = connect(m_transferManager, SIGNAL(jobFinished(QXmppTransferJob*)),
                    this, SLOT(slotTransferFinished(QXmppTransferJob*)));
    Q_ASSERT(check);

    // FIXME: is this the right place for config defaults?
    setStatisticsFile("/var/lib/xmpp-share-server/statistics");
}

QString ShareClient::directory() const
{
    return m_db->directory();
}

void ShareClient::setDirectory(const QString &directory)
{
    m_db->setDirectory(directory);
}

QString ShareClient::statisticsFile() const
{
    if (m_statistics)
        return m_statistics->fileName();
    else
        return QString();
}

void ShareClient::setStatisticsFile(const QString &statisticsFile)
{
    if (m_statistics) {
        delete m_statistics;
        m_statistics = 0;
    }
    if (!statisticsFile.isEmpty()) {
        m_statistics = new QSettings(statisticsFile, QSettings::IniFormat, this);
        m_statistics->beginGroup("share-client");
    }
}

void ShareClient::shareServerFound(const QString &shareServer)
{
    m_shareServer = shareServer;

    // register with server
    QXmppElement x;
    x.setTagName("x");
    x.setAttribute("xmlns", ns_shares);

    QXmppElement nickName;
    nickName.setTagName("nickName");
    nickName.setValue("ShareAgent");
    x.appendChild(nickName);

    QXmppPresence presence;
    presence.setTo(shareServer);
    presence.setExtensions(QXmppElementList() << x);
    sendPacket(presence);
}

void ShareClient::slotConnected()
{
    qDebug() << "Share client connected to XMPP server";

    /* discover services */
    QXmppDiscoveryIq disco;
    disco.setTo(configuration().domain());
    disco.setQueryType(QXmppDiscoveryIq::ItemsQuery);
    sendPacket(disco);
}

void ShareClient::slotDisconnected()
{
    qWarning() << "Share client disconnected from XMPP server";
}

void ShareClient::slotDiscoveryIqReceived(const QXmppDiscoveryIq &disco)
{
    // we only want results
    if (disco.type() != QXmppIq::Result)
        return;

    if (disco.queryType() == QXmppDiscoveryIq::ItemsQuery &&
        disco.from() == configuration().domain())
    {
        // root items
        discoQueue.clear();
        foreach (const QXmppDiscoveryIq::Item &item, disco.items())
        {
            if (!item.jid().isEmpty() && item.node().isEmpty())
            {
                discoQueue.append(item.jid());
                // get info for item
                QXmppDiscoveryIq info;
                info.setQueryType(QXmppDiscoveryIq::InfoQuery);
                info.setTo(item.jid());
                sendPacket(info);
            }
        }
    }
    else if (disco.queryType() == QXmppDiscoveryIq::InfoQuery &&
             discoQueue.contains(disco.from()))
    {
        discoQueue.removeAll(disco.from());
        foreach (const QXmppDiscoveryIq::Identity &id, disco.identities())
        {
            if (id.category() == "proxy" && id.type() == "bytestreams")
            {
                logger()->log(QXmppLogger::InformationMessage, "Found bytestream proxy " + disco.from());
                m_transferManager->setProxy(disco.from());
            }
            if (id.category() == "store" && id.type() == "file")
            {
                logger()->log(QXmppLogger::InformationMessage, "Found share server " + disco.from());
                shareServerFound(disco.from());
            }
        }
    }
}

void ShareClient::slotTransferFinished(QXmppTransferJob *job)
{
    if (m_statistics && job->direction() == QXmppTransferJob::OutgoingDirection)
    {
        m_statistics->setValue("total-downloads",
            m_statistics->value("total-downloads").toULongLong() + 1);
    }
}

void ShareClient::slotPresenceReceived(const QXmppPresence &presence)
{
    if (presence.from() != m_shareServer ||
        presence.type() != QXmppPresence::Available)
        return;

    // find shares extension
    foreach (const QXmppElement &extension, presence.extensions())
    {
        if (extension.attribute("xmlns") == ns_shares)
        {
            const QString forceProxy = extension.firstChildElement("force-proxy").value();
            if (forceProxy == "1" && !m_transferManager->proxyOnly())
            {
                logger()->log(QXmppLogger::InformationMessage, "Forcing SOCKS5 proxy");
                m_transferManager->setProxyOnly(true);
            }
            break;
        }
    }
}

static int aborted = 0;
static void signal_handler(int sig)
{
    Q_UNUSED(sig);

    if (aborted)
        exit(1);

    qApp->quit();
    aborted = 1;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        qWarning() << "Usage: xmpp-share-client <config_file>";
        return EXIT_FAILURE;
    }

    QCoreApplication app(argc, argv);
    app.setApplicationName("xmpp-share-client");
    app.setApplicationVersion(SERVER_VERSION);

    // Install signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // read settings
    const QString settingsPath(QString::fromLocal8Bit(argv[1]));
    if (!QFileInfo(settingsPath).isReadable()) {
        qWarning() << "Could not read configuration" << settingsPath;
        return EXIT_FAILURE;
    }
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.beginGroup("share-client");

    // logging output
    QXmppLogger *logger = QXmppLogger::getLogger();
    const QString logfile = settings.value("logFile").toString();
    if (!logfile.isEmpty()) {
        logger->setLogFilePath(logfile);
        logger->setLoggingType(QXmppLogger::FileLogging);
    } else {
        logger->setLoggingType(QXmppLogger::StdoutLogging);
    }

    // logging level
    const int logLevel = settings.value("logLevel").toInt();
    QXmppLogger::MessageTypes types = QXmppLogger::InformationMessage | QXmppLogger::WarningMessage;
    if (logLevel >= 1)
        types |= QXmppLogger::DebugMessage;
    if (logLevel >= 2) {
        types |= QXmppLogger::SentMessage;
        types |= QXmppLogger::ReceivedMessage;
    }
    logger->setMessageTypes(types);

    // create client
    logger->log(QXmppLogger::InformationMessage, QString("Starting version %1").arg(qApp->applicationVersion()));
    ShareClient client;
    foreach (const QString &key, settings.childKeys())
        client.setProperty(key.toAscii(), settings.value(key));

    // connect to server
    QXmppConfiguration config;
    config.setIgnoreSslErrors(false);
    config.setJid(settings.value("jid").toString());
    config.setPassword(settings.value("password").toString());
    config.setHost(settings.value("host").toString());
    client.connectToServer(config);

    /* Run application */
    int ret = app.exec();
    logger->log(QXmppLogger::InformationMessage, "Exiting");
    delete logger;
    return ret;
}

