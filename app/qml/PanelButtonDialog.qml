/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts
import QtQuick.Dialogs

C.Dialog {
    id: dialog
    objectName: "panelButtonDialog"
    property var editor
    property var systemPresets
    property int slotIndex: 0
    property var original: ({})
    property string iconId: ""
    property var editSnapshot: ({})
    property string validationError: ""
    readonly property var kinds: ["program", "website", "file", "folder", "shortcut", "system", "command"]
    readonly property string kind: kinds[typeBox.currentIndex]
    parent: C.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(530, parent ? parent.width - 32 : 530)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : implicitHeight)
    modal: true
    title: qsTr("编辑按钮")

    function openAt(index, button) {
        slotIndex = index;
        original = button || {};
        editSnapshot = editor.editContext(index);
        validationError = "";
        iconId = original.icon || "";
        typeBox.currentIndex = Math.max(0, kinds.indexOf(original.kind || "program"));
        nameField.text = original.name || "";
        targetField.text = original.target?.url || original.target?.path || original.target?.key || "";
        argumentsField.text = JSON.stringify(original.target?.arguments || []);
        workingDirectoryField.text = original.target?.workingDirectory || "";
        advanced.checked = !!original.target?.arguments?.length || !!original.target?.workingDirectory;
        const keys = original.target?.keys || [];
        ctrl.checked = keys.indexOf("CTRL") >= 0;
        alt.checked = keys.indexOf("ALT") >= 0;
        shift.checked = keys.indexOf("SHIFT") >= 0;
        win.checked = keys.indexOf("WIN") >= 0;
        keyBox.currentIndex = Math.max(0, keyBox.model.indexOf(keys[keys.length - 1] || "A"));
        systemBox.currentIndex = Math.max(0, systemPresets.findIndex(item => item.target.action === original.target?.action));
        open();
    }
    function reportError(code) {
        const messages = {
            edit_conflict: qsTr("配置或当前页面已变化。请取消并重新打开按钮，避免覆盖新的内容。"),
            stale_revision: qsTr("配置已被另一窗口修改。请取消并重新打开按钮。"),
            target_missing: qsTr("找不到目标文件或文件夹，请检查位置。"),
            invalid_target: qsTr("目标类型或内容无效，请检查路径、网址或快捷键。"),
            invalid_image: qsTr("请选择有效的 JPG 或 PNG 图片，尺寸不能超过 4096 像素。"),
            image_too_large_or_unreadable: qsTr("图片无法读取或文件过大，请选择较小的 JPG 或 PNG。"),
            icon_unavailable: qsTr("所选图标不可用，请重新选择或恢复默认图标。")
        };
        validationError = messages[code] || qsTr("无法保存：%1").arg(code || qsTr("请检查输入"));
    }
    function targetPath() {
        let value = targetField.text.trim();
        if (value.length >= 2 && value.startsWith('"') && value.endsWith('"')) value = value.slice(1, -1);
        return value;
    }
    function saveButton() {
        validationError = "";
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
        else {
            target.path = targetPath();
            if (kind === "program") {
                let args;
                try { args = JSON.parse(argumentsField.text || "[]"); }
                catch (_) { validationError = qsTr("启动参数应为 JSON 字符串数组，例如 [\"--profile\",\"work\"]。"); return false; }
                if (!Array.isArray(args) || args.length > 64 || args.some(arg => typeof arg !== "string" || arg.length > 4096 || arg.indexOf("\u0000") >= 0)) {
                    validationError = qsTr("最多 64 个启动参数，每项必须是长度不超过 4096 的字符串。"); return false;
                }
                if (args.length) target.arguments = args;
                if (workingDirectoryField.text.trim()) target.workingDirectory = workingDirectoryField.text.trim();
            }
        }
        let name = nameField.text.trim();
        if (!name) {
            if (kind === "website") name = target.url.replace(/^https?:\/\//i, "").split(/[/?#]/)[0].split("@").pop();
            else if (kind === "shortcut") name = target.keys.join(" + ");
            else if (kind === "system") name = systemPresets[systemBox.currentIndex].name;
            else if (kind === "command") name = target.key;
            else {
                const filename = target.path.replace(/\\/g, "/").replace(/\/+$/, "").split("/").pop();
                name = kind === "folder" ? filename : filename.replace(/\.[^.]+$/, "") || filename;
                if (!name) name = qsTr("根目录");
            }
        }
        const saved = editor.setButtonChecked(slotIndex, {kind: kind, name: name, icon: iconId, target: target}, editSnapshot);
        if (!saved && !validationError) reportError(editor.error);
        return saved;
    }

    contentItem: C.ScrollView {
        id: scroll
        objectName: "panelButtonScroll"
        implicitHeight: form.implicitHeight
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
        id: form
        width: scroll.availableWidth
        spacing: 12
        C.Label { text: qsTr("按钮类型") }
        C.ComboBox { id: typeBox; objectName: "panelButtonType"; model: [qsTr("程序"), qsTr("网站"), qsTr("文件"), qsTr("文件夹"), qsTr("快捷键"), qsTr("系统功能"), qsTr("KDE 远程命令")]; Layout.fillWidth: true }
        C.Label { text: qsTr("名称（留空时自动填写）") }
        C.TextField { id: nameField; objectName: "panelButtonName"; maximumLength: 120; Layout.fillWidth: true }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Image { source: dialog.iconId ? editor.assetUrl(dialog.iconId) : ""; width: 48; height: 48; fillMode: Image.PreserveAspectFit }
            C.Button { text: qsTr("选择 JPG / PNG 图标"); onClicked: iconPicker.open() }
            C.Button { visible: dialog.kind === "program"; text: qsTr("读取程序图标"); onClicked: { const id = editor.programIcon(dialog.targetPath()); if (id) dialog.iconId = id; } }
            C.Button { text: qsTr("恢复默认图标"); onClicked: dialog.iconId = "" }
        }
        RowLayout {
            visible: dialog.kind !== "shortcut" && dialog.kind !== "system"
            C.TextField { id: targetField; objectName: "panelButtonTarget"; maximumLength: 32767; Layout.fillWidth: true; placeholderText: dialog.kind === "website" ? "https://example.com/path?query=1" : qsTr("目标路径 / 命令标识") }
            C.Button { visible: ["program", "file", "folder"].indexOf(dialog.kind) >= 0; text: qsTr("浏览..."); onClicked: dialog.kind === "folder" ? folderPicker.open() : filePicker.open() }
        }
        C.CheckBox { id: advanced; objectName: "panelProgramAdvanced"; text: qsTr("高级启动选项"); visible: dialog.kind === "program" }
        C.Label { text: qsTr("启动参数（JSON 字符串数组，空数组为 []）"); visible: dialog.kind === "program" && advanced.checked; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        C.TextField { id: argumentsField; objectName: "panelProgramArguments"; visible: dialog.kind === "program" && advanced.checked; Layout.fillWidth: true; maximumLength: 262144; placeholderText: '["--profile","work"]' }
        C.Label { text: qsTr("工作目录（可空；快捷方式自身的参数/目录请在 .lnk 中设置）"); visible: dialog.kind === "program" && advanced.checked; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        C.TextField { id: workingDirectoryField; objectName: "panelProgramDirectory"; visible: dialog.kind === "program" && advanced.checked; Layout.fillWidth: true; maximumLength: 32767 }
        RowLayout {
            visible: dialog.kind === "shortcut"
            C.CheckBox { id: ctrl; objectName: "panelShortcutCtrl"; text: "Ctrl" }
            C.CheckBox { id: alt; objectName: "panelShortcutAlt"; text: "Alt" }
            C.CheckBox { id: shift; objectName: "panelShortcutShift"; text: "Shift" }
            C.CheckBox { id: win; objectName: "panelShortcutWin"; text: "Win" }
            C.ComboBox { id: keyBox; objectName: "panelShortcutKey"; model: dialog.editor.keyNames.filter(key => ["CTRL", "ALT", "SHIFT", "WIN"].indexOf(key) < 0); Layout.fillWidth: true }
        }
        C.ComboBox { id: systemBox; objectName: "panelButtonSystem"; visible: dialog.kind === "system"; model: dialog.systemPresets.map(item => item.name); Layout.fillWidth: true }
        C.Label { visible: dialog.kind === "shortcut"; text: qsTr("快捷键发送到电脑当前前台程序。"); color: "#69778b" }
        C.Label { objectName: "panelButtonError"; visible: !!dialog.validationError; text: dialog.validationError; textFormat: Text.PlainText; color: "#b3261e"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
    }
    footer: Item {
        implicitHeight: 52
        RowLayout {
            anchors.fill: parent; anchors.margins: 8
            C.Button { objectName: "panelButtonDelete"; text: qsTr("删除按钮"); enabled: !!dialog.original.id; onClicked: {
                if (editor.clearButtonChecked(slotIndex, editSnapshot)) dialog.close(); else dialog.reportError(editor.error);
            } }
            Item { Layout.fillWidth: true }
            C.Button { objectName: "panelButtonCancel"; text: qsTr("取消"); onClicked: dialog.close() }
            C.Button { objectName: "panelButtonSave"; text: qsTr("保存"); highlighted: true; palette.dark: "#286fe0"; palette.mid: "#205ab8"; palette.brightText: "#ffffff"; onClicked: if (dialog.saveButton()) dialog.close() }
        }
    }
    FileDialog {
        id: iconPicker
        nameFilters: [qsTr("图片 (*.png *.jpg *.jpeg)")]
        onAccepted: { const id = editor.importIcon(selectedFile); if (id) dialog.iconId = id; }
    }
    FileDialog {
        id: filePicker
        nameFilters: dialog.kind === "program" ? [qsTr("应用程序 (*.exe *.lnk)")] : [qsTr("所有文件 (*)")]
        onAccepted: {
            const value = editor.inspectFile(selectedFile);
            if (!value.target?.path) { dialog.validationError = qsTr("所选文件已不可用，请重新选择。"); return; }
            targetField.text = value.target.path;
            if (!nameField.text) nameField.text = value.name;
        }
    }
    FolderDialog {
        id: folderPicker
        onAccepted: {
            const value = editor.inspectFile(selectedFolder);
            if (!value.target?.path) { dialog.validationError = qsTr("所选文件夹已不可用，请重新选择。"); return; }
            targetField.text = value.target.path;
            if (!nameField.text) nameField.text = value.name;
        }
    }
}
