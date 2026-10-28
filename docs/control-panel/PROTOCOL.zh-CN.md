# 控制面板协议 v1

此协议随个人 fork 发布。传输复用 KDE Connect 的已配对连接；不是匿名 HTTP 服务。当前文档及模型是基础实现，网络插件、UI 和执行器仍按 PLAN 分阶段完成。

## 配置

`kdeconnect.controlpanel.config` 的 `config` 字段为 JSON 对象。测试样本：本目录 fixtures/config-v1.json。Android 和 C++ 测试读取相同内容，修改协议时必须同时更新两端及样本。

- schemaVersion 必须为整数 1，revision 为 0..9007199254740991 的整数。
- mode 为 single/pages/follow。homePageId、activePageId 必须引用 pages 中的页面。
- pages 为 1..10 项；每页 slots **必须为 18 项**，空位用 null，不压缩。所有页面 ID 唯一，所有非空按钮 ID 全局唯一。ID 匹配 [A-Za-z0-9][A-Za-z0-9._-]{0,63}。
- 页面 name 与按钮 name 为 1..120 字符；application 可空，最长 260 字符，以不区分大小写的可执行文件名匹配。
- 按钮 kind 为 program/website/file/folder/shortcut/system/command。icon 为空或 64 位小写十六进制 SHA-256。
- appearance 的 background 为 #RRGGBB；showLabels/showVolume 为布尔；swipeDirection 为 horizontal/vertical。
- idle 的 enabled 为布尔；timeoutMs 为 1000..86400000 的整数毫秒；dialId 为 digital/analog/weather/info/car；city 最长 120，message 最长 240 字符。以上范围为本 fork 的明确设计限制，不声称参考软件相同。
- 配置 UTF-8 编码不超过 256 KiB，JSON 容器嵌套不超过 64 层（包括未知字段）。不支持的版本、类型错误、重复 ID 或越界配置整体拒绝，不能替换上个有效配置。
- 桌面私有文件可在按钮中包含 target 对象（路径、网址、快捷键或既有命令引用）。发送配置由白名单投影生成，绝不向手机下发 target 或未知私有字段。动作目标另由执行器按 kind 验证。
- 新增未知字段允许兼容读取，但不会未经白名单投影重新发布。必需字段不能缺失。

## 数据包

所有请求及响应含 protocolVersion: 1；未配对设备不加载本插件。

| 类型 | 主体字段 | 用途 |
| --- | --- | --- |
| kdeconnect.controlpanel.request | operation=config | 连接建立后请求完整配置 |
| kdeconnect.controlpanel.config | config | 完整显示配置 |
| kdeconnect.controlpanel.request | operation=action, requestId, revision, buttonId | 请求本地已配置动作 |
| kdeconnect.controlpanel.result | requestId, success, error | 结果；error 为稳定机器码 |
| kdeconnect.controlpanel.request | operation=page, revision, pageId | 多页手动选择 |
| kdeconnect.controlpanel.state | revision, activePageId, foregroundApplication | 活动页面；不改变配置 revision |
| kdeconnect.controlpanel.request | operation=asset, assetId | 获取配置引用图标 |
| kdeconnect.controlpanel.asset | assetId, mime, size + payload | 限量 PNG 内容，接收端校验 SHA-256 |
| kdeconnect.controlpanel.dial | city, observedAt, fetchedAt, temperature, weatherCode, status | 天气快照；不得以伪造数据代替网络失败 |

请求 ID 使用 UUID；同一设备会话中重复请求复用已记录结果，不重复执行。revision 不匹配返回 stale_revision 并触发配置重取。错误码至少含 invalid_request、unsupported_version、stale_revision、unknown_button、invalid_target、launch_failed、permission_denied、offline。

网络执行消息不接受可执行路径、shell 命令或键码覆盖。执行器只解析桌面私有 target。断线不缓存待执行点击；重连同步完成前按钮不可用。更旧配置/状态不覆盖更新状态。若配置文件恢复为更低 revision，桌面存储层应分配新 revision。
