/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kdeconnect

/**
 * Device management rendered inside the main control-panel window.
 *
 * DeviceManager.qml remains a standalone Kirigami.ApplicationWindow, but the control panel no
 * longer instantiates it as a second top-level window. Pairing, device pages and plugin settings
 * all live in the PageRow below, so every action stays in the same window.
 */
Item {
    id: root
    AppTheme { id: ui }
    property string pendingDeviceId: ""
    property string pendingPluginConfig: ""

    function openDevice(deviceId, pluginConfig) {
        if (devicesModel.busy) {
            root.pendingDeviceId = deviceId;
            root.pendingPluginConfig = pluginConfig;
            return;
        }
        root.pendingDeviceId = "";
        root.pendingPluginConfig = "";
        deviceStack.clear();
        const device = devicesModel.deviceForId(deviceId);
        if (!device) {
            console.warn("Could not find", deviceId);
            return;
        }
        const devicePage = deviceStack.push(deviceComponent, {currentDevice: device});
        if (pluginConfig) {
            const settingsPage = devicePage.openSettings();
            settingsPage.openConfiguration(pluginConfig, pluginConfig);
        }
    }

    function refreshDevices() {
        DaemonDbusInterface.forceOnNetworkChange();
    }

    DevicesModel {
        id: devicesModel
        onBusyChanged: (busy) => {
            if (!busy && root.pendingDeviceId)
                root.openDevice(root.pendingDeviceId, root.pendingPluginConfig);
        }
    }

    DBusProperty {
        id: announcedName
        object: DaemonDbusInterface
        read: "announcedName"
        defaultValue: ""
    }

    Component {
        id: deviceComponent
        DevicePage {}
    }

    RowLayout {
        anchors.fill: parent
        spacing: ui.spacing

        AppCard {
            Layout.preferredWidth: Math.min(250, Math.max(180, root.width * 0.28))
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                QQC2.ToolBar {
                    Layout.fillWidth: true
                    background: Rectangle { color: ui.surfaceAlt }
                    contentItem: RowLayout {
                        QQC2.Label {
                            text: i18nd("kdeconnect-app", "Devices")
                            color: ui.text
                            font.pixelSize: 14
                            font.bold: true
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.largeSpacing
                        }
                        QQC2.ToolButton {
                            text: i18nc("@action:button", "Refresh")
                            icon.name: "view-refresh-symbolic"
                            display: QQC2.AbstractButton.IconOnly
                            onClicked: root.refreshDevices()
                        }
                    }
                }

                QQC2.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ListView {
                        id: devices
                        clip: true
                        activeFocusOnTab: true
                        model: DevicesSortProxyModel {
                            sourceModel: devicesModel
                        }
                        delegate: QQC2.ItemDelegate {
                            id: deviceItem
                            width: ListView.view.width
                            icon.name: model.iconName
                            text: model.name
                            hoverEnabled: true
                            background: Rectangle {
                                radius: ui.radiusSmall
                                color: deviceItem.hovered ? ui.surfaceHover : "transparent"
                            }
                            onClicked: {
                                deviceStack.clear();
                                deviceStack.push(deviceComponent, {currentDevice: model.device});
                            }
                        }
                        QQC2.BusyIndicator {
                            anchors.centerIn: parent
                            visible: devicesModel.busy
                        }
                    }
                }

                QQC2.ToolBar {
                    Layout.fillWidth: true
                    position: QQC2.ToolBar.Footer
                    background: Rectangle { color: ui.surfaceAlt }
                    contentItem: RowLayout {
                        QQC2.Label {
                            text: announcedName.value
                            color: ui.textMuted
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        QQC2.ToolButton {
                            text: i18nc("@action:button", "Settings")
                            icon.name: "settings-configure"
                            display: QQC2.AbstractButton.IconOnly
                            onClicked: deviceStack.pushDialogLayer(Qt.resolvedUrl("Settings.qml"), {}, {
                                title: i18n("Settings")
                            })
                        }
                    }
                }
            }
        }

        AppCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Form-card pages use Kirigami's own colour set; switch it with the theme.
            Kirigami.Theme.colorSet: ui.dark ? Kirigami.Theme.Complementary : Kirigami.Theme.Window
            Kirigami.Theme.inherit: false

            Kirigami.PageRow {
                id: deviceStack
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                initialPage: Qt.resolvedUrl("WelcomePage.qml")
                globalToolBar.canContainHandles: true
                globalToolBar.showNavigationButtons: deviceStack.currentIndex > 0 ? Kirigami.ApplicationHeaderStyle.ShowBackButton : 0
                Keys.onEscapePressed: {
                    if (deviceStack.currentIndex)
                        deviceStack.removePage(deviceStack.currentIndex);
                }
            }
        }
    }
}
