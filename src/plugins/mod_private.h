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

#ifndef XMPP_SERVER_PRIVATE_H
#define XMPP_SERVER_PRIVATE_H

#include "QDjangoModel.h"
#include "QXmppServerExtension.h"

class QXmppElement;

class PrivateStorage : public QDjangoModel
{
    Q_OBJECT
    Q_PROPERTY(QString jid READ jid WRITE setJid)
    Q_PROPERTY(QString xmlns READ xmlns WRITE setXmlns)
    Q_PROPERTY(QString data READ data WRITE setData)

    Q_CLASSINFO("jid", "max_length=255 db_index=true")
    Q_CLASSINFO("xmlns", "max_length=255 db_index=true")

public:
    QString data() const;
    void setData(const QString &data);

    QString jid() const;
    void setJid(const QString &jid);

    QXmppElement xml() const;
    void setXml(const QXmppElement &element);

    QString xmlns() const;
    void setXmlns(const QString &jid);

private:
    QString m_data;
    QString m_jid;
    QString m_xmlns;
};

/// \brief QXmppServer extension for XEP-0049: Private XML Storage.
///
class XmppServerPrivate : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "private");

public:
    XmppServerPrivate();

    QStringList discoveryFeatures() const;
    bool handleStanza(const QDomElement &element);
};

#endif
