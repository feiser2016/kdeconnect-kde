/**
 * Copyright (C) 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
 * Copyright (C) 2018 Simon Redman <simon@ergotech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "conversationmodel.h"
#include <QLoggingCategory>
#include "interfaces/conversationmessage.h"

Q_LOGGING_CATEGORY(KDECONNECT_SMS_CONVERSATION_MODEL, "kdeconnect.sms.conversation")

ConversationModel::ConversationModel(QObject* parent)
    : QStandardItemModel(parent)
    , m_conversationsInterface(nullptr)
{
    auto roles = roleNames();
    roles.insert(FromMeRole, "fromMe");
    roles.insert(DateRole, "date");
    setItemRoleNames(roles);
}

ConversationModel::~ConversationModel()
{
}

QString ConversationModel::threadId() const
{
    return m_threadId;
}

void ConversationModel::setThreadId(const QString &threadId)
{
    if (m_threadId == threadId)
        return;

    m_threadId = threadId;
    clear();
    if (!threadId.isEmpty()) {
        m_conversationsInterface->requestConversation(threadId, 0, 10);
    }
}

void ConversationModel::setDeviceId(const QString& deviceId)
{
    if (deviceId == m_deviceId)
        return;

    qCDebug(KDECONNECT_SMS_CONVERSATION_MODEL) << "setDeviceId" << "of" << this;
    if (m_conversationsInterface) {
        disconnect(m_conversationsInterface, SIGNAL(conversationMessageReceived(QVariantMap, int)), this, SLOT(createRowFromMessage(QVariantMap, int)));
        disconnect(m_conversationsInterface, SIGNAL(conversationUpdated(QVariantMap)), this, SLOT(handleConversationUpdate(QVariantMap)));
        delete m_conversationsInterface;
    }

    m_deviceId = deviceId;

    m_conversationsInterface = new DeviceConversationsDbusInterface(deviceId, this);
    connect(m_conversationsInterface, SIGNAL(conversationMessageReceived(QVariantMap,int)), this, SLOT(createRowFromMessage(QVariantMap,int)));
    connect(m_conversationsInterface, SIGNAL(conversationUpdated(QVariantMap)), this, SLOT(handleConversationUpdate(QVariantMap)));
}

void ConversationModel::sendReplyToConversation(const QString& message)
{
    qCDebug(KDECONNECT_SMS_CONVERSATION_MODEL) << "Trying to send" << message << "to conversation with ID" << m_threadId;
    m_conversationsInterface->replyToConversation(m_threadId, message);
}

void ConversationModel::createRowFromMessage(const QVariantMap& msg, int pos)
{
    const ConversationMessage message(msg);

    if (!(message.threadID() == m_threadId.toInt())) {
        // Because of the asynchronous nature of the current implementation of this model, if the
        // user clicks quickly between threads or for some other reason a message comes when we're
        // not expecting it, we should not display it in the wrong place
        qCDebug(KDECONNECT_SMS_CONVERSATION_MODEL)
                << "Got a message for a thread" << message.threadID()
                << "but we are currently viewing" << m_threadId
                << "Discarding.";
        return;
    }
    auto item = new QStandardItem;
    item->setText(message.body());
    item->setData(message.type() == ConversationMessage::MessageTypeSent, FromMeRole);
    item->setData(message.date(), DateRole);
    insertRow(pos, item);
}

void ConversationModel::handleConversationUpdate(const QVariantMap& msg)
{
    const ConversationMessage message(msg);

    if (!(message.threadID() == m_threadId.toInt())) {
        // If a conversation which we are not currently viewing was updated, discard the information
        qCDebug(KDECONNECT_SMS_CONVERSATION_MODEL)
                << "Saw update for thread" << message.threadID()
                << "but we are currently viewing" << m_threadId;
        return;
    }

    createRowFromMessage(msg, 0);
}
