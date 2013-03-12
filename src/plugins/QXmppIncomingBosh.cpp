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

#include <QDomDocument>

#include "QXmppConstants.h"
#include "QXmppIncomingBosh.h"
#include "QXmppStreamFeatures.h"
#include "QXmppUtils.h"

static const char * ns_http_bind = "http://jabber.org/protocol/httpbind";

QXmppIncomingBosh::QXmppIncomingBosh(const QString &domain, QObject *parent)
    : QXmppIncomingClient(0, domain, parent),
    m_pendingStart(false),
    m_pendingStop(false),
    m_signalsEmitted(false),
    m_wait(55)
{
    m_isConnected = true;
    m_id = QXmppUtils::generateStanzaHash();
    setInactivityTimeout(m_wait + 10);
}

void QXmppIncomingBosh::disconnectFromHost()
{
    QXmppStream::disconnectFromHost();
    if (m_isConnected) {
        m_isConnected = false;
        emit disconnected();
    }
}

void QXmppIncomingBosh::emitSignals()
{
    if (hasPendingData())
        emit readyRead();
    m_signalsEmitted = false;
}

void QXmppIncomingBosh::handleStanza(const QDomElement &stanza)
{
    QXmppIncomingClient::handleStanza(stanza);
}

void QXmppIncomingBosh::handleStream(const QDomElement &stream)
{
    QXmppIncomingClient::handleStream(stream);
}

bool QXmppIncomingBosh::hasPendingData() const
{
    return m_pendingStart || m_pendingStop || !m_buffer.isEmpty();
}

QString QXmppIncomingBosh::id() const
{
    return m_id;
}

bool QXmppIncomingBosh::isConnected() const
{
    return m_isConnected && !QXmppUtils::jidToResource(jid()).isEmpty();
}

QByteArray QXmppIncomingBosh::readPendingData()
{
    QByteArray data;
    if (m_pendingStop) {
        data = QString("<body xmlns=\"%1\" type=\"terminate\">").arg(ns_http_bind).toUtf8();
        m_pendingStop = false;
    } else if (m_pendingStart) {
        data = QString("<body xmlns=\"%1\" xmlns:stream=\"%2\" sid=\"%3\" wait=\"%4\">").arg(
            ns_http_bind,
            ns_client,
            m_id,
            QString::number(m_wait)).toUtf8();
        m_pendingStart = false;
    } else {
        data = QString("<body xmlns=\"%1\">").arg(ns_http_bind).toUtf8();
    }
    data += m_buffer;
    data += "</body>";

    m_buffer.clear();
    return data;
}

bool QXmppIncomingBosh::sendData(const QByteArray &data)
{
    if (!m_isConnected)
        return false;
    if (data.startsWith("<?xml")) {
        m_pendingStart = true;
    } else if (data == "</stream:stream>") {
        m_pendingStop = true;
    } else if (!data.isEmpty()) {
        logSent(QString::fromUtf8(data));
        m_buffer += data;
    }
    if (!m_signalsEmitted) {
        m_signalsEmitted = true;
        QMetaObject::invokeMethod(this, "emitSignals", Qt::QueuedConnection);
    }
    return true;
}

int QXmppIncomingBosh::wait() const
{
    return m_wait;
}

