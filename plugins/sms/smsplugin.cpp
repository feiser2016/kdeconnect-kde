/**
 * Copyright 2013 Albert Vaca <albertvaka@gmail.com>
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

#include "smsplugin.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include <QDebug>
#include <QDBusConnection>
#include <QLoggingCategory>

#include <core/device.h>
#include <core/daemon.h>

#include "sendreplydialog.h"

K_PLUGIN_FACTORY_WITH_JSON( KdeConnectPluginFactory, "kdeconnect_sms.json", registerPlugin< SmsPlugin >(); )

Q_LOGGING_CATEGORY(KDECONNECT_PLUGIN_SMS, "kdeconnect.plugin.sms")

SmsPlugin::SmsPlugin(QObject* parent, const QVariantList& args)
    : KdeConnectPlugin(parent, args)
    , m_telepathyInterface(QStringLiteral("org.freedesktop.Telepathy.ConnectionManager.kdeconnect"), QStringLiteral("/kdeconnect"))
    , m_conversationInterface(new ConversationsDbusInterface(this))
{
}

SmsPlugin::~SmsPlugin()
{
}

bool SmsPlugin::receivePacket(const NetworkPacket& np)
{
    if (np.type() == PACKET_TYPE_SMS_MESSAGES) {
        return handleBatchMessages(np);
    }

    return true;
}

void SmsPlugin::sendSms(const QString& phoneNumber, const QString& messageBody)
{
    NetworkPacket np(PACKET_TYPE_SMS_REQUEST, {
        {"sendSms", true},
        {"phoneNumber", phoneNumber},
        {"messageBody", messageBody}
    });
    qCDebug(KDECONNECT_PLUGIN_SMS) << "Dispatching SMS send request to remote";
    sendPacket(np);
}

void SmsPlugin::requestAllConversations()
{
    NetworkPacket np(PACKET_TYPE_SMS_REQUEST_CONVERSATIONS);

    sendPacket(np);
}

void SmsPlugin::requestConversation (const QString& conversationID) const
{
    NetworkPacket np(PACKET_TYPE_SMS_REQUEST_CONVERSATION);
    np.set("threadID", conversationID.toInt());

    sendPacket(np);
}

void SmsPlugin::forwardToTelepathy(const ConversationMessage& message)
{
    // If we don't have a valid Telepathy interface, bail out
    if (!(m_telepathyInterface.isValid())) return;

    qCDebug(KDECONNECT_PLUGIN_SMS) << "Passing a text message to the telepathy interface";
    connect(&m_telepathyInterface, SIGNAL(messageReceived(QString,QString)), SLOT(sendSms(QString,QString)), Qt::UniqueConnection);
    const QString messageBody = message.body();
    const QString contactName; // TODO: When telepathy support is improved, look up the contact with KPeople
    const QString phoneNumber = message.address();
    m_telepathyInterface.call(QDBus::NoBlock, QStringLiteral("sendMessage"), phoneNumber, contactName, messageBody);
}

bool SmsPlugin::handleBatchMessages(const NetworkPacket& np)
{
    const auto messages = np.get<QVariantList>("messages");

    for (const QVariant& body : messages) {
        ConversationMessage message(body.toMap());
        forwardToTelepathy(message);
        m_conversationInterface->addMessage(message);
    }

    return true;
}


QString SmsPlugin::dbusPath() const
{
    return "/modules/kdeconnect/devices/" + device()->id() + "/sms";
}

#include "smsplugin.moc"

