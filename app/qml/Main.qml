/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C

C.ApplicationWindow {
    id: root
    visible: true
    width: 1240
    height: 800
    minimumWidth: 980
    minimumHeight: 640
    title: qsTr("KDE Connect - Control Panel")
    color: "#f5f7fb"
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 14

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
            leftMargin: 214
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
