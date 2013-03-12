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
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QSettings>
#include <QStringList>
#include <QtPlugin>
#include <QUdpSocket>
#include <QTemporaryFile>
#include <QXmlStreamReader>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "QDjango.h"
#include "QXmppLogger.h"
#include "QXmppUtils.h"
#include "QXmppServer.h"
#include "QXmppServerExtension.h"
#include "QXmppServerPlugin.h"

#include "config.h"
#include "server.h"

static XmppLogger *logger = 0;
static QSettings *settings = 0;

static QTemporaryFile tmpFile;

static bool openDatabase(const QSettings &settings)
{
    const QString databaseDriver = settings.value("database/driver", "QSQLITE").toString();
    QString databaseName = settings.value("database/name").toString();
    if (databaseDriver == "QSQLITE" && databaseName.isEmpty()) {
        if (!tmpFile.open()) {
            qWarning("Could not open SQLite temporary file");
            return false;
        }
        databaseName = tmpFile.fileName();
    }
    QSqlDatabase database = QSqlDatabase::addDatabase(databaseDriver);
    if (databaseDriver == "QMYSQL")
        database.setConnectOptions("MYSQL_OPT_RECONNECT=1");
    database.setDatabaseName(databaseName);
    database.setHostName(settings.value("database/host").toString());
    database.setUserName(settings.value("database/user").toString());
    database.setPassword(settings.value("database/password").toString());
    if (!database.open()) {
        qWarning("Could not open SQL database '%s'", qPrintable(databaseName));
        return false;
    }
    QDjango::setDatabase(database);
    QDjango::setDebugEnabled(settings.value("database/debug", false).toBool());
    return true;
}

XmppLogger::XmppLogger(QObject *parent)
    : QXmppLogger(parent)
    , m_statsdPort(0)
{
    m_socket = new QUdpSocket(this);
}

void XmppLogger::setGauge(const QString &gauge, double value)
{
    if (!m_statsdHost.isNull() && m_statsdPort > 0) {
        m_socket->writeDatagram((m_statsdPrefix + gauge).toUtf8() + ":" + QByteArray::number(value) + "|g", m_statsdHost, m_statsdPort);
    }
}

void XmppLogger::updateCounter(const QString &counter, qint64 amount)
{
    if (!m_statsdHost.isNull() && m_statsdPort > 0) {
        m_socket->writeDatagram((m_statsdPrefix + counter).toUtf8() + ":" + QByteArray::number(amount) + "|c", m_statsdHost, m_statsdPort);
    }
}

void XmppLogger::readSettings()
{
    settings->sync();

    // logging output
    const QString logfile = settings->value("logFile").toString();
    if (!logfile.isEmpty()) {
        setLogFilePath(logfile);
        setLoggingType(QXmppLogger::FileLogging);
    } else {
        setLoggingType(QXmppLogger::StdoutLogging);
    }

    // logging level
    const int logLevel = settings->value("logLevel").toInt();
    QXmppLogger::MessageTypes types = QXmppLogger::InformationMessage | QXmppLogger::WarningMessage;
    if (logLevel >= 1)
        types |= QXmppLogger::DebugMessage;
    if (logLevel >= 2) {
        types |= QXmppLogger::SentMessage;
        types |= QXmppLogger::ReceivedMessage;
    }
    setMessageTypes(types);

    // statistics
    m_statsdHost = QHostAddress(settings->value("statsdHost").toString());
    m_statsdPort = settings->value("statsdPort", "8125").toUInt();
    m_statsdPrefix = settings->value("statsdPrefix", "xmpp-server.").toString();

    // re-open log
    reopen();
}

static void log_handler(QtMsgType type, const char *msg)
{
    switch (type) {
    case QtDebugMsg:
        logger->log(QXmppLogger::DebugMessage, msg);
        break;
    case QtWarningMsg:
    case QtCriticalMsg:
    case QtFatalMsg:
        logger->log(QXmppLogger::WarningMessage, msg);
        break;
    }
}

static void signal_handler(int sig)
{
    static int aborted = 0;
    if (sig == SIGHUP) {
        logger->readSettings();
        logger->log(QXmppLogger::InformationMessage, QString("Reloaded version %1").arg(qApp->applicationVersion()));
    } else if (sig == SIGINT || sig == SIGTERM) {
        if (aborted)
            exit(EXIT_FAILURE);

        qApp->quit();
        aborted = 1;
    }
}

class PluginManager;

class PluginSpec
{
public:
    QString description() const;
    QString location() const;
    QString name() const;
    QXmppServerPlugin *plugin() const;

private:
    PluginSpec();
    bool read(const QString &location);

    QStringList m_dependencyList;
    QString m_description;
    QString m_name;
    QString m_location;
    bool m_loading;
    QXmppServerPlugin *m_plugin;

    friend class PluginManager;
};

class PluginManager
{
public:
    PluginManager();
    ~PluginManager();

    void loadPlugins();

    QStringList pluginPaths() const;
    void setPluginPaths(const QStringList &paths);

    QList<PluginSpec*> plugins() const;

private:
    bool load(PluginSpec *spec);
    QStringList m_pluginPaths;
    QMap<QString, PluginSpec*> m_plugins;
};

PluginManager::PluginManager()
{
}

PluginManager::~PluginManager()
{
    foreach (PluginSpec *spec, m_plugins.values())
        delete spec;
}

bool PluginManager::load(PluginSpec *spec)
{
    if (spec->m_plugin) {
        return true;
    } else if (spec->m_loading) {
        qWarning("Not loading plugin %s, found loop", qPrintable(spec->name()));
        return false;
    }

    // load dependencies
    spec->m_loading = true;
    foreach (const QString &dep, spec->m_dependencyList) {
        if (!m_plugins.contains(dep) || !load(m_plugins.value(dep))) {
            qWarning("Could not satisfy plugin %s dependency on %s", qPrintable(spec->name()), qPrintable(dep));
            spec->m_loading = false;
            return false;
        }
    }
    spec->m_loading = false;

    const QDir dir = QFileInfo(spec->location()).dir();
    const QString pluginName = QString("libmod_%1.so").arg(spec->name());
    const QString pluginPath = QFileInfo(spec->location()).dir().absoluteFilePath(pluginName);

    QPluginLoader loader(pluginPath);
    spec->m_plugin = qobject_cast<QXmppServerPlugin*>(loader.instance());
    return spec->m_plugin != 0;
}

void PluginManager::loadPlugins()
{
    foreach (PluginSpec *spec, m_plugins.values())
        load(spec);
}

QStringList PluginManager::pluginPaths() const
{
    return m_pluginPaths;
}

void PluginManager::setPluginPaths(const QStringList &paths)
{
    if (paths != m_pluginPaths) {
        foreach (const QString &dirName, paths) {
            QDir path(dirName);
            path.setNameFilters(QStringList() << "*.pluginspec");
            foreach (QString pname, path.entryList(QDir::Files)) {
                PluginSpec *spec = new PluginSpec;
                if (spec->read(path.absoluteFilePath(pname))) {
                    if (m_plugins.contains(spec->name())) {
                        qWarning("Skipping duplicate plugin %s at %s", qPrintable(spec->name()), qPrintable(spec->location()));
                        delete spec;
                    }
                    m_plugins.insert(spec->name(), spec);
                } else {
                    delete spec;
                }
            }
        }
        m_pluginPaths = paths;
    }
}

QList<PluginSpec*> PluginManager::plugins() const
{
    return m_plugins.values();
}

PluginSpec::PluginSpec()
    : m_loading(false)
    , m_plugin(0)
{
}

QString PluginSpec::description() const
{
    return m_description;
}

QString PluginSpec::location() const
{
    return m_location;
}

QString PluginSpec::name() const
{
    return m_name;
}

QXmppServerPlugin *PluginSpec::plugin() const
{
    return m_plugin;
}

bool PluginSpec::read(const QString &location)
{
    QFile file(location);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QXmlStreamReader xs(&file);
    if (!xs.readNextStartElement() || xs.name() != "plugin")
        return false;

    const QString name = xs.attributes().value("name").toString();
    if (name.isEmpty())
        return false;

    QStringList dependencyList;
    QString description;
    while (xs.readNextStartElement()) {
        if (xs.name() == "dependencyList") {
            while (xs.readNextStartElement()) {
                if (xs.name() == "dependency") {
                    const QString dep = xs.attributes().value("name").toString();
                    if (!dep.isEmpty()) {
                        dependencyList << dep;
                    }
                } else {
                    qWarning("Element %s cannot be a child of dependencyList", qPrintable(xs.name().toString()));
                }
                xs.skipCurrentElement();
            }
        } else if (xs.name() == "description") {
            description = xs.readElementText();
            xs.skipCurrentElement();
        } else {
            xs.skipCurrentElement();
        }
    }

    m_dependencyList = dependencyList;
    m_description = description;
    m_location = location;
    m_name = name;
    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        qWarning("No config file specified");
        return EXIT_FAILURE;
    }
    const QString settingsPath(argv[1]);

    /* Create application */
    QCoreApplication app(argc, argv);
    app.setApplicationName("xmpp-share-server");
    app.setApplicationVersion(SERVER_VERSION);

    /* Install signal handler */
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Read settings */
    if (!QFileInfo(settingsPath).isReadable())
    {
        qWarning() << "Could not read configuration";
        return EXIT_FAILURE;
    }
    settings = new QSettings(settingsPath, QSettings::IniFormat);

    /* Set up logging */
    logger = new XmppLogger;
    qInstallMsgHandler(log_handler);
    logger->readSettings();
    logger->log(QXmppLogger::InformationMessage, QString("Starting version %1").arg(qApp->applicationVersion()));

    /* Set up database */
    if (!openDatabase(*settings))
        return EXIT_FAILURE;

    /* Create XMPP server */
    const QString domain = settings->value("domain").toString();
    logger->log(QXmppLogger::InformationMessage, QString("Creating XMPP server %1").arg(domain));
    QXmppServer server;
    server.setDomain(domain);
    server.setLogger(logger);
    const QString sslCertificate = settings->value("ssl-certificate").toString();
    if (!sslCertificate.isEmpty()) {
        server.addCaCertificates(sslCertificate);
        server.setLocalCertificate(sslCertificate);
    }
    const QString sslKey = settings->value("ssl-key").toString();
    if (!sslKey.isEmpty())
        server.setPrivateKey(sslKey);

    // load extensions
    PluginManager pluginManager;
    pluginManager.setPluginPaths(QStringList() << app.applicationDirPath() + "/plugins" << SERVER_PLUGIN_DIR);
    pluginManager.loadPlugins();
    foreach (PluginSpec *spec, pluginManager.plugins()) {
        QXmppServerPlugin *plugin = spec->plugin();
        if (plugin) {
            foreach (const QString &key, plugin->keys())
                server.addExtension(plugin->create(key));
        }
    }

    // configure extensions
    foreach (QXmppServerExtension *extension, server.extensions()) {
        const QString name = extension->extensionName();
        settings->beginGroup(name);
        foreach (const QString &key, settings->childKeys())
            extension->setProperty(key.toAscii(), settings->value(key));
        settings->endGroup();
    }

    // start server
    if (settings->value("c2s/enabled", true).toBool()) {
        const quint16 port = settings->value("c2s/port", "5222").toInt();
        if (!server.listenForClients(QHostAddress::Any, port))
            return EXIT_FAILURE;
        server.listenForClients(QHostAddress::AnyIPv6, port);
    }
    if (settings->value("s2s/enabled", false).toBool()) {
        const quint16 port = settings->value("s2s/port", "5269").toInt();
        if (!server.listenForServers(QHostAddress::Any, port))
            return EXIT_FAILURE;
        server.listenForServers(QHostAddress::AnyIPv6, port);
    }

    /* Run application */
    int ret = app.exec();
    logger->log(QXmppLogger::InformationMessage, "Exiting");
    delete logger;
    return ret;
}

