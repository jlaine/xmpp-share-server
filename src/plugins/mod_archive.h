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

#ifndef XMPP_SERVER_ARCHIVE_H
#define XMPP_SERVER_ARCHIVE_H

#include "QDjangoModel.h"
#include "QXmppArchiveIq.h"
#include "QXmppServerExtension.h"

class ArchiveChat : public QDjangoModel, public QXmppArchiveChat
{
    Q_OBJECT
    Q_PROPERTY(QString jid READ jid WRITE setJid)
    Q_PROPERTY(QDateTime start READ start WRITE setStart)
    Q_PROPERTY(QString subject READ subject WRITE setSubject)
    Q_PROPERTY(QString thread READ threadString WRITE setThreadString)
    Q_PROPERTY(int version READ version WRITE setVersion)
    Q_PROPERTY(QString with READ with WRITE setWith)

    Q_CLASSINFO("jid", "max_length=255 db_index=true")
    Q_CLASSINFO("subject", "max_length=255")
    Q_CLASSINFO("thread", "max_length=255")
    Q_CLASSINFO("with", "max_length=255 db_index=true")

public:
    ArchiveChat(QObject *parent = 0);

    QString jid() const;
    void setJid(const QString &jid);

    QString threadString() const;
    void setThreadString(const QString &thread);

private:
    QString m_jid;
};

class ArchiveMessage : public QDjangoModel, public QXmppArchiveMessage
{
    Q_OBJECT
    Q_PROPERTY(ArchiveChat* chat READ chat WRITE setChat);
    Q_PROPERTY(QString body READ body WRITE setBody)
    Q_PROPERTY(QDateTime date READ date WRITE setDate)
    Q_PROPERTY(bool received READ isReceived WRITE setReceived)

public:
    ArchiveMessage();

    ArchiveChat *chat() const;
    void setChat(ArchiveChat *chat);

private:
    int m_chatId;
};

class OfflineMessage : public QDjangoModel
{
    Q_OBJECT
    Q_PROPERTY(QString jid READ jid WRITE setJid)
    Q_PROPERTY(QString data READ data WRITE setData)
    Q_PROPERTY(QDateTime stamp READ stamp WRITE setStamp)

    Q_CLASSINFO("jid", "max_length=255 db_index=true")

public:
    QString data() const;
    void setData(const QString &data);

    QString jid() const;
    void setJid(const QString &jid);

    QDateTime stamp() const;
    void setStamp(const QDateTime &stamp);

private:
    QString m_jid;
    QString m_data;
    QDateTime m_stamp;
};

/// \brief QXmppServer extension for XEP-0136: Message Archiving.
///
class XmppServerArchive : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "archive");

public:
    XmppServerArchive();
    QStringList discoveryFeatures() const;
    bool handleStanza(const QDomElement &element);
};

#endif
