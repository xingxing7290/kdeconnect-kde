/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Controls as C
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

Item {
    id: root
    signal openConnections()
    property int section: 1
    readonly property var editor: panelEditor
    readonly property var config: editor.config
    readonly property var page: editor.currentPage
    readonly property var presets: [
        {group: qsTr("系统功能"), name: qsTr("此电脑"), kind: "system", target: {action: "thisPc"}},
        {group: qsTr("系统功能"), name: qsTr("回收站"), kind: "system", target: {action: "recycleBin"}},
        {group: qsTr("系统功能"), name: qsTr("命令提示符"), kind: "system", target: {action: "commandPrompt"}},
        {group: qsTr("系统功能"), name: qsTr("任务管理器"), kind: "system", target: {action: "taskManager"}},
        {group: qsTr("系统功能"), name: qsTr("显示桌面"), kind: "system", target: {action: "showDesktop"}},
        {group: qsTr("系统功能"), name: qsTr("屏幕截图"), kind: "system", target: {action: "screenshot"}},
        {group: qsTr("系统设置"), name: qsTr("系统设置"), kind: "system", target: {action: "settings"}},
        {group: qsTr("系统设置"), name: qsTr("网络和 Internet"), kind: "system", target: {action: "networkSettings"}},
        {group: qsTr("系统设置"), name: qsTr("个性化"), kind: "system", target: {action: "personalization"}},
        {group: qsTr("系统设置"), name: qsTr("显示设置"), kind: "system", target: {action: "displaySettings"}},
        {group: qsTr("系统设置"), name: qsTr("声音设置"), kind: "system", target: {action: "soundSettings"}},
        {group: qsTr("常用网站"), name: "KDE", kind: "website", target: {url: "https://kde.org"}},
        {group: qsTr("常用网站"), name: "Wikipedia", kind: "website", target: {url: "https://www.wikipedia.org"}},
        {group: qsTr("常用网站"), name: "Bing", kind: "website", target: {url: "https://www.bing.com"}}
    ]
    readonly property var systemPresets: presets.filter(item => item.kind === "system")
    function kindIcon(kind) {
        return ({program: "application-x-executable", website: "internet-web-browser", file: "text-x-generic", folder: "folder", shortcut: "input-keyboard", system: "preferences-system", command: "utilities-terminal"})[kind] || "application-x-executable";
    }
    function addPreset(preset) {
        const empty = page.slots.findIndex(item => !item);
        if (empty >= 0) editor.setButton(empty, preset);
    }

    Rectangle { width: 190; height: parent.height; color: "#eef1f6" }
    ColumnLayout {
        x: 18; y: 28; width: 154; height: parent.height - 48; spacing: 12
        Kirigami.Icon { source: "qrc:/icons/kdeconnect/kdeconnect.svg"; Layout.preferredWidth: 42; Layout.preferredHeight: 42; Layout.leftMargin: 10 }
        C.Label { text: "KDE Connect"; font.pixelSize: 19; font.bold: true; color: "#273448"; Layout.leftMargin: 10; Layout.bottomMargin: 30 }
        Repeater {
            model: [qsTr("表盘选择"), qsTr("桌面控制"), qsTr("开启连接"), qsTr("关于")]
            delegate: C.ItemDelegate {
                id: navigation
                required property int index
                required property string modelData
                text: modelData; Layout.fillWidth: true; highlighted: root.section === index
                implicitHeight: 44
                contentItem: C.Label { text: navigation.text; color: root.section === index ? "#286fe0" : "#354158"; verticalAlignment: Text.AlignVCenter; font.pixelSize: 15 }
                onClicked: root.section = index
                background: Rectangle { radius: 9; color: root.section === index ? "#dce7fb" : parent.hovered ? "#e5eaf2" : "transparent" }
            }
        }
        Item { Layout.fillHeight: true }
        C.Label { text: qsTr("配置自动同步到\n已配对的 Android 设备"); wrapMode: Text.WordWrap; color: "#7b8799"; font.pixelSize: 11; Layout.fillWidth: true }
    }

    StackLayout {
        anchors { left: parent.left; leftMargin: 214; right: parent.right; rightMargin: 24; top: parent.top; topMargin: 26; bottom: parent.bottom; bottomMargin: 20 }
        currentIndex: root.section
        PanelDialGallery { editor: root.editor }
        ColumnLayout {
            spacing: 14
            RowLayout {
                Layout.fillWidth: true
                C.Label { text: qsTr("桌面控制"); font.pixelSize: 26; font.bold: true; color: "#273448"; Layout.fillWidth: true }
                C.Button { text: qsTr("撤回上次修改"); onClicked: editor.restoreBackup() }
            }
            C.Label { text: qsTr("点击槽位编辑，拖动按钮换位，或从资源管理器拖入程序、文件和文件夹。"); color: "#7b8799"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            C.Label { visible: !!editor.error; text: qsTr("配置暂不可用：") + editor.error; color: "#b3261e"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 22
                Item {
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 420
                    Rectangle {
                        id: phone
                        width: Math.min(parent.width, parent.height * 1.85); height: width / 1.85
                        anchors.centerIn: parent; color: "#171a20"; radius: 28
                        Rectangle {
                            anchors.fill: parent; anchors.margins: 14; radius: 16; color: config.appearance.background
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 12; spacing: 12
                                GridLayout {
                                    columns: 6; rows: 3; columnSpacing: 8; rowSpacing: 8; Layout.fillWidth: true; Layout.fillHeight: true
                                    Repeater {
                                        model: 18
                                        delegate: Rectangle {
                                            id: cell
                                            required property int index
                                            property var button: page.slots?.[index] || null
                                            Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 70; Layout.preferredHeight: 75
                                            radius: 10; color: drop.containsDrag ? "#dceaff" : button ? "#eef3fa" : "#dfe3e9"
                                            border.width: drop.containsDrag ? 2 : 0; border.color: "#4988f1"
                                            Column {
                                                anchors.centerIn: parent; width: parent.width - 8; spacing: 4
                                                Kirigami.Icon { visible: cell.button !== null; source: root.kindIcon(cell.button?.kind); width: Math.min(32, cell.width * 0.5); height: width; anchors.horizontalCenter: parent.horizontalCenter }
                                                C.Label { visible: !cell.button; text: "+"; font.pixelSize: 27; color: "#99a3b1"; anchors.horizontalCenter: parent.horizontalCenter }
                                                C.Label { visible: config.appearance.showLabels && cell.button !== null; text: cell.button?.name || ""; width: parent.width; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 11; color: "#344158" }
                                            }
                                            Drag.active: drag.active
                                            Drag.dragType: Drag.Automatic
                                            Drag.supportedActions: Qt.MoveAction
                                            Drag.mimeData: {"application/x-kdeconnect-slot": String(index)}
                                            DragHandler { id: drag; target: null; enabled: cell.button !== null }
                                            TapHandler { onTapped: buttonDialog.openAt(index, cell.button) }
                                            DropArea {
                                                id: drop; anchors.fill: parent
                                                onDropped: event => {
                                                    if (event.hasUrls) editor.dropFile(index, event.urls[0]);
                                                    else if (event.formats.indexOf("application/x-kdeconnect-slot") >= 0) editor.moveButton(Number(event.getDataAsString("application/x-kdeconnect-slot")), index);
                                                    else if (event.formats.indexOf("application/x-kdeconnect-preset") >= 0) editor.setButton(index, JSON.parse(event.getDataAsString("application/x-kdeconnect-preset")));
                                                    else return;
                                                    event.acceptProposedAction();
                                                }
                                            }
                                        }
                                    }
                                }
                                ColumnLayout {
                                    visible: config.appearance.showVolume; Layout.preferredWidth: 30; Layout.fillHeight: true
                                    Kirigami.Icon { source: "audio-volume-high"; Layout.preferredWidth: 22; Layout.preferredHeight: 22; Layout.alignment: Qt.AlignHCenter }
                                    C.Slider { orientation: Qt.Vertical; enabled: false; value: 0; Layout.fillHeight: true; Layout.alignment: Qt.AlignHCenter }
                                    Kirigami.Icon { source: "audio-volume-muted"; Layout.preferredWidth: 22; Layout.preferredHeight: 22; Layout.alignment: Qt.AlignHCenter }
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 210; Layout.fillHeight: true; color: "#ffffff"; radius: 14
                    C.ScrollView {
                        id: libraryView
                        anchors.fill: parent; anchors.margins: 12; clip: true
                        contentWidth: availableWidth
                        Column {
                            width: libraryView.availableWidth; spacing: 5
                            Repeater {
                                model: root.presets
                                delegate: Column {
                                    required property var modelData
                                    required property int index
                                    width: parent.width
                                    C.Label { visible: index === 0 || presets[index - 1].group !== modelData.group; text: modelData.group; font.bold: true; color: "#69778b"; topPadding: 12; bottomPadding: 7 }
                                    C.ItemDelegate {
                                        id: libraryButton
                                        text: modelData.name; width: parent.width
                                        contentItem: RowLayout {
                                            spacing: 10
                                            Kirigami.Icon { source: root.kindIcon(modelData.kind); Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                                            C.Label { text: libraryButton.text; Layout.fillWidth: true; elide: Text.ElideRight }
                                        }
                                        onClicked: root.addPreset(modelData)
                                        Drag.active: presetDrag.active
                                        Drag.dragType: Drag.Automatic
                                        Drag.supportedActions: Qt.CopyAction
                                        Drag.mimeData: {"application/x-kdeconnect-preset": JSON.stringify(modelData)}
                                        DragHandler { id: presetDrag; target: null }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Repeater {
                    model: config.pages
                    delegate: C.Button {
                        required property var modelData
                        required property int index
                        text: String(index + 1); highlighted: editor.currentPageId === modelData.id
                        onClicked: editor.selectPage(modelData.id)
                        onHoveredChanged: if (hovered) editor.selectPage(modelData.id)
                        C.ToolTip.visible: hovered
                        C.ToolTip.text: modelData.name
                    }
                }
                C.Button { text: "+"; enabled: config.pages.length < 10; onClicked: editor.addPage() }
                C.Button { text: qsTr("删除页"); enabled: config.pages.length > 1; onClicked: deletePageDialog.open() }
                C.Button { text: qsTr("页面名称 / 应用绑定"); onClicked: { pageName.text = page.name; applicationField.text = page.application || ""; pageDialog.open(); } }
                Item { Layout.fillWidth: true }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#e1e6ef" }
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                C.Label { text: qsTr("页面模式") }
                C.ComboBox { model: [qsTr("默认单页"), qsTr("多页面"), qsTr("智能跟随")]; currentIndex: ["single", "pages", "follow"].indexOf(config.mode); onActivated: editor.setMode(["single", "pages", "follow"][currentIndex]) }
                C.Button { text: qsTr("识别当前程序"); visible: config.mode === "follow"; onClicked: foregroundDialog.open() }
                C.CheckBox { text: qsTr("音量控制"); checked: config.appearance.showVolume; onClicked: editor.setAppearance("showVolume", checked) }
                C.CheckBox { text: qsTr("显示名称"); checked: config.appearance.showLabels; onClicked: editor.setAppearance("showLabels", checked) }
                Item { Layout.fillWidth: true }
            }
            C.Label {
                visible: config.mode === "follow"; Layout.fillWidth: true; elide: Text.ElideMiddle; color: "#69778b"; font.pixelSize: 11
                text: editor.foregroundSupported ? qsTr("当前程序：%1；手动切页保持到下次程序切换。").arg(editor.foregroundApplication || qsTr("请切到要识别的软件")) : qsTr("当前平台暂不支持前台识别，请使用 Windows 版本。")
            }
            RowLayout {
                C.Label { text: qsTr("背景色") }
                C.TextField { text: config.appearance.background; maximumLength: 7; Layout.preferredWidth: 105; onEditingFinished: editor.setAppearance("background", text) }
                C.Button { text: qsTr("选择颜色"); onClicked: { colorPicker.selectedColor = config.appearance.background; colorPicker.open(); } }
                C.Label { text: qsTr("切换方向"); Layout.leftMargin: 16 }
                C.ComboBox { model: [qsTr("左右滑动"), qsTr("上下滑动")]; currentIndex: config.appearance.swipeDirection === "vertical" ? 1 : 0; onActivated: editor.setAppearance("swipeDirection", currentIndex ? "vertical" : "horizontal") }
            }
        }
        ColumnLayout {
            C.Label { text: qsTr("开启连接"); font.pixelSize: 26; font.bold: true; color: "#273448" }
            C.Label { text: qsTr("在 Android 和电脑上打开 KDE Connect，连接同一网络，在设备管理中配对。\n配对完成后，手机打开“控制面板”即可读取电脑配置。"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#69778b"; lineHeight: 1.6 }
            C.Button { text: qsTr("打开设备管理与配对"); highlighted: true; onClicked: root.openConnections() }
            Item { Layout.fillHeight: true }
        }
        ColumnLayout {
            C.Label { text: qsTr("关于"); font.pixelSize: 26; font.bold: true; color: "#273448" }
            C.Label { text: "KDE Connect 控制面板"; font.pixelSize: 22 }
            C.Label { text: qsTr("Android 与 Windows 控制面板开发分支\n基于 KDE Connect 开源项目，使用 GPL 许可证。"); color: "#69778b"; lineHeight: 1.6 }
            C.Button { text: qsTr("查看源代码"); onClicked: Qt.openUrlExternally("https://github.com/xingxing7290/kdeconnect-kde") }
            C.Button { text: qsTr("KDE Connect 原有设备功能"); onClicked: root.openConnections() }
            Item { Layout.fillHeight: true }
        }
    }
    PanelButtonDialog { id: buttonDialog; editor: root.editor; systemPresets: root.systemPresets }
    ColorDialog { id: colorPicker; onAccepted: editor.setAppearance("background", selectedColor.toString()) }
    C.Dialog {
        id: deletePageDialog; parent: C.Overlay.overlay; anchors.centerIn: parent; modal: true; title: qsTr("删除当前页面？")
        standardButtons: C.Dialog.Ok | C.Dialog.Cancel
        C.Label { text: qsTr("此页按钮会一同删除，可用“撤回上次修改”恢复。"); wrapMode: Text.WordWrap }
        onAccepted: editor.deletePage(editor.currentPageId)
    }
    C.Dialog {
        id: pageDialog; parent: C.Overlay.overlay; anchors.centerIn: parent; width: 440; modal: true; title: qsTr("页面设置")
        ColumnLayout {
            width: parent.width
            C.TextField { id: pageName; placeholderText: qsTr("页面名称"); maximumLength: 120; Layout.fillWidth: true }
            C.TextField { id: applicationField; placeholderText: qsTr("绑定的程序名，例如 notepad.exe"); maximumLength: 260; Layout.fillWidth: true }
            C.Button { text: qsTr("使用当前识别的程序"); visible: config.mode === "follow"; enabled: !!editor.foregroundApplication; onClicked: applicationField.text = editor.foregroundApplication }
            C.Button { text: qsTr("保存"); highlighted: true; onClicked: if (editor.renamePage(editor.currentPageId, pageName.text, applicationField.text)) pageDialog.close() }
        }
    }
    C.Dialog {
        id: foregroundDialog; parent: C.Overlay.overlay; anchors.centerIn: parent; width: 480; modal: true; title: qsTr("识别当前操作的程序")
        standardButtons: C.Dialog.Close
        ColumnLayout {
            width: parent.width; spacing: 14
            C.Label { text: qsTr("保持此窗口打开，点击要绑定的软件，等待片刻后返回。识别只使用程序名，不读取窗口标题或文档内容。"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            C.TextField { text: editor.foregroundApplication; readOnly: true; selectByMouse: true; placeholderText: qsTr("等待前台程序"); Layout.fillWidth: true }
            RowLayout {
                C.Button { text: qsTr("复制程序名称"); enabled: !!editor.foregroundApplication; onClicked: editor.copyForegroundApplication() }
                C.Button { text: qsTr("绑定到当前页"); highlighted: true; enabled: !!editor.foregroundApplication; onClicked: if (editor.renamePage(editor.currentPageId, page.name, editor.foregroundApplication)) foregroundDialog.close() }
            }
            C.Label { text: qsTr("未知程序回到首页；同名绑定按页面顺序取第一项。KDE 自身和短暂焦点变化不会打断当前页。"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#69778b"; font.pixelSize: 12 }
        }
    }
    Connections {
        target: editor
        function onErrorChanged() { if (editor.error) { errorMessage.text = qsTr("无法保存配置：") + editor.error; errorMessage.open(); } }
    }
    MessageDialog { id: errorMessage; title: qsTr("配置需要检查"); buttons: MessageDialog.Ok }
}
