# 阶段八：按钮编辑、页面预览与拖入安全

日期：2026-11-12。个人 fork 的阶段进展，不代表完整参考复刻、所有动作验收或正式发行完成。

## 实现与使用

本轮对应 BTN-01..06、LIB-01、PAGE-01..02 的编辑流程，修改仅限桌面 controlpanel 编辑器、其 QML/测试与双端说明。Android 运行代码、共享网络和原 KDE 插件没有在本轮改变。

- 七种按钮仍为程序、网站、文件、文件夹、快捷键、系统功能、KDE 远程命令。程序按钮新增可展开的启动参数和工作目录。参数采用 JSON 字符串数组，例如 `["--profile","work"]`；不是 shell 命令行，不重新拆分引号。折叠高级选项不会清空已有值，重命名保留参数、目录和按钮 ID。
- 程序/文件路径可粘贴带一对外层双引号的路径。文件夹自动名称保留点号，隐藏文件名不回退成私人绝对路径；网站自动名称去掉 HTTP(S) 协议和用户名/密码，不改私有 URL 中的路径、查询及片段。
- 按钮表单打开时记录页面、配置版本、槽位和按钮 ID。保存/删除前检查上下文，改变页面或配置后拒绝旧表单，显示内联提示。磁盘上的另一编辑器更改仍由已有 PanelStore 修订锁保护。
- 表单可滚动，底部保存/取消/删除不随内容滚走；小窗口也可退出。保存按钮使用蓝色强调，名称/错误按纯文本显示。
- 单页模式显示首页并隐藏页码/新增/删除页入口；其他页面数据保留。多页模式恢复这些入口，显式启用悬浮切页。编辑器配置通知与当前预览页通知分离，切页不再重新绑定整份配置数组。
- 文件拖入只接受单个本地目标，并且拖动源必须支持 Copy；即使提出 Move 也只接受 Copy，创建引用，不移动或删除源文件。多个目标整次拒绝，不静默丢弃其余项。保存失败不接受拖放。
- 内部按钮拖动记录开始时的上下文，只在当前页面和版本仍匹配时换位；无效、过期和损坏的 JSON 不当成槽位 0。快捷库使用复制，新按钮具有独立 ID。

程序 .lnk 自带的参数/工作目录应在快捷方式属性中设置；原后端不接受在其外再追加参数。本轮没有修改该规则，也没有执行用户配置的程序或快捷键。

## 当前验证证据

证据根目录 D:/KDEBuild，源码 E:/git；未向当前华为手机安装。

| 检查 | 结果 | 原始证据 |
| --- | --- | --- |
| Windows 完整构建 | 成功 | logs/desktop-button-build9.log |
| 全量 CTest | 21/21 套通过，32.94 秒 | logs/desktop-button-fullctest2.log |
| 新生产 QML 交互 | 16 个功能用例及初始化/清理共 18 项通过，无跳过；警告使测试失败 | logs/desktop-button-ui7.txt；tests/panelbuttonuitest.cpp |
| 短期重复性 | 新 UI 套件连续 3 轮通过，12.85 秒 | logs/desktop-button-stability1.log |
| 编辑器后端 | 上下文类型/版本/页面/槽位/ID、第二编辑器磁盘修订、换位、单页保留页面、隐藏文件名测试通过 | tests/paneleditortest.cpp；完整 CTest |
| 图像 | 已查看程序高级选项、小窗口和多页画面；加载生产 QML、Breeze 图标与 14px 中文字体 | logs/button-editor-render-final/{program-advanced,small-window,multiple-pages}.png |
| 内部 Windows 候选包 | 149 个非系统运行库；5 项生命周期检查通过 | logs/windows-buttons-package1.log、windows-buttons-lifecycle2.log；package-lifecycle-buttons-r2/result.json |
| Android 本轮专项 | RunCommandPluginTest 6/6，51 秒任务完成；固定模拟 deviceId 已在旧提交中，本轮未重复改动 | logs/android-runcommand-device-id-recheck-20261112.log |

七种按钮用例操作实际生产表单和保存按钮，核对私有配置及重启读取。输入文字/类型通过控件属性注入，点击通过 Qt 窗口事件；拖入通过真实 QDragEnter/QDragMove/QDrop 事件送到生产 DropArea，并核对接受动作和源文件字节未变。这不等同于 Windows Explorer/OLE 原生拖动、系统文件选择器或真实动作执行，也不把模拟音频设备当成硬件回归。

首轮构建存在新测试的类型/include/CMake 导入作用域错误；界面测试也曾因缺应用图标、延迟销毁、配置值容器类型和未完成布局时点击坐标失败。补齐生产资源、先销毁 QML、等待实际布局并按 JSON 类型建夹具后复跑。另显式启用页码悬浮。失败日志 build1/2/5/8、ui1/3/4/5、fullctest1 保留，不把失败轮次算通过。测试仍兼容较早 Qt 的拖动事件构造器，当前 Qt 6.12 编译报告两项该构造器的弃用警告；不影响运行检查，未全局屏蔽。

候选包生命周期首轮因调用环境 TEMP 仍为 C 盘，Add-Type 在启动应用前报磁盘不足；设置 TEMP/TMP=D:/KDEBuild/temp 后在新证据目录重跑成功。未删除用户文件；所有包与失败记录保留。

## 候选产物

目录：D:/KDEBuild/packages/controlpanel-buttons-win64-20261112-r1，入口 Start-ControlPanel.cmd。仅内部候选包，不是最终安装器或完整发行物。

kdeconnect-app.exe SHA256：`E3A3F31699F118025260262D26EDB803D60F2A43904A70D7E34C539BB00F7E26`。

本轮不生成新的 Android APK。上一信息表盘 APK 和全量 200 项单测证据仍见 MILESTONE-07-INFO.zh-CN.md；没有用本轮 6 项专项替代 Android 全量回归。

## 下一阶段与边界

- 用户已授权：逐项核查后最小修复清单服务、短信缩略图、输入设备、SFTP 文件属性和默认偏好资源中的旧 Android 兼容风险，保留现有功能；TV 与全语言翻译另记。
- 用户已授权：仅测试配置增加匹配现有 Compose 版本的官方 ui-test-junit4，不进入 APK。触摸、缩放及无障碍回归尚待补完。
- 当前 Lint 最近完整报告仍为 486 errors、212 warnings、1 hint，尚未因新授权自动视为修复。
- 后续还需原生 .lnk/资源管理器/系统文件选择器、所有动作与前台快捷键、页面设置/删除弹窗的并发更新、全视觉与高 DPI、Android 触摸/生命周期、配对/断网/多设备、长期运行、KDE 原功能、干净 Windows 普通账户、最终签名/安装/卸载和许可证对应源码审计。
- 所有原验收清单保持未勾选。本轮 21 个本地套件和 5 项包检查不能证明完整目标完成。
