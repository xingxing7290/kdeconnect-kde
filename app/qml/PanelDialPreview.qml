/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick

Rectangle {
    id: root
    property string dialId: "digital"
    property string city: ""
    property string message: ""
    property var weather: ({})
    property var now: new Date()
    readonly property bool light: dialId === "info"
    readonly property color ink: light ? "#202722" : "#f5f7fa"
    readonly property string dateText: Qt.formatDate(now, "yyyy/MM/dd ddd")
    readonly property bool hasObservation: weather.city === city.trim() && weather.source === "open-meteo" && Number.isFinite(weather.temperature) && Number.isFinite(weather.observedAt) && Number.isFinite(weather.fetchedAt)
    readonly property real age: now.getTime() / 1000 - (weather.observedAt || 0)
    readonly property real fetchedAge: now.getTime() / 1000 - (weather.fetchedAt || 0)
    readonly property bool freshWeather: hasObservation && age >= -600 && age <= 10800 && fetchedAge >= -600 && fetchedAge <= 7200 && weather.status !== "disabled"
    readonly property string weatherLabel: weather.status === "disabled" ? qsTr("空闲天气已关闭") : hasObservation && !freshWeather ? qsTr("天气数据已过期") : weather.status === "loading" ? qsTr("正在更新天气") : weather.status === "ready" && freshWeather ? codeText(weather.weatherCode) : qsTr("天气暂不可用")
    function codeText(code) {
        if (code === 0) return qsTr("晴");
        if ([1, 2].indexOf(code) >= 0) return qsTr("晴间多云");
        if (code === 3) return qsTr("阴");
        if ([45, 48].indexOf(code) >= 0) return qsTr("雾");
        if ([51, 53, 55].indexOf(code) >= 0) return qsTr("毛毛雨");
        if ([56, 57, 66, 67].indexOf(code) >= 0) return qsTr("冻雨或冻毛毛雨");
        if ([61, 63, 65].indexOf(code) >= 0) return qsTr("雨");
        if ([71, 73, 75, 77, 85, 86].indexOf(code) >= 0) return qsTr("雪");
        if ([80, 81, 82].indexOf(code) >= 0) return qsTr("阵雨");
        if ([95, 96, 99].indexOf(code) >= 0) return qsTr("雷暴");
        return qsTr("天气暂不可用");
    }
    radius: 20
    color: light ? "#eceee8" : "black"
    clip: true
    Item {
        anchors.fill: parent; anchors.margins: 14
        Canvas {
            id: clock
            visible: root.dialId === "analog" || root.dialId === "weather"
            width: root.dialId === "weather" ? parent.width * 0.47 : parent.width
            height: parent.height - 16
            onPaint: {
                const ctx = getContext("2d"); ctx.reset();
                const rx = root.dialId === "weather" ? Math.min(width * 0.46, height * 0.44) : width * 0.46;
                const ry = root.dialId === "weather" ? rx : height * 0.43;
                ctx.translate(width / 2, height / 2);
                function point(angle, scale) { return [Math.sin(angle) * rx * scale, -Math.cos(angle) * ry * scale]; }
                function line(angle, start, end, color, weight) {
                    const a = point(angle, start), b = point(angle, end);
                    ctx.strokeStyle = color; ctx.lineWidth = weight; ctx.lineCap = "round";
                    ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke();
                }
                for (let index = 0; index < 60; ++index) line(index * Math.PI / 30, index % 5 === 0 ? 0.86 : 0.94, 1, index % 5 === 0 ? "#d7dfe7" : "#5e6671", index % 5 === 0 ? 2 : 1);
                ctx.fillStyle = "white"; ctx.font = Math.max(10, Math.min(rx, ry) * 0.2) + "px sans-serif"; ctx.textAlign = "center"; ctx.textBaseline = "middle";
                for (const hour of [12, 3, 6, 9]) { const p = point(hour * Math.PI / 6, 0.71); ctx.fillText(hour.toString(), p[0], p[1]); }
                const second = root.now.getSeconds();
                const minute = root.now.getMinutes() + second / 60;
                const hour = root.now.getHours() % 12 + minute / 60;
                line(hour * Math.PI / 6, -0.06, 0.48, "white", 4);
                line(minute * Math.PI / 30, -0.08, 0.72, "white", 2);
                line(second * Math.PI / 30, -0.15, 0.88, "#dba353", 1);
                ctx.fillStyle = "#dba353"; ctx.beginPath(); ctx.arc(0, 0, 3, 0, Math.PI * 2); ctx.fill();
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Connections { target: root; function onNowChanged() { clock.requestPaint(); } function onDialIdChanged() { clock.requestPaint(); } }
        }
        Text { visible: root.dialId === "analog"; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; text: root.dateText; color: root.ink; font.pixelSize: 10 }
        Text {
            visible: root.dialId === "digital" || root.dialId === "info"
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
            width: parent.width * 0.7; height: parent.height * 0.8
            text: Qt.formatTime(root.now, "hh:mm"); color: root.light ? root.ink : "#85b4ff"
            font.pixelSize: Math.max(12, parent.height * 0.58); minimumPixelSize: 12; fontSizeMode: Text.Fit; font.weight: Font.Light
            verticalAlignment: Text.AlignVCenter
        }
        Column {
            visible: root.dialId === "digital" || root.dialId === "info" || root.dialId === "weather"
            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            width: parent.width * (root.dialId === "weather" ? 0.47 : 0.29); spacing: 4
            Text { width: parent.width; visible: root.dialId === "weather" || root.dialId === "info"; text: (root.hasObservation ? root.weather.resolvedCity : root.city) || qsTr("未设置城市"); color: root.ink; font.pixelSize: 12; elide: Text.ElideRight }
            Text { objectName: "panelWeatherTemperature"; visible: root.dialId === "weather" || root.dialId === "info" || (root.dialId === "digital" && !!root.city); text: root.freshWeather ? Math.round(root.weather.temperature) + "°C" : "--°C"; color: root.ink; font.pixelSize: Math.max(12, root.height * (root.dialId === "weather" ? 0.25 : 0.12)) }
            Text { objectName: "panelWeatherStatus"; width: parent.width; visible: root.dialId === "weather" || root.dialId === "info" || (root.dialId === "digital" && !!root.city); text: (root.freshWeather && root.weather.status !== "ready" ? qsTr("缓存 · ") : "") + root.weatherLabel; color: root.ink; font.pixelSize: 10; wrapMode: Text.WordWrap }
            Text { width: parent.width; visible: root.dialId === "weather" || root.dialId === "info" || (root.dialId === "digital" && !!root.city); text: qsTr("Open-Meteo 模型天气"); color: root.ink; opacity: 0.65; font.pixelSize: 9; wrapMode: Text.WordWrap }
            Text { width: parent.width; visible: root.hasObservation && (root.dialId === "weather" || root.dialId === "info" || root.dialId === "digital"); text: qsTr("模型 %1 · 获取 %2").arg(Qt.formatDateTime(new Date((root.weather.observedAt || 0) * 1000), "MM-dd hh:mm")).arg(Qt.formatTime(new Date((root.weather.fetchedAt || 0) * 1000), "hh:mm")); color: root.ink; opacity: 0.65; font.pixelSize: 9; wrapMode: Text.WordWrap }
            Text { width: parent.width; visible: root.dialId !== "weather"; text: root.dateText; color: root.ink; font.pixelSize: 10; wrapMode: Text.WordWrap }
            Text { width: parent.width; visible: !!root.message; text: root.message; color: root.ink; font.pixelSize: 12; wrapMode: Text.WrapAnywhere; maximumLineCount: 2; elide: Text.ElideRight }
        }
        Canvas {
            id: car
            visible: root.dialId === "car"
            width: parent.width * 0.56; height: parent.height
            onPaint: {
                const ctx = getContext("2d"); ctx.reset();
                const x = width * 0.06, y = height * 0.36, w = width * 0.88, h = height * 0.28;
                ctx.fillStyle = "#263d56"; ctx.beginPath(); ctx.roundedRect(x, y, w, h, h * 0.28, h * 0.28); ctx.fill();
                ctx.strokeStyle = "#94c3ed"; ctx.lineWidth = 2; ctx.beginPath(); ctx.roundedRect(x + w * 0.24, y - h * 0.42, w * 0.49, h * 0.65, h * 0.2, h * 0.2); ctx.stroke();
                for (const part of [0.2, 0.8]) {
                    ctx.fillStyle = "#111720"; ctx.beginPath(); ctx.arc(x + w * part, y + h * 0.89, h * 0.29, 0, Math.PI * 2); ctx.fill();
                    ctx.strokeStyle = "#8ab9e8"; ctx.beginPath(); ctx.arc(x + w * part, y + h * 0.89, h * 0.15, 0, Math.PI * 2); ctx.stroke();
                }
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }
        Column {
            visible: root.dialId === "car"; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: parent.width * 0.4; spacing: 8
            Text { text: Qt.formatTime(root.now, "hh:mm"); color: "#85b4ff"; font.pixelSize: Math.max(12, root.height * 0.22); font.family: "monospace" }
            Text { width: parent.width; text: root.dateText; color: root.ink; font.pixelSize: 10; wrapMode: Text.WordWrap }
            Text { width: parent.width; text: root.message; color: root.ink; maximumLineCount: 2; elide: Text.ElideRight; wrapMode: Text.WrapAnywhere; font.pixelSize: 11 }
        }
    }
}
