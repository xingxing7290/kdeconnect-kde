/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    AppTheme { id: ui }
    objectName: "panelDialGallery"
    property var editor
    property var now: new Date()
    readonly property var config: editor.config
    readonly property var dials: [{id: "digital", name: qsTr("数字时钟")}, {id: "analog", name: qsTr("经典指针")}, {id: "weather", name: qsTr("时钟与天气")}, {id: "info", name: qsTr("信息表盘")}, {id: "car", name: qsTr("汽车视觉")}]
    spacing: ui.spacing
    function saveError(code) {
        if (code === "edit_conflict" || code === "stale_revision") return qsTr("配置已被修改，请关闭窗口后重新打开，避免覆盖新的设置。");
        if (code === "store_busy") return qsTr("配置正由其他窗口保存，请稍后重试。");
        if (code === "directory_unavailable" || code === "write_failed" || code === "backup_failed") return qsTr("配置未保存，请检查磁盘空间和配置目录权限后重试。");
        if (code === "invalid_idle_timeout") return qsTr("空闲时长应为 1000 至 86400000 毫秒。");
        if (code === "invalid_dial_text") return qsTr("城市最多 120 个字符，个性化文本最多 240 个字符。");
        return qsTr("无法保存配置：%1").arg(code);
    }
    Timer { interval: 1000; running: root.visible; repeat: true; onTriggered: root.now = new Date() }
    RowLayout {
        Layout.fillWidth: true; spacing: 14
        Rectangle { Layout.preferredWidth: 4; Layout.preferredHeight: 34; radius: 2; color: ui.accent }
        ColumnLayout {
            spacing: 3; Layout.fillWidth: true
            C.Label { text: qsTr("表盘选择"); font.pixelSize: 24; font.bold: true; color: ui.text }
            C.Label { text: qsTr("空闲后自动显示；首次触摸只唤醒，不执行按钮。"); color: ui.textMuted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        C.ToolButton {
            objectName: "panelDialSettingsGear"
            Accessible.name: qsTr("表盘设置")
            contentItem: Kirigami.Icon { source: "preferences-system"; implicitWidth: 24; implicitHeight: 24 }
            onClicked: settings.openForCurrent()
        }
    }
    C.Label { visible: !!editor.error; text: root.saveError(editor.error); textFormat: Text.PlainText; color: ui.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true }
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
                    C.Label { text: qsTr("精选推荐"); font.bold: true; font.pixelSize: 13; color: ui.textMuted }
                    PanelDialPreview {
                        objectName: "panelDialFeatured"
                        Layout.fillWidth: true; Layout.preferredHeight: Math.max(160, width / 2.2)
                        dialId: "analog"; city: config.idle.city; message: config.idle.message; now: root.now; weather: editor.weather
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("选择经典指针表盘")
                        Accessible.onPressAction: confirmation.openForDial("analog")
                        Keys.onReturnPressed: confirmation.openForDial("analog")
                        Keys.onSpacePressed: confirmation.openForDial("analog")
                        border.color: activeFocus ? ui.accent : ui.border; border.width: 2
                        TapHandler { onTapped: confirmation.openForDial("analog") }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    C.Label { text: qsTr("当前表盘"); font.bold: true; font.pixelSize: 13; color: ui.textMuted }
                    PanelDialPreview {
                        objectName: "panelDialCurrent"
                        Layout.fillWidth: true; Layout.preferredHeight: Math.max(160, width / 2.2)
                        dialId: config.idle.dialId; city: config.idle.city; message: config.idle.message; now: root.now; weather: editor.weather
                        border.color: ui.accent; border.width: 2
                    }
                }
            }
            C.Label { text: qsTr("更多表盘"); font.bold: true; font.pixelSize: 13; color: ui.textMuted }
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
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("选择%1表盘").arg(card.modelData.name)
                            Accessible.onPressAction: confirmation.openForDial(card.modelData.id)
                            Keys.onReturnPressed: confirmation.openForDial(card.modelData.id)
                            Keys.onSpacePressed: confirmation.openForDial(card.modelData.id)
                            border.color: activeFocus || config.idle.dialId === dialId ? ui.accent : ui.border; border.width: 2
                            TapHandler { onTapped: confirmation.openForDial(card.modelData.id) }
                        }
                        C.Label { text: card.modelData.name; color: ui.textMuted; font.pixelSize: 12 }
                    }
                }
            }
            C.Label { text: qsTr("汽车视觉不代表车辆实时数据。天气为 Open-Meteo 模型数据，显示解析后的城市、来源与时间；失败不伪造数据。"); color: ui.textSubtle; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            C.Label { text: qsTr("信息表盘的电量来自手机本机，电脑预览不假设某一部手机的电量。预报日期按天气城市时区显示。"); color: ui.textSubtle; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        C.CheckBox { objectName: "panelIdleEnabled"; text: qsTr("空闲自动进入表盘"); checked: config.idle.enabled; onClicked: editor.setIdle("enabled", checked) }
        C.Label { text: qsTr("时长（毫秒）") }
        C.SpinBox { objectName: "panelIdleTimeout"; from: 1000; to: 86400000; stepSize: 1000; editable: true; value: config.idle.timeoutMs; onValueModified: editor.setIdle("timeoutMs", value) }
        AppButton { objectName: "panelDialSettingsButton"; text: qsTr("城市与文本"); onClicked: settings.openForCurrent() }
    }
    AppDialog {
        id: confirmation
        objectName: "panelDialConfirmation"
        property string dialId
        property double expectedRevision: -1
        property string validationError: ""
        parent: C.Overlay.overlay; anchors.centerIn: parent; modal: true
        width: Math.min(460, parent ? parent.width - 32 : 460)
        title: qsTr("使用这个表盘？")
        function openForDial(id) {
            dialId = id;
            expectedRevision = config.revision;
            validationError = "";
            open();
        }
        ColumnLayout {
            width: parent.width
            C.Label { text: qsTr("手机空闲后将显示所选表盘。"); color: ui.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            C.Label { objectName: "panelDialSelectError"; visible: !!confirmation.validationError; text: confirmation.validationError; textFormat: Text.PlainText; color: ui.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                AppButton { objectName: "panelDialSelectCancel"; text: qsTr("取消"); onClicked: confirmation.reject() }
                AppButton { objectName: "panelDialSelectSave"; text: qsTr("使用表盘"); highlighted: true; palette.dark: ui.accent; palette.mid: ui.accentHover; palette.brightText: ui.textOnAccent; onClicked: {
                    if (editor.setIdleOptionsChecked({dialId: confirmation.dialId}, confirmation.expectedRevision)) confirmation.close();
                    else confirmation.validationError = root.saveError(editor.error);
                } }
            }
        }
    }
    AppDialog {
        id: settings
        objectName: "panelDialSettings"
        property double expectedRevision: -1
        property string validationError: ""
        parent: C.Overlay.overlay; anchors.centerIn: parent; width: Math.min(440, parent ? parent.width - 32 : 440); modal: true
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
                C.SpinBox { id: settingsTimeout; objectName: "panelDialSettingsTimeout"; from: 1000; to: 86400000; stepSize: 1000; editable: true; Accessible.name: qsTr("空闲时长（毫秒）") }
            }
            C.Label { text: qsTr("城市名将发送给 Open-Meteo 查询天气，不读取 GPS。重名城市可写“Paris, France”等限定名称。免费接口仅限非商业用途。"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: ui.textMuted }
            C.TextField { id: cityField; objectName: "panelDialCity"; placeholderText: qsTr("天气城市"); maximumLength: 120; Layout.fillWidth: true; Accessible.name: placeholderText }
            C.TextField { id: messageField; objectName: "panelDialMessage"; placeholderText: qsTr("个性化文本"); maximumLength: 240; Layout.fillWidth: true; Accessible.name: placeholderText }
            C.Label { visible: !!settings.validationError; text: settings.validationError; color: ui.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true; textFormat: Text.PlainText }
            RowLayout {
                AppButton { objectName: "panelDialSettingsCancel"; text: qsTr("取消"); onClicked: settings.close() }
                AppButton { objectName: "panelDialSettingsSave"; text: qsTr("保存"); highlighted: true; palette.dark: ui.accent; palette.mid: ui.accentHover; palette.brightText: ui.textOnAccent; onClicked: {
                    if (editor.setIdleOptionsChecked({enabled: settingsEnabled.checked, timeoutMs: settingsTimeout.value, city: cityField.text, message: messageField.text}, settings.expectedRevision)) settings.close();
                    else settings.validationError = root.saveError(editor.error);
                } }
            }
        }
    }
}
