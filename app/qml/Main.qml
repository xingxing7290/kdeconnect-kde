/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C

C.ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 820
    minimumWidth: 1000
    minimumHeight: 660
    title: qsTr("KDE Connect · 控制面板")
    color: ui.window
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 14

    AppTheme { id: ui }
    Behavior on color { ColorAnimation { duration: ui.animation } }

    // Qt Quick Controls "Basic" style reads this palette, so text fields, combo
    // boxes, check boxes and sliders follow the selected theme without per-item styling.
    palette.window: ui.window
    palette.windowText: ui.text
    palette.base: ui.field
    palette.alternateBase: ui.surfaceAlt
    palette.text: ui.text
    palette.button: ui.surface
    palette.buttonText: ui.text
    palette.highlight: ui.accent
    palette.highlightedText: ui.textOnAccent
    palette.light: ui.surface
    palette.mid: ui.border
    palette.dark: ui.borderStrong
    palette.link: ui.link

    // Soft ambient tint on the far edge keeps the flat background from looking bare.
    Rectangle {
        anchors.fill: parent
        z: -1
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0) }
            GradientStop { position: 1.0; color: Qt.rgba(ui.accent.r, ui.accent.g, ui.accent.b, ui.dark ? 0.10 : 0.06) }
        }
    }

    // Device management lives inside this window's "开启连接" section. The standalone
    // DeviceManager.qml is no longer instantiated, so pairing never opens a second window.
    function openConnections() {
        dashboard.section = 2;
    }
    function openDevice(deviceId, pluginConfig) {
        openConnections();
        Qt.callLater(function() {
            if (deviceLoader.item)
                deviceLoader.item.openDevice(deviceId, pluginConfig);
        });
    }

    ControlDashboard {
        id: dashboard
        anchors.fill: parent
        onOpenConnections: root.openConnections()
    }

    // Covers the dashboard content area only; the left navigation stays available.
    Loader {
        id: deviceLoader
        anchors {
            left: parent.left
            leftMargin: ui.sidebarWidth
            right: parent.right
            rightMargin: 24
            top: parent.top
            topMargin: 26
            bottom: parent.bottom
            bottomMargin: 20
        }
        active: dashboard.section == 2
        sourceComponent: EmbeddedDevices {
            objectName: "panelEmbeddedDevices"
        }
    }
}
