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

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHostInfo>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>

#include "QXmppPasswordChecker.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStun.h"
#include "QXmppUtils.h"

#include "mod_turn.h"

#define QXMPP_DEBUG_STUN

static QByteArray generateNonce(const QByteArray &key)
{
    // expiry
    uint t = QDateTime::currentDateTime().toTime_t() + 3600;
    QByteArray nonce = QByteArray::number(t, 16);

    // padding
    nonce += QByteArray(8, 0x30);

    // hash
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(nonce + ":" + key);
    nonce += hash.result().toHex();
    return nonce;
}

static bool verifyNonce(const QByteArray &nonce, const QByteArray &key)
{
    // check size and padding
    if (nonce.size() != 48 || nonce.mid(8, 8) != QByteArray(8, 0x30))
        return false;

    // check expiry
    bool ok;
    uint t = nonce.left(8).toUInt(&ok, 16);
    if (!ok || t < QDateTime::currentDateTime().toTime_t())
        return false;

    // check hash
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(nonce.left(16) + ":" + key);
    if (nonce.right(32) != hash.result().toHex())
        return false;

    return true;
}

struct XmppServerTurnPending
{
    QByteArray buffer;
    QHostAddress remoteHost;
    quint16 remotePort;
    QXmppStunMessage response;
};

class XmppServerTurnPrivate
{
public:
    XmppServerTurnPrivate(XmppServerTurn *qq);
    void writeStun(const QXmppStunMessage &message, const QHostAddress &host, quint16 port);

    QList<XmppServerTurnAllocation*> allocations;
    QMap<QXmppPasswordReply*, XmppServerTurnPending> pendingPackets;
    QByteArray secret;
    QUdpSocket *socket;

    // config
    quint16 port;
    QHostAddress hostAddress;
    QString hostName;
    QString realm;
    quint32 defaultLifetime;
    quint32 maximumLifetime;

private:
    XmppServerTurn *q;
};

XmppServerTurnAllocation::XmppServerTurnAllocation(XmppServerTurn *srv)
    : QXmppLoggable(srv),
    transferBytes(0),
    server(srv)
{
    bool check;
    Q_UNUSED(check);

    socket = new QUdpSocket(this);
    check = connect(socket, SIGNAL(readyRead()),
                    this, SLOT(readyRead()));
    Q_ASSERT(check);

    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, SIGNAL(timeout()),
            this, SIGNAL(timeout()));
    Q_ASSERT(check);
}

void XmppServerTurnAllocation::readyRead()
{
    const qint64 size = socket->pendingDatagramSize();
    QHostAddress remoteHost;
    quint16 remotePort;

    QByteArray data(size, 0);
    socket->readDatagram(data.data(), data.size(), &remoteHost, &remotePort);

    quint16 channel = channels.key(qMakePair(remoteHost, remotePort));
    if (channel) {
        QByteArray channelData;
        channelData.reserve(4 + data.size());
        QDataStream stream(&channelData, QIODevice::WriteOnly);
        stream << channel;
        stream << quint16(data.size());
        stream.writeRawData(data.data(), data.size());
        server->d->socket->writeDatagram(channelData, clientHost, clientPort);
    }
}

void XmppServerTurnAllocation::refresh(quint32 lifetime)
{
    timer->start(lifetime * 1000);
}

QString XmppServerTurnAllocation::toString() const
{
    return QString("%1 port %2 (username: %3)").arg(clientHost.toString(), QString::number(clientPort), username);
}

XmppServerTurnPrivate::XmppServerTurnPrivate(XmppServerTurn *qq)
    : port(3478),
    defaultLifetime(600),
    maximumLifetime(3600),
    q(qq)
{
    secret = QXmppUtils::generateRandomBytes(16);
}

void XmppServerTurnPrivate::writeStun(const QXmppStunMessage &message, const QHostAddress &host, quint16 port)
{
    socket->writeDatagram(message.encode(), host, port);
#ifdef QXMPP_DEBUG_STUN
    q->logSent(QString("STUN packet to %1 port %2\n%3").arg(
            host.toString(),
            QString::number(port),
            message.toString()));
#endif
}

XmppServerTurn::XmppServerTurn()
{
    bool check;

    d = new XmppServerTurnPrivate(this);
    d->socket = new QUdpSocket(this);
    check = connect(d->socket, SIGNAL(readyRead()),
                    this, SLOT(readyRead()));
    Q_ASSERT(check);
    Q_UNUSED(check);
}

XmppServerTurn::~XmppServerTurn()
{
    delete d;
}

/// Returns the host on which to listen for TURN connections.
///

QString XmppServerTurn::host() const
{
    return d->hostName;
}

/// Sets the host on which to listen for TURN connections.
///
/// If not defined, defaults to the server's domain.
///
/// \param host
///

void XmppServerTurn::setHost(const QString &host)
{
    d->hostName = host;
}

/// Returns the port on which to listen for TURN requests.
///

quint16 XmppServerTurn::port() const
{
    return d->port;
}

/// Sets the port on which to listen for TURN requests.
///
/// If not defined, defaults to 3478.
///
/// \param port

void XmppServerTurn::setPort(quint16 port)
{
    d->port = port;
}

/// Returns the authentication realm for TURN requests.
///
QString XmppServerTurn::realm() const
{
    return d->realm;
}

/// Sets the authentication realm for TURN requests.
///
/// If not defined, defaults to the server's domain.
///
/// \param realm

void XmppServerTurn::setRealm(const QString &realm)
{
    d->realm = realm;
}

void XmppServerTurn::readyRead()
{
    while (d->socket->hasPendingDatagrams()) {
        const qint64 size = d->socket->pendingDatagramSize();

        QHostAddress remoteHost;
        quint16 remotePort;

        QByteArray buffer(size, 0);
        d->socket->readDatagram(buffer.data(), buffer.size(), &remoteHost, &remotePort);

        handlePacket(buffer, remoteHost, remotePort);
    }
}

void XmppServerTurn::handlePacket(const QByteArray &buffer, const QHostAddress &remoteHost, const quint16 remotePort)
{
    // find allocation
    XmppServerTurnAllocation *allocation = 0;
    foreach (XmppServerTurnAllocation *alloc, d->allocations) {
        if (alloc->clientHost == remoteHost && alloc->clientPort == remotePort) {
            allocation = alloc;
            break;
        }
    }

    // demultiplex channel data
    if (buffer.size() >= 4 && (buffer[0] & 0xc0) == 0x40) {
        QDataStream stream(buffer);
        quint16 channel, length;
        stream >> channel;
        stream >> length;
        if (allocation && allocation->channels.contains(channel) && length <= buffer.size() - 4) {
            allocation->socket->writeDatagram(buffer.mid(4, length),
                allocation->channels[channel].first,
                allocation->channels[channel].second);
            allocation->transferBytes += (length - 4);
        }
        return;
    }

    // parse STUN message
    QXmppStunMessage message;
    QStringList errors;
    if (!message.decode(buffer, QByteArray(), &errors)) {
        foreach (const QString &error, errors)
            warning(error);
        return;
    }
#ifdef QXMPP_DEBUG_STUN
    logReceived(QString("TURN packet from %1 port %2\n%3").arg(
            remoteHost.toString(),
            QString::number(remotePort),
            message.toString()));
#endif
    if (message.messageClass() != QXmppStunMessage::Request)
        return;

    // start building response
    const quint16 method = message.messageMethod();
    QXmppStunMessage response;
    response.setId(message.id());
    response.setSoftware(qApp->applicationName() + "/" + qApp->applicationVersion());
    response.setType(method | QXmppStunMessage::Response);

    if (method == QXmppStunMessage::Binding) {
        response.setType(method | QXmppStunMessage::Response);
        response.xorMappedHost = remoteHost;
        response.xorMappedPort = remotePort;
        d->writeStun(response, remoteHost, remotePort);
        return;
    } else if (method != QXmppStunMessage::Allocate &&
               method != QXmppStunMessage::ChannelBind &&
               method != QXmppStunMessage::Refresh) {
        // unsupported
        return;
    }

    // validate input
    const QString realm = message.realm();
    const QString username = message.username();
    if (realm != d->realm
        || username.isEmpty()
        || !verifyNonce(message.nonce(), d->secret)
        || !server()->passwordChecker())
    {
        response.setType(method | QXmppStunMessage::Error);
        response.errorCode = 401;
        response.errorPhrase = "Unauthorized";
        response.setNonce(generateNonce(d->secret));
        response.setRealm(d->realm);
        d->writeStun(response, remoteHost, remotePort);
        return;
    }

    // check authentication
    QXmppPasswordRequest request;
    request.setDomain(realm);
    request.setUsername(username);
    QXmppPasswordReply *reply = server()->passwordChecker()->getDigest(request);
    d->pendingPackets[reply].buffer = buffer;
    d->pendingPackets[reply].response = response;
    d->pendingPackets[reply].remoteHost = remoteHost;
    d->pendingPackets[reply].remotePort = remotePort;
    connect(reply, SIGNAL(finished()), this, SLOT(onPasswordReply()));
}

void XmppServerTurn::onPasswordReply()
{
    QXmppPasswordReply *reply = qobject_cast<QXmppPasswordReply*>(sender());
    if (!reply || !d->pendingPackets.contains(reply))
        return;
    reply->deleteLater();
    XmppServerTurnPending pending = d->pendingPackets.take(reply);

    // start building response
    QXmppStunMessage response = pending.response;
    QXmppStunMessage message;
    if (reply->error() != QXmppPasswordReply::NoError || !message.decode(pending.buffer, reply->digest())) {
        response.setType(response.messageMethod() | QXmppStunMessage::Error);
        response.errorCode = 401;
        response.errorPhrase = "Unauthorized";
        response.setNonce(generateNonce(d->secret));
        response.setRealm(d->realm);
        d->writeStun(response, pending.remoteHost, pending.remotePort);
        return;
    }

    // find allocation
    XmppServerTurnAllocation *allocation = 0;
    foreach (XmppServerTurnAllocation *alloc, d->allocations) {
        if (alloc->clientHost == pending.remoteHost && alloc->clientPort == pending.remotePort) {
            allocation = alloc;
            break;
        }
    }

    const quint16 method = message.messageMethod();
    if (method == QXmppStunMessage::Allocate) {
        response.setNonce(message.nonce());
        response.setRealm(message.realm());

        // check the 5-TUPLE
        if (allocation) {
            response.setType(method | QXmppStunMessage::Error);
            response.errorCode = 437;
            response.errorPhrase = "Allocation Mismatch";
            d->writeStun(response, pending.remoteHost, pending.remotePort);
            return;
        }

        // check the requested transport
        if (message.requestedTransport() != 0x11) {
            response.setType(method | QXmppStunMessage::Error);
            response.errorCode = 442;
            response.errorPhrase = "Unsupported Transport Protocol";
            d->writeStun(response, pending.remoteHost, pending.remotePort);
            return;
        }

        // create allocation
        const quint32 lifetime = qMax(d->defaultLifetime, qMin(d->maximumLifetime, message.lifetime()));
        allocation = new XmppServerTurnAllocation(this);
        allocation->clientHost = pending.remoteHost;
        allocation->clientPort = pending.remotePort;
        allocation->username = message.username();
        if (!allocation->socket->bind(d->hostAddress, 0)) {
            delete allocation;

            response.setType(method | QXmppStunMessage::Error);
            response.errorCode = 508;
            response.errorPhrase = "Insufficient Capacity";
            d->writeStun(response, pending.remoteHost, pending.remotePort);
            return;
        }
        connect(allocation, SIGNAL(timeout()),
                this, SLOT(onTimeout()));
        allocation->refresh(lifetime);
        d->allocations << allocation;
        allocation->info(QString("Created allocation %1").arg(allocation->toString()));

        response.setLifetime(lifetime);
        response.xorMappedHost = pending.remoteHost;
        response.xorMappedPort = pending.remotePort;
        response.xorRelayedHost = allocation->socket->localAddress();
        response.xorRelayedPort = allocation->socket->localPort();
        d->writeStun(response, pending.remoteHost, pending.remotePort);
        return;
    }

    // check the 5-TUPLE
    if (!allocation) {
        response.setType(method | QXmppStunMessage::Error);
        response.errorCode = 437;
        response.errorPhrase = "Allocation Mismatch";
        d->writeStun(response, pending.remoteHost, pending.remotePort);
        return;
    }

    if (method == QXmppStunMessage::ChannelBind) {
        const quint16 channelNumber = message.channelNumber();
        const QPair<QHostAddress, quint16> dest = qMakePair(message.xorPeerHost, message.xorPeerPort);
        const quint16 channelForDest = allocation->channels.key(dest);

        // check validity
        if (channelNumber < 0x4000 ||
            channelNumber > 0x7ffe ||
            message.xorPeerHost.isNull() ||
            !message.xorPeerPort ||
            (allocation->channels.contains(channelNumber) && allocation->channels.value(channelNumber) != dest) ||
            (channelForDest != 0 && channelForDest != channelNumber)) {
            response.setType(method | QXmppStunMessage::Error);
            response.errorCode = 400;
            response.errorPhrase = "Bad Request";
            d->writeStun(response, pending.remoteHost, pending.remotePort);
            return;
        }

        // create channel binding
        allocation->debug(QString("Channel %1 bound to %2 port %3").arg(QString::number(message.channelNumber()), dest.first.toString(), QString::number(dest.second)));
        allocation->channels[message.channelNumber()] = dest;
        d->writeStun(response, pending.remoteHost, pending.remotePort);

    } else if (method == QXmppStunMessage::Refresh) {

        quint32 lifetime;
        if (!message.lifetime()) {
            // remove allocation
            lifetime = 0;
            allocation->info(QString("Removed allocation (bytes: %1)").arg(QString::number(allocation->transferBytes)));
            d->allocations.removeAll(allocation);
            updateCounter("turn.bytes", allocation->transferBytes);
            updateCounter("turn.transfers");
            delete allocation;
        } else {
            lifetime = qMax(d->defaultLifetime, qMin(d->maximumLifetime, message.lifetime()));
            allocation->refresh(lifetime);
            allocation->info("Refreshed allocation");
        }
        response.setLifetime(lifetime);
        d->writeStun(response, pending.remoteHost, pending.remotePort);

    }
}

bool XmppServerTurn::start()
{
    // determine authentication realm
    if (d->realm.isEmpty())
        d->realm = server()->domain();

    // determine bind address
    if (d->hostName.isEmpty())
        d->hostName = server()->domain();
    if (!d->hostAddress.setAddress(d->hostName)) {
        QHostInfo hostInfo = QHostInfo::fromName(d->hostName);
        foreach (const QHostAddress &address, hostInfo.addresses()) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                d->hostAddress = address;
                break;
            }
        }
        if (d->hostAddress.isNull()) {
            warning(QString("Could not lookup host %1").arg(d->hostName));
            return false;
        }
    }
    return d->socket->bind(d->hostAddress, d->port);
}

void XmppServerTurn::stop()
{
    d->socket->close();
    foreach (XmppServerTurnAllocation *allocation, d->allocations)
        delete allocation;
    d->allocations.clear();
}

void XmppServerTurn::onTimeout()
{
    XmppServerTurnAllocation *allocation = qobject_cast<XmppServerTurnAllocation*>(sender());
    if (!allocation)
        return;
    allocation->info(QString("Timed out allocation (bytes: %1)").arg(QString::number(allocation->transferBytes)));
    d->allocations.removeAll(allocation);
    updateCounter("turn.bytes", allocation->transferBytes);
    updateCounter("turn.transfers");
    allocation->deleteLater();
}

// PLUGIN

class XmppServerTurnPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("turn"))
            return new XmppServerTurn;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("turn");
    };
};

Q_EXPORT_PLUGIN2(turn, XmppServerTurnPlugin)

