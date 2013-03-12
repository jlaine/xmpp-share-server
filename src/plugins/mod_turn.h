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

#ifndef XMPP_SERVER_TURN_H
#define XMPP_SERVER_TURN_H

#include <QHostAddress>

#include "QXmppServerExtension.h"

class QTimer;
class QUdpSocket;
class XmppServerTurn;
class XmppServerTurnPrivate;

class XmppServerTurnAllocation : public QXmppLoggable
{
    Q_OBJECT

public:
    XmppServerTurnAllocation(XmppServerTurn *server);
    QString toString() const;

    QMap<quint16, QPair<QHostAddress, quint16> > channels;
    QHostAddress clientHost;
    quint16 clientPort;
    QUdpSocket *socket;
    QString username;
    qint64 transferBytes;

signals:
    void timeout();

public slots:
    void readyRead();
    void refresh(quint32 lifetime);

private:
    XmppServerTurn *server;
    QTimer *timer;
    friend class XmppServerTurn;
};

class XmppServerTurn : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "turn");
    Q_PROPERTY(QString host READ host WRITE setHost);
    Q_PROPERTY(quint16 port READ port WRITE setPort);
    Q_PROPERTY(QString realm READ realm WRITE setRealm);

public:
    XmppServerTurn();
    ~XmppServerTurn();

    QString host() const;
    void setHost(const QString &host);

    quint16 port() const;
    void setPort(quint16 port);

    QString realm() const;
    void setRealm(const QString &realm);

    /// \cond
    bool start();
    void stop();
    /// \endcond

private slots:
    void onPasswordReply();
    void readyRead();
    void onTimeout();

private:
    void handlePacket(const QByteArray &buffer, const QHostAddress &remoteHost, const quint16 port);

    friend class XmppServerTurnAllocation;
    friend class XmppServerTurnPrivate;
    XmppServerTurnPrivate *d;
};

#endif
