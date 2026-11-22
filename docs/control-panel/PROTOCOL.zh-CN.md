# 控制面板协议 v1

此协议随个人 fork 发布，传输复用 KDE Connect 的已配对连接。配置、动作、结果、页状态、图标和天气快照已接入双端；跟随使用 state 包，音量复用 System Volume 包。阶段证据见 MILESTONE-03.zh-CN.md 至 MILESTONE-06.zh-CN.md；资源及天气限制分别见 ASSETS.zh-CN.md、DIALS.zh-CN.md。

## 配置

`kdeconnect.controlpanel.config` 的 `config` 字段为 JSON 对象。测试样本：本目录 fixtures/config-v1.json。Android 和 C++ 测试读取相同内容，修改协议时必须同时更新两端及样本。

- schemaVersion 必须为整数 1，revision 为 0..9007199254740991 的整数。
- mode 为 single/pages/follow。homePageId、activePageId 必须引用 pages 中的页面。
- pages 为 1..10 项；每页 slots **必须为 18 项**，空位用 null，不压缩。所有页面 ID 唯一，所有非空按钮 ID 全局唯一。ID 匹配 [A-Za-z0-9][A-Za-z0-9._-]{0,63}。
- 页面 name 与按钮 name 为 1..120 字符；application 可空，最长 260 字符，以不区分大小写的可执行文件名匹配。
- 电脑公共投影的 application 只保留小写程序文件名，不传完整目录。state 的 foregroundApplication 为空或最长 260 字符的程序文件名，Android 拒绝目录分隔符和控制字符。跟随只改变 state，不改变配置 revision；手动选择保持到下一次稳定程序变化，详细优先级见 FOLLOW.zh-CN.md。
- 按钮 kind 为 program/website/file/folder/shortcut/system/command。icon 为空或 64 位小写十六进制 SHA-256。
- appearance 的 background 为 #RRGGBB；showLabels/showVolume 为布尔；swipeDirection 为 horizontal/vertical。
- idle 的 enabled 为布尔；timeoutMs 为 1000..86400000 的整数毫秒；dialId 为 digital/analog/weather/info/car；city 最长 120，message 最长 240 字符。以上范围为本 fork 的明确设计限制，不声称参考软件相同。
- idle.info 为「信息表盘右侧信息区」的可选配置对象，缺省时按旧行为渲染（单条 message 备注 + 三日预报，不显示工作进度）。公共投影只下发 notes、showForecast、showProgress、staleMs 四键：notes 为最多 4 条、每条最长 120 字符的字符串数组；showForecast/showProgress 为布尔；staleMs 为 5000..120000 的整数毫秒。桌面私有的进度源字段（source、path、url、intervalMs）**绝不投影到手机**，详见下文「信息表盘右侧信息区」。
- 配置 UTF-8 编码不超过 256 KiB，JSON 容器嵌套不超过 64 层（包括未知字段）。不支持的版本、类型错误、重复 ID 或越界配置整体拒绝，不能替换上个有效配置。
- 桌面私有文件可在按钮中包含 target 对象（路径、网址、快捷键或既有命令引用）。发送配置由白名单投影生成，绝不向手机下发 target 或未知私有字段。动作目标另由执行器按 kind 验证。
- 新增未知字段允许兼容读取，但不会未经白名单投影重新发布。必需字段不能缺失。

## 数据包

所有请求及响应含 protocolVersion: 1；未配对设备不加载本插件。

阶段十五为配置、状态、结果和天气响应追加可选的 sessionId 包装字段，配置对象/固定样本及请求字段不变。握手后响应标记目标控制器的当前会话；握手前主动发布可省略标记。Android 始终拒绝显式但不匹配的标记；首次接受匹配标记的有效配置后，本插件会话内也拒绝省略标记的上述响应，避免降回未绑定模式。图标的 sessionId 仍是原有必填字段。

兼容旧服务端时可继续接收没有该字段的响应，但不能声称具有同等的迟到响应隔离；此路径仍依赖配对连接、revision、requestId 和断线不补发保护。sessionId 不替代 TLS/配对身份，不是授权凭证，不写入展示缓存。运行中降级为旧服务端应重新建立插件连接，不自动放宽已绑定会话。

| 类型 | 主体字段 | 用途 |
| --- | --- | --- |
| kdeconnect.controlpanel.request | operation=config, sessionId | 建立会话并请求完整配置 |
| kdeconnect.controlpanel.config | config | 完整显示配置 |
| kdeconnect.controlpanel.request | operation=action, sessionId, sequence, requestId, revision, buttonId | 请求本地已配置动作 |
| kdeconnect.controlpanel.result | requestId, success, error | 结果；error 为稳定机器码 |
| kdeconnect.controlpanel.request | operation=page, sessionId, revision, pageId | 多页手动选择 |
| kdeconnect.controlpanel.state | revision, activePageId, foregroundApplication | 活动页面；不改变配置 revision |
| kdeconnect.controlpanel.request | operation=asset, sessionId, revision, assetId | 获取当前配置引用图标 |
| kdeconnect.controlpanel.asset | sessionId, revision, assetId, mime, size, error + payload | 256 x 256 PNG，接收端校验哈希与实际解码；错误响应无附件 |
| kdeconnect.controlpanel.dial | revision, city, source, status；可选 resolvedCity, observedAt, fetchedAt, temperature, weatherCode | 当前配置的模型天气快照；不得以伪造数据代替网络失败 |
| kdeconnect.controlpanel.progress | revision, source, status, title, stage, detail；可选 percent, sequence, updatedAt, fetchedAt | WorkBuddy 工作进度快照；实时状态，不写配置、不改 revision |
| kdeconnect.controlpanel.hass | revision, status, error, fetchedAt, entities[]；可选 sessionId, lastCommand | 智能家居快照；状态变化即时推送，200 毫秒内的连续变化合并为一包；不含地址与令牌 |
| kdeconnect.controlpanel.hass_command | protocolVersion=1, sessionId, entityId, action；可选 brightness | 请求桌面端调用 HA 服务；会话不匹配或已退休时丢弃，不执行 |

智能家居快照的共用样本：`fixtures/hass-snapshot.json`，两端测试读取同一内容。`error` 与 `lastCommand.error` 只使用稳定机器码（`hass_auth_invalid`、`hass_request_failed` 等），翻译成可读文案是 UI 的职责；未知码由 UI 原样显示。

sessionId、requestId 使用小写 UUID。Android 每次重新建立插件会话生成新的 sessionId，配置重取沿用当前 sessionId；sequence 从 1 单调递增，最大为安全 JSON 整数上限。同一会话最近 256 个动作请求保留原始请求和结果：相同请求返回原结果，ID 冲突返回 invalid_request。结果缓存淘汰后仍保留最高 sequence，更早或乱序请求返回 request_expired，不能重新执行。每个连接最多记录 4096 个已退休会话 ID，退休会话不能通过重复握手复活。

success 必须与 error 为空一致，矛盾结果不确认待处理动作。有效动作结果中的 invalid_session 和 stale_revision 只触发当前会话重新握手/同步，保持 sequence 递增，不重发原动作；握手本身被拒绝时进入可见错误，不递归无限重试。过期请求提示“未重新执行”，不能理解为原操作从未发生。手动同步或配置变化清掉过期动作反馈。

多控制端使用同一个桌面运行时当前页，手动选择和智能跟随广播给各控制器；各设备的插件/PanelPeer 独立维护会话、序号、结果缓存和既有远程命令配置。相同 requestId 出现在不同设备时不共用执行结果。原生执行若重入并建立新会话，旧结果仍标记原会话，不写入新会话的去重缓存。

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

## 天气快照

桌面端每次完整配置成功进入发送队列后重推当前天气快照，包括配置广播与重连；天气变化仍单独推送。这样即使共享天气对象先发出新修订快照，Android 严格拒绝未来 revision 后仍会在 CONFIG 之后收到有效重发。不缓存未来包、不放宽会话/城市/revision 校验，入队成功不等于网络送达确认。生产插件回归见阶段十九，真实重连对照见 MILESTONE-20-WEATHER-E2E.zh-CN.md。

天气包无二进制附件，protocolVersion=1、revision 为当前配置修订，city 必须匹配配置的去首尾空白城市名，source 固定为 open-meteo。status 为 ready/loading/unavailable/disabled/no_city。完整数值组 resolvedCity、observedAt、fetchedAt、temperature、weatherCode 要么全部存在且合法，要么全部缺省；ready 不允许缺省。observedAt 表示模型时刻，不代表传感器实测。

时间为 Unix 秒，接收端检查整数/范围和明显未来值；温度必须是有限 JSON 数值，范围 -100..70，代码必须是支持的整数 WMO 码。UTF-8 主体不超过 16 KiB、JSON 容器不超过 64 层。修订/城市不符、乱序或类型错误不能替换有效快照；天气错误也不能把动作会话置为失败。加载或不可用消息可保留同城已知快照，但不制造新的获取时间，显示时仍检查过期。来源、缓存、过期和隐私详见 DIALS.zh-CN.md。

### 可选三日预报扩展

天气包仍为 v1；旧端忽略未知字段，新 Android 也接受不含预报的旧端快照。新增可选 forecast 数组（为空或恰好 3 项），每项为 date、minimum、maximum、weatherCode。date 必须是严格 yyyy-MM-dd，按查询城市日期连续排列；温度有限且在 -100..70、minimum <= maximum，WMO 码沿用当前天气的白名单。

非空预报必须同时携带 timezone（非空、无控制字符、最长 80）和 utcOffsetSeconds（整数，-43200..50400）。第一天必须等于 fetchedAt 加该偏移所得的城市日期；不能按电脑或手机时区重新解释。预报与当前观测一同校验、存储、保留原获取时间和过期，不单独制造新鲜度。缺省预报显示待更新，不推断温度或天气代码。跨城市、旧修订、旧观测及总体 16 KiB/64 层限制不变。

手机本机电量仅由本机系统广播读取，不加入该天气包，不把某个设备的电量混作所有控制端的公共配置。

## 音量复用包

音量继续使用 kdeconnect.systemvolume 和 kdeconnect.systemvolume.request，不混入面板 action/sessionId/revision。Windows sinkList 的 name 为稳定端点 ID，description 为友好名称，volume 为整数 0..100，maxVolume=100，muted/enabled 为布尔。没有默认输出时不自动选列表首项。

请求为 requestSinks=true，或 name 加 volume、muted、enabled。Windows 写入前完整校验字段与类型，拒绝越界和已消失目标。仅在配对且可达时接受和发布；原生状态按变化发送，包括空列表以清理旧设备。手机拖动以 80 毫秒节流并发送最终值，使用手势初始设备 ID。默认切换角色及原协议没有逐次结果确认的边界见 VOLUME.zh-CN.md。

## 信息表盘右侧信息区与工作进度

完整设计与职责边界见 INFO.zh-CN.md；本节规定线上格式。

### 左右两侧的职责划分

| 区域 | 内容 | 数据来源 | 配置影响 |
| --- | --- | --- | --- |
| 左侧（固定） | 手机本机电量、主时钟与秒、当前天气（图标、温度、状态、今日温度区间） | 本机 ACTION_BATTERY_CHANGED 广播、本机时钟、`kdeconnect.controlpanel.dial` 天气快照 | **不变**：布局、内容、字体、控件标识全部保持原样，不接受 info 配置 |
| 右侧（可配置） | 用户自定义备注、WorkBuddy 工作进度、三日预报 | `idle.info` 配置 + `kdeconnect.controlpanel.progress` 快照 + 天气快照的 forecast | 备注条数/内容、进度开关与过期阈值、是否保留预报均可配置 |
| 页脚（跨整宽，不属于左右分区） | 日期、解析城市、Open-Meteo 来源、模型/获取时间 | 本机时钟 + 天气快照 | 不变 |

旧版配置不含 `idle.info` 时，右侧按旧行为渲染：单条 `idle.message` 备注 + 三日预报，不显示工作进度；这样旧桌面端、旧配置与既有 UI/可访问性测试都保持兼容。`showForecast` 缺省为 true，因此 `panel-forecast-0..2` 等控件标识继续存在。

### 右侧信息的展示方式

- **备注卡**：`info.notes` 逐条显示（最多 4 条，每条最长 120 字符，纯文本、自动换行、超出省略）；列表为空时回退到 `idle.message`，仍为空时显示提示文案。备注属于配置，随 `panel.json` 修订同步，不参与实时推送。
- **进度卡**（仅 `showProgress=true`）：标题 `title`、阶段 `stage`、补充说明 `detail`、百分比 `percent` 的进度条、`status` 文案与「N 秒前更新」相对时间。百分比缺省时只显示阶段文本，不臆造数值。新鲜度由 `staleMs` 判定：超时显示「进度已过期，等待更新」并保留最后一次内容；连接断开时额外标注「离线」，不伪造数据。
- **预报卡**（`showForecast=false` 可隐藏）：沿用原有三日预报行与控件标识。
- 紧凑高度（小于 200dp/字体放大）时走紧凑布局：保留原有四列结构，备注合并为首条，进度压缩为单行摘要（阶段 + 百分比 + 更新时间），不放大行高、不裁剪关键数值。

### WorkBuddy 进度源与刷新机制

`idle.info` 的桌面私有字段：`source` = `none`/`file`/`url`，`path` 为本地 JSON 文件绝对路径，`url` 为 HTTP(S) 地址，`intervalMs` 为 1000..60000 的轮询间隔（默认 2000）。这四项**只存在于桌面 `panel.json`**，公共投影不发送路径或 URL。WorkBuddy 侧通过自动化任务或脚本把当前工作状态写入该 JSON（或由本地 HTTP 端点提供）；桌面端只读取、不执行其中任何内容。

进度文件示例（UTF-8，不超过 64 KiB）：

```json
{
  "status": "running",
  "title": "整理季度报告",
  "stage": "生成图表",
  "detail": "已完成 3/8 个文件",
  "percent": 38,
  "updatedAt": 1763200000
}
```

`status` 为 running/idle/done/error/paused；`title` ≤120、`stage` ≤120、`detail` ≤240 字符且不含控制字符；`percent` 为 0..100 的可选整数；`updatedAt` 为可选 Unix 秒，缺省时使用文件修改时间/读取时刻。`source` 固定为 `workbuddy`。

- **桌面端**：`QFileSystemWatcher` 监听文件，写入后毫秒级触发读取；`QTimer` 按 `intervalMs` 兜底轮询（HTTP 源按同一间隔 GET，3 秒超时、拒绝重定向、64 KiB 上限）。解析后规范化为快照，内容变化时立即发出 `changed()`，内容不变时按 2 秒心跳重推（`fetchedAt` 刷新、`sequence` 递增），既保证手机端不过期，又避免每轮轮询都发包。HTTP 失败或文件缺失时发 `status=unavailable`，按间隔重试，不保留伪造的旧进度值。
- **发送**：`kdeconnect.controlpanel.progress` 无附件，含 `protocolVersion=1`、当前 `revision`、可选 `sessionId`；仅配对且可达时发送。与天气一致，CONFIG 成功进入发送队列后重推一次当前进度，避免手机因未来 revision 拒绝而丢失状态。此包不写 `panel.json`、不递增修订号、不触发配置广播。
- **手机端**：16 KiB/64 层限制、字段线类型与范围校验、`status` 白名单、`sequence` 不倒退、`revision` 必须匹配当前配置、连接必须 READY；非法包直接忽略，不影响动作会话或按钮可用性。表盘已有每秒刷新的时钟，因此相对更新时间每秒重算，即使没有新包也能立即看出陈旧。新鲜度只看 `fetchedAt`（链路是否还在持续收到心跳）；`updatedAt` 是 WorkBuddy 产生数据的时间，仅用于「N 秒前更新」文案，固定不变也不会把正常链路误判为过期。`staleMs`（默认 15000 毫秒）内为新鲜；超时或断线保留最后一次快照并明确标注，不做跨进程持久化（过期进度没有离线价值）。

配置与实时状态严格分离：备注与开关走带修订号的 `panel.json`，进度数值走独立 packet。桌面私有源地址、轮询间隔和过期阈值中的私有部分不会出现在发给手机的配置里。
