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

#ifndef SHARE_SERVER_H
#define SHARE_SERVER_H

#include <QStringList>

#include "QXmppServerExtension.h"

class QDomElement;

class QXmppPresence;
class QXmppShareSearchIq;
class XmppServerSharePrivate;

class XmppServerShare : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "share");
    Q_PROPERTY(QStringList allowedDomains READ allowedDomains WRITE setAllowedDomains);
    Q_PROPERTY(bool forceProxy READ forceProxy WRITE setForceProxy);
    Q_PROPERTY(QString jid READ jid WRITE setJid);
    Q_PROPERTY(QString redirectDomain READ redirectDomain WRITE setRedirectDomain);
    Q_PROPERTY(QString redirectServer READ redirectServer WRITE setRedirectServer);

public:
    XmppServerShare();
    ~XmppServerShare();

    QStringList allowedDomains() const;
    void setAllowedDomains(const QStringList &allowedDomains);

    bool forceProxy() const;
    void setForceProxy(bool forceProxy);

    QString jid() const;
    void setJid(const QString &jid);

    QString redirectDomain() const;
    void setRedirectDomain(const QString &domain);

    QString redirectServer() const;
    void setRedirectServer(const QString &server);

    /// \cond
    QStringList discoveryItems() const;
    bool handleStanza(const QDomElement &element);
    bool start();
    void stop();
    /// \endcond

private slots:
    void checkTimeout();
    void explorePeers();

private:
    bool handlePresence(const QXmppPresence &presence);
    void handleShareSearchIq(const QXmppShareSearchIq &shareIq);

    XmppServerSharePrivate * const d;
};

#endif
