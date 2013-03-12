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

#include <QHostAddress>

#include "QXmppLogger.h"

class QUdpSocket;

class XmppLogger : public QXmppLogger
{
    Q_OBJECT

public:
    XmppLogger(QObject *parent = 0);
    void readSettings();

public slots:
    virtual void setGauge(const QString &gauge, double value);
    virtual void updateCounter(const QString &counter, qint64 amount);

private:
    QUdpSocket *m_socket;
    QHostAddress m_statsdHost;
    quint16 m_statsdPort;
    QString m_statsdPrefix;
};

