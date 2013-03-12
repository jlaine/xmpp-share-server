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

#include <QBuffer>
#include <QDomElement>
#include <QStringList>

#include "QDjangoQuerySet.h"

#include "QXmppArchiveIq.h"
#include "QXmppConstants.h"
#include "QXmppMessage.h"
#include "QXmppPresence.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppUtils.h"

#include "mod_archive.h"
#include "mod_presence.h"

template <class T1, class T2>
void rsmFilter(QDjangoQuerySet<T1> &qs, const QXmppResultSetQuery &rsmQuery, QList<T2> &results, QXmppResultSetReply &rsmReply)
{
    // if count was requested, stop here
    if (rsmQuery.max() == 0) {
        rsmReply.setCount(qs.count());
        return;
    }

    rsmReply.setCount(qs.size());

    T1 result;
    if (rsmQuery.before().isNull()) {
        // page forwards
        bool rsmAfterReached = rsmQuery.after().isEmpty();
        for (int i = 0; i < qs.size(); ++i) {
            if (rsmQuery.max() >= 0 && results.size() >= rsmQuery.max())
                break;

            // fetch from database
            if (!qs.at(i, &result))
                break;
            const QString uid = result.pk().toString();

            // if an "after" was specified, check it was reached
            if (!rsmAfterReached) {
                if (uid == rsmQuery.after())
                    rsmAfterReached = true;
                continue;
            }

            if (results.isEmpty()) {
                rsmReply.setFirst(uid);
                rsmReply.setIndex(i);
            }
            rsmReply.setLast(uid);
            results << result;
        }
    } else {
        // page backwards
        bool rsmBeforeReached = rsmQuery.before().isEmpty();
        for (int i = qs.size() - 1; i >= 0; --i) {
            if (rsmQuery.max() >= 0 && results.size() >= rsmQuery.max())
                break;

            // fetch from database
            if (!qs.at(i, &result))
                break;
            const QString uid = result.pk().toString();

            // if a "before" was specified, check it was reached
            if (!rsmBeforeReached) {
                if (uid == rsmQuery.before())
                    rsmBeforeReached = true;
                continue;
            }

            if (results.isEmpty())
                rsmReply.setLast(uid);
            rsmReply.setFirst(uid);
            rsmReply.setIndex(i);
            results.prepend(result);
        }
    }
}

ArchiveChat::ArchiveChat(QObject *parent)
    : QDjangoModel(parent)
{
}

QString ArchiveChat::jid() const
{
    return m_jid;
}

void ArchiveChat::setJid(const QString &jid)
{
    m_jid = jid;
}

QString ArchiveChat::threadString() const
{
    return QXmppArchiveChat::thread();
}

void ArchiveChat::setThreadString(const QString &thread)
{
    QXmppArchiveChat::setThread(thread);
}

ArchiveMessage::ArchiveMessage()
{
    setForeignKey("chat", new ArchiveChat(this));
}

ArchiveChat* ArchiveMessage::chat() const
{
    return qobject_cast<ArchiveChat*>(foreignKey("chat"));
}

void ArchiveMessage::setChat(ArchiveChat *chat)
{
    setForeignKey("chat", chat);
}

QString OfflineMessage::data() const
{
    return m_data;
}

void OfflineMessage::setData(const QString &data)
{
    m_data = data;
}

QString OfflineMessage::jid() const
{
    return m_jid;
}

void OfflineMessage::setJid(const QString &jid)
{
    m_jid = jid;
}

QDateTime OfflineMessage::stamp() const
{
    return m_stamp;
}

void OfflineMessage::setStamp(const QDateTime &stamp)
{
    m_stamp = stamp;
}

static void saveMessage(const QXmppMessage &message, const QDateTime &now, bool received)
{
    const QString localJid = QXmppUtils::jidToBareJid(received ? message.to() : message.from());
    const QString remoteJid = QXmppUtils::jidToBareJid(received ? message.from() : message.to());

    // get or create collection
    int chatId;
    QDjangoQuerySet<ArchiveMessage> qs;
    qs = qs.filter(QDjangoWhere("chat__jid", QDjangoWhere::Equals, localJid));
    qs = qs.filter(QDjangoWhere("chat__with", QDjangoWhere::Equals, remoteJid));
    qs = qs.orderBy(QStringList() << "-date").limit(0, 1);
    ArchiveMessage tmp;
    if (qs.size() > 0 && qs.at(0, &tmp) && tmp.date().secsTo(now) < 3600) {
        chatId = tmp.property("chat_id").toInt();
    } else {
        ArchiveChat chat;
        chat.setJid(localJid);
        chat.setWith(remoteJid);
        chat.setStart(now);
        chat.save();
        chatId = chat.pk().toInt();
    }

    // save outgoing message
    ArchiveMessage msg;
    msg.setProperty("chat_id", chatId);
    msg.setBody(message.body());
    msg.setDate(now);
    msg.setReceived(received);
    msg.save();
}

XmppServerArchive::XmppServerArchive()
{
    QDjango::registerModel<ArchiveChat>();
    QDjango::registerModel<ArchiveMessage>();
    QDjango::registerModel<OfflineMessage>();
    QDjango::createTables();
}

QStringList XmppServerArchive::discoveryFeatures() const
{
    return QStringList() << ns_archive;
}

bool XmppServerArchive::handleStanza(const QDomElement &element)
{
    const QString domain = server()->domain();
    const QString from = element.attribute("from");
    const QString to = element.attribute("to");

    if (element.tagName() == "message" &&
        to != domain &&
        (QXmppUtils::jidToDomain(from) == domain || QXmppUtils::jidToDomain(to) == domain) &&
        element.attribute("type") != "error" &&
        element.attribute("type") != "groupchat" &&
        element.attribute("type") != "headline" &&
        !element.firstChildElement("body").text().isEmpty())
    {
        const QDateTime now = QDateTime::currentDateTime().toUTC();
        QXmppMessage message;
        message.parse(element);

        if (QXmppUtils::jidToDomain(from) == domain)
            saveMessage(message, now, false);

        if (QXmppUtils::jidToDomain(to) == domain) {
            saveMessage(message, now, true);

            // offline messages
            bool found = false;

            XmppServerPresence *presenceExtension = XmppServerPresence::instance(server());
            Q_ASSERT(presenceExtension);
            foreach (const QXmppPresence &presence, presenceExtension->availablePresences(QXmppUtils::jidToBareJid(to))) {
                if (QXmppUtils::jidToResource(to).isEmpty() || presence.from() == to) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                message.setStamp(now);
                message.setState(QXmppMessage::None);
                message.setTo(QXmppUtils::jidToBareJid(to));

                QBuffer buffer;
                buffer.open(QIODevice::WriteOnly);
                QXmlStreamWriter writer(&buffer);
                message.toXml(&writer);

                OfflineMessage offline;
                offline.setData(QString::fromUtf8(buffer.data()));
                offline.setJid(QXmppUtils::jidToBareJid(to));
                offline.setStamp(now);
                offline.save();
                return true;
            }
        }

        return false;

    } else if (element.tagName() == "presence" &&
               (element.attribute("type").isEmpty() || element.attribute("type") == "available") &&
               QXmppUtils::jidToDomain(from) == domain && to == domain) {

        // check this is an initial presence
        XmppServerPresence *presenceExtension = XmppServerPresence::instance(server());
        Q_ASSERT(presenceExtension);
        foreach (const QXmppPresence &presence, presenceExtension->availablePresences(QXmppUtils::jidToBareJid(from))) {
            if (presence.from() == from)
                return false;
        }

        // send offline messages
        QDjangoQuerySet<OfflineMessage> qs;
        OfflineMessage offline;
        qs = qs.filter(QDjangoWhere("jid", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(from)));
        for (int i = 0; i < qs.size(); ++i) {
            if (qs.at(i, &offline)) {
                QDomDocument doc;
                doc.setContent(offline.data());
                if (server()->sendElement(doc.documentElement()))
                    offline.remove();
            }
        }

    } else if (element.tagName() == "iq" &&
               to == domain &&
               QXmppArchiveListIq::isArchiveListIq(element)) {

        QXmppArchiveListIq request;
        request.parse(element);

        QXmppArchiveListIq response;
        response.setTo(request.from());
        response.setId(request.id());
        response.setType(QXmppIq::Result);

        if (request.type() == QXmppIq::Get) {
            const QXmppResultSetQuery rsmQuery = request.resultSetQuery();

            QDjangoQuerySet<ArchiveChat> qs;
            qs = qs.filter(QDjangoWhere("jid", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(request.from())));
            if (!request.with().isEmpty())
                qs = qs.filter(QDjangoWhere("with", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(request.with())));
            if (request.start().isValid())
                qs = qs.filter(QDjangoWhere("start", QDjangoWhere::GreaterOrEquals, request.start()));
            if (request.end().isValid())
                qs = qs.filter(QDjangoWhere("start", QDjangoWhere::LessOrEquals, request.end()));
            qs = qs.orderBy(QStringList() << "start");

            // perform RSM
            QList<QXmppArchiveChat> chats;
            QXmppResultSetReply rsmReply;
            rsmFilter(qs, rsmQuery, chats, rsmReply);
            response.setChats(chats);
            response.setResultSetReply(rsmReply);
        } else {
            response.setType(QXmppIq::Error);
        }
        server()->sendPacket(response);
        return true;

    } else if (element.tagName() == "iq" &&
               to == server()->domain() &&
               QXmppArchiveRemoveIq::isArchiveRemoveIq(element)) {

        QXmppArchiveRemoveIq request;
        request.parse(element);

        QXmppArchiveRemoveIq response;
        response.setTo(request.from());
        response.setId(request.id());
        response.setType(QXmppIq::Result);

        if (request.type() == QXmppIq::Set) {
            QDjangoQuerySet<ArchiveChat> qs;
            qs = qs.filter(QDjangoWhere("jid", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(request.from())));

            if (!request.with().isEmpty())
                qs = qs.filter(QDjangoWhere("with", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(request.with())));
            if (request.start().isValid())
                qs = qs.filter(QDjangoWhere("start", QDjangoWhere::GreaterOrEquals, request.start()));
            if (request.end().isValid())
                qs = qs.filter(QDjangoWhere("start", QDjangoWhere::LessOrEquals, request.end()));

            if (qs.size() > 0) {
                // collect deleted collection ids
                QList<QVariant> chatIds;
                foreach (const QList<QVariant> &values, qs.valuesList(QStringList() << "id"))
                    chatIds << values[0];

                // filter deleted items
                QDjangoQuerySet<ArchiveMessage> deleted_msgs;
                deleted_msgs = deleted_msgs.filter(QDjangoWhere("chat_id", QDjangoWhere::IsIn, chatIds));

                // remove messages and collections
                if (!deleted_msgs.remove() || !qs.remove())
                    response.setType(QXmppIq::Error);
            } else {
                // not found
                response.setType(QXmppIq::Error);
                response.setError(QXmppStanza::Error(
                    QXmppStanza::Error::Cancel,
                    QXmppStanza::Error::ItemNotFound));
            }

        } else {
            response.setType(QXmppIq::Error);
        }
        server()->sendPacket(response);
        return true;

    } else if (element.tagName() == "iq" &&
               to == server()->domain() &&
               QXmppArchiveRetrieveIq::isArchiveRetrieveIq(element)) {

        QXmppArchiveRetrieveIq request;
        request.parse(element);

        QXmppArchiveChatIq response;
        response.setTo(request.from());
        response.setId(request.id());
        response.setType(QXmppIq::Result);

        if (request.type() == QXmppIq::Get) {
            ArchiveChat chat;
            QDjangoQuerySet<ArchiveChat> qs;
            qs = qs.filter(QDjangoWhere("jid", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(request.from())));
            if (!request.with().isEmpty())
                qs = qs.filter(QDjangoWhere("with", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(request.with())));
            if (qs.get(QDjangoWhere("start", QDjangoWhere::Equals, request.start()), &chat)) {
                const QXmppResultSetQuery rsmQuery = request.resultSetQuery();

                QDjangoQuerySet<ArchiveMessage> qs;
                qs = qs.filter(QDjangoWhere("chat_id", QDjangoWhere::Equals, chat.pk()));
                qs = qs.orderBy(QStringList() << "date");

                QList<QXmppArchiveMessage> messages;
                QXmppResultSetReply rsmReply;
                rsmFilter(qs, rsmQuery, messages, rsmReply);

                // FIXME: this is a hack for clients using QXmpp < 0.4.93
                if (element.firstChildElement("retrieve").firstChildElement().isNull() && messages.size() > 2) {
                    int delta = 0;
                    QDateTime start = messages[0].date();
                    for (int i = 1; i < messages.size(); ++i) {
                        const int newDelta = start.secsTo(messages[i].date());
                        messages[i].setDate(messages[i].date().addSecs(delta));
                        delta = newDelta;
                    }
                }
                chat.setMessages(messages);

                response.setChat(chat);
                response.setResultSetReply(rsmReply);
            } else {
                response.setType(QXmppIq::Error);
            }

        } else {
            response.setType(QXmppIq::Error);
        }
        server()->sendPacket(response);
        return true;
    }

    return false;
}

// PLUGIN

class XmppServerArchivePlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("archive"))
            return new XmppServerArchive;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("archive");
    };
};

Q_EXPORT_PLUGIN2(archive, XmppServerArchivePlugin)

