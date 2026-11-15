/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts

/** Theme picker placed at the bottom of the navigation rail. */
Item {
    id: root
    AppTheme { id: ui }

    readonly property var swatches: ["#8a97ab", "#3b6ef5", "#d9753f", "#6c9cff", "#a78bfa", "#4fd1a1"]
    readonly property string currentName: ui.themeNames[ui.themeIndex] || ui.themeNames[1] || qsTr("晴空蓝")

    implicitHeight: 84
    Layout.fillWidth: true

    Rectangle {
        id: card
        anchors.fill: parent
        radius: ui.radiusSmall
        color: ui.sidebarActive
        border.width: 1
        border.color: ui.sidebarHover

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6
            C.Label {
                text: qsTr("外观主题")
                color: ui.sidebarMuted
                font.pixelSize: 11
                Layout.fillWidth: true
            }
            C.Button {
                id: themeButton
                objectName: "panelThemeSwitcher"
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                hoverEnabled: true
                font.pixelSize: 12
                onClicked: menu.open()
                Accessible.name: qsTr("切换外观主题")

                contentItem: RowLayout {
                    spacing: 8
                    Rectangle {
                        Layout.preferredWidth: 12
                        Layout.preferredHeight: 12
                        radius: 6
                        color: root.swatches[ui.themeIndex] || ui.accent
                    }
                    C.Label {
                        text: root.currentName
                        color: ui.sidebarText
                        font: themeButton.font
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    C.Label {
                        text: "\u203a"
                        color: ui.sidebarMuted
                        font.pixelSize: 16
                        rotation: menu.visible ? 90 : 0
                        Behavior on rotation { NumberAnimation { duration: ui.animation } }
                    }
                }
                background: Rectangle {
                    radius: 7
                    color: themeButton.hovered ? ui.sidebarHover : "transparent"
                }
            }
        }
    }

    C.Popup {
        id: menu
        parent: root
        x: 0
        y: -implicitHeight - 8
        width: 218
        padding: 6
        modal: false
        dim: false
        closePolicy: C.Popup.CloseOnEscape | C.Popup.CloseOnPressOutside

        background: Rectangle {
            radius: ui.radiusSmall
            color: ui.surface
            border.width: 1
            border.color: ui.border
        }

        contentItem: Column {
            spacing: 2
            Repeater {
                model: ui.themeNames
                delegate: C.ItemDelegate {
                    id: themeItem
                    required property int index
                    required property string modelData
                    width: menu.availableWidth
                    height: 34
                    highlighted: index === ui.themeIndex
                    hoverEnabled: true
                    onClicked: {
                        ui.setThemeIndex(index);
                        menu.close();
                    }
                    contentItem: RowLayout {
                        spacing: 10
                        Rectangle {
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                            radius: 7
                            color: root.swatches[themeItem.index] || ui.accent
                            border.width: 1
                            border.color: ui.border
                        }
                        C.Label {
                            text: themeItem.modelData
                            color: ui.text
                            font.pixelSize: 13
                            Layout.fillWidth: true
                        }
                        C.Label {
                            visible: themeItem.highlighted
                            text: "\u2713"
                            color: ui.accent
                            font.pixelSize: 14
                        }
                    }
                    background: Rectangle {
                        radius: 7
                        color: themeItem.highlighted ? ui.accentSoft : (themeItem.hovered ? ui.surfaceHover : "transparent")
                    }
                }
            }
        }
    }
}
