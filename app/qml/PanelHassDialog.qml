/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts

AppDialog {
    id: dialog
    AppTheme { id: ui }
    objectName: "panelHassDialog"
    property var editor
    property string validationError: ""
    property bool showPicker: false
    property var draft: ({ enabled: false, url: "", token: "", entities: [] })
    parent: C.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(600, parent ? parent.width - 32 : 600)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : implicitHeight)
    modal: true
    title: qsTr("智能家居（Home Assistant）")

    function openEditor() {
        const source = editor.hass || ({});
        const list = source.entities || [];
        draft = {
            enabled: !!source.enabled,
            url: source.url || "",
            token: source.token || "",
            entities: list.map(item => ({
                id: item.id,
                name: item.name || "",
                type: item.type || "switch",
                group: item.group || "",
                order: item.order === undefined ? 0 : item.order
            }))
        };
        validationError = "";
        showPicker = false;
        open();
        // Editing a field breaks its binding, so re-seed the inputs from the fresh draft.
        Qt.callLater(function() {
            urlField.text = draft.url;
            tokenField.text = draft.token;
        });
    }
    function has(entityId) {
        return draft.entities.some(item => item.id === entityId);
    }
    function addEntity(entity) {
        if (has(entity.id)) return;
        const next = draft.entities.slice();
        next.push({ id: entity.id, name: entity.name || "", type: entity.type || "switch", group: "", order: next.length });
        draft = { enabled: draft.enabled, url: draft.url, token: draft.token, entities: next };
    }
    function removeEntity(entityId) {
        const next = draft.entities.filter(item => item.id !== entityId);
        draft = { enabled: draft.enabled, url: draft.url, token: draft.token, entities: next };
    }
    // Renames mutate the entry in place: replacing the array would rebuild the Repeater and the
    // editor would lose focus after every keystroke. save() reads the same array, so it still sees
    // the new text.
    function renameEntity(entityId, value) {
        const item = draft.entities.find(entry => entry.id === entityId);
        if (item) item.name = value;
    }
    function renameGroup(entityId, value) {
        const item = draft.entities.find(entry => entry.id === entityId);
        if (item) item.group = value;
    }
    function move(entityId, delta) {
        const list = draft.entities.slice();
        const index = list.findIndex(item => item.id === entityId);
        const target = index + delta;
        if (index < 0 || target < 0 || target >= list.length) return;
        const swapped = list[target];
        list[target] = list[index];
        list[index] = swapped;
        list.forEach((item, position) => { item.order = position; });
        draft = { enabled: draft.enabled, url: draft.url, token: draft.token, entities: list };
    }
    function statusText() {
        const state = editor.hassState || ({});
        const status = state.status || "disabled";
        if (status === "ready") return qsTr("已连接，实体状态实时更新");
        if (status === "connecting") return qsTr("正在连接…");
        if (status === "authenticating") return qsTr("正在验证令牌…");
        if (status === "error") return qsTr("连接失败：%1").arg(state.error || qsTr("未知错误"));
        return qsTr("未启用");
    }
    function save() {
        validationError = "";
        const url = draft.url.trim();
        if (draft.enabled && !url) { validationError = qsTr("启用前请填写 Home Assistant 地址。"); return; }
        if (draft.enabled && !draft.token.trim()) { validationError = qsTr("启用前请填写长期访问令牌。"); return; }
        if (url && !/^https?:\/\/[^\s/]+/i.test(url)) { validationError = qsTr("地址需以 http:// 或 https:// 开头。"); return; }
        const revision = editor.config && editor.config.revision !== undefined ? editor.config.revision : -1;
        if (editor.setHassChecked({ enabled: draft.enabled, url: url, token: draft.token.trim(), entities: draft.entities }, revision)) {
            close();
        } else {
            validationError = editor.error || qsTr("保存失败，请重试。");
        }
    }

    contentItem: C.ScrollView {
        implicitHeight: form.implicitHeight
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            id: form
            width: parent.width
            spacing: 12

            C.Label { text: qsTr("由这台电脑连接 Home Assistant，再把状态转发给平板。地址和令牌只保存在本机，不会同步到手机。"); wrapMode: Text.WordWrap; color: ui.textMuted; Layout.fillWidth: true }

            C.CheckBox {
                objectName: "panelHassEnabled"
                text: qsTr("启用智能家居")
                checked: dialog.draft.enabled
                onClicked: dialog.draft = { enabled: checked, url: dialog.draft.url, token: dialog.draft.token, entities: dialog.draft.entities }
            }
            C.Label { text: qsTr("地址（例如 http://192.168.1.5:8123）"); Layout.fillWidth: true }
            C.TextField {
                id: urlField
                objectName: "panelHassUrl"
                text: dialog.draft.url
                placeholderText: "http://192.168.1.5:8123"
                maximumLength: 200
                Layout.fillWidth: true
                onTextEdited: dialog.draft = { enabled: dialog.draft.enabled, url: text, token: dialog.draft.token, entities: dialog.draft.entities }
            }
            C.Label { text: qsTr("长期访问令牌（在 HA 的「用户资料 → 长期访问令牌」中创建）"); Layout.fillWidth: true; wrapMode: Text.WordWrap }
            C.TextField {
                id: tokenField
                objectName: "panelHassToken"
                text: dialog.draft.token
                echoMode: TextInput.Password
                placeholderText: qsTr("粘贴令牌")
                maximumLength: 512
                Layout.fillWidth: true
                onTextEdited: dialog.draft = { enabled: dialog.draft.enabled, url: dialog.draft.url, token: text, entities: dialog.draft.entities }
            }

            RowLayout {
                Layout.fillWidth: true
                C.Label { text: dialog.statusText(); color: (editor.hassState && editor.hassState.status === "error") ? ui.danger : ui.textMuted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                AppButton { objectName: "panelHassRefresh"; text: qsTr("重新连接"); onClicked: editor.refreshHass() }
            }

            RowLayout {
                Layout.fillWidth: true
                C.Label { text: qsTr("要显示的设备与读数"); font.bold: true; Layout.fillWidth: true }
                AppButton { objectName: "panelHassAdd"; text: qsTr("添加实体"); onClicked: dialog.showPicker = !dialog.showPicker }
            }

            C.Label {
                visible: dialog.showPicker && (editor.hassEntities || []).length === 0
                text: qsTr("还没有读取到实体。请先填写地址和令牌、启用并保存，再点「重新连接」。")
                color: ui.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true
            }

            ColumnLayout {
                visible: dialog.showPicker
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: dialog.showPicker ? (editor.hassEntities || []) : []
                    delegate: C.CheckBox {
                        text: (modelData.name && modelData.name.length ? modelData.name : modelData.id) + "  ·  " + modelData.id
                        checked: dialog.has(modelData.id)
                        Layout.fillWidth: true
                        onClicked: checked ? dialog.addEntity(modelData) : dialog.removeEntity(modelData.id)
                    }
                }
            }

            C.Label {
                visible: dialog.draft.entities.length === 0
                text: qsTr("还没有添加任何设备。点「添加实体」从已读取的列表中选择。")
                color: ui.textSubtle; wrapMode: Text.WordWrap; Layout.fillWidth: true
            }

            Repeater {
                model: dialog.draft.entities
                delegate: C.Frame {
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            C.Label { text: modelData.id; color: ui.textMuted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                            C.Label { text: modelData.type === "light" ? qsTr("灯") : (modelData.type === "sensor" ? qsTr("读数") : qsTr("开关")); color: ui.textMuted; font.pixelSize: 11 }
                            AppButton { text: "↑"; enabled: index > 0; onClicked: dialog.move(modelData.id, -1) }
                            AppButton { text: "↓"; enabled: index < dialog.draft.entities.length - 1; onClicked: dialog.move(modelData.id, 1) }
                            AppButton { text: qsTr("移除"); danger: true; onClicked: dialog.removeEntity(modelData.id) }
                        }
                        C.TextField {
                            text: modelData.name
                            placeholderText: qsTr("显示名称（留空则用 HA 中的名称）")
                            maximumLength: 120
                            Layout.fillWidth: true
                            onTextEdited: dialog.renameEntity(modelData.id, text)
                        }
                        C.TextField {
                            text: modelData.group || ""
                            placeholderText: qsTr("分组（可选，例如「客厅」，表盘和手机端会按分组显示）")
                            maximumLength: 60
                            Layout.fillWidth: true
                            onTextEdited: dialog.renameGroup(modelData.id, text)
                        }
                    }
                }
            }

            C.Label { objectName: "panelHassError"; visible: !!dialog.validationError; text: dialog.validationError; textFormat: Text.PlainText; color: ui.danger; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
    }

    footer: Item {
        implicitHeight: 52
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            Item { Layout.fillWidth: true }
            AppButton { objectName: "panelHassCancel"; text: qsTr("取消"); onClicked: dialog.close() }
            AppButton { objectName: "panelHassSave"; text: qsTr("保存"); primary: true; onClicked: dialog.save() }
        }
    }
}
