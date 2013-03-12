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

#ifndef XMPP_SERVER_VCARD_H
#define XMPP_SERVER_VCARD_H

#include "QDjangoModel.h"

#include "QXmppServerExtension.h"
#include "QXmppVCardIq.h"

class VCard : public QDjangoModel, public QXmppVCardIq
{
    Q_OBJECT
    Q_PROPERTY(QString jid READ jid WRITE setJid)
    Q_PROPERTY(QDate birthday READ birthday WRITE setBirthday)
    Q_PROPERTY(QString email READ email WRITE setEmail)
    Q_PROPERTY(QString firstName READ firstName WRITE setFirstName)
    Q_PROPERTY(QString fullName READ fullName WRITE setFullName)
    Q_PROPERTY(QString lastName READ lastName WRITE setLastName)
    Q_PROPERTY(QString middleName READ middleName WRITE setMiddleName)
    Q_PROPERTY(QString nickName READ nickName WRITE setNickName)
    Q_PROPERTY(QString url READ url WRITE setUrl)
    Q_PROPERTY(QByteArray photo READ photo WRITE setPhoto)
    Q_PROPERTY(QString photoType READ photoType WRITE setPhotoType)

    Q_CLASSINFO("jid", "max_length=255 primary_key=true")
    Q_CLASSINFO("email", "max_length=255")
    Q_CLASSINFO("firstName", "max_length=255")
    Q_CLASSINFO("fullName", "max_length=255")
    Q_CLASSINFO("lastName", "max_length=255")
    Q_CLASSINFO("middleName", "max_length=255")
    Q_CLASSINFO("nickName", "max_length=255")
    Q_CLASSINFO("url", "max_length=255")
    Q_CLASSINFO("photoType", "max_length=255")

public:
    VCard();

    QString jid() const;
    void setJid(const QString &jid);

private:
    QString m_jid;
};

/// \brief QXmppServer extension for XEP-0054: vcard-temp.
///
class XmppServerVCard : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "vcard");

public:
    XmppServerVCard();
    QStringList discoveryFeatures() const;
    bool handleStanza(const QDomElement &element);
};

#endif
