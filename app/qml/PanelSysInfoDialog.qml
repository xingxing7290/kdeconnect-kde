/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts

AppDialog {
    id: dialog
    AppTheme { id: ui }
    objectName: "panelSysInfoDialog"
    property var editor
    parent: C.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, parent ? parent.width - 32 : 460)
    modal: true
    title: qsTr("电脑状态表盘")

    function openEditor() {
        enabledBox.checked = !!editor.sysInfoEnabled;
        const interval = editor.sysInfo ? (editor.sysInfo.intervalMs || 2000) : 2000;
        intervalCombo.currentIndex = [1000, 2000, 5000, 10000, 30000].indexOf(interval);
        if (intervalCombo.currentIndex < 0) intervalCombo.currentIndex = 1;
        validationError = "";
        open();
    }
    property string validationError: ""

    contentItem: ColumnLayout {
        spacing: 12
        C.Label {
            text: qsTr("在平板表盘上显示这台电脑的 CPU、内存、磁盘、网络和运行时间。数据为聚合数值，不包含路径、机器名或进程列表。")
            wrapMode: Text.WordWrap; color: ui.textMuted; Layout.fillWidth: true; lineHeight: 1.6
        }
        C.CheckBox {
            id: enabledBox
            objectName: "panelSysInfoEnabled"
            text: qsTr("启用电脑状态采集")
            checked: !!editor.sysInfoEnabled
            Layout.fillWidth: true
        }
        C.Label { text: qsTr("采样间隔"); Layout.fillWidth: true }
        C.ComboBox {
            id: intervalCombo
            objectName: "panelSysInfoInterval"
            model: [qsTr("1 秒"), qsTr("2 秒"), qsTr("5 秒"), qsTr("10 秒"), qsTr("30 秒")]
            Layout.fillWidth: true
        }
        C.Label {
            text: qsTr("间隔越短数据越及时，但会产生更多流量。默认 2 秒。")
            wrapMode: Text.WordWrap; color: ui.textSubtle; Layout.fillWidth: true
        }
        C.Label {
            objectName: "panelSysInfoError"
            visible: !!dialog.validationError
            text: dialog.validationError; textFormat: Text.PlainText
            color: ui.danger; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
    }

    footer: Item {
        implicitHeight: 52
        RowLayout {
            anchors.fill: parent; anchors.margins: 8
            Item { Layout.fillWidth: true }
            AppButton { objectName: "panelSysInfoCancel"; text: qsTr("取消"); onClicked: dialog.close() }
            AppButton {
                objectName: "panelSysInfoSave"; text: qsTr("保存"); primary: true
                onClicked: {
                    const values = [1000, 2000, 5000, 10000, 30000];
                    const interval = values[intervalCombo.currentIndex];
                    if (editor.setSysInfoEnabled(enabledBox.checked) && editor.setSysInfoInterval(interval))
                        dialog.close();
                    else
                        dialog.validationError = editor.error || qsTr("保存失败，请重试。");
                }
            }
        }
    }
}
