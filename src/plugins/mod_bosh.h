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

#ifndef XMPP_SERVER_BOSH_H
#define XMPP_SERVER_BOSH_H

#include "QDjangoHttpRequest.h"
#include "QDjangoHttpResponse.h"
#include "QXmppServerExtension.h"

class XmppServerBoshPrivate;

class BoshResponse : public QDjangoHttpResponse
{
    Q_OBJECT

public:
    BoshResponse();
    bool isReady() const;
    void setReady(bool ready);

signals:
    void expired();

private:
    bool m_ready;
};

class XmppServerBosh : public QXmppServerExtension
{
    Q_OBJECT
    Q_CLASSINFO("ExtensionName", "bosh");

public:
    XmppServerBosh();
    ~XmppServerBosh();

    bool start();

private slots:
    QDjangoHttpResponse *respondToRequest(const QDjangoHttpRequest &request);
    void responseDestroyed(QObject *obj);
    void responseExpired();
    void streamDisconnected();
    void writeData();

private:
    XmppServerBoshPrivate * const d;
};

#endif
