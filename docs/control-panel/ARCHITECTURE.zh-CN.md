# KDE 控制台技术方案

状态：设计基线，尚未实现。更新：2026-10-28。

## 现有代码与差距

Android 使用 Kotlin/Java、Compose、Gradle；已有 RunCommandPlugin、SystemVolumePlugin、设备配对和加密网络。用户已有方形网格、排序、favicon、全屏定制，但当前数据只有 name/command/key，没有固定空槽、多页、应用绑定或表盘。

桌面使用 C++20、Qt 6.7+、KDE Frameworks 6.3+、QML/Kirigami、CMake。已有 models/commandsmodel、plugins/runcommand、plugins/systemvolume、plugins/mousepad，以及 app/qml/Main.qml。命令插件以电脑预配置 key 执行，系统音量已有 Windows Core Audio 实现，Windows 键盘输入已有平台代码可复用。

以现有 KDE 设备和插件机制新增控制面板能力。业务 UI 使用两端原生技术，协议和状态模型保持一致。

## 模块划分

| 层 | Android | Desktop |
| --- | --- | --- |
| 入口与连接 | 新控制台入口，复用设备列表/配对，保留旧功能页 | 主窗口控制台导航，复用 DevicesModel 与现有连接页 |
| 面板模型 | plugins/controlpanel 中纯 Kotlin 配置/验证/状态模型 | 共享 panel 模型、验证和持久化组件，供 UI 与插件复用 |
| 协议 | ControlPanelPlugin 接收配置/状态/资源、发送已配置按钮 ID | plugins/controlpanel 发布配置，校验请求并调用动作执行层 |
| 编辑器 | 展示桌面权威配置，允许本地显示偏好 | QML 6 x 3 预览、槽位编辑、拖入、快捷库、页与选项编辑 |
| 动作 | 仅发已配置按钮 ID | 类型化程序/路径/URL/快捷键/系统动作执行 |
| 音量 | 复用 SystemVolumePlugin | 复用 systemvolume 平台实现 |
| 表盘 | Compose 自绘时钟/天气/信息样式 | QML 同款预览、选择、参数编辑 |
| 智能跟随 | 接收活动页面并更新 | Windows 前台窗口/进程识别，去抖并映射页面 |
| 测试 | JVM/Robolectric、Compose/设备测试 | Qt Test、QML 测试、Windows 原生动作测试、两端联调 |

## 配置模型

- schemaVersion：可演进的配置版本；拒绝不支持的主版本。
- revision：每次成功保存后递增，用于同步及拒绝过期点击。
- mode：single、pages、follow；activePageId 与 homePageId 必须引用现存页面。
- pages：1..10 页，稳定 UUID、名称、可选程序匹配标识、18 个固定槽位。
- buttons：稳定 ID、类型、名称、图标资源 ID；本地私有目标由电脑保存，Android 只获取展示与触发信息。
- appearance：背景色、名称显示、音量显示、滑动方向。
- idle：启用、idleTimeoutMs、dialId、城市与自定义文本等参数。
- assets：内容哈希、mime、大小；原始 exe/文件路径不当作资源 URL 下发。
- 配置采用校验后原子写入，保留上个有效版本；UI 提供验证错误而不覆写有效配置。
- 电脑为单一配置写入端，支持多个已配对控制端。组件间复用明确的配置服务/文件监听机制，避免 GUI 与 daemon 各存一份不一致数据。

## 协议与执行规则

继续通过 KDE Connect NetworkPacket 和已配对 TLS 链路。新增明确命名的面板包，不改变旧 runcommand 包含义。

| 类别 | 内容 |
| --- | --- |
| config.request / config | 能力与版本协商、完整显示配置及 revision |
| action.request / action.result | requestId、buttonId、revision；执行结果、错误码 |
| state | 活动页、前台应用标识、连接及同步状态 |
| asset.request / asset | 按内容哈希请求图标；限制大小、验证 mime/摘要、缓存 |
| dial.data | 时间相关配置、天气快照、更新时间及错误状态 |

- 接收入口验证配对状态、版本、数据类型、长度、页数和资源大小。
- 执行端按 buttonId 查找本地记录，不能把网络请求携带的路径或 shell 文本直接执行。
- 快捷键使用结构化键码；所有退出路径释放修饰键。
- 应用/文件/文件夹/网页通过平台原生打开方式；shell 命令仍由既有显式配置的 Run Command 功能处理。
- 请求 ID 去重，避免重传造成重复打开；断线点击不会排队到重连后执行。
- 手机重连先请求最新配置，完成后再启用按钮；旧修订点击返回 stale_revision。
- Windows 运行在普通用户会话；不能控制提权窗口时明确反馈，不自动提升权限。
- 图片资源由桌面导入后压缩规范化再传输；路径、URL、JSON和图片均有边界检查。

## 界面与交互

桌面控制台主体围绕表盘、桌面控制、连接、关于组织。旧设备功能仍可进入。控制页面内按钮预览、页码和右侧库同步显示编辑结果；所有字段有保存/取消语义，拖放保存可撤回到上次有效配置。

Android 使用横屏沉浸式控制面板，保持屏幕常亮，尊重系统返回和退出入口。进入表盘后首次触摸只唤醒，第二次交互才能执行。离线提示固定可见。旋转、后台恢复和进程重建都恢复合法状态。

表盘至少涵盖参考图中的数字、模拟、模拟+天气、信息和汽车视觉类型。自行绘制图形与排版，不直接打包参考程序图片。城市文本由桌面设置；天气供应适配层隔离网络源，缓存和离线提示独立测试。

智能跟随根据前台进程而不是窗口标题做主要匹配，忽略编辑器自身和短暂弹窗，去抖后广播活动页。电脑识别弹窗显示可复制程序名称。首页作为无匹配回退；页面显示名与程序绑定分开存储以允许重命名。

## 兼容及迁移

- 原 KDE Connect 插件继续工作；对方缺少面板能力时不显示可执行的假面板。
- 原远程命令可显式导入为面板按钮，不删除或重写原配置。
- Android 原 runcommand 排序键保留；新的面板顺序来自电脑配置，避免两个排序来源互相覆盖。
- 迁移、加载损坏配置、未知字段、未来 schema 版本均有专门测试。
- Windows 是本次桌面实测与交付平台；保持其他平台编译隔离，不能用 Windows 验证声称其他平台已通过。

## 构建与测试策略

先记录两端当前基线。Android 使用仓库 Gradle Wrapper、SDK 37；设备测试用独立 debug applicationId，已有连接设备可供后续联调。桌面使用官方 Craft 方式获得 Qt/KF/DBus 依赖，再用 CMake/CTest 和 Windows 打包流程。

每条长任务拆分成下载、配置、构建、单测、打包、联调阶段；每阶段输出日志，10 分钟无明确进展时停止并记录。测试失败不得替换成更小范围检查后声称全部通过。
