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

#include <QDomElement>
#include <QMutexLocker>
#include <QStringList>

#include "QDjango.h"
#include "QDjangoQuerySet.h"

#include "QXmppConstants.h"
#include "QXmppDiscoveryIq.h"
#include "QXmppMessage.h"
#include "QXmppMucIq.h"
#include "QXmppPresence.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStream.h"
#include "QXmppUtils.h"

#include "mod_muc.h"

static bool isBareJid(const QString &jid)
{
    QRegExp jidValidator("^[^@/=]+@[^@/=]+$");
    return jidValidator.exactMatch(jid);
}

class MucUser
{
public:
    QString realJid;
    QString roomJid;
    QXmppMucItem::Role role;
};

QXmppMucItem::Affiliation MucAffiliation::affiliation() const
{
    return m_affiliation;
}

void MucAffiliation::setAffiliation(QXmppMucItem::Affiliation affiliation)
{
    m_affiliation = affiliation;
}

void MucAffiliation::setAffiliationInt(int affiliation)
{
    m_affiliation = static_cast<QXmppMucItem::Affiliation>(affiliation);
}

QString MucAffiliation::room() const
{
    return m_room;
}

void MucAffiliation::setRoom(const QString &room)
{
    m_room = room;
}

QString MucAffiliation::user() const
{
    return m_user;
}

void MucAffiliation::setUser(const QString &user)
{
    m_user = user;
}

MucRoom::MucRoom()
    : m_isMembersOnly(false),
    m_isPersistent(false),
    m_isPublic(false)
{
}

MucRoom::~MucRoom()
{
    foreach (MucUser *user, users)
        delete user;
}

bool MucRoom::isMembersOnly() const
{
    return m_isMembersOnly;
}

void MucRoom::setMembersOnly(bool isMembersOnly)
{
    m_isMembersOnly = isMembersOnly;
}

bool MucRoom::isPersistent() const
{
    return m_isPersistent;
}

void MucRoom::setPersistent(bool isPersistent)
{
    m_isPersistent = isPersistent;
}

bool MucRoom::isPublic() const
{
    return m_isPublic;
}

void MucRoom::setPublic(bool isPublic)
{
    m_isPublic = isPublic;
}

QString MucRoom::jid() const
{
    return m_jid;
}

void MucRoom::setJid(const QString &jid)
{
    m_jid = jid;
}

QString MucRoom::name() const
{
    return m_name;
}

void MucRoom::setName(const QString &name)
{
    m_name = name;
}

MucUser *MucRoom::userForRealJid(const QString &realJid) const
{
    foreach (MucUser *user, users)
        if (user->realJid == realJid)
            return user;
    return 0;
}

MucUser *MucRoom::userForRoomJid(const QString &roomJid) const
{
    foreach (MucUser *user, users)
        if (user->roomJid == roomJid)
            return user;
    return 0;
}

class XmppServerMucPrivate
{
public:
    QStringList admins;
    QString jid;
    QMutex mutex;
    qint64 participantCount;
    QMap<QString, MucRoom*> rooms;

    QXmppMucItem::Affiliation affiliation(MucRoom *room, const QString &realJid) const
    {
        const QString bareJid = QXmppUtils::jidToBareJid(realJid);
        if (admins.contains(bareJid))
            return QXmppMucItem::OwnerAffiliation;
        else
            return room->affiliations.value(bareJid, QXmppMucItem::NoAffiliation);
    }

    void setExtensions(QXmppPresence *presence, MucRoom *room, MucUser *user, MucUser *recipient, int code = 0, const QString &reason = QString()) const;
};

void XmppServerMucPrivate::setExtensions(QXmppPresence *presence, MucRoom *room, MucUser *user, MucUser *recipient, int code, const QString &reason) const
{
    QXmppMucItem item;
    item.setAffiliation(affiliation(room, user->realJid));
    // only show real jid to moderators
    if (recipient->role == QXmppMucItem::ModeratorRole)
        item.setJid(user->realJid);
    item.setRole(user->role);
    item.setReason(reason);
    presence->setMucItem(item);

    // add status code(s)
    QList<int> codes;
    if (code != 0)
        codes << code;
    if (recipient == user)
        codes << 110;
    presence->setMucStatusCodes(codes);
}

XmppServerMuc::XmppServerMuc()
    : d(new XmppServerMucPrivate)
{
    d->participantCount = 0;
    QDjango::registerModel<MucAffiliation>();
    QDjango::registerModel<MucRoom>();
    QDjango::createTables();

    // load rooms
    QDjangoQuerySet<MucRoom> rooms;
    for (int i = 0; i < rooms.size(); ++i) {
        MucRoom *room = rooms.at(i);
        if (room) {
            room->setPersistent(true);

            // load affiliations
            MucAffiliation aff;
            QDjangoQuerySet<MucAffiliation> qs;
            qs = qs.filter(QDjangoWhere("room", QDjangoWhere::Equals, room->jid()));
            for (int j = 0; j < qs.size(); ++j) {
                if (qs.at(j, &aff))
                    room->affiliations[aff.user()] = aff.affiliation();
            }
            d->rooms[QXmppUtils::jidToUser(room->jid())] = room;
        }
    }
    setGauge("muc.room.count", d->rooms.size());
    setGauge("muc.participant.count", d->participantCount);
}

XmppServerMuc::~XmppServerMuc()
{
    foreach (MucRoom *room, d->rooms.values())
        delete room;
    delete d;
}

QStringList XmppServerMuc::discoveryItems() const
{
    return QStringList() << d->jid;
}

bool XmppServerMuc::handleStanza(const QDomElement &element)
{
    const QString to = element.attribute("to");
    if (QXmppUtils::jidToDomain(to) != d->jid)
        return false;

    if (to == d->jid) {
        // handle stanza for component

        if (element.tagName() == "iq" && QXmppDiscoveryIq::isDiscoveryIq(element))
        {
            QXmppDiscoveryIq request;
            request.parse(element);

            if (request.type() == QXmppIq::Get)
            {
                QXmppDiscoveryIq response;
                response.setFrom(request.to());
                response.setTo(request.from());
                response.setId(request.id());
                response.setType(QXmppIq::Result);
                response.setQueryType(request.queryType());
                if (request.queryType() == QXmppDiscoveryIq::InfoQuery)
                {
                    QStringList features = QStringList() << ns_disco_info << ns_disco_items << ns_muc;
                    QList<QXmppDiscoveryIq::Identity> identities;
                    QXmppDiscoveryIq::Identity identity;
                    identity.setCategory("conference");
                    identity.setType("text");
                    identity.setName("Chatrooms");
                    identities.append(identity);
                    response.setFeatures(features);
                    response.setIdentities(identities);
                } else if (request.queryType() == QXmppDiscoveryIq::ItemsQuery) {

                    // FIXME: make locking more granular
                    QMutexLocker locker(&d->mutex);

                    QList<QXmppDiscoveryIq::Item> items;
                    foreach (MucRoom *room, d->rooms.values()) {
                        // don't list private rooms
                        if (!room->isPublic() && !room->userForRealJid(request.from())) {
                            QXmppMucItem::Affiliation affiliation = d->affiliation(room, request.from());
                            if (affiliation == QXmppMucItem::NoAffiliation ||
                                affiliation == QXmppMucItem::OutcastAffiliation)
                                continue;
                        }

                        QXmppDiscoveryIq::Item item;
                        item.setJid(room->jid());
                        QString info = QString::number(room->users.size());
                        if (!room->isPublic())
                            info = "private, " + info;
                        item.setName(QString("%1 (%2)").arg(room->name(), info));
                        items << item;
                    }
                    response.setItems(items);
                }

                server()->sendPacket(response);
                return true;
            }
        }

        // drop packet
        return true;
    }

    MucRoom *room = d->rooms.value(QXmppUtils::jidToUser(to));

    if (QXmppUtils::jidToResource(to).isEmpty()) {

        // stanza for room
        if (!room) {
            // drop packet
            return true;
        }

        // FIXME: make locking more granular
        QMutexLocker locker(&d->mutex);

        if (element.tagName() == "iq" && QXmppDiscoveryIq::isDiscoveryIq(element))
        {
            QXmppDiscoveryIq request;
            request.parse(element);

            if (request.type() == QXmppIq::Get)
            {
                QXmppDiscoveryIq response;
                response.setFrom(request.to());
                response.setTo(request.from());
                response.setId(request.id());
                response.setType(QXmppIq::Result);
                response.setQueryType(request.queryType());
                if (request.queryType() == QXmppDiscoveryIq::InfoQuery)
                {
                    QStringList features = QStringList() << ns_disco_info << ns_disco_items << ns_muc;
                    if (!room->isPublic())
                        features << "muc_hidden";
                    if (!room->isPersistent())
                        features << "muc_temporary";
                    features << "muc_semianonymous";
                    QList<QXmppDiscoveryIq::Identity> identities;
                    QXmppDiscoveryIq::Identity identity;
                    identity.setCategory("conference");
                    identity.setType("text");
                    identity.setName(room->name());
                    identities.append(identity);
                    response.setFeatures(features);
                    response.setIdentities(identities);
                } else if (request.queryType() == QXmppDiscoveryIq::ItemsQuery) {
                    QList<QXmppDiscoveryIq::Item> items;
                    foreach (MucUser *user, room->users) {
                        QXmppDiscoveryIq::Item item;
                        item.setJid(user->roomJid);
                        items << item;
                    }
                    response.setItems(items);
                }
                server()->sendPacket(response);
                return true;
            }

        } else if (element.tagName() == "iq" && QXmppMucAdminIq::isMucAdminIq(element)) {

            QXmppMucAdminIq request;
            request.parse(element);

            QXmppMucAdminIq response;
            response.setFrom(request.to());
            response.setTo(request.from());
            response.setType(QXmppIq::Result);
            response.setId(request.id());

            // check permissions
            const QXmppMucItem::Affiliation requestAffiliation = d->affiliation(room, request.from());
            if (requestAffiliation < QXmppMucItem::AdminAffiliation) {
                response.setError(QXmppStanza::Error(QXmppStanza::Error::Auth, QXmppStanza::Error::Forbidden));
                response.setType(QXmppIq::Error);
                server()->sendPacket(response);
                return true;
            }

            if (request.type() == QXmppIq::Get && !request.items().isEmpty()) {
                // retrieve requested affiliations
                QXmppMucItem::Affiliation affiliation = request.items().first().affiliation();
                QList<QXmppMucItem> items;
                foreach (const QString &jid, room->affiliations.keys()) {
                    QXmppMucItem::Affiliation itemAffiliation = room->affiliations.value(jid);
                    if (itemAffiliation != affiliation)
                        continue;

                    QXmppMucItem item;
                    item.setJid(jid);
                    item.setAffiliation(itemAffiliation);
                    items << item;
                }
                response.setItems(items);
                server()->sendPacket(response);
            } else if (request.type() == QXmppIq::Set) {
                // check operation is allowed
                QSet<QString> ownerJids = QSet<QString>::fromList(room->affiliations.keys(QXmppMucItem::OwnerAffiliation));
                foreach (const QXmppMucItem &item, request.items()) {
                    // check affiliation changes
                    if (item.affiliation() != QXmppMucItem::UnspecifiedAffiliation) {
                        // check a bare JID was specified
                        const QString jid = item.jid();
                        if (!isBareJid(jid)) {
                            response.setError(QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::BadRequest));
                            response.setType(QXmppIq::Error);
                            server()->sendPacket(response);
                            return true;
                        }
                        // don't allow admins to change admin/owner affiliations
                        const QXmppMucItem::Affiliation currentAffiliation = d->affiliation(room, jid);
                        if (requestAffiliation < QXmppMucItem::OwnerAffiliation &&
                            (currentAffiliation >= QXmppMucItem::AdminAffiliation ||
                             item.affiliation() >= QXmppMucItem::AdminAffiliation)) {
                            response.setError(QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::NotAllowed));
                            response.setType(QXmppIq::Error);
                            server()->sendPacket(response);
                            return true;
                        }

                        // update temporary list of owners
                        if (item.affiliation() == QXmppMucItem::OwnerAffiliation)
                            ownerJids += jid;
                        else
                            ownerJids -= jid;
                    }

                    if (item.role() != QXmppMucItem::UnspecifiedRole) {
                        // don't allow role changes to self
                        MucUser *user = room->userForRoomJid(room->jid() + "/" + item.nick());
                        if (user && user->realJid == request.from()) {
                            response.setError(QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::Conflict));
                            response.setType(QXmppIq::Error);
                            server()->sendPacket(response);
                            return true;
                        }
                    }
                }

                // check there are some room owners left
                if (ownerJids.isEmpty()) {
                    response.setError(QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::Conflict));
                    response.setType(QXmppIq::Error);
                    server()->sendPacket(response);
                    return true;
                }

                // perform changes
                QDjangoQuerySet<MucAffiliation> affiliations;
                affiliations = affiliations.filter(QDjangoWhere("room", QDjangoWhere::Equals, room->jid()));

                QList<QXmppPresence> presences;
                foreach (const QXmppMucItem &item, request.items()) {
                    QSet<MucUser*> changedUsers;

                    // change affiliation
                    if (item.affiliation() != QXmppMucItem::UnspecifiedAffiliation) {
                        // find user(s) by real JID
                        const QString jid = item.jid();
                        foreach (MucUser *user, room->users) {
                            if (QXmppUtils::jidToBareJid(user->realJid) == jid) {
                                changedUsers << user;
                                break;
                            }
                        }

                        if (item.affiliation() == QXmppMucItem::NoAffiliation) {
                            room->affiliations.remove(jid);
                            if (room->isPersistent())
                                affiliations.filter(QDjangoWhere("user", QDjangoWhere::Equals, jid)).remove();
                        } else {
                            room->affiliations[jid] = item.affiliation();
                            if (room->isPersistent()) {
                                MucAffiliation aff;
                                if (!affiliations.get(QDjangoWhere("user", QDjangoWhere::Equals, jid), &aff)) {
                                    aff.setRoom(room->jid());
                                    aff.setUser(jid);
                                }
                                aff.setAffiliation(item.affiliation());
                                aff.save();
                            }
                        }
                    }

                    // change role
                    if (item.role() != QXmppMucItem::UnspecifiedRole) {
                        MucUser *user = room->userForRoomJid(room->jid() + "/" + item.nick());
                        if (user) {
                            if (item.role() == QXmppMucItem::NoRole) {
                                user->role = item.role();

                                // kick user
                                QXmppPresence presence;
                                presence.setFrom(user->roomJid);
                                presence.setTo(user->realJid);
                                presence.setType(QXmppPresence::Unavailable);
                                d->setExtensions(&presence, room, user, user, 307, item.reason());
                                server()->sendPacket(presence);

                                // queue presence to other occupants
                                foreach (MucUser *recipient, room->users) {
                                    if (recipient == user)
                                        continue;
                                    d->setExtensions(&presence, room, user, recipient, 307);
                                    presence.setTo(recipient->realJid);
                                    presences << presence;
                                }

                                // remove occupant
                                info(QString("Kicking MUC user %1 (%2) from room %3").arg(QXmppUtils::jidToResource(user->roomJid), user->realJid, room->jid()));
                                room->users.removeAll(user);
                                changedUsers -= user;
                                delete user;
                                setGauge("muc.participant.count", --d->participantCount);
                            } else {
                                user->role = item.role();
                                changedUsers += user;
                            }
                        }
                    }

                    // queue presence to occupants
                    foreach (MucUser *user, changedUsers) {
                        foreach (MucUser *recipient, room->users) {
                            QXmppPresence presence;
                            presence.setFrom(user->roomJid);
                            presence.setTo(recipient->realJid);
                            d->setExtensions(&presence, room, user, recipient);
                            presences << presence;
                        }
                    }
                }

                // send response
                server()->sendPacket(response);

                // send queued presences
                foreach (const QXmppPresence &presence, presences)
                    server()->sendPacket(presence);
            }
            return true;

        } else if (element.tagName() == "iq" && QXmppMucOwnerIq::isMucOwnerIq(element)) {

            QXmppMucOwnerIq request;
            request.parse(element);

            QXmppMucOwnerIq response;
            response.setFrom(request.to());
            response.setTo(request.from());
            response.setType(QXmppIq::Result);
            response.setId(request.id());

            // check permissions
            const bool isAdmin = d->admins.contains(QXmppUtils::jidToBareJid(request.from()));
            QXmppMucItem::Affiliation affiliation = d->affiliation(room, request.from());
            if (affiliation != QXmppMucItem::OwnerAffiliation) {
                response.setError(QXmppStanza::Error(QXmppStanza::Error::Auth, QXmppStanza::Error::Forbidden));
                response.setType(QXmppIq::Error);
                server()->sendPacket(response);
                return true;
            }

            if (request.type() == QXmppIq::Get) {
                QXmppDataForm form;
                form.setType(QXmppDataForm::Form);
                QList<QXmppDataForm::Field> fields;
                form.setTitle(QString("Configuration of room %1").arg(room->jid()));
                QXmppDataForm::Field field;

                field.setKey("FORM_TYPE");
                field.setType(QXmppDataForm::Field::HiddenField);
                field.setValue("http://jabber.org/protocol/muc#roomconfig");
                fields << field;

                field.setKey("muc#roomconfig_roomname");
                field.setType(QXmppDataForm::Field::TextSingleField);
                field.setLabel("Room title");
                field.setValue(room->name());
                fields << field;

                field.setKey("muc#roomconfig_membersonly");
                field.setType(QXmppDataForm::Field::BooleanField);
                field.setLabel("Make room members-only");
                field.setValue(room->isMembersOnly());
                fields << field;

                if (isAdmin) {
                    field.setKey("muc#roomconfig_persistentroom");
                    field.setType(QXmppDataForm::Field::BooleanField);
                    field.setLabel("Make room persistent");
                    field.setValue(room->isPersistent());
                    fields << field;

                    field.setKey("muc#roomconfig_publicroom");
                    field.setType(QXmppDataForm::Field::BooleanField);
                    field.setLabel("Make room public searchable");
                    field.setValue(room->isPublic());
                    fields << field;
                }

                form.setFields(fields);

                response.setForm(form);
                server()->sendPacket(response);
            } else if (request.type() == QXmppIq::Set) {
                QXmppDataForm form = request.form();

                const bool wasPersistent = room->isPersistent();
                foreach (const QXmppDataForm::Field &field, form.fields()) {
                    if (field.key() == "muc#roomconfig_roomname")
                        room->setName(field.value().toString());
                    else if (field.key() == "muc#roomconfig_membersonly")
                        room->setMembersOnly(field.value().toBool());
                    else if (field.key() == "muc#roomconfig_persistentroom" && isAdmin)
                        room->setPersistent(field.value().toBool());
                    else if (field.key() == "muc#roomconfig_publicroom" && isAdmin)
                        room->setPublic(field.value().toBool());
                }

                // save or remove database entry
                QDjangoQuerySet<MucAffiliation> affiliations;
                affiliations = affiliations.filter(QDjangoWhere("room", QDjangoWhere::Equals, room->jid()));
                if (room->isPersistent()) {
                    room->save();
                    if (!wasPersistent) {
                        foreach (const QString &jid, room->affiliations.keys()) {
                            MucAffiliation aff;
                            if (!affiliations.get(QDjangoWhere("jid", QDjangoWhere::Equals, jid), &aff)) {
                                aff.setRoom(room->jid());
                                aff.setUser(jid);
                            }
                            aff.setAffiliation(room->affiliations.value(jid));
                            aff.save();
                        }
                    }
                } else if (wasPersistent) {
                    affiliations.remove();
                    room->remove();
                }

                server()->sendPacket(response);
            }
            return true;
        } else if (element.tagName() == "message") {
            QXmppMessage message;
            message.parse(element);

            // drop non-groupchat messages
            if (message.type() != QXmppMessage::GroupChat)
                return true;

            // check permissions
            MucUser *user = room->userForRealJid(message.from());
            if (!user ||
                user->role < QXmppMucItem::ParticipantRole ||
                (user->role != QXmppMucItem::ModeratorRole && !message.subject().isEmpty())) {
                QXmppMessage response = message;
                response.setFrom(message.to());
                response.setTo(message.from());
                response.setType(QXmppMessage::Error);
                response.setError(QXmppStanza::Error(QXmppStanza::Error::Auth, QXmppStanza::Error::Forbidden));
                server()->sendPacket(response);
                return true;
            }

            // log long messages
            if (message.body().size() > 256) {
                warning(QString("Long MUC message from %1 to %2").arg(message.from(), message.to()));
            }

            // truncate long messages
            if (message.body().size() > 1024)
                message.setBody(message.body().left(1024) + " [truncated]");

            // store to history
            message.setFrom(user->roomJid);
            message.setStamp(QDateTime::currentDateTime().toUTC());
            room->history << message;
            if (room->history.size() > 20)
                room->history = room->history.mid(1);
            updateCounter("muc.message.send");

            // broadcast message
            foreach (MucUser *recipient, room->users) {
                message.setTo(recipient->realJid);
                server()->sendPacket(message);
            }
            return true;
        }

        // drop packet
        return true;
    }

    // message for a specific user
    if (element.tagName() == "presence")
    {
        QXmppPresence presence;
        presence.parse(element);

        // FIXME: make locking more granular
        QMutexLocker locker(&d->mutex);

        bool created = false;
        if (!room && presence.type() == QXmppPresence::Available) {
            // create room
            const QString name = QXmppUtils::jidToUser(presence.to());
            debug(QString("Creating MUC room %1").arg(name));
            room = new MucRoom;
            room->setJid(name + "@" + d->jid);
            room->setName(name);
            room->affiliations[QXmppUtils::jidToBareJid(presence.from())] = QXmppMucItem::OwnerAffiliation;
            d->rooms[name] = room;
            setGauge("muc.room.count", d->rooms.size());
            created = true;
        }

        if (room) {

            MucUser *user = room->userForRealJid(presence.from());

            if (!user) {

                if (presence.type() != QXmppPresence::Available) {
                    // the user is not part of the room and is not joining it,
                    // so silently discard the presence
                    return true;
                }

                // check the user is not banned
                const QXmppMucItem::Affiliation requestAffiliation = d->affiliation(room, presence.from());
                if (requestAffiliation == QXmppMucItem::OutcastAffiliation) {
                    QXmppPresence pres(presence);
                    pres.setFrom(presence.to());
                    pres.setTo(presence.from());
                    pres.setType(QXmppPresence::Error);
                    pres.setError(QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::Forbidden));
                    server()->sendPacket(pres);
                    return true;
                }

                // check the user's membership
                if (room->isMembersOnly() && requestAffiliation < QXmppMucItem::MemberAffiliation) {
                    QXmppPresence pres(presence);
                    pres.setFrom(presence.to());
                    pres.setTo(presence.from());
                    pres.setType(QXmppPresence::Error);
                    pres.setError(QXmppStanza::Error(QXmppStanza::Error::Auth, QXmppStanza::Error::RegistrationRequired));
                    server()->sendPacket(pres);
                    return true;
                }

                // check the nickname is available
                if (room->userForRoomJid(presence.to())) {
                    QXmppPresence pres(presence);
                    pres.setFrom(presence.to());
                    pres.setTo(presence.from());
                    pres.setType(QXmppPresence::Error);
                    pres.setError(QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::Conflict));
                    server()->sendPacket(pres);
                    return true;
                }

                // create new occupant
                user = new MucUser;
                user->realJid = presence.from();
                user->roomJid = presence.to();
                if (d->affiliation(room, user->realJid) >= QXmppMucItem::AdminAffiliation)
                    user->role = QXmppMucItem::ModeratorRole;
                else
                    user->role = QXmppMucItem::ParticipantRole;
                info(QString("Adding MUC user %1 (%2) to room %3").arg(QXmppUtils::jidToResource(user->roomJid), user->realJid, room->jid()));

                // send existing occupants
                QXmppPresence pres;
                pres.setType(QXmppPresence::Available);
                pres.setTo(presence.from());
                foreach (MucUser *existing, room->users) {
                    pres.setFrom(existing->roomJid);
                    d->setExtensions(&pres, room, existing, user);
                    server()->sendPacket(pres);
                }

                // send room history
                QXmppMessage message;
                foreach (message, room->history) {
                    message.setTo(presence.from());
                    server()->sendPacket(message);
                }

                // add new occupant
                room->users << user;
                setGauge("muc.participant.count", ++d->participantCount);

            } else if (user->roomJid != presence.to()) {

                // the user is already in the room but with another nickname, deny changes
                QXmppPresence pres(presence);
                pres.setFrom(presence.to());
                pres.setTo(presence.from());
                pres.setType(QXmppPresence::Error);
                pres.setError(QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::NotAcceptable));
                server()->sendPacket(pres);
                return true;

            }

            // relay presence
            QXmppPresence pres(presence);
            pres.setFrom(user->roomJid);
            foreach (MucUser *target, room->users) {
                pres.setTo(target->realJid);
                d->setExtensions(&pres, room, user, target, (created && target == user) ? 201 : 0);
                server()->sendPacket(pres);
            }

            if (user && presence.type() == QXmppPresence::Unavailable) {
                // remove occupant
                info(QString("Removing MUC user %1 (%2) from room %3").arg(QXmppUtils::jidToResource(user->roomJid), user->realJid, room->jid()));
                room->users.removeAll(user);
                delete user;
                setGauge("muc.participant.count", --d->participantCount);

                if (room->users.isEmpty() && !room->isPersistent()) {
                    debug(QString("Removing MUC room %1").arg(room->jid()));
                    d->rooms.remove(d->rooms.key(room));
                    setGauge("muc.room.count", d->rooms.size());
                    delete room;
                }
            }

            // we allow the server to handle directed presences
            return false;
        }

        return true;
    }
    else if (room)
    {
        MucUser *user = room->userForRealJid(element.attribute("from"));
        if (user) {
            // rewrite sender
            QDomElement changed = element.cloneNode(true).toElement();
            changed.setAttribute("from", user->roomJid);

            // rewrite recipient
            MucUser *recipient = room->userForRoomJid(element.attribute("to"));
            if (recipient) {
                changed.setAttribute("to", recipient->realJid);
                server()->handleElement(changed);
            }
        }
        return true;
    }

    // drop packet
    return true;
}

QStringList XmppServerMuc::admins() const
{
    return d->admins;
}

void XmppServerMuc::setAdmins(const QStringList &admins)
{
    d->admins = admins;
}

QString XmppServerMuc::jid() const
{
    return d->jid;
}

void XmppServerMuc::setJid(const QString &jid)
{
    d->jid = jid;
}

bool XmppServerMuc::start()
{
    // determine jid
    if (d->jid.isEmpty())
        d->jid = "conference." + server()->domain();

    return true;
}

// PLUGIN

class XmppServerMucPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("muc"))
            return new XmppServerMuc;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("muc");
    };
};

Q_EXPORT_PLUGIN2(muc, XmppServerMucPlugin)

