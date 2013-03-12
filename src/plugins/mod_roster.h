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

#ifndef XMPP_SERVER_ROSTER_H
#define XMPP_SERVER_ROSTER_H

#include <QSet>

#include "QDjangoModel.h"

#include "QXmppRosterIq.h"
#include "QXmppServerExtension.h"

class QXmppPresence;

class Contact : public QDjangoModel
{
    Q_OBJECT
    Q_PROPERTY(QString user READ user WRITE setUser)
    Q_PROPERTY(QString jid READ jid WRITE setJid)
    Q_PROPERTY(QString groups READ groupString WRITE setGroupString)
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(int subscription READ subscription WRITE setSubscription)
    Q_PROPERTY(int ask READ ask WRITE setAsk)
    Q_PROPERTY(bool hidden READ hidden WRITE setHidden)

    Q_CLASSINFO("user", "max_length=255 db_index=true")
    Q_CLASSINFO("jid", "max_length=255 db_index=true")
    Q_CLASSINFO("name", "max_length=255")

public:
    Contact();

    QString user() const;
    void setUser(const QString &user);

    QString jid() const;
    void setJid(const QString &jid);

    QSet<QString> groups() const;
    void setGroups(const QSet<QString> &groups);

    QString groupString() const;
    void setGroupString(const QString &groupString);

    QString name() const;
    void setName(const QString &name);

    int subscription() const;
    void setSubscription(int subscription);

    int ask() const;
    void setAsk(int ask);

    bool hidden() const;
    void setHidden(bool hidden);

    bool hasSubscription(int subscription) const;

    QXmppRosterIq::Item toRosterItem() const;

private:
    QString m_user;
    QString m_jid;
    QSet<QString> m_groups;
    QString m_name;
    int m_subscription;
    int m_ask;
    bool m_hidden;
};

class XmppServerRoster : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "roster");

public:
    XmppServerRoster();
    QStringList discoveryFeatures() const;
    bool handleStanza(const QDomElement &element);
    QSet<QString> presenceSubscribers(const QString &jid);
    QSet<QString> presenceSubscriptions(const QString &jid);
    bool start();
    void stop();

private slots:
    void _q_clientConnected(const QString &jid);

private:
    bool handleInboundPresence(const QXmppPresence &presence);
    bool handleOutboundPresence(const QXmppPresence &presence);
    QSet<QString> m_connected;
};

#endif
