/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "panelInformationDial"
    property string city: ""
    property string message: ""
    property var weather: ({})
    property var info: ({})
    property var progress: ({})
    property var now: new Date()
    property bool fresh: false
    property string statusText: ""
    property var codeText: function(code) { return ""; }
    readonly property real u: Math.max(0.1, Math.min(width/720, height/360))
    readonly property color ink: "#202722"
    readonly property color quiet: "#5f655f"
    readonly property var noteList: Array.from(info.notes || [])
    readonly property bool showProgress: info.showProgress === true
    readonly property bool showForecast: info.showForecast !== false
    readonly property int staleMs: info.staleMs || 15000
    readonly property bool progressUsable: !!progress.status && progress.status !== "unavailable" && progress.source === "workbuddy"
    readonly property real progressAge: now.getTime() / 1000 - (progress.fetchedAt || 0)
    readonly property bool progressFresh: progressUsable && progressAge >= -60 && progressAge * 1000 <= staleMs
    readonly property string progressState: !progressUsable ? qsTr("等待 WorkBuddy 进度")
        : !progressFresh ? qsTr("进度已过期，等待更新")
        : progress.status === "running" ? qsTr("进行中")
        : progress.status === "idle" ? qsTr("空闲")
        : progress.status === "done" ? qsTr("已完成")
        : progress.status === "error" ? qsTr("出错")
        : progress.status === "paused" ? qsTr("已暂停") : qsTr("暂不可用")
    readonly property string cityToday: new Date(now.getTime() + (weather.utcOffsetSeconds || 0) * 1000).toISOString().slice(0,10)
    readonly property var days: fresh && weather.forecast ? Array.from(weather.forecast).filter(day => day.date >= cityToday) : []
    readonly property var today: days.length && days[0].date === cityToday ? days[0] : null
    function dayLabel(date) { const p=date.split("-"); return Qt.formatDate(new Date(Number(p[0]),Number(p[1])-1,Number(p[2]),12), "MM/dd ddd"); }
    function range(day) { return day ? Math.round(day.minimum)+"° / "+Math.round(day.maximum)+"°" : "--° / --°"; }
    color: "#eceee8"; border.color: ink; border.width: 2; radius: 10
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 8*root.u; spacing: 5*root.u
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 7*root.u
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 330*root.u; spacing: 6*root.u
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 34*root.u; color: "transparent"; border.color: root.ink
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 5*root.u
                        Text { text: qsTr("手机本机电量"); color: root.ink; font.pixelSize: Math.max(9,17*root.u); Layout.fillWidth: true }
                        Text { objectName: "panelInfoBattery"; text: "--%"; color: root.ink; font.family: "monospace"; font.pixelSize: Math.max(10,23*root.u) }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredHeight: 94*root.u
                    Text { text: Qt.formatTime(root.now,"hh:mm"); color: root.ink; font.pixelSize: Math.max(12,79*root.u); font.weight: Font.Light; font.family: "monospace"; Layout.fillWidth: true }
                    Text { text: Qt.formatTime(root.now,"ss"); color: root.quiet; font.pixelSize: Math.max(9,28*root.u); Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 8*root.u }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: root.ink
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 5*root.u; spacing: 5*root.u
                        PanelWeatherGlyph { code: root.fresh && Number.isFinite(root.weather.weatherCode) ? root.weather.weatherCode : -1; ink: root.ink; Layout.fillHeight: true; Layout.preferredWidth: 100*root.u }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 3*root.u
                            Text { objectName: "panelInfoTemperature"; text: root.fresh ? Math.round(root.weather.temperature)+"°C" : "--°C"; color: root.ink; font.pixelSize: Math.max(11,37*root.u) }
                            Text { objectName: "panelInfoStatus"; text: root.statusText; color: root.ink; font.pixelSize: Math.max(8,14*root.u); wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { objectName: "panelInfoTodayRange"; text: root.range(root.today); color: root.quiet; font.pixelSize: Math.max(8,15*root.u) }
                        }
                    }
                }
            }
            // ---- RIGHT: configurable information area (notes, WorkBuddy progress, forecast). ----
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 350*root.u; spacing: 6*root.u
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 74*root.u; color: "transparent"; border.color: root.ink
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 6*root.u; spacing: 2*root.u
                        Text {
                            objectName: "panelInfoMessage"; Layout.fillWidth: true; Layout.fillHeight: true
                            text: root.noteList.length ? root.noteList[0] : (root.message || qsTr("可在电脑端设置个性化文本"))
                            textFormat: Text.PlainText; color: root.noteList.length || root.message ? root.ink : root.quiet
                            font.pixelSize: Math.max(9, 18*root.u); wrapMode: Text.WrapAnywhere
                            maximumLineCount: root.noteList.length > 1 ? 2 : 3
                            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                        }
                        Repeater {
                            model: root.noteList.length > 1 ? root.noteList.slice(1) : []
                            Text {
                                required property int index
                                required property string modelData
                                objectName: "panelInfoNote-" + (index + 1)
                                Layout.fillWidth: true
                                text: "· " + modelData; textFormat: Text.PlainText; color: root.ink
                                font.pixelSize: Math.max(9, 18*root.u); wrapMode: Text.WrapAnywhere
                                maximumLineCount: 1; elide: Text.ElideRight
                            }
                        }
                    }
                }
                Rectangle {
                    visible: root.showProgress
                    Layout.fillWidth: true; Layout.preferredHeight: 82*root.u; color: "transparent"; border.color: root.ink
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 5*root.u; spacing: 2*root.u
                        RowLayout {
                            Layout.fillWidth: true
                            Text { objectName: "panelInfoProgressTitle"; text: qsTr("工作进度"); color: root.ink; font.pixelSize: Math.max(9, 14*root.u); Layout.fillWidth: true }
                            Text { objectName: "panelInfoProgressState"; text: root.progressState; color: root.progressFresh ? root.ink : root.quiet; font.pixelSize: Math.max(8, 10*root.u) }
                        }
                        Text {
                            objectName: "panelInfoProgressStage"; visible: root.progressUsable; Layout.fillWidth: true
                            text: (root.progress.title || "") + (root.progress.stage ? " · " + root.progress.stage : "")
                            textFormat: Text.PlainText; color: root.ink; font.pixelSize: Math.max(8, 15*root.u)
                            maximumLineCount: 1; elide: Text.ElideRight
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                objectName: "panelInfoProgressDetail"; Layout.fillWidth: true
                                text: root.progressUsable ? (root.progress.detail || "") : qsTr("等待 WorkBuddy 写入进度")
                                textFormat: Text.PlainText; color: root.quiet; font.pixelSize: Math.max(8, 11*root.u)
                                maximumLineCount: 1; elide: Text.ElideRight
                            }
                            Text {
                                visible: root.progressUsable && Number.isFinite(root.progress.percent)
                                objectName: "panelInfoProgressPercent"
                                text: Math.round(root.progress.percent || 0) + "%"; color: root.ink
                                font.family: "monospace"; font.pixelSize: Math.max(9, 14*root.u)
                            }
                        }
                        Rectangle {
                            visible: root.progressUsable && Number.isFinite(root.progress.percent)
                            Layout.fillWidth: true; Layout.preferredHeight: Math.max(4, 6*root.u); radius: 3
                            color: Qt.rgba(root.ink.r, root.ink.g, root.ink.b, 0.18)
                            Rectangle {
                                width: parent.width * Math.min(100, Math.max(0, Number(root.progress.percent) || 0)) / 100
                                height: parent.height; radius: 3; color: root.ink
                            }
                        }
                    }
                }
                Rectangle {
                    visible: root.showForecast
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: root.ink
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 5*root.u; spacing: 2*root.u
                        Repeater {
                            model: 3
                            delegate: RowLayout {
                                id: row
                                required property int index
                                readonly property var day: root.days[index] || null
                                objectName: "panelInfoForecast-"+index
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4*root.u
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 1
                                    Text { text: row.day ? root.dayLabel(row.day.date) : qsTr("预报待更新"); color: root.ink; font.pixelSize: Math.max(8,18*root.u); Layout.fillWidth: true; elide: Text.ElideRight }
                                    Text { text: row.day ? root.codeText(row.day.weatherCode) : "--"; color: root.quiet; font.pixelSize: Math.max(7,12*root.u); Layout.fillWidth: true; elide: Text.ElideRight }
                                }
                                PanelWeatherGlyph { code: row.day ? row.day.weatherCode : -1; ink: root.ink; Layout.preferredWidth: 34*root.u; Layout.preferredHeight: 35*root.u }
                                Text { objectName: "panelInfoForecastRange-"+row.index; text: root.range(row.day); color: root.ink; font.family: "monospace"; font.pixelSize: Math.max(9,20*root.u) }
                            }
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 29*root.u; color: "transparent"; border.color: root.ink
            RowLayout {
                anchors.fill: parent; anchors.margins: 4*root.u
                Text { text: Qt.formatDate(root.now,"ddd  MM/dd"); color: root.ink; font.pixelSize: Math.max(9,19*root.u); Layout.preferredWidth: 250*root.u }
                Text { text: root.weather.resolvedCity || root.city || qsTr("未设置城市"); textFormat: Text.PlainText; color: root.ink; font.pixelSize: Math.max(9,17*root.u); Layout.fillWidth: true; elide: Text.ElideRight }
            }
        }
        Text { text: qsTr("Open-Meteo 模型天气")+(root.weather.timezone ? " · "+root.weather.timezone : ""); textFormat: Text.PlainText; color: root.quiet; font.pixelSize: Math.max(7,10*root.u); Layout.fillWidth: true; elide: Text.ElideRight }
        Text { visible: !!root.weather.observedAt; text: qsTr("模型 %1 · 获取 %2").arg(Qt.formatDateTime(new Date((root.weather.observedAt || 0)*1000),"MM-dd hh:mm")).arg(Qt.formatDateTime(new Date((root.weather.fetchedAt || 0)*1000),"MM-dd hh:mm")); color: root.quiet; font.pixelSize: Math.max(7,10*root.u); Layout.fillWidth: true; elide: Text.ElideRight }
    }
}
