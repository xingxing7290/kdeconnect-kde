/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick

/**
 * Shared design tokens for the control-panel window.
 *
 * The C++ Appearance object is exposed as the "appearance" context property. Every
 * QML page creates a lightweight AppTheme instance, so a theme change refreshes all
 * colours through that single binding. If the object is absent (for example when a
 * UI test loads ControlDashboard.qml on its own), the light palette below is used.
 */
QtObject {
    id: root
    readonly property var manager: typeof appearance !== "undefined" ? appearance : null
    readonly property var colors: manager ? manager.colors : ({})
    readonly property bool dark: manager ? manager.dark : false

    readonly property color window: colors.window || "#f4f7fc"
    readonly property color surface: colors.surface || "#ffffff"
    readonly property color surfaceAlt: colors.surfaceAlt || "#f7f9fd"
    readonly property color surfaceHover: colors.surfaceHover || "#eef3fc"
    readonly property color border: colors.border || "#e3e9f2"
    readonly property color borderStrong: colors.borderStrong || "#cbd7e8"
    readonly property color divider: colors.divider || "#e8edf5"
    readonly property color text: colors.text || "#1b2a40"
    readonly property color textMuted: colors.textMuted || "#5c6b81"
    readonly property color textSubtle: colors.textSubtle || "#8a97ab"
    readonly property color textOnAccent: colors.textOnAccent || "#ffffff"
    readonly property color accent: colors.accent || "#3b6ef5"
    readonly property color accentHover: colors.accentHover || "#2f5cd8"
    readonly property color accentSoft: colors.accentSoft || "#e7eeff"
    readonly property color accentSoftText: colors.accentSoftText || "#2549a8"
    readonly property color link: colors.link || "#3b6ef5"
    readonly property color field: colors.field || "#ffffff"
    readonly property color fieldBorder: colors.fieldBorder || "#d7e0ee"
    readonly property color sidebarTop: colors.sidebarTop || "#16233d"
    readonly property color sidebarBottom: colors.sidebarBottom || "#233c6e"
    readonly property color sidebarText: colors.sidebarText || "#f5f8ff"
    readonly property color sidebarMuted: colors.sidebarMuted || "#a6b8d8"
    readonly property color sidebarActive: colors.sidebarActive || "#33518f"
    readonly property color sidebarHover: colors.sidebarHover || "#243b6b"
    readonly property color sidebarIndicator: colors.sidebarIndicator || "#7fb0ff"
    readonly property color danger: colors.danger || "#d34545"
    readonly property color warning: colors.warning || "#b2740f"
    readonly property color success: colors.success || "#2e9e6b"
    readonly property color slot: colors.slot || "#ffffff"
    readonly property color slotBorder: colors.slotBorder || "#dde5f0"
    readonly property color slotHover: colors.slotHover || "#e8f0ff"
    readonly property color chip: colors.chip || "#eef2f9"
    readonly property color chipText: colors.chipText || "#46566e"
    readonly property color phoneFrame: colors.phoneFrame || "#0d1220"
    readonly property color shadow: colors.shadow || "#16233d"
    readonly property color scrim: colors.scrim || "#0b1220"

    readonly property int radius: 16
    readonly property int radiusSmall: 10
    readonly property int radiusLarge: 22
    readonly property int spacing: 16
    readonly property int spacingSmall: 8
    readonly property int sidebarWidth: 214
    readonly property int animation: 180
    readonly property real shadowOpacity: dark ? 0.34 : 0.10

    readonly property var themeNames: manager ? manager.themeNames : [qsTr("跟随系统"), qsTr("晴空蓝"), qsTr("暖砂白"), qsTr("夜幕蓝"), qsTr("石墨紫"), qsTr("松林绿")]
    readonly property int themeIndex: manager ? manager.themeIndex : 1
    function setThemeIndex(index) {
        if (manager)
            manager.themeIndex = index;
    }
}
