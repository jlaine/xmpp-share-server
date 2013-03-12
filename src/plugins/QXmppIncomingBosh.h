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

#ifndef QXMPPINCOMINGBOSH_H
#define QXMPPINCOMINGBOSH_H

#include "QXmppIncomingClient.h"

class QXmppIncomingBosh : public QXmppIncomingClient
{
    Q_OBJECT

public:
    QXmppIncomingBosh(const QString &domain, QObject *parent = 0);

    bool hasPendingData() const;
    QString id() const;
    QByteArray readPendingData();
    int wait() const;

    /// \cond
    void handleStanza(const QDomElement &element);
    void handleStream(const QDomElement &element);
    bool isConnected() const;
    /// \endcond

signals:
    void readyRead();

public slots:
    void disconnectFromHost();
    bool sendData(const QByteArray&);

private slots:
    void emitSignals();

private:
    QByteArray m_buffer;
    QString m_id;
    bool m_isConnected;
    bool m_pendingStart;
    bool m_pendingStop;
    bool m_signalsEmitted;
    int m_wait;
};

#endif
