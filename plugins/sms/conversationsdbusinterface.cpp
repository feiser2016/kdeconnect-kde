/**
 * Copyright 2018 Simon Redman <simon@ergotech.com>
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

#include "conversationsdbusinterface.h"
#include "interfaces/dbusinterfaces.h"
#include "interfaces/conversationmessage.h"

#include <QDBusConnection>

#include <core/device.h>
#include <core/kdeconnectplugin.h>

Q_LOGGING_CATEGORY(KDECONNECT_CONVERSATIONS, "kdeconnect.conversations")

ConversationsDbusInterface::ConversationsDbusInterface(KdeConnectPlugin* plugin)
    : QDBusAbstractAdaptor(const_cast<Device*>(plugin->device()))
    , m_device(plugin->device())
    , m_plugin(plugin)
    , m_lastId(0)
    , m_smsInterface(m_device->id())
{
    ConversationMessage::registerDbusType();
}

ConversationsDbusInterface::~ConversationsDbusInterface()
{
}

QVariantList ConversationsDbusInterface::activeConversations()
{
    QList<QVariant> toReturn;
    toReturn.reserve(m_conversations.size());

    for (auto it = m_conversations.cbegin(); it != m_conversations.cend(); ++it) {
        const auto& conversation = it.value().values();
        if (conversation.isEmpty()) {
            // This should really never happen because we create a conversation at the same time
            // as adding a message, but better safe than sorry
            qCWarning(KDECONNECT_CONVERSATIONS)
                    << "Conversation with ID" << it.key() << "is unexpectedly empty";
            break;
        }
        const QVariantMap& message = (*conversation.crbegin()).toVariant();
        toReturn.append(message);
    }

    return toReturn;
}

void ConversationsDbusInterface::requestConversation(const QString& conversationID, int start, int end)
{
    const auto messagesList = m_conversations[conversationID].values();

    if (messagesList.isEmpty()) {
        // Since there are no messages in the conversation, it's likely that it is a junk ID, but go ahead anyway
        qCWarning(KDECONNECT_CONVERSATIONS) << "Got a conversationID for a conversation with no messages!" << conversationID;
    }

    // TODO: Check local cache before requesting new messages
    // TODO: Make Android interface capable of requesting small window of messages
    m_smsInterface.requestConversation(conversationID);

    // Messages are sorted in ascending order of keys, meaning the front of the list has the oldest
    // messages (smallest timestamp number)
    // Therefore, return the end of the list first (most recent messages)
    int i = start;
    for(auto it = messagesList.crbegin() + start; it != messagesList.crend(); ++it) {
        Q_EMIT conversationMessageReceived(it->toVariant(), i);
        i++;
        if (i >= end) {
            break;
        }
    }
}

void ConversationsDbusInterface::addMessage(const ConversationMessage &message)
{
    const QString& threadId = QString::number(message.threadID());

    if (m_known_messages[threadId].contains(message.uID())) {
        // This message has already been processed. Don't do anything.
        return;
    }

    // Store the Message in the list corresponding to its thread
    bool newConversation = !m_conversations.contains(threadId);
    m_conversations[threadId].insert(message.date(), message);
    m_known_messages[threadId].insert(message.uID());

    // Tell the world about what just happened
    if (newConversation) {
        Q_EMIT conversationCreated(message.toVariant());
    } else {
        Q_EMIT conversationUpdated(message.toVariant());
    }
}

void ConversationsDbusInterface::removeMessage(const QString& internalId)
{
    // TODO: Delete the specified message from our internal structures
}

void ConversationsDbusInterface::replyToConversation(const QString& conversationID, const QString& message)
{
    const auto messagesList = m_conversations[conversationID];
    if (messagesList.isEmpty()) {
        // Since there are no messages in the conversation, we can't do anything sensible
        qCWarning(KDECONNECT_CONVERSATIONS) << "Got a conversationID for a conversation with no messages!";
        return;
    }
    // Caution:
    // This method assumes that the address of any message (in this case, whichever one pops out
    // with .first()) will be the same. This works fine for single-target SMS but might break down
    // for group MMS, etc.
    const QString& address = messagesList.first().address();
    m_smsInterface.sendSms(address, message);
}

void ConversationsDbusInterface::requestAllConversationThreads()
{
    // Prepare the list of conversations by requesting the first in every thread
    m_smsInterface.requestAllConversations();
}

QString ConversationsDbusInterface::newId()
{
    return QString::number(++m_lastId);
}
