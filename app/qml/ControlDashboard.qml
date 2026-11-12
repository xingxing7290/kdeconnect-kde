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
    property var expandedLibraryGroups: ({system: true, settings: false, websites: false})
    property string libraryMessage: ""
    readonly property var libraryGroups: [
        {id: "system", title: qsTr("系统功能"), items: presets.filter(item => item.group === qsTr("系统功能"))},
        {id: "settings", title: qsTr("系统设置"), items: presets.filter(item => item.group === qsTr("系统设置"))},
        {id: "websites", title: qsTr("常用网站"), items: presets.filter(item => item.group === qsTr("常用网站"))}
    ]
    function toggleLibraryGroup(id) {
        const next = Object.assign({}, expandedLibraryGroups);
        next[id] = !next[id];
        expandedLibraryGroups = next;
    }
    function kindIcon(kind) {
        return ({program: "application-x-executable", website: "internet-web-browser", file: "text-x-generic", folder: "folder", shortcut: "input-keyboard", system: "preferences-system", command: "utilities-terminal"})[kind] || "application-x-executable";
    }
    function buttonIcon(button) {
        const systemIcons = {thisPc: "computer", recycleBin: "user-trash", commandPrompt: "utilities-terminal", taskManager: "utilities-system-monitor", showDesktop: "user-desktop", screenshot: "camera-photo", settings: "preferences-system", networkSettings: "network-wireless", personalization: "preferences-desktop-theme", displaySettings: "video-display", soundSettings: "audio-volume-high"};
        return systemIcons[button?.target?.action] || kindIcon(button?.kind);
    }
    function configurationError(code) {
        const messages = {
            edit_conflict: qsTr("配置已变化，请关闭窗口后重新打开，避免覆盖新的内容。"),
            stale_revision: qsTr("另一窗口已修改配置，请重新打开后再保存。"),
            unknown_page: qsTr("该页面已不存在，请重新选择页面。"),
            last_page: qsTr("至少需要保留一个页面。"),
            page_limit: qsTr("最多只能创建十个页面。"),
            invalid_application: qsTr("请输入有效的程序名称，例如 notepad.exe。"),
            invalid_page_name: qsTr("请填写不超过 120 个字符的页面名称。"),
            invalid_background: qsTr("背景色应为 #RRGGBB 格式，例如 #FFFFFF。"),
            invalid_idle_timeout: qsTr("空闲时长应为 1000 至 86400000 毫秒。"),
            invalid_dial_text: qsTr("城市最多 120 个字符，个性化文本最多 240 个字符。")
        };
        return messages[code] || qsTr("无法保存配置：%1").arg(code);
    }
    function requestDeletePage(id) {
        const target = config.pages.find(item => item.id === id);
        if (!target || config.pages.length <= 1) return;
        deletePageDialog.targetPageId = id;
        deletePageDialog.targetPageName = target.name;
        deletePageDialog.editSnapshot = editor.pageEditContext(id);
        deletePageDialog.validationError = "";
        deletePageDialog.open();
    }
    function revealCurrentPage() {
        Qt.callLater(function() {
            const index = config.pages.findIndex(item => item.id === editor.currentPageId);
            const item = pageRepeater.itemAt(index);
            const viewport = pageScroll.contentItem;
            if (!item || !viewport || viewport.contentX === undefined) return;
            let position = viewport.contentX;
            if (item.x < position) position = item.x;
            else if (item.x + item.width > position + viewport.width) position = item.x + item.width - viewport.width;
            viewport.contentX = Math.max(0, Math.min(Math.max(0, pageScroll.contentWidth - viewport.width), position));
        });
    }
    function addPreset(preset) {
        const empty = page.slots.findIndex(item => !item);
        if (empty < 0) {
            libraryMessage = qsTr("当前页已满。可拖到需要替换的槽位，或新增页面。");
            return;
        }
        libraryMessage = "";
        editor.setButton(empty, preset);
    }
    function applyDrop(index, event) {
        let saved = false;
        let action = Qt.CopyAction;
        if (event.hasUrls) {
            if (event.urls.length !== 1 || !(event.supportedActions & Qt.CopyAction)) return;
            saved = editor.dropFile(index, event.urls[0]);
        } else if (event.formats.indexOf("application/x-kdeconnect-slot") >= 0) {
            if (!(event.supportedActions & Qt.MoveAction)) return;
            let context;
            try { context = JSON.parse(event.getDataAsString("application/x-kdeconnect-slot")); }
            catch (_) { return; }
            if (!context || typeof context !== "object" || !Number.isInteger(context.slotIndex)) return;
            saved = editor.moveButtonChecked(context.slotIndex, index, context);
            action = Qt.MoveAction;
        } else if (event.formats.indexOf("application/x-kdeconnect-preset") >= 0) {
            if (!(event.supportedActions & Qt.CopyAction)) return;
            let preset;
            try { preset = JSON.parse(event.getDataAsString("application/x-kdeconnect-preset")); }
            catch (_) { return; }
            if (!preset || typeof preset !== "object" || Array.isArray(preset)) return;
            saved = editor.setButton(index, preset);
        }
        // External files and presets create references, never filesystem moves.
        if (saved) event.accept(action);
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
            C.Label { visible: !!editor.error; text: root.configurationError(editor.error); color: "#b3261e"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 22
                Item {
                    Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 420
                    Repeater {
                        model: 3
                        delegate: Rectangle {
                            required property int index
                            anchors.centerIn: phone; anchors.verticalCenterOffset: 7
                            width: phone.width + 6 + index * 7; height: phone.height + 6 + index * 7
                            radius: 28 + index * 3; color: "#0b1728"; opacity: 0.035 * (3 - index); z: -1
                        }
                    }
                    Rectangle {
                        id: phone
                        objectName: "panelPhonePreview"
                        width: Math.min(parent.width - 20, parent.height * 2.1 - 20); height: width / 2.1
                        anchors.centerIn: parent; color: "#090b0e"; radius: 24
                        Rectangle {
                            anchors.fill: parent; anchors.margins: 8; radius: 16; color: config.appearance.background
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 12; spacing: 12
                                GridLayout {
                                    columns: 6; rows: 3; columnSpacing: 8; rowSpacing: 8; Layout.fillWidth: true; Layout.fillHeight: true
                                    Repeater {
                                        model: 18
                                        delegate: Rectangle {
                                            id: cell
                                            objectName: "panelSlot-" + index
                                            required property int index
                                            property var button: page.slots?.[index] || null
                                            property var dragContext: ({})
                                            Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 70; Layout.preferredHeight: 75
                                            radius: 10; color: drop.containsDrag ? "#dceaff" : "#fbfcfe"
                                            border.width: drop.containsDrag || activeFocus ? 2 : 1; border.color: drop.containsDrag || activeFocus ? "#4988f1" : "#dce1e8"
                                            activeFocusOnTab: true
                                            Accessible.role: Accessible.Button
                                            Accessible.name: button ? button.name : qsTr("添加按钮，槽位 %1").arg(index + 1)
                                            Accessible.onPressAction: buttonDialog.openAt(index, cell.button)
                                            Keys.onReturnPressed: buttonDialog.openAt(index, cell.button)
                                            Keys.onSpacePressed: buttonDialog.openAt(index, cell.button)
                                            Column {
                                                anchors.centerIn: parent; width: parent.width - 8; spacing: 4
                                                Image { id: slotImage; visible: status === Image.Ready; source: cell.button?.icon ? editor.assetUrl(cell.button.icon) : ""; width: Math.min(40, cell.width * 0.6); height: visible ? width : 0; fillMode: Image.PreserveAspectFit; anchors.horizontalCenter: parent.horizontalCenter }
                                                Kirigami.Icon { visible: cell.button !== null && slotImage.status !== Image.Ready; source: root.buttonIcon(cell.button); width: Math.min(32, cell.width * 0.5); height: visible ? width : 0; anchors.horizontalCenter: parent.horizontalCenter }
                                                C.Label { visible: !cell.button; text: "+"; font.pixelSize: 27; color: "#99a3b1"; anchors.horizontalCenter: parent.horizontalCenter }
                                                C.Label { visible: config.appearance.showLabels && cell.button !== null; text: cell.button?.name || ""; textFormat: Text.PlainText; width: parent.width; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 11; color: "#344158" }
                                            }
                                            Drag.active: drag.active && !!cell.dragContext.pageId
                                            Drag.dragType: Drag.Automatic
                                            Drag.supportedActions: Qt.MoveAction
                                            Drag.mimeData: {"application/x-kdeconnect-slot": JSON.stringify(cell.dragContext)}
                                            DragHandler { id: drag; target: null; enabled: cell.button !== null; onActiveChanged: if (active) cell.dragContext = editor.editContext(index) }
                                            TapHandler { onTapped: { cell.forceActiveFocus(); buttonDialog.openAt(index, cell.button); } }
                                            DropArea {
                                                id: drop; anchors.fill: parent
                                                onDropped: event => root.applyDrop(index, event)
                                            }
                                        }
                                    }
                                }
                                ColumnLayout {
                                    visible: config.appearance.showVolume; Layout.preferredWidth: 30; Layout.fillHeight: true
                                    Kirigami.Icon { source: "audio-volume-high"; Layout.preferredWidth: 20; Layout.preferredHeight: 20; Layout.alignment: Qt.AlignHCenter }
                                    C.Label { text: localVolume.available ? localVolume.volume + "%" : "--"; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                                    C.Slider {
                                        id: previewVolume
                                        objectName: "panelLocalVolumeSlider"
                                        property string dragOutput: ""
                                        orientation: Qt.Vertical; enabled: localVolume.available; from: 0; to: 100; stepSize: 1
                                        value: localVolume.volume; Layout.fillHeight: true; Layout.alignment: Qt.AlignHCenter
                                        Accessible.name: qsTr("电脑音量")
                                        background: Rectangle {
                                            x: previewVolume.leftPadding + (previewVolume.availableWidth - width) / 2
                                            y: previewVolume.topPadding; width: 4; height: previewVolume.availableHeight; radius: 2; color: "#dde5f1"
                                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: (1 - previewVolume.visualPosition) * parent.height; radius: 2; color: "#3984f5" }
                                        }
                                        handle: Rectangle {
                                            x: previewVolume.leftPadding + (previewVolume.availableWidth - width) / 2
                                            y: previewVolume.topPadding + previewVolume.visualPosition * (previewVolume.availableHeight - height)
                                            implicitWidth: 14; implicitHeight: 14; radius: 7; color: "white"; border.color: "#b7c6dc"
                                        }
                                        onPressedChanged: if (pressed) dragOutput = localVolume.defaultId
                                        onMoved: {
                                            const output = pressed ? dragOutput : localVolume.defaultId;
                                            if (output === localVolume.defaultId) localVolume.setSinkVolume(output, Math.round(value));
                                        }
                                    }
                                    C.ToolButton {
                                        objectName: "panelLocalMuteButton"
                                        enabled: localVolume.available; Layout.alignment: Qt.AlignHCenter
                                        Accessible.name: localVolume.muted ? qsTr("取消静音") : qsTr("静音")
                                        contentItem: Kirigami.Icon { source: localVolume.muted ? "audio-volume-muted" : "audio-volume-high"; implicitWidth: 22; implicitHeight: 22 }
                                        onClicked: localVolume.setSinkMuted(localVolume.defaultId, !localVolume.muted)
                                    }
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
                            C.Label { visible: !!root.libraryMessage; width: parent.width; text: root.libraryMessage; wrapMode: Text.WordWrap; color: "#a25712"; textFormat: Text.PlainText }
                            Repeater {
                                model: root.libraryGroups
                                delegate: Column {
                                    id: libraryGroup
                                    required property var modelData
                                    width: parent.width
                                    C.ItemDelegate {
                                        objectName: "panelLibraryGroup-" + libraryGroup.modelData.id
                                        width: parent.width; text: libraryGroup.modelData.title
                                        Accessible.name: text
                                        Accessible.description: root.expandedLibraryGroups[libraryGroup.modelData.id] ? qsTr("已展开，点击收起") : qsTr("已收起，点击展开")
                                        onClicked: root.toggleLibraryGroup(libraryGroup.modelData.id)
                                        contentItem: RowLayout {
                                            C.Label { text: libraryGroup.modelData.title; color: "#354158"; font.bold: true; Layout.fillWidth: true }
                                            C.Label { text: root.expandedLibraryGroups[libraryGroup.modelData.id] ? "-" : "+"; color: "#69778b" }
                                        }
                                    }
                                    Repeater {
                                        model: root.expandedLibraryGroups[libraryGroup.modelData.id] ? libraryGroup.modelData.items : []
                                        delegate: C.ItemDelegate {
                                        id: libraryButton
                                        required property var modelData
                                        text: modelData.name; width: parent.width
                                        contentItem: RowLayout {
                                            spacing: 10
                                            Kirigami.Icon { source: root.buttonIcon(modelData); Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
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
            }
            RowLayout {
                Layout.fillWidth: true
                C.ScrollView {
                    id: pageScroll
                    objectName: "panelPageStrip"
                    Layout.fillWidth: true; Layout.preferredHeight: 44
                    visible: config.mode !== "single"
                    clip: true; contentWidth: pageRow.implicitWidth; contentHeight: pageRow.implicitHeight
                    C.ScrollBar.horizontal.policy: C.ScrollBar.AsNeeded
                    C.ScrollBar.vertical.policy: C.ScrollBar.AlwaysOff
                    Row {
                    id: pageRow; spacing: 6
                    Repeater {
                    id: pageRepeater
                    model: config.pages
                    delegate: Row {
                        id: pageTab
                        required property var modelData
                        required property int index
                        spacing: 1
                        C.Button {
                        id: pageSelect
                        objectName: "panelPage-" + pageTab.index
                        text: (pageTab.index + 1) + "  " + pageTab.modelData.name
                        highlighted: editor.currentPageId === pageTab.modelData.id
                        enabled: config.mode !== "single"
                        implicitWidth: Math.min(146, Math.max(64, contentItem.implicitWidth + leftPadding + rightPadding)); height: 36
                        palette.dark: "#286fe0"; palette.mid: "#205ab8"; palette.brightText: "#ffffff"
                        contentItem: C.Label { text: pageSelect.text; textFormat: Text.PlainText; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: pageSelect.highlighted ? "white" : "#354158" }
                        Accessible.name: qsTr("页面 %1：%2").arg(pageTab.index + 1).arg(pageTab.modelData.name)
                        hoverEnabled: true
                        onClicked: editor.selectPage(pageTab.modelData.id)
                        onHoveredChanged: if (hovered) editor.selectPage(pageTab.modelData.id)
                        C.ToolTip.visible: hovered
                        C.ToolTip.text: pageTab.modelData.name
                        }
                        C.ToolButton {
                            objectName: "panelDeletePage-" + pageTab.index
                            text: "x"; width: 24; height: 36
                            visible: config.pages.length > 1 && pageTab.modelData.id !== config.homePageId
                            Accessible.name: qsTr("删除页面：%1").arg(pageTab.modelData.name)
                            onClicked: root.requestDeletePage(pageTab.modelData.id)
                        }
                    }
                    }
                    }
                }
                C.Button { objectName: "panelAddPage"; text: "+"; visible: config.mode !== "single"; enabled: config.pages.length < 10; Accessible.name: qsTr("添加页面"); onClicked: editor.addPage() }
                C.Button { objectName: "panelDeletePage"; text: qsTr("删除页"); visible: config.mode !== "single"; enabled: config.pages.length > 1; onClicked: root.requestDeletePage(editor.currentPageId) }
                C.Label { visible: config.mode === "single" && config.pages.length > 1; text: qsTr("单页显示首页，其他页面仍保留"); color: "#69778b"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Item { visible: config.mode === "single" && config.pages.length <= 1; Layout.fillWidth: true }
                C.Button { objectName: "panelPageSettings"; text: qsTr("页面名称 / 应用绑定"); onClicked: {
                    pageDialog.targetPageId = editor.currentPageId;
                    pageDialog.editSnapshot = editor.pageEditContext(editor.currentPageId);
                    pageDialog.validationError = "";
                    pageName.text = page.name;
                    applicationField.text = page.application || "";
                    pageDialog.open();
                } }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#e1e6ef" }
            Flow {
                Layout.fillWidth: true; spacing: 14
                RowLayout {
                width: implicitWidth
                spacing: 8
                C.Label { text: qsTr("页面模式") }
                C.ComboBox { objectName: "panelPageMode"; model: [qsTr("默认单页"), qsTr("多页面"), qsTr("智能跟随")]; currentIndex: ["single", "pages", "follow"].indexOf(config.mode); onActivated: editor.setMode(["single", "pages", "follow"][currentIndex]) }
                }
                C.Button { text: qsTr("识别当前程序"); visible: config.mode === "follow"; onClicked: foregroundDialog.open() }
                C.CheckBox { text: qsTr("音量控制"); checked: config.appearance.showVolume; onClicked: editor.setAppearance("showVolume", checked) }
                C.CheckBox { text: qsTr("显示名称"); checked: config.appearance.showLabels; onClicked: editor.setAppearance("showLabels", checked) }
            }
            C.Label {
                visible: config.mode === "follow"; Layout.fillWidth: true; elide: Text.ElideMiddle; color: "#69778b"; font.pixelSize: 11
                text: editor.foregroundSupported ? qsTr("当前程序：%1；手动切页保持到下次程序切换。").arg(editor.foregroundApplication || qsTr("请切到要识别的软件")) : qsTr("当前平台暂不支持前台识别，请使用 Windows 版本。")
            }
            Flow {
                Layout.fillWidth: true; spacing: 14
                RowLayout {
                width: implicitWidth
                spacing: 8
                C.Label { text: qsTr("背景色") }
                C.Button {
                    objectName: "panelBackgroundPicker"; implicitWidth: 38
                    Accessible.name: qsTr("选择背景颜色")
                    contentItem: Rectangle { implicitWidth: 22; implicitHeight: 18; color: config.appearance.background; border.color: "#8793a5" }
                    onClicked: { colorPicker.selectedColor = config.appearance.background; colorPicker.open(); }
                }
                C.TextField { objectName: "panelBackgroundHex"; text: config.appearance.background; maximumLength: 7; Layout.preferredWidth: 105; Accessible.name: qsTr("背景色十六进制值"); onEditingFinished: editor.setAppearance("background", text) }
                }
                RowLayout {
                width: implicitWidth
                spacing: 8
                visible: config.mode !== "single"
                C.Label { text: qsTr("切换方向") }
                C.ComboBox { model: [qsTr("左右滑动"), qsTr("上下滑动")]; currentIndex: config.appearance.swipeDirection === "vertical" ? 1 : 0; onActivated: editor.setAppearance("swipeDirection", currentIndex ? "vertical" : "horizontal") }
                }
            }
            RowLayout {
                visible: config.appearance.showVolume; Layout.fillWidth: true
                C.Label { text: qsTr("音频输出") }
                C.ComboBox {
                    Layout.fillWidth: true; model: localVolume.outputs; textRole: "description"
                    objectName: "panelLocalOutputSelector"
                    currentIndex: localVolume.outputs.findIndex(item => item.enabled)
                    enabled: localVolume.outputs.length > 0
                    onActivated: localVolume.setDefaultSink(localVolume.outputs[currentIndex].name)
                    Accessible.name: qsTr("默认音频输出设备")
                }
                C.Label { text: qsTr("无默认输出"); color: "#b3261e"; visible: !localVolume.available }
            }
            C.Label { visible: !!localVolume.error; text: qsTr("音量操作暂不可用：") + localVolume.error; color: "#b3261e"; font.pixelSize: 11; Layout.fillWidth: true }
        }
        ColumnLayout {
            C.Label { text: qsTr("开启连接"); font.pixelSize: 26; font.bold: true; color: "#273448" }
            C.Label { text: qsTr("在 Android 和电脑上打开 KDE Connect，连接同一网络，在设备管理中配对。\n配对完成后，手机打开“控制面板”即可读取电脑配置。"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#69778b"; lineHeight: 1.6 }
            C.Button { text: qsTr("打开设备管理与配对"); highlighted: true; palette.dark: "#286fe0"; palette.mid: "#205ab8"; palette.brightText: "white"; onClicked: root.openConnections() }
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
        id: deletePageDialog; parent: C.Overlay.overlay; anchors.centerIn: parent; modal: true; title: qsTr("删除页面？")
        objectName: "panelDeletePageDialog"
        property string targetPageId: ""
        property string targetPageName: ""
        property var editSnapshot: ({})
        property string validationError: ""
        width: Math.min(440, parent ? parent.width - 32 : 440)
        ColumnLayout {
            width: parent.width
            C.Label { text: qsTr("将删除“%1”及其中的按钮，可用“撤回上次修改”恢复。").arg(deletePageDialog.targetPageName); textFormat: Text.PlainText; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            C.Label { visible: !!deletePageDialog.validationError; text: deletePageDialog.validationError; textFormat: Text.PlainText; color: "#b3261e"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                C.Button { text: qsTr("取消"); onClicked: deletePageDialog.close() }
                C.Button { objectName: "panelDeletePageConfirm"; text: qsTr("删除页面"); highlighted: true; palette.dark: "#286fe0"; palette.mid: "#205ab8"; palette.brightText: "white"; onClicked: {
                    if (editor.deletePageChecked(deletePageDialog.targetPageId, deletePageDialog.editSnapshot)) deletePageDialog.close();
                    else deletePageDialog.validationError = root.configurationError(editor.error);
                } }
            }
        }
    }
    C.Dialog {
        id: pageDialog; parent: C.Overlay.overlay; anchors.centerIn: parent; width: 440; modal: true; title: qsTr("页面设置")
        objectName: "panelPageSettingsDialog"
        property string targetPageId: ""
        property var editSnapshot: ({})
        property string validationError: ""
        ColumnLayout {
            width: parent.width
            C.TextField { id: pageName; objectName: "panelPageName"; placeholderText: qsTr("页面名称"); maximumLength: 120; Layout.fillWidth: true; Accessible.name: placeholderText }
            C.TextField { id: applicationField; objectName: "panelPageApplication"; placeholderText: qsTr("绑定的程序名，例如 notepad.exe"); maximumLength: 260; Layout.fillWidth: true; Accessible.name: placeholderText }
            C.Button { text: qsTr("使用当前识别的程序"); visible: config.mode === "follow"; enabled: !!editor.foregroundApplication; onClicked: applicationField.text = editor.foregroundApplication }
            C.Label { visible: !!pageDialog.validationError; text: pageDialog.validationError; textFormat: Text.PlainText; color: "#b3261e"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                C.Button { text: qsTr("取消"); onClicked: pageDialog.close() }
                C.Button { objectName: "panelPageSettingsSave"; text: qsTr("保存"); highlighted: true; palette.dark: "#286fe0"; palette.mid: "#205ab8"; palette.brightText: "white"; onClicked: {
                    if (editor.renamePageChecked(pageDialog.targetPageId, pageName.text, applicationField.text, pageDialog.editSnapshot)) pageDialog.close();
                    else pageDialog.validationError = root.configurationError(editor.error);
                } }
            }
        }
    }
    C.Dialog {
        id: foregroundDialog; parent: C.Overlay.overlay; anchors.centerIn: parent; width: 480; modal: true; title: qsTr("识别当前操作的程序")
        property string targetPageId: ""
        property string targetPageName: ""
        property var editSnapshot: ({})
        property string validationError: ""
        onOpened: {
            targetPageId = editor.currentPageId;
            targetPageName = page.name;
            editSnapshot = editor.pageEditContext(targetPageId);
            validationError = "";
        }
        standardButtons: C.Dialog.Close
        ColumnLayout {
            width: parent.width; spacing: 14
            C.Label { text: qsTr("保持此窗口打开，点击要绑定的软件，等待片刻后返回。识别只使用程序名，不读取窗口标题或文档内容。"); wrapMode: Text.WordWrap; Layout.fillWidth: true }
            C.TextField { text: editor.foregroundApplication; readOnly: true; selectByMouse: true; placeholderText: qsTr("等待前台程序"); Layout.fillWidth: true }
            RowLayout {
                C.Button { text: qsTr("复制程序名称"); enabled: !!editor.foregroundApplication; onClicked: editor.copyForegroundApplication() }
                C.Button { text: qsTr("绑定到当前页"); highlighted: true; palette.dark: "#286fe0"; palette.mid: "#205ab8"; palette.brightText: "white"; enabled: !!editor.foregroundApplication; onClicked: {
                    if (editor.renamePageChecked(foregroundDialog.targetPageId, foregroundDialog.targetPageName, editor.foregroundApplication, foregroundDialog.editSnapshot)) foregroundDialog.close();
                    else foregroundDialog.validationError = root.configurationError(editor.error);
                } }
            }
            C.Label { visible: !!foregroundDialog.validationError; text: foregroundDialog.validationError; textFormat: Text.PlainText; color: "#b3261e"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            C.Label { text: qsTr("未知程序回到首页；同名绑定按页面顺序取第一项。KDE 自身和短暂焦点变化不会打断当前页。"); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#69778b"; font.pixelSize: 12 }
        }
    }
    Connections {
        target: editor
        function onChanged() { root.libraryMessage = ""; root.revealCurrentPage(); }
        function onErrorChanged() { if (editor.error) {
            if (buttonDialog.visible) buttonDialog.reportError(editor.error);
            else if (pageDialog.visible) pageDialog.validationError = root.configurationError(editor.error);
            else if (deletePageDialog.visible) deletePageDialog.validationError = root.configurationError(editor.error);
            else if (foregroundDialog.visible) foregroundDialog.validationError = root.configurationError(editor.error);
            else { errorMessage.text = root.configurationError(editor.error); errorMessage.open(); }
        } }
    }
    MessageDialog { id: errorMessage; title: qsTr("配置需要检查"); buttons: MessageDialog.Ok }
}
