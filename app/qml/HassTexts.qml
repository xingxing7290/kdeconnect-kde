/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
import QtQuick

/**
 * Shared human-readable translations for Home Assistant status and error codes.
 *
 * The daemon sends stable machine codes so the wire format stays small and language
 * independent; only the UI turns them into sentences. Unknown codes (for example a raw
 * Qt socket error string) are shown verbatim rather than swallowed.
 */
QtObject {
    function errorText(code) {
        if (!code) return qsTr("未知错误");
        const messages = {
            hass_disabled: qsTr("智能家居尚未启用"),
            hass_offline: qsTr("Home Assistant 当前不在线"),
            hass_invalid_url: qsTr("Home Assistant 地址无效，请在配置中检查"),
            hass_auth_invalid: qsTr("长期访问令牌无效，请重新生成并填写"),
            hass_request_failed: qsTr("请求 Home Assistant 失败，请稍后重试"),
            hass_unknown_entity: qsTr("该设备不在已选列表中"),
            hass_unsupported_entity: qsTr("该设备类型暂不支持控制"),
            hass_unknown_action: qsTr("不支持的操作"),
            disconnected: qsTr("与 Home Assistant 的连接已断开")
        };
        return messages[code] || qsTr("Home Assistant 返回：%1").arg(code);
    }

    function statusText(status, error) {
        if (status === "ready") return qsTr("已连接 Home Assistant，正在实时同步。");
        if (status === "connecting") return qsTr("正在连接 Home Assistant…");
        if (status === "authenticating") return qsTr("正在验证长期访问令牌…");
        if (status === "error") return qsTr("连接失败：%1").arg(errorText(error));
        return qsTr("尚未启用智能家居，点击「配置」填写地址和令牌。");
    }
}
