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
    property var connections: null
    function openConnections() {
        if (!connections) connections = deviceManager.createObject(root);
        connections.show();
    }
    function openDevice(deviceId, pluginConfig) {
        openConnections();
        connections.openDevice(deviceId, pluginConfig);
    }
    Component { id: deviceManager; DeviceManager { visible: false } }
    ControlDashboard { anchors.fill: parent; onOpenConnections: root.openConnections() }
    onClosing: if (connections) connections.close()
}
