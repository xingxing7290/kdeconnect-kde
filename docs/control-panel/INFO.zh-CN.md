# 信息表盘：左右职责与右侧实时信息区

更新：2026-11-15。本文说明「表盘选择 → 信息表盘」中左右两侧的职责划分、右侧用户备注与 WorkBuddy 工作进度的展示方式、以及端到端刷新机制。线上字段格式以 `PROTOCOL.zh-CN.md` 为准。

## 1. 左右两侧的职责划分

信息表盘是一个「左右分栏 + 跨整宽页脚」的全屏表盘。桌面端 `app/qml/PanelInformationDial.qml` 是预览，手机端 `src/main/java/org/kde/kdeconnect/plugins/controlpanel/PanelInformationDial.kt` 是实际渲染，两者采用相同的分区规则。

| 区域 | 职责 | 内容 | 数据来源 | 是否可配置 |
| --- | --- | --- | --- | --- |
| **左侧** | 设备与环境状态 | 手机本机电量、主时钟与秒、当前天气（图标、温度、状态、今日温度区间） | 本机 `ACTION_BATTERY_CHANGED` 广播、本机时钟、`kdeconnect.controlpanel.dial` 天气快照 | **否**。布局、内容、权重、字体与控件标识全部保持原样，不读取 `idle.info` |
| **右侧** | 用户与工作上下文 | 用户自定义备注（最多 4 条）、WorkBuddy 实时工作进度（可选）、三日预报（可隐藏） | `idle.info` 配置（备注、开关、过期阈值）+ `kdeconnect.controlpanel.progress` 快照 + 天气快照的 forecast | **是**，由桌面「表盘设置 → 配置信息表盘右侧」编辑 |
| **页脚（跨整宽）** | 时间与来源说明 | 日期、解析后的城市、Open-Meteo 来源、模型/获取时间 | 本机时钟 + 天气快照 | 否 |

这样划分的原因：左侧是「这台设备现在怎么样」，任何时刻都必须稳定、可预期、不随用户编辑变化；右侧是「用户想看到什么、正在做什么」，属于可编排的信息展示区，允许配置与高频更新。

**兼容性**：旧版配置不含 `idle.info` 时，右侧自动回退为「单条 `idle.message` 备注 + 三日预报」，不显示工作进度，布局权重与旧版完全一致（0.28 / 0.72）。因此旧桌面端、旧配置文件、既有布局与可访问性测试都不受影响；`showForecast` 默认为 true，`panel-forecast-0..2` / `panelInfoForecast-0..2` 等控件标识继续存在。

## 2. 右侧信息的展示方式

右侧是一列可配置的卡片，权重按启用的模块动态分配；默认组合严格保持历史比例。

| 模块 | 展示内容 | 说明 |
| --- | --- | --- |
| 备注卡 | `info.notes` 逐条显示，每条最多 120 字符、最多 4 条，纯文本，自动换行、超出省略 | 列表为空时回退到 `idle.message`，再为空显示提示文案。备注属于**配置**，随 `panel.json` 修订同步，不走实时通道 |
| 进度卡 | 标题 `title`、阶段 `stage`、补充 `detail`、`percent` 进度条、状态文案、「N 秒/分钟前更新」 | 仅 `showProgress=true` 时出现。百分比缺省时只显示阶段文字，不臆造数值；`unavailable` 或未收到包时显示「等待 WorkBuddy 进度」 |
| 预报卡 | 原有三日预报行 | `showForecast=false` 可隐藏，把空间留给备注与进度 |

进度卡有四种明确状态，绝不伪造数据：

- **新鲜**：`connection == READY` 且 `now - fetchedAt` 在 `staleMs` 内 → 显示完整标题、阶段、进度条与相对更新时间。
- **已过期**：超过 `staleMs` → 保留最后一次标题/阶段/数值，状态改为「进度已过期，等待更新」，时间戳继续走。
- **离线**：连接断开 → 标注「离线 · 最后一次进度」，内容仍保留但不声称实时。
- **不可用**：源缺失、格式错误或 HTTP 失败 → 状态 `unavailable`，卡片显示等待文案，不保留伪造的旧百分比。

紧凑高度（小于 200dp 或字体放大）时启用紧凑布局：保留原有四列结构与控件标识，备注只取第一条，进度压缩成单行摘要（阶段 + 百分比 + 更新时间），不放大行高、不裁剪关键数值。

## 3. 连接 WorkBuddy 的进度源

桌面端 `idle.info` 的私有字段（**只存在于桌面 `panel.json`，公共投影不下发手机**）：

| 字段 | 取值 | 默认 |
| --- | --- | --- |
| `source` | `none` / `file` / `url` | `none` |
| `path` | 本地 JSON 文件绝对路径，最长 4096 字符 | 空 |
| `url` | HTTP(S) 地址，最长 2048 字符 | 空 |
| `intervalMs` | 1000..60000 轮询间隔 | 2000 |

WorkBuddy 侧通过自动化任务、脚本或本地 HTTP 端点把当前工作状态写入该 JSON；桌面端**只读取、不执行其中任何内容**。进度文件示例（UTF-8，不超过 64 KiB）：

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

`status` ∈ running / idle / done / error / paused；`title`、`stage` ≤120 字符，`detail` ≤240 字符且不含控制字符；`percent` 为可选的 0..100 整数；`updatedAt` 为可选 Unix 秒，缺省时用文件修改时间。`source` 固定为 `workbuddy`，由桌面端添加，不接受来源端伪造。

配置入口：桌面「表盘选择 → 表盘设置 → 配置信息表盘右侧：备注与 WorkBuddy 工作进度」。备注每行一条；进度源类型、路径/URL、轮询间隔与过期阈值都在这里编辑。桌面预览会立即显示备注与进度卡片，保存后配置通过 `kdeconnect.controlpanel.config` 同步到手机。

## 4. 刷新机制（为什么实时性较高）

1. **桌面采集**：`PanelProgress`（`controlpanel/panelprogress.cpp`）用 `QFileSystemWatcher` 监听进度文件，WorkBuddy 一写入即在毫秒级触发读取；`QTimer` 按 `intervalMs` 兜底轮询，HTTP 源按同一间隔 GET（3 秒超时、拒绝重定向、64 KiB 上限）。文件不存在、JSON 非法或请求失败时生成 `status=unavailable` 并按间隔重试。
2. **变化才推 + 独立心跳**：桌面把内容字段（status/title/stage/detail/percent/updatedAt）算成签名。**内容变化立即推送**（文件监控触发）；内容不变时由一条与 `intervalMs` 解耦的 2 秒定时器重发上次快照，只刷新 `fetchedAt` 并递增 `sequence`，不重新读取源文件。因此即使用户把轮询间隔设到 60 秒，手机也不会把健康链路误判为过期，同时不会产生按轮询频率重复读取慢源的问题。
3. **传输**：`kdeconnect.controlpanel.progress` 是无附件的轻量 JSON 包，含 `protocolVersion=1`、当前 `revision`、可选 `sessionId`。与天气一致，CONFIG 成功进入发送队列后重推一次，避免手机因未来 revision 拒绝而丢失状态。此包**不写 `panel.json`、不递增修订号、不触发配置广播**；仅配对且可达时发送。
4. **手机接收**：16 KiB / 64 层限制、字段线类型与范围校验、`status` 白名单、`sequence` 不倒退、`revision` 必须匹配当前配置、连接必须 READY。非法包直接忽略，不影响动作会话或按钮可用性；进度不做跨进程持久化（过期进度没有离线价值）。
5. **手机重绘**：表盘本来就有每秒刷新的时钟，因此相对更新时间每秒重算，即使没有新包也能立刻看出陈旧。`staleMs`（默认 15000 毫秒，可配置 5000..120000）内为新鲜；超时或断线保留最后一次快照并明确标注。快照放在 `PanelSessionState.progress`，Compose 收到新 State 即重组，无额外轮询。

配置与实时状态严格分离：备注、开关、阈值走带修订号的 `panel.json`；进度数值走独立 packet。桌面私有路径/URL 不下发手机，手机也不能反向控制桌面读取哪个源。

## 5. 验证边界

本次按项目约定只做源码修改与静态检查，**未编译、未运行、未生成程序**。静态检查覆盖：协议文档与双端样本一致、Kotlin/C++/QML 括号与 XML 结构、资源引用、objectName/testTag 兼容性、`QT_NO_KEYWORDS` / `QT_NO_CAST_FROM_ASCII` 规范。仍需真机/构建后验证：Qt `QFileSystemWatcher` 在 Windows 上对原子写入的事件可靠性、WorkBuddy 写入频率与 `sequence` 单调性、桌面预览与手机端在深色主题下的进度条观感。
