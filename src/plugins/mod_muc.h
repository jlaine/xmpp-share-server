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

#ifndef XMPP_SERVER_MUC_H
#define XMPP_SERVER_MUC_H

#include "QDjangoModel.h"
#include "QXmppMessage.h"
#include "QXmppMucIq.h"
#include "QXmppServerExtension.h"

class XmppServerMucPrivate;

/// \brief QXmppServer extension for XEP-0045: Multi-User Chat.
///
class XmppServerMuc : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "muc");
    Q_PROPERTY(QString jid READ jid WRITE setJid);
    Q_PROPERTY(QStringList admins READ admins WRITE setAdmins);

public:
    XmppServerMuc();
    ~XmppServerMuc();

    QStringList admins() const;
    void setAdmins(const QStringList &admins);

    QString jid() const;
    void setJid(const QString &jid);

    QStringList discoveryItems() const;
    bool handleStanza(const QDomElement &element);
    bool start();

private:
    XmppServerMucPrivate * const d;
};

class MucUser;

class MucAffiliation : public QDjangoModel
{
    Q_OBJECT
    Q_PROPERTY(QString room READ room WRITE setRoom)
    Q_PROPERTY(QString user READ user WRITE setUser)
    Q_PROPERTY(int affiliation READ affiliation WRITE setAffiliationInt)

    Q_CLASSINFO("room", "max_length=255 db_index=true")
    Q_CLASSINFO("user", "max_length=255 db_index=true")

public:
    QXmppMucItem::Affiliation affiliation() const;
    void setAffiliation(QXmppMucItem::Affiliation affiliation);
    void setAffiliationInt(int affiliation);

    QString room() const;
    void setRoom(const QString &room);

    QString user() const;
    void setUser(const QString &user);

private:
    QXmppMucItem::Affiliation m_affiliation;
    QString m_room;
    QString m_user;
};

class MucRoom : public QDjangoModel
{
    Q_OBJECT
    Q_PROPERTY(QString jid READ jid WRITE setJid)
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(bool membersOnly READ isMembersOnly WRITE setMembersOnly)
    Q_PROPERTY(bool public READ isPublic WRITE setPublic)

    Q_CLASSINFO("jid", "max_length=255 primary_key=true")
    Q_CLASSINFO("name", "max_length=255")

public:
    MucRoom();
    ~MucRoom();

    MucUser *userForRealJid(const QString &realJid) const;
    MucUser *userForRoomJid(const QString &roomJid) const;

    QList<QXmppMessage> history;
    QList<MucUser*> users;

    // configuration
    QString jid() const;
    void setJid(const QString &jid);

    QString name() const;
    void setName(const QString &name);

    bool isMembersOnly() const;
    void setMembersOnly(bool isMembersOnly);

    bool isPersistent() const;
    void setPersistent(bool isPersistent);

    bool isPublic() const;
    void setPublic(bool isPublic);

    QHash<QString, QXmppMucItem::Affiliation> affiliations;

private:
    bool m_isMembersOnly;
    bool m_isPersistent;
    bool m_isPublic;
    QString m_jid;
    QString m_name;
};

#endif
