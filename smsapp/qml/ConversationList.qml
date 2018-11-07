/**
 * Copyright (C) 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
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

import QtQuick 2.5
import QtQuick.Controls 2.1
import QtQuick.Layouts 1.1
import org.kde.people 1.0
import org.kde.plasma.core 2.0 as Core
import org.kde.kirigami 2.2 as Kirigami
import org.kde.kdeconnect 1.0
import org.kde.kdeconnect.sms 1.0

Kirigami.ScrollablePage
{
    footer: ComboBox {
        id: devicesCombo
        enabled: count > 0
        model: DevicesSortProxyModel {
            id: devicesModel
            //TODO: make it possible to sort only if they can do sms
            sourceModel: DevicesModel { displayFilter: DevicesModel.Paired | DevicesModel.Reachable }
            onRowsInserted: if (devicesCombo.currentIndex < 0) {
                devicesCombo.currentIndex = 0
            }
        }
        textRole: "display"
    }
    
    Label {
        text: i18n("No devices available")
        anchors.centerIn: parent
        visible: !devicesCombo.enabled
    }

    readonly property QtObject device: devicesCombo.currentIndex >= 0 ? devicesModel.data(devicesModel.index(devicesCombo.currentIndex, 0), DevicesModel.DeviceRole) : null

    Component {
        id: chatView
        ConversationDisplay {}
    }

    ListView {
        id: view
        currentIndex: 0

        model: QSortFilterProxyModel {
            sortOrder: Qt.DescendingOrder
            sortRole: ConversationListModel.DateRole
            filterCaseSensitivity: Qt.CaseInsensitive
            sourceModel: ConversationListModel {
                deviceId: device ? device.id() : ""
            }
        }

        header: TextField {
            id: filter
            placeholderText: i18n("Filter...")
            width: parent.width
            onTextChanged: {
                view.model.setFilterFixedString(filter.text);
                view.currentIndex = 0
            }
            Keys.onUpPressed: view.currentIndex = Math.max(view.currentIndex-1, 0)
            Keys.onDownPressed: view.currentIndex = Math.min(view.currentIndex+1, view.count-1)
            onAccepted: {
                view.currentItem.startChat()
            }
            Shortcut {
                sequence: "Ctrl+F"
                onActivated: filter.forceActiveFocus()
            }
        }

        delegate: Kirigami.BasicListItem
        {
            hoverEnabled: true

            label: i18n("<b>%1</b> <br> %2", display, toolTip)
            icon: decoration
            function startChat() {
                applicationWindow().pageStack.push(chatView, {
                                                       personUri: model.personUri,
                                                       phoneNumber: address,
                                                       conversationId: model.conversationId,
                                                       device: device})
            }
            onClicked: { startChat(); }
        }

    }
}
