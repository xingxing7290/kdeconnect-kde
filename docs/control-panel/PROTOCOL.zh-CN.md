# 控制面板协议 v1

此协议随个人 fork 发布，传输复用 KDE Connect 的已配对连接。配置、动作、结果和页状态已接入两端插件，并完成真实手机/Windows 文件按钮联调；资源、天气和智能跟随仍按 PLAN 继续实现。

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
| kdeconnect.controlpanel.request | operation=config, sessionId | 建立会话并请求完整配置 |
| kdeconnect.controlpanel.config | config | 完整显示配置 |
| kdeconnect.controlpanel.request | operation=action, sessionId, sequence, requestId, revision, buttonId | 请求本地已配置动作 |
| kdeconnect.controlpanel.result | requestId, success, error | 结果；error 为稳定机器码 |
| kdeconnect.controlpanel.request | operation=page, sessionId, revision, pageId | 多页手动选择 |
| kdeconnect.controlpanel.state | revision, activePageId, foregroundApplication | 活动页面；不改变配置 revision |
| kdeconnect.controlpanel.request | operation=asset, assetId | 获取配置引用图标 |
| kdeconnect.controlpanel.asset | assetId, mime, size + payload | 限量 PNG 内容，接收端校验 SHA-256 |
| kdeconnect.controlpanel.dial | city, observedAt, fetchedAt, temperature, weatherCode, status | 天气快照；不得以伪造数据代替网络失败 |

sessionId、requestId 使用小写 UUID。Android 每次重新建立插件会话生成新的 sessionId，配置重取沿用当前 sessionId；sequence 从 1 单调递增，最大为安全 JSON 整数上限。同一会话最近 256 个动作请求保留原始请求和结果：相同请求返回原结果，ID 冲突返回 invalid_request。结果缓存淘汰后仍保留最高 sequence，更早或乱序请求返回 request_expired，不能重新执行。每个连接最多记录 4096 个已退休会话 ID，退休会话不能通过重复握手复活。

动作请求最大 4096 字节，按 operation 严格白名单检查字段。revision 不匹配返回 stale_revision 并发送最新完整配置。错误码包含 invalid_request、unsupported_version、invalid_session、request_expired、stale_revision、unknown_button、invalid_target、target_missing、launch_failed、permission_denied、config_unavailable、offline。

网络执行消息不接受可执行路径、shell 命令或键码覆盖。执行器只解析桌面私有 target。断线不缓存待执行点击；重连同步完成前按钮不可用。更旧配置/状态不覆盖更新状态。若配置文件恢复为更低 revision，桌面存储层应分配新 revision。

## 桌面私有动作目标

- program：path 为存在的绝对 exe/lnk 路径；可选 arguments 为最多 64 个字符串，workingDirectory 为存在的绝对目录。lnk 不接受额外参数或工作目录覆盖。
- website：url 为完整 HTTP(S) 地址，保留路径、查询和片段。编辑界面可给裸域名补 https://。
- file/folder：path 为存在的绝对文件或目录，以系统关联方式打开。
- shortcut：keys 为依次排列的修饰键和一个普通键，例如 CTRL、SHIFT、S。键名白名单包括 CTRL/ALT/SHIFT/WIN、A..Z、0..9、F1..F24 和常用控制/媒体键。重复键、未知键、只有修饰键或顺序错误整体拒绝。发送失败时尝试释放全部键；物理修饰键已按下时拒绝执行。
- system：action 为固定库中的标识，如 thisPc、recycleBin、taskManager、commandPrompt、settings、networkSettings、personalization、displaySettings、showDesktop、screenshot；不会执行传入的任意命令。
- command：key 引用该设备既有 Run Command 插件中的已配置命令；输出和停止继续使用原插件。

动作 success 表示已提交给操作系统或既有命令插件，不表示目标程序已经退出或完成其业务任务。Windows 快捷键和系统动作通过平台 API 执行；其他平台返回明确的不支持错误。
