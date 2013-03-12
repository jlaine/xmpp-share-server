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

#ifndef XMPP_SERVER_PROXY65_H
#define XMPP_SERVER_PROXY65_H

#include <QStringList>
#include <QTime>

#include "QXmppServerExtension.h"

class QTcpSocket;

class QTcpSocketPair : public QXmppLoggable
{
    Q_OBJECT

public:
    QTcpSocketPair(const QString &hash, QObject *parent = 0);

    bool activate();
    void addSocket(QTcpSocket *socket);

    QString key;
    QTime time;
    qint64 transfer;

signals:
    void finished();

private slots:
    void disconnected();
    void sendData();

private:
    QTcpSocket *target;
    QTcpSocket *source;
};

class XmppServerProxy65Private;

/// \brief QXmppServer extension for XEP-0065: SOCKS5 Bytestreams.
///

class XmppServerProxy65 : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "proxy65");
    Q_PROPERTY(QStringList allowedDomains READ allowedDomains WRITE setAllowedDomains);
    Q_PROPERTY(QString jid READ jid WRITE setJid);
    Q_PROPERTY(QString host READ host WRITE setHost);
    Q_PROPERTY(quint16 port READ port WRITE setPort);

public:
    XmppServerProxy65();
    ~XmppServerProxy65();

    QStringList allowedDomains() const;
    void setAllowedDomains(const QStringList &allowedDomains);

    QString jid() const;
    void setJid(const QString &jid);

    QString host() const;
    void setHost(const QString &host);

    quint16 port() const;
    void setPort(quint16 port);

    /// \cond
    QStringList discoveryItems() const;
    bool handleStanza(const QDomElement &element);
    bool start();
    void stop();
    /// \endcond

private slots:
    void slotPairFinished();
    void slotSocketConnected(QTcpSocket *socket, const QString &hostName, quint16 port);

private:
    XmppServerProxy65Private * const d;
};

#endif
