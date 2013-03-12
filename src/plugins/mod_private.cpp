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

#include "QXmppElement.h"
#include "QXmppIq.h"
#include "QXmppServer.h"
#include "QXmppServerPlugin.h"
#include "QXmppUtils.h"

#include "mod_private.h"

static const char * ns_private_storage = "jabber:iq:private";

QString PrivateStorage::data() const
{
    return m_data;
}

void PrivateStorage::setData(const QString &data)
{
    m_data = data;
}

QString PrivateStorage::jid() const
{
    return m_jid;
}

void PrivateStorage::setJid(const QString &jid)
{
    m_jid = jid;
}

QXmppElement PrivateStorage::xml() const
{
    QDomDocument doc;
    doc.setContent(m_data);
    return QXmppElement(doc.documentElement());
}

void PrivateStorage::setXml(const QXmppElement &element)
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    element.toXml(&writer);
    m_data = QString::fromUtf8(buffer.data());
}

QString PrivateStorage::xmlns() const
{
    return m_xmlns;
}

void PrivateStorage::setXmlns(const QString &xmlns)
{
    m_xmlns = xmlns;
}

class PrivateStorageIq : public QXmppIq
{
public:
    QXmppElement payload() const;
    void setPayload(const QXmppElement &payload);

    static bool isPrivateStorageIq(const QDomElement &element);

protected:
    void parseElementFromChild(const QDomElement &element);
    void toXmlElementFromChild(QXmlStreamWriter *writer) const;

private:
    QXmppElement m_payload;
    QString m_data;
};

bool PrivateStorageIq::isPrivateStorageIq(const QDomElement &element)
{
    const QDomElement queryElement = element.firstChildElement("query");
    return queryElement.namespaceURI() == ns_private_storage;
}

void PrivateStorageIq::parseElementFromChild(const QDomElement &element)
{
    const QDomElement queryElement = element.firstChildElement("query");
    m_payload = queryElement.firstChildElement();
}

void PrivateStorageIq::toXmlElementFromChild(QXmlStreamWriter *writer) const
{
    writer->writeStartElement("query");
    helperToXmlAddAttribute(writer, "xmlns", ns_private_storage);
    m_payload.toXml(writer);
    writer->writeEndElement();
}

QXmppElement PrivateStorageIq::payload() const
{
    return m_payload;
}

void PrivateStorageIq::setPayload(const QXmppElement &payload)
{
    m_payload = payload;
}

XmppServerPrivate::XmppServerPrivate()
{
    QDjango::registerModel<PrivateStorage>();
    QDjango::createTables();
}

QStringList XmppServerPrivate::discoveryFeatures() const
{
    return QStringList() << ns_private_storage;
}

bool XmppServerPrivate::handleStanza(const QDomElement &element)
{
    const QString to = element.attribute("to");
    if (QXmppUtils::jidToDomain(to) != server()->domain())
        return false;

    if (element.tagName() == "iq" && PrivateStorageIq::isPrivateStorageIq(element))
    {
        PrivateStorageIq request;
        request.parse(element);

        if (request.type() == QXmppIq::Result)
            return true;

        // check the namespace is valid
        const QString xmlns = request.payload().attribute("xmlns");
        if (xmlns.isEmpty()) {
            QXmppIq response;
            response.setId(request.id());
            response.setTo(request.from());
            response.setType(QXmppIq::Error);
            server()->sendPacket(response);
            return true;
        }

        const QString bareFrom = QXmppUtils::jidToBareJid(request.from());
        QDjangoQuerySet<PrivateStorage> qs;
        qs = qs.filter(QDjangoWhere("jid", QDjangoWhere::Equals, bareFrom));

        if (request.type() == QXmppIq::Get) {
            PrivateStorageIq response;
            response.setId(request.id());
            response.setTo(request.from());
            response.setType(QXmppIq::Result);
 
            PrivateStorage storage;
            if (qs.get(QDjangoWhere("xmlns", QDjangoWhere::Equals, xmlns), &storage)) {
                QDomDocument doc;
                doc.setContent(storage.data());
                response.setPayload(doc.documentElement());
            } else {
                response.setPayload(request.payload());
            }
            server()->sendPacket(response);

        } else if (request.type() == QXmppIq::Set) {
            PrivateStorage storage;
            if (!qs.get(QDjangoWhere("xmlns", QDjangoWhere::Equals, xmlns), &storage)) {
                storage.setJid(bareFrom);
                storage.setXmlns(xmlns);
            }
            storage.setXml(request.payload());
            storage.save();

            // reply
            QXmppIq response;
            response.setId(request.id());
            response.setTo(request.from());
            response.setType(QXmppIq::Result);
            server()->sendPacket(response);
        }
        return true;
    }
    return false;
}

// PLUGIN

class XmppServerPrivatePlugin : public QXmppServerPlugin
{
public:
    QXmppServerExtension *create(const QString &key)
    {
        if (key == QLatin1String("private"))
            return new XmppServerPrivate;
        else
            return 0;
    };

    QStringList keys() const
    {
        return QStringList() << QLatin1String("private");
    };
};

Q_EXPORT_PLUGIN2(private, XmppServerPrivatePlugin)

