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
#include <QStringList>

#include "QDjangoQuerySet.h"

#include "QXmppConstants.h"
#include "QXmppPresence.h"
#include "QXmppRosterIq.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppStream.h"
#include "QXmppUtils.h"

#include "mod_presence.h"
#include "mod_roster.h"

Contact::Contact()
    : m_subscription(QXmppRosterIq::Item::None),
    m_ask(QXmppRosterIq::Item::None),
    m_hidden(false)
{
}

QString Contact::user() const
{
    return m_user;
}

void Contact::setUser(const QString &user)
{
    m_user = user;
}

QString Contact::jid() const
{
    return m_jid;
}

void Contact::setJid(const QString &jid)
{
    m_jid = jid;
}

QSet<QString> Contact::groups() const
{
    return m_groups;
}

void Contact::setGroups(const QSet<QString> &groups)
{
    m_groups = groups;
}

QString Contact::groupString() const
{
    QStringList groups;
    QString group;
    foreach (group, m_groups) {
        group.replace("\n", "");
        groups << group;
    }
    return groups.join("\n");
}

void Contact::setGroupString(const QString &groupString)
{
    QStringList groups = groupString.split("\n");
    m_groups = QSet<QString>::fromList(groups);
}

QString Contact::name() const
{
    return m_name;
}

void Contact::setName(const QString &name)
{
    m_name = name;
}

int Contact::subscription() const
{
    return m_subscription;
}

void Contact::setSubscription(int subscription)
{
    m_subscription = subscription;
}

int Contact::ask() const
{
    return m_ask;
}

void Contact::setAsk(int ask)
{
    m_ask = ask;
}

bool Contact::hidden() const
{
    return m_hidden;
}

void Contact::setHidden(bool hidden)
{
    m_hidden = hidden;
}

bool Contact::hasSubscription(int subscription) const
{
    return (m_ask & subscription) || (m_subscription & subscription);
}

QXmppRosterIq::Item Contact::toRosterItem() const
{
    QXmppRosterIq::Item item;
    item.setName(m_name);
    item.setBareJid(m_jid);
    item.setGroups(m_groups);
    item.setSubscriptionType(static_cast<QXmppRosterIq::Item::SubscriptionType>(m_subscription));
    if (m_ask & QXmppRosterIq::Item::To)
        item.setSubscriptionStatus("subscribe");
    return item;
}

static bool getContact(const QString &userJid, const QString &contactJid, Contact &contact)
{
    const QString bareUser = QXmppUtils::jidToBareJid(userJid);
    const QString bareContact = QXmppUtils::jidToBareJid(contactJid);
    QDjangoQuerySet<Contact> contacts;
    contacts = contacts.filter(QDjangoWhere("user", QDjangoWhere::Equals, bareUser));
    if (!contacts.get(QDjangoWhere("jid", QDjangoWhere::Equals, bareContact), &contact))
    {
        contact.setUser(bareUser);
        contact.setJid(bareContact);
        return false;
    }
    return true;
}

static bool pushContact(QXmppServer *server, const Contact &contact)
{
    QXmppRosterIq push;
    push.setType(QXmppIq::Set);
    push.setTo(contact.user());
    push.addItem(contact.toRosterItem());
    return server->sendPacket(push);
}

XmppServerRoster::XmppServerRoster()
{
    QDjango::registerModel<Contact>();
    QDjango::createTables();
}

QStringList XmppServerRoster::discoveryFeatures() const
{
    return QStringList() << ns_roster;
}

/// Handles a presence TO a local user.

bool XmppServerRoster::handleInboundPresence(const QXmppPresence &presence)
{
    Contact contact;
    getContact(presence.to(), presence.from(), contact);

    if (presence.type() == QXmppPresence::Subscribe)
    {
        // if we are already subscribed, drop the packet
        if (contact.hasSubscription(QXmppRosterIq::Item::From))
            return true;

        // update roster item
        contact.setAsk(contact.ask() | QXmppRosterIq::Item::From);
        contact.setHidden(contact.pk().isNull());
        contact.save();
    }
    else if (presence.type() == QXmppPresence::Unsubscribe)
    {
        // if we are not subscribed, drop the packet
        if (!contact.hasSubscription(QXmppRosterIq::Item::From))
            return true;

        if (contact.hidden()) {
            // remove temporary item
            contact.remove();
        } else {
            // update roster item
            contact.setAsk(contact.ask() & ~QXmppRosterIq::Item::From);
            contact.setSubscription(contact.subscription() & ~QXmppRosterIq::Item::From);
            contact.save();

            // send notification to all connected resources
            pushContact(server(), contact);
        }
    }
    else if (presence.type() == QXmppPresence::Subscribed)
    {
        // if we did not ask for a subscription or were already subscribed,
        // drop the packet
        if (!(contact.ask() & QXmppRosterIq::Item::To) ||
            (contact.subscription() & QXmppRosterIq::Item::To))
            return true;

        contact.setAsk(contact.ask() & ~QXmppRosterIq::Item::To);
        contact.setSubscription(contact.subscription() | QXmppRosterIq::Item::To);
        contact.save();

        // send notification to all connected resources
        pushContact(server(), contact);
    }
    else if (presence.type() == QXmppPresence::Unsubscribed)
    {
        // if we did not ask for a subscription and were not subscribed,
        // drop the packet
        if (!contact.hasSubscription(QXmppRosterIq::Item::To))
            return true;

        contact.setAsk(contact.ask() & ~QXmppRosterIq::Item::To);
        contact.setSubscription(contact.subscription() & ~QXmppRosterIq::Item::To);
        contact.save();

        // send notification to all connected resources
        pushContact(server(), contact);
    }

    return false;
}

/// Handles a presence FROM a local user.

bool XmppServerRoster::handleOutboundPresence(const QXmppPresence &presence)
{
    // get handle to the presence module
    XmppServerPresence *presenceExtension = XmppServerPresence::instance(server());
    Q_ASSERT(presenceExtension);

    Contact contact;
    getContact(presence.from(), presence.to(), contact);

    if (presence.type() == QXmppPresence::Subscribe)
    {
        if (!contact.hasSubscription(QXmppRosterIq::Item::To))
        {
            // update roster item
            contact.setAsk(contact.ask() | QXmppRosterIq::Item::To);
            contact.save();

            // send notification to all connected resources
            pushContact(server(), contact);
        }
    }
    else if (presence.type() == QXmppPresence::Unsubscribe)
    {
        if (contact.hasSubscription(QXmppRosterIq::Item::To))
        {
            // update roster item
            contact.setAsk(contact.ask() & ~QXmppRosterIq::Item::To);
            contact.setSubscription(contact.subscription() & ~QXmppRosterIq::Item::To);
            contact.save();

            // send notification to all connected resources
            pushContact(server(), contact);
        }
    }
    else if (presence.type() == QXmppPresence::Subscribed)
    {
        // update roster item
        contact.setAsk(contact.ask() & ~QXmppRosterIq::Item::From);
        contact.setSubscription(contact.subscription() | QXmppRosterIq::Item::From);
        contact.setHidden(false);
        contact.save();

        // send notification to all connected resources
        pushContact(server(), contact);

        // send available presence from all connected resources
        const QString bareFrom = QXmppUtils::jidToBareJid(presence.from());
        QXmppPresence availablePresence;
        foreach (availablePresence, presenceExtension->availablePresences(bareFrom))
        {
            availablePresence.setTo(contact.jid());
            server()->sendPacket(availablePresence);
        }
    }
    else if (presence.type() == QXmppPresence::Unsubscribed)
    {
        if (contact.hasSubscription(QXmppRosterIq::Item::From)) {
            if (contact.hidden()) {
                contact.remove();
            } else {
                // update roster item
                contact.setAsk(contact.ask() & ~QXmppRosterIq::Item::From);
                contact.setSubscription(contact.subscription() & ~QXmppRosterIq::Item::From);
                contact.save();

                // send notification to all connected resources
                pushContact(server(), contact);
            }
        }

        // send unavailable presence from all connected resources
        const QString bareFrom = QXmppUtils::jidToBareJid(presence.from());
        QXmppPresence unavailablePresence;
        unavailablePresence.setType(QXmppPresence::Unavailable);
        unavailablePresence.setTo(contact.jid());
        foreach (const QXmppPresence &availablePresence, presenceExtension->availablePresences(bareFrom))
        {
            unavailablePresence.setFrom(availablePresence.from());
            server()->sendPacket(unavailablePresence);
        }
    }

    return false;
}

bool XmppServerRoster::handleStanza(const QDomElement &element)
{
    const QString from = element.attribute("from");
    const QString to = element.attribute("to");
    const QString domain = server()->domain();

    if (element.tagName() == "iq" && to == domain)
    {
        const QString type = element.attribute("type");

        // only handle stanzas from local users
        if (QXmppUtils::jidToDomain(from) != domain)
            return false;

        if (QXmppRosterIq::isRosterIq(element))
        {
            QXmppRosterIq request;
            request.parse(element);

            // start building response
            QXmppRosterIq response;
            response.setId(request.id());
            response.setTo(request.from());
            response.setType(QXmppIq::Result);

            const QString userJid = QXmppUtils::jidToBareJid(from);
            QDjangoQuerySet<Contact> contacts;
            contacts = contacts.filter(QDjangoWhere("user", QDjangoWhere::Equals, userJid));
            if (request.type() == QXmppIq::Get)
            {
                bool sendQueued = m_connected.remove(from);
                QList<QXmppPresence> presenceQueue;

                // retrieve roster
                Contact contact;
                for (int i = 0; i < contacts.size(); i++)
                {
                    if (contacts.at(i, &contact)) {
                        // add item
                        if (!contact.hidden())
                            response.addItem(contact.toRosterItem());

                        // check whether we have a pending subscribe
                        if (sendQueued && (contact.ask() & QXmppRosterIq::Item::From)) {
                            QXmppPresence presence;
                            presence.setFrom(contact.jid());
                            presence.setTo(contact.user());
                            presence.setType(QXmppPresence::Subscribe);
                            presenceQueue << presence;
                        }
                    }
                }
                server()->sendPacket(response);

                // send pending subscribe requests
                foreach (const QXmppPresence &presence, presenceQueue)
                    server()->sendPacket(presence);
            }
            else if (request.type() == QXmppIq::Set)
            {
                QSet<QString> removedContacts;

                // modify roster
                QXmppRosterIq push;
                push.setType(QXmppRosterIq::Set);
                push.setTo(QXmppUtils::jidToBareJid(request.from()));
                foreach (const QXmppRosterIq::Item &item, request.items())
                {
                    Contact *contact = contacts.get(
                        QDjangoWhere("jid", QDjangoWhere::Equals, item.bareJid()));
                    if (item.subscriptionType() == QXmppRosterIq::Item::Remove)
                    {
                        // remove entry
                        if (contact) {
                            contact->remove();
                            push.addItem(item);

                            // unsubscribe
                            QXmppPresence presence;
                            presence.setFrom(contact->user());
                            presence.setTo(contact->jid());
                            presence.setType(QXmppPresence::Unsubscribe);
                            server()->sendPacket(presence);
                            if (QXmppUtils::jidToDomain(contact->jid()) == domain) {
                                handleInboundPresence(presence);
                            }

                            // unsubscribed
                            presence.setType(QXmppPresence::Unsubscribed);
                            server()->sendPacket(presence);
                            if (QXmppUtils::jidToDomain(contact->jid()) == domain) {
                                handleInboundPresence(presence);
                            }

                            // mark as removed
                            removedContacts.insert(contact->jid());
                        }
                    } else {
                        // create/update entry
                        if (!contact) {
                            contact = new Contact;
                            contact->setUser(userJid);
                            contact->setJid(item.bareJid());
                        }
                        contact->setGroups(item.groups());
                        contact->setHidden(false);
                        contact->setName(item.name());
                        contact->setSubscription(item.subscriptionType());
                        contact->save();
                        push.addItem(contact->toRosterItem());
                    }
                    delete contact;
                }
                server()->sendPacket(push);

                // response to request
                server()->sendPacket(response);

                // send unavailable presence from all connected resources to removed contacts
                if (!removedContacts.isEmpty()) {
                    // get handle to the presence module
                    XmppServerPresence *presenceExtension = XmppServerPresence::instance(server());
                    Q_ASSERT(presenceExtension);

                    QXmppPresence unavailablePresence;
                    unavailablePresence.setType(QXmppPresence::Unavailable);
                    const QString bareFrom = QXmppUtils::jidToBareJid(request.from());
                    foreach (const QXmppPresence &availablePresence, presenceExtension->availablePresences(bareFrom)) {
                        unavailablePresence.setFrom(availablePresence.from());
                        foreach (const QString &jid, removedContacts) {
                            unavailablePresence.setTo(jid);
                            server()->sendPacket(unavailablePresence);
                        }
                    }
                }
            }

            // disable any further processing of this stanza
            return true;
        }
    }
    else if (element.tagName() == "presence" && to != domain)
    {
        QXmppPresence presence;
        presence.parse(element);

        // handle presence probes
        if (presence.type() == QXmppPresence::Probe &&
            QXmppUtils::jidToDomain(to) == domain &&
            QXmppUtils::jidToDomain(from) != domain)
        {
            Contact contact;
            if (getContact(presence.to(), presence.from(), contact) &&
                contact.subscription() & QXmppRosterIq::Item::From)
            {
                // get handle to the presence module
                XmppServerPresence *presenceExtension = XmppServerPresence::instance(server());
                Q_ASSERT(presenceExtension);

                QXmppPresence availablePresence;
                foreach (availablePresence, presenceExtension->availablePresences(QXmppUtils::jidToBareJid(to))) {
                    availablePresence.setTo(from);
                    server()->sendPacket(availablePresence);
                }
            }
            return true;
        }

        // we are only interested in subscriptions
        if (presence.type() != QXmppPresence::Subscribe &&
            presence.type() != QXmppPresence::Subscribed &&
            presence.type() != QXmppPresence::Unsubscribe &&
            presence.type() != QXmppPresence::Unsubscribed)
            return false;

        if (QXmppUtils::jidToDomain(from) == domain)
        {
            handleOutboundPresence(presence);

            // change sender JID to bare JID before routing
            QDomElement changed(element);
            changed.setAttribute("from", QXmppUtils::jidToBareJid(presence.from()));
        }

        if (QXmppUtils::jidToDomain(to) == domain)
        {
            if (handleInboundPresence(presence))
                return true;
        }

        // let the presence be routed
    }
    return false;
}

QSet<QString> XmppServerRoster::presenceSubscribers(const QString &from)
{
    QSet<QString> subscribers;

    // check the request is from a local user
    if (QXmppUtils::jidToDomain(from) != server()->domain())
        return subscribers;

    // return subscribers
    QDjangoQuerySet<Contact> qs;
    qs = qs.filter(QDjangoWhere("user", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(from)));
    Contact contact;
    for (int i = 0; i < qs.size(); i++) {
        if (qs.at(i, &contact) &&
            (contact.subscription() & QXmppRosterIq::Item::From))
            subscribers << contact.jid();
    }
    return subscribers;
}

QSet<QString> XmppServerRoster::presenceSubscriptions(const QString &from)
{
    QSet<QString> subscribers;

    // check the request is from a local user
    if (QXmppUtils::jidToDomain(from) != server()->domain())
        return subscribers;

    // return subscribers
    QDjangoQuerySet<Contact> qs;
    qs = qs.filter(QDjangoWhere("user", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(from)));
    Contact contact;
    for (int i = 0; i < qs.size(); i++) {
        if (qs.at(i, &contact) &&
            (contact.subscription() & QXmppRosterIq::Item::To))
            subscribers << contact.jid();
    }

    return subscribers;
}

bool XmppServerRoster::start()
{
    bool check;
    Q_UNUSED(check);

    check = connect(server(), SIGNAL(clientConnected(QString)),
                    this, SLOT(_q_clientConnected(QString)));
    Q_ASSERT(check);

    return true;
}

void XmppServerRoster::stop()
{
    disconnect(server(), SIGNAL(clientConnected(QString)),
               this, SLOT(_q_clientConnected(QString)));
}

void XmppServerRoster::_q_clientConnected(const QString &jid)
{
    m_connected.insert(jid);
}

// PLUGIN

class XmppRosterPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("roster"))
            return new XmppServerRoster;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("roster");
    };
};

Q_EXPORT_PLUGIN2(roster, XmppRosterPlugin)

