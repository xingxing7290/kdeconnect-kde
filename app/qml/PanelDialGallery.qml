/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts

ColumnLayout {
    id: root
    property var editor
    property var now: new Date()
    readonly property var config: editor.config
    Timer { interval: 1000; running: root.visible; repeat: true; onTriggered: root.now = new Date() }
    C.Label { text: qsTr("表盘选择"); font.pixelSize: 26; font.bold: true; color: "#273448" }
    C.Label { text: qsTr("选择空闲时显示的表盘，并设置城市和个性化文本。"); color: "#7b8799" }
    GridLayout {
        columns: 2; rowSpacing: 18; columnSpacing: 18; Layout.fillWidth: true; Layout.fillHeight: true
        Repeater {
            model: [{id: "digital", name: qsTr("数字时钟")}, {id: "analog", name: qsTr("经典指针")}, {id: "weather", name: qsTr("时钟与天气")}, {id: "info", name: qsTr("信息表盘")}, {id: "car", name: qsTr("驾驶风格")}]
            delegate: Rectangle {
                id: card
                required property var modelData
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 100
                radius: 20; color: modelData.id === "info" ? "#e8e9e5" : "#151b27"
                border.color: config.idle.dialId === modelData.id ? "#4988f1" : "transparent"; border.width: 3
                RowLayout {
                    anchors.centerIn: parent; spacing: 20
                    Canvas {
                        id: clockCanvas
                        visible: card.modelData.id === "analog" || card.modelData.id === "weather" || card.modelData.id === "car"
                        Layout.preferredWidth: 82; Layout.preferredHeight: 82
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.reset();
                            ctx.translate(width / 2, height / 2);
                            const radius = width * 0.45;
                            ctx.strokeStyle = card.modelData.id === "car" ? "#58b5f5" : "#b9c7da";
                            ctx.lineWidth = 2;
                            ctx.beginPath(); ctx.arc(0, 0, radius, 0, Math.PI * 2); ctx.stroke();
                            for (let index = 0; index < 12; ++index) {
                                const angle = index * Math.PI / 6;
                                ctx.beginPath(); ctx.moveTo(Math.sin(angle) * radius * 0.86, -Math.cos(angle) * radius * 0.86); ctx.lineTo(Math.sin(angle) * radius, -Math.cos(angle) * radius); ctx.stroke();
                            }
                            const hours = root.now.getHours() % 12 + root.now.getMinutes() / 60;
                            const minutes = root.now.getMinutes() + root.now.getSeconds() / 60;
                            ctx.lineWidth = 3; ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(Math.sin(hours * Math.PI / 6) * radius * 0.52, -Math.cos(hours * Math.PI / 6) * radius * 0.52); ctx.stroke();
                            ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(Math.sin(minutes * Math.PI / 30) * radius * 0.78, -Math.cos(minutes * Math.PI / 30) * radius * 0.78); ctx.stroke();
                        }
                        Connections { target: root; function onNowChanged() { clockCanvas.requestPaint(); } }
                    }
                    ColumnLayout {
                        C.Label { text: Qt.formatTime(root.now, "hh:mm"); color: card.modelData.id === "info" ? "#242931" : "white"; font.pixelSize: 32; font.weight: Font.Light }
                        C.Label { text: card.modelData.id === "weather" ? (config.idle.city || qsTr("未设置城市")) + " · " + qsTr("天气暂不可用") : card.modelData.id === "info" ? Qt.formatDate(root.now, "yyyy/MM/dd dddd") : card.modelData.name; color: card.modelData.id === "info" ? "#555d68" : "#aab9d1"; font.pixelSize: 12 }
                    }
                }
                TapHandler { onTapped: { confirmation.dialId = card.modelData.id; confirmation.open(); } }
            }
        }
    }
    RowLayout {
        C.CheckBox { text: qsTr("空闲自动进入表盘"); checked: config.idle.enabled; onClicked: editor.setIdle("enabled", checked) }
        C.Label { text: qsTr("空闲时长（毫秒）") }
        C.SpinBox { from: 1000; to: 86400000; stepSize: 1000; editable: true; value: config.idle.timeoutMs; onValueModified: editor.setIdle("timeoutMs", value) }
        C.Button { text: qsTr("城市与文本"); onClicked: { cityField.text = config.idle.city; messageField.text = config.idle.message; settings.open(); } }
    }
    C.Dialog {
        id: confirmation; property string dialId; parent: C.Overlay.overlay; anchors.centerIn: parent; modal: true; title: qsTr("使用这个表盘？"); standardButtons: C.Dialog.Ok | C.Dialog.Cancel
        onAccepted: editor.setIdle("dialId", dialId)
    }
    C.Dialog {
        id: settings; parent: C.Overlay.overlay; anchors.centerIn: parent; width: 440; modal: true; title: qsTr("表盘参数")
        ColumnLayout {
            width: parent.width
            C.TextField { id: cityField; placeholderText: qsTr("天气城市"); maximumLength: 120; Layout.fillWidth: true }
            C.TextField { id: messageField; placeholderText: qsTr("个性化文本"); maximumLength: 240; Layout.fillWidth: true }
            C.Button { text: qsTr("保存"); onClicked: if (editor.setIdleOptions({city: cityField.text, message: messageField.text})) settings.close() }
        }
    }
}
