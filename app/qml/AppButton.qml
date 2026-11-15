/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C

/** Consistent button: soft neutral by default, accent filled when highlighted/primary. */
C.Button {
    id: root
    property bool primary: false
    property bool danger: false

    AppTheme { id: ui }
    readonly property bool filled: root.highlighted || root.primary
    readonly property color fillColor: root.danger && !filled ? ui.danger : ui.accent

    implicitHeight: 36
    leftPadding: 16
    rightPadding: 16
    hoverEnabled: true
    font.pixelSize: 13

    contentItem: C.Label {
        text: root.text
        font: root.font
        color: root.filled ? (root.danger ? "#ffffff" : ui.textOnAccent)
                           : (root.danger ? ui.danger : ui.text)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: ui.radiusSmall
        color: root.filled ? (root.hovered ? (root.danger ? Qt.darker(ui.danger, 1.15) : ui.accentHover) : (root.danger ? ui.danger : ui.accent))
                           : (root.hovered ? ui.surfaceHover : ui.surface)
        border.width: root.filled ? 0 : 1
        border.color: root.danger && !root.filled ? Qt.rgba(ui.danger.r, ui.danger.g, ui.danger.b, 0.45) : ui.border
        opacity: root.enabled ? 1 : 0.55

        Behavior on color { ColorAnimation { duration: ui.animation } }
        Behavior on border.color { ColorAnimation { duration: ui.animation } }
    }
}
