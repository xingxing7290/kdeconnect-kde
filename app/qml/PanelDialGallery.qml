/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "panelDialGallery"
    property var editor
    property var now: new Date()
    readonly property var config: editor.config
    readonly property var dials: [{id: "digital", name: qsTr("数字时钟")}, {id: "analog", name: qsTr("经典指针")}, {id: "weather", name: qsTr("时钟与天气")}, {id: "info", name: qsTr("信息表盘")}, {id: "car", name: qsTr("汽车视觉")}]
    spacing: 12
    Timer { interval: 1000; running: root.visible; repeat: true; onTriggered: root.now = new Date() }
    RowLayout {
        Layout.fillWidth: true
        C.Label { text: qsTr("表盘选择"); font.pixelSize: 26; font.bold: true; color: "#273448"; Layout.fillWidth: true }
        C.ToolButton {
            objectName: "panelDialSettingsGear"
            Accessible.name: qsTr("表盘设置")
            contentItem: Kirigami.Icon { source: "preferences-system"; implicitWidth: 24; implicitHeight: 24 }
            onClicked: settings.openForCurrent()
        }
    }
    C.Label { text: qsTr("空闲后自动显示；首次触摸只唤醒，不执行按钮。"); color: "#7b8799"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    C.ScrollView {
        Layout.fillWidth: true; Layout.fillHeight: true
        contentWidth: availableWidth
        C.ScrollBar.horizontal.policy: C.ScrollBar.AlwaysOff
        ColumnLayout {
            width: parent.width
            spacing: 12
            RowLayout {
                Layout.fillWidth: true; spacing: 18
                ColumnLayout {
                    Layout.fillWidth: true
                    C.Label { text: qsTr("精选推荐"); font.bold: true; color: "#273448" }
                    PanelDialPreview {
                        objectName: "panelDialFeatured"
                        Layout.fillWidth: true; Layout.preferredHeight: Math.max(160, width / 2.2)
                        dialId: "analog"; city: config.idle.city; message: config.idle.message; now: root.now; weather: editor.weather
                        TapHandler { onTapped: { confirmation.dialId = "analog"; confirmation.open(); } }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    C.Label { text: qsTr("当前表盘"); font.bold: true; color: "#273448" }
                    PanelDialPreview {
                        objectName: "panelDialCurrent"
                        Layout.fillWidth: true; Layout.preferredHeight: Math.max(160, width / 2.2)
                        dialId: config.idle.dialId; city: config.idle.city; message: config.idle.message; now: root.now; weather: editor.weather
                        border.color: "#4988f1"; border.width: 2
                    }
                }
            }
            C.Label { text: qsTr("更多表盘"); font.bold: true; color: "#273448" }
            GridLayout {
                columns: 2; uniformCellWidths: true; rowSpacing: 12; columnSpacing: 18; Layout.fillWidth: true
                Repeater {
                    model: root.dials
                    delegate: ColumnLayout {
                        id: card
                        required property var modelData
                        Layout.fillWidth: true
                        PanelDialPreview {
                            objectName: "panel-dial-card-" + card.modelData.id
                            Layout.fillWidth: true; Layout.preferredHeight: Math.max(160, width / 2.2)
                            dialId: card.modelData.id; city: config.idle.city; message: config.idle.message; now: root.now; weather: editor.weather
                            border.color: config.idle.dialId === dialId ? "#4988f1" : "transparent"; border.width: 2
                            TapHandler { onTapped: { confirmation.dialId = card.modelData.id; confirmation.open(); } }
                        }
                        C.Label { text: card.modelData.name; color: "#69778b"; font.pixelSize: 12 }
                    }
                }
            }
            C.Label { text: qsTr("汽车视觉不代表车辆实时数据。天气为 Open-Meteo 模型数据，显示解析后的城市、来源与时间；失败不伪造数据。"); color: "#7b8799"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            C.Label { text: qsTr("信息表盘的电量来自手机本机，电脑预览不假设某一部手机的电量。预报日期按天气城市时区显示。"); color: "#7b8799"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        C.CheckBox { objectName: "panelIdleEnabled"; text: qsTr("空闲自动进入表盘"); checked: config.idle.enabled; onClicked: editor.setIdle("enabled", checked) }
        C.Label { text: qsTr("时长（毫秒）") }
        C.SpinBox { objectName: "panelIdleTimeout"; from: 1000; to: 86400000; stepSize: 1000; editable: true; value: config.idle.timeoutMs; onValueModified: editor.setIdle("timeoutMs", value) }
        C.Button { objectName: "panelDialSettingsButton"; text: qsTr("城市与文本"); onClicked: settings.openForCurrent() }
    }
    C.Dialog {
        id: confirmation
        objectName: "panelDialConfirmation"
        property string dialId
        parent: C.Overlay.overlay; anchors.centerIn: parent; modal: true
        title: qsTr("使用这个表盘？")
        onAccepted: editor.setIdle("dialId", dialId)
        ColumnLayout {
            C.Label { text: qsTr("手机空闲后将显示所选表盘。"); color: "#69778b" }
            RowLayout {
                C.Button { objectName: "panelDialSelectCancel"; text: qsTr("取消"); onClicked: confirmation.reject() }
                C.Button { objectName: "panelDialSelectSave"; text: qsTr("使用表盘"); highlighted: true; palette.dark: "#286fe0"; palette.mid: "#205ab8"; palette.brightText: "white"; onClicked: confirmation.accept() }
            }
        }
    }
    C.Dialog {
        id: settings
        objectName: "panelDialSettings"
        property double expectedRevision: -1
        property string validationError: ""
        parent: C.Overlay.overlay; anchors.centerIn: parent; width: 440; modal: true
        title: qsTr("表盘设置")
        function openForCurrent() {
            cityField.text = config.idle.city;
            messageField.text = config.idle.message;
            settingsEnabled.checked = config.idle.enabled;
            settingsTimeout.value = config.idle.timeoutMs;
            expectedRevision = config.revision;
            validationError = "";
            open();
        }
        ColumnLayout {
            width: parent.width
            C.CheckBox { id: settingsEnabled; objectName: "panelDialSettingsEnabled"; text: qsTr("空闲自动进入表盘") }
            RowLayout {
                C.Label { text: qsTr("空闲时长（毫秒）") }
                C.SpinBox { id: settingsTimeout; objectName: "panelDialSettingsTimeout"; from: 1000; to: 86400000; stepSize: 1000; editable: true }
            }
            C.Label { text: qsTr("城市名将发送给 Open-Meteo 查询天气，不读取 GPS。重名城市可写“Paris, France”等限定名称。免费接口仅限非商业用途。"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#69778b" }
            C.TextField { id: cityField; objectName: "panelDialCity"; placeholderText: qsTr("天气城市"); maximumLength: 120; Layout.fillWidth: true }
            C.TextField { id: messageField; objectName: "panelDialMessage"; placeholderText: qsTr("个性化文本"); maximumLength: 240; Layout.fillWidth: true }
            C.Label { visible: !!settings.validationError; text: settings.validationError; color: "#b3261e"; wrapMode: Text.WordWrap; Layout.fillWidth: true; textFormat: Text.PlainText }
            RowLayout {
                C.Button { objectName: "panelDialSettingsCancel"; text: qsTr("取消"); onClicked: settings.close() }
                C.Button { objectName: "panelDialSettingsSave"; text: qsTr("保存"); highlighted: true; palette.dark: "#286fe0"; palette.mid: "#205ab8"; palette.brightText: "white"; onClicked: {
                    if (config.revision !== settings.expectedRevision) {
                        settings.validationError = qsTr("配置已被修改，请关闭窗口后重新打开，避免覆盖新的设置。");
                        return;
                    }
                    if (editor.setIdleOptions({enabled: settingsEnabled.checked, timeoutMs: settingsTimeout.value, city: cityField.text, message: messageField.text})) settings.close();
                } }
            }
        }
    }
}
