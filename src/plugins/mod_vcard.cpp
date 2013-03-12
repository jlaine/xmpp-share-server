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

#include "mod_vcard.h"

VCard::VCard()
{
}

QString VCard::jid() const
{
    return m_jid;
}

void VCard::setJid(const QString &jid)
{
    m_jid = jid;
}

XmppServerVCard::XmppServerVCard()
{
    QDjango::registerModel<VCard>();
    QDjango::createTables();
}

QStringList XmppServerVCard::discoveryFeatures() const
{
    return QStringList() << ns_vcard;
}

bool XmppServerVCard::handleStanza(const QDomElement &element)
{
    const QString to = element.attribute("to");
    if (QXmppUtils::jidToDomain(to) != server()->domain())
        return false;

    // XEP-0054: vcard-temp
    if (element.tagName() == "iq" && QXmppVCardIq::isVCard(element))
    {
        QXmppVCardIq request;
        request.parse(element);

        if (request.type() == QXmppIq::Get)
        {
            // if the request is not for a local card, drop it
            const QString cardJid = QXmppUtils::jidToUser(request.to()).isEmpty() ? request.from() : request.to();
            if (QXmppUtils::jidToDomain(cardJid) != server()->domain() ||
                QXmppUtils::jidToUser(cardJid).isEmpty())
                return true;

            // retrieve vCard
            QXmppVCardIq response;
            response.setType(QXmppIq::Result);
            response.setId(request.id());
            response.setFrom(request.to());
            response.setTo(request.from());
            VCard card;
            if (QDjangoQuerySet<VCard>().get(
                QDjangoWhere("jid", QDjangoWhere::Equals, QXmppUtils::jidToBareJid(cardJid)), &card))
            {
                response.setEmail(card.email());
                response.setBirthday(card.birthday());
                response.setFirstName(card.firstName());
                response.setMiddleName(card.middleName());
                response.setLastName(card.lastName());
                response.setNickName(card.nickName());
                response.setUrl(card.url());
                response.setPhoto(card.photo());
                response.setPhotoType(card.photoType());
            } else {
                response.setType(QXmppIq::Error);
                response.setError(QXmppStanza::Error(QXmppStanza::Error::Cancel, QXmppStanza::Error::ServiceUnavailable));
            }

            // FIXME : this hack is needed so that vCards work in conference rooms
            if (QXmppUtils::jidToDomain(response.to()) != server()->domain()) {
                QByteArray data;
                QXmlStreamWriter xmlStream(&data);
                response.toXml(&xmlStream);

                QDomDocument doc;
                doc.setContent(data);
                server()->handleElement(doc.documentElement());
            } else {
                server()->sendPacket(response);
            }
        }
        else if (request.type() == QXmppIq::Set)
        {
            // if the request was not from a local user, drop it
            if (QXmppUtils::jidToDomain(request.from()) != server()->domain() ||
                QXmppUtils::jidToUser(request.from()).isEmpty())
                return true;

            // store vCard
            VCard card;
            card.setJid(QXmppUtils::jidToBareJid(request.from()));
            card.setBirthday(request.birthday());
            card.setEmail(request.email());
            card.setFirstName(request.firstName());
            card.setMiddleName(request.middleName());
            card.setLastName(request.lastName());
            card.setNickName(request.nickName());
            card.setUrl(request.url());
            card.setPhoto(request.photo());
            card.setPhotoType(request.photoType());
            card.save();

            // respond
            QXmppIq response;
            response.setType(QXmppIq::Result);
            response.setId(request.id());
            response.setFrom(request.to());
            response.setTo(request.from());
            server()->sendPacket(response);
        }
        return true;
    }
    return false;
}

// PLUGIN

class XmppVCardPlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("vcard"))
            return new XmppServerVCard;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("vcard");
    };
};

Q_EXPORT_PLUGIN2(vcard, XmppVCardPlugin)

