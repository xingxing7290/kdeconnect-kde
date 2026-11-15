/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C

/** Rounded dialog shell shared by every modal in the control panel. */
C.Dialog {
    id: root
    AppTheme { id: ui }

    modal: true
    padding: 22
    spacing: 12

    background: Rectangle {
        radius: ui.radiusLarge
        color: ui.surface
        border.width: 1
        border.color: ui.border

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 8
            radius: parent.radius
            color: ui.shadow
            opacity: ui.shadowOpacity
            z: -1
        }
    }

    header: Item {
        implicitHeight: visible ? 42 : 0
        visible: !!root.title
        Rectangle {
            width: 4
            height: 20
            radius: 2
            color: ui.accent
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
        }
        C.Label {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: ui.text
            font.pixelSize: 17
            font.bold: true
        }
    }
}
