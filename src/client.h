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

#include "QXmppClient.h"

class QDomElement;
class QSettings;
class QTemporaryFile;
class QXmppDiscoveryIq;
class QXmppShareDatabase;
class QXmppShareGetIq;
class QXmppShareSearchIq;
class QXmppShareItem;
class QXmppTransferJob;
class QXmppTransferManager;

class ShareClient : public QXmppClient
{
    Q_OBJECT
    Q_PROPERTY(QString directory READ directory WRITE setDirectory)
    Q_PROPERTY(QString statisticsFile READ statisticsFile WRITE setStatisticsFile)

public:
    ShareClient();

    QString directory() const;
    void setDirectory(const QString &directory);

    QString statisticsFile() const;
    void setStatisticsFile(const QString &statisticsFile);

private slots:
    void slotConnected();
    void slotDisconnected();
    void slotDiscoveryIqReceived(const QXmppDiscoveryIq &disco);
    void slotTransferFinished(QXmppTransferJob *job);
    void slotPresenceReceived(const QXmppPresence &presence);

private:
    void shareServerFound(const QString &shareServer);

    QXmppShareDatabase *m_db;
    QString m_shareServer;
    QStringList discoQueue;
    QSettings *m_statistics;
    QTemporaryFile *m_temporaryFile;
    QXmppTransferManager *m_transferManager;
};

