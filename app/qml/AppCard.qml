/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick

/** Surface container: rounded corners, hairline border and a soft drop shadow. */
Rectangle {
    id: root
    property bool elevated: true

    AppTheme { id: ui }
    radius: ui.radius
    color: ui.surface
    border.width: 1
    border.color: ui.border

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: elevated ? 4 : 0
        anchors.leftMargin: elevated ? 1 : 0
        anchors.rightMargin: elevated ? 1 : 0
        radius: root.radius
        color: ui.shadow
        opacity: elevated ? ui.shadowOpacity : 0
        z: -1
    }
}
