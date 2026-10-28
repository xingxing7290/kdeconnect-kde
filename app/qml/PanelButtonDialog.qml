/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts
import QtQuick.Dialogs

C.Dialog {
    id: dialog
    property var editor
    property var systemPresets
    property int slotIndex: 0
    property var original: ({})
    readonly property var kinds: ["program", "website", "file", "folder", "shortcut", "system", "command"]
    readonly property string kind: kinds[typeBox.currentIndex]
    parent: C.Overlay.overlay
    anchors.centerIn: parent
    width: 530
    modal: true
    title: qsTr("编辑按钮")

    function openAt(index, button) {
        slotIndex = index;
        original = button || {};
        typeBox.currentIndex = Math.max(0, kinds.indexOf(original.kind || "program"));
        nameField.text = original.name || "";
        targetField.text = original.target?.url || original.target?.path || original.target?.key || "";
        const keys = original.target?.keys || [];
        ctrl.checked = keys.indexOf("CTRL") >= 0;
        alt.checked = keys.indexOf("ALT") >= 0;
        shift.checked = keys.indexOf("SHIFT") >= 0;
        win.checked = keys.indexOf("WIN") >= 0;
        keyBox.currentIndex = Math.max(0, keyBox.model.indexOf(keys[keys.length - 1] || "A"));
        systemBox.currentIndex = Math.max(0, systemPresets.findIndex(item => item.target.action === original.target?.action));
        open();
    }
    function saveButton() {
        let target = {};
        if (kind === "website") {
            const value = targetField.text.trim();
            target.url = value.indexOf("://") < 0 ? "https://" + value : value;
        } else if (kind === "shortcut") {
            target.keys = [];
            if (ctrl.checked) target.keys.push("CTRL");
            if (alt.checked) target.keys.push("ALT");
            if (shift.checked) target.keys.push("SHIFT");
            if (win.checked) target.keys.push("WIN");
            target.keys.push(keyBox.currentText);
        } else if (kind === "system") target.action = systemPresets[systemBox.currentIndex].target.action;
        else if (kind === "command") target.key = targetField.text.trim();
        else target.path = targetField.text;
        let name = nameField.text.trim();
        if (!name) {
            if (kind === "website") name = target.url.replace(/^https?:\/\//, "").split("/")[0];
            else if (kind === "shortcut") name = target.keys.join(" + ");
            else if (kind === "system") name = systemPresets[systemBox.currentIndex].name;
            else name = targetField.text.replace(/\\/g, "/").split("/").pop().replace(/\.[^.]+$/, "");
        }
        return editor.setButton(slotIndex, {kind: kind, name: name, icon: original.icon || "", target: target});
    }

    ColumnLayout {
        width: parent.width
        spacing: 12
        C.Label { text: qsTr("按钮类型") }
        C.ComboBox { id: typeBox; model: [qsTr("程序"), qsTr("网站"), qsTr("文件"), qsTr("文件夹"), qsTr("快捷键"), qsTr("系统功能"), qsTr("KDE 远程命令")]; Layout.fillWidth: true }
        C.Label { text: qsTr("名称（留空时自动填写）") }
        C.TextField { id: nameField; maximumLength: 120; Layout.fillWidth: true }
        RowLayout {
            visible: dialog.kind !== "shortcut" && dialog.kind !== "system"
            C.TextField { id: targetField; Layout.fillWidth: true; placeholderText: dialog.kind === "website" ? "https://example.com/path?query=1" : qsTr("目标路径 / 命令标识") }
            C.Button { visible: ["program", "file", "folder"].indexOf(dialog.kind) >= 0; text: qsTr("浏览…"); onClicked: dialog.kind === "folder" ? folderPicker.open() : filePicker.open() }
        }
        RowLayout {
            visible: dialog.kind === "shortcut"
            C.CheckBox { id: ctrl; text: "Ctrl" }
            C.CheckBox { id: alt; text: "Alt" }
            C.CheckBox { id: shift; text: "Shift" }
            C.CheckBox { id: win; text: "Win" }
            C.ComboBox { id: keyBox; model: dialog.editor.keyNames.filter(key => ["CTRL", "ALT", "SHIFT", "WIN"].indexOf(key) < 0); Layout.fillWidth: true }
        }
        C.ComboBox { id: systemBox; visible: dialog.kind === "system"; model: dialog.systemPresets.map(item => item.name); Layout.fillWidth: true }
        C.Label { visible: dialog.kind === "shortcut"; text: qsTr("快捷键发送到电脑当前前台程序。"); color: "#69778b" }
        RowLayout {
            Layout.fillWidth: true
            C.Button { text: qsTr("删除按钮"); enabled: !!dialog.original.id; onClicked: if (editor.clearButton(slotIndex)) dialog.close() }
            Item { Layout.fillWidth: true }
            C.Button { text: qsTr("取消"); onClicked: dialog.close() }
            C.Button { text: qsTr("保存"); highlighted: true; onClicked: if (dialog.saveButton()) dialog.close() }
        }
    }
    FileDialog {
        id: filePicker
        nameFilters: dialog.kind === "program" ? [qsTr("应用程序 (*.exe *.lnk)")] : [qsTr("所有文件 (*)")]
        onAccepted: { const value = editor.inspectFile(selectedFile); targetField.text = value.target.path; if (!nameField.text) nameField.text = value.name; }
    }
    FolderDialog {
        id: folderPicker
        onAccepted: { const value = editor.inspectFile(selectedFolder); targetField.text = value.target.path; if (!nameField.text) nameField.text = value.name; }
    }
}
