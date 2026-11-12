# 阶段十七：当前包双端联调与大字体裁切修复

日期：2026-11-12。继续统一验证和修复复测，不缩减完整目标，不将隔离通道联调当作普通局域网或实体手机验收。

## 环境与运行基线

- Windows 运行包仍为 controlpanel-unified16-win64-r1，app SHA256 为 306AAAA657B9A6CBF1F2E1BB50F4A4169DF18175DCE18ADB617AD67E466F0704；运行源码基线 e1153b466。
- 前半段使用阶段十六 APK（A68B22BE 开头）；发现大字体问题后只修 Android 控制面板布局，完整测试并输出新 APK，再复跑横向十页、在线动作和离线新进程恢复。两个阶段的日志分开保留，不能将前半段全部结果直接归到新 APK。
- 继续使用 D 盘专用 Android 34 AVD KDEPanel34_E2E_20261112、emulator-5580、独立 ADB 5038。新建独立 desktop-profile，重新走生产 TLS 配对，未复用日常身份。
- 测试 peer 只启用 controlpanel，其他插件仅在此隔离配置禁用。没有向华为手机安装、操作日常应用、抢占主机焦点或写入主机音频。
- 通过 ADB reverse 进行定向发现与附件连接；普通 LAN 发现、跨电脑网络、实体设备和多控制端不因此通过。

## 真实双端证据

1. 原生 PanelEditor 生成十页、每页十八槽位，revision 37；第十一页被拒绝且配置不变。切为横向后 revision 38，通过原文件监听和插件同步，不直接改 Android 缓存制造界面。
2. 首页自定义 PNG、此电脑和回收站默认 PNG 经生产附件传输，Android 缓存 SHA256 与桌面文件、公共 icon ID 三者一致，实际截图显示不同图标。
3. Android 公共缓存有十页、十八槽位、背景 #DDEBFF、隐藏音量栏和切页方向，不包含 target、arguments 或 workingDirectory。
4. 阶段十六 APK 纵向从首页到第十页以及上界额外滑动通过；横向到第十页再返回首页及首尾额外滑动共二十步通过。滑动没有执行探针。
5. 第十页一次触摸只写一条专用探针记录，中文/空格参数与中文/空格工作目录匹配。停止独立桌面后离线触摸不执行；正常重启保留配对，重新同步不补发，新的在线触摸才增加一条。
6. 在桌面已停止后，以应用自己的 UID 结束 Android 进程，从最近任务恢复新进程。离线面板、十页配置和图标仍可显示，离线触摸无执行。
7. 修复后的新 APK 再完成二十步横向往返及首尾边界，探针总数保持二；随后一次新的在线点击增加到三。停止桌面并再次结束 Android 进程，PID 从 8127 变为 9917，生产 Activity 恢复离线缓存，离线触摸仍为三条。

三个实际传输图标的 SHA256：

- 自定义：0eb1df7c7380a843209e673a6d9b520d271d49748364df69b2e155eda36c0df7
- 此电脑：801b7bf3a0b289e04613a8dcc37ce3246f19ab02850e050f7f0a95a6d292fe82
- 回收站：d00a27b7dbe495f2b6581a561bd4dab77294e767be419d6a188731a74d6a6e13

系统按钮只核验图标，没有点击执行。这些记录不能代替全部程序、lnk、文件关联、网站、快捷键或系统动作验收。

## 大字体缺口与修复

最初在 1.5/2.0 字体缩放下检查十八槽位和触摸尺寸都成立，但查看 2.0 的实际离线截图发现名称被裁掉。因此“网格在屏内”不算大字体界面通过。

原因是按钮 Column 先测量固定 48dp 的已加载图标，留给名称的高度不足；旧 UI 测试只用了较小的类别占位图，且主要检查槽位可见，未检查文字完整行高。

- 只改 Android ControlPanelScreen：名称先取得完整单行空间，图标在剩余的加权区域中受约束缩放，保留 11sp 字号和系统缩放，显式单行行距 14sp；长名称仍省略，完整名称仍用于无障碍描述。
- 不修改网格数、协议、缓存、连接、音量、共享逻辑或其他插件。
- 新回归使用真实 256 x 256 缓存图，检查 TextLayoutResult 的实际行底与可见文本高度、按钮边界和离线不发送，覆盖 SDK 28/37。
- 修复前精确筛选的默认 SDK 用例失败：Label must retain its full line height。失败报告保留；修复后完整触摸测试组十项通过，再跑全部 Android 测试。
- 新 APK 在真实 AVD 的在线、离线新进程两种状态均复查 2.0 字体：三张已加载图标下名称的完整行高可见，长名称保持省略。
- 测试专用 AVD 原 font_scale 键为 null（实际默认 1.0）；删除测试设置后 Android 将键规范化为 1.0，最终实际配置为 1.0。原始键缺省状态未精确恢复，此差异已单独记录，不影响主机或实体手机。

## 自动回归与产物

| 检查 | 结果 |
| --- | --- |
| 修复前裁切回归 | 1 项失败，保留原始断言及报告 |
| 修复后触摸测试组 | 10 项通过 |
| Android 完整单测 + APK + Lint 任务 | 4 分 32 秒结束，退出 0；43 套件、271 项，失败/错误/跳过均 0 |
| Lint 实际报告 | 473 Error + 20 Fatal = 493 错误，211 Warning、1 Hint；NewApi=0、MissingDefaultResource=0，仍未通过 |
| Windows 测试工具构建 | 两次退出 0，仅测试目标变化，无 Windows 运行模块改动 |
| Windows 全部 CTest | 21/21，失败/跳过 0，28.24 秒 |
| 新 APK 签名及安装核验 | v1/v2 通过，安装到专用 AVD 的 APK 哈希与产物一致 |
| 当前 Android 进程日志 | 所采样的当前 PID 无 FATAL EXCEPTION/Fatal signal，不代表整个系统或长时无错误 |
| 正常桌面会话退出 | 后两次启动器均退出 0，清理后包进程 0 |
| 最终清理 | 专用模拟器和 ADB 5038 已停止；用户 ADB 5037 与无关 Craft D-Bus 仍在 |

新 APK 仅为内部候选，不覆盖已发布的 0.1.0-alpha.1：

~~~text
D:/KDEBuild/artifacts/controlpanel-unified17-20261112/kdeconnect-controlpanel-unified17-debug.apk
30872225 字节
SHA256 9D4DE20E8BF7B8D3EA8E81BE4E49F250D89EA3E283D0C1746B73464B65352C56
~~~

Lint 错误数与阶段十六相同，不是绿色；其中仍包括阶段十六已记录的十六项新增缺译，不把它们全部描述为上游旧问题。没有降低严重度、忽略翻译或修改 abortOnError。

## 测试工具、异常及证据边界

桌面 panelemulatorintegration 只作为测试目标，不安装到产品包。增加 Breeze 默认图标初始化和强制 offscreen，新增 --extend-unified 及 --unified-direction；只接受私有 profile 标记和本工具已生成的探针配置，校验十页中每个探针目标/参数以及空槽，再通过 PanelEditor 改方向。重复扩展、非法方向和未标记目录均被拒绝，配置或文件保持不变。

断网步骤曾误传后端名称 LanLinkProvider，而正式 ID 是 lan。D-Bus 返回 NoReply，原 daemon 消失并由私有 D-Bus 重新激活；首次启动器最终退出 1，不能列为成功退出。源码共享接口目前只靠 Q_ASSERT 后再使用查询结果，未抓取崩溃 dump，不声称取得异常堆栈。本次未扩大修改 core/daemon.cpp，已向用户请求仅增加无效名称保护的范围确认。

移除 ADB reverse 监听没有切断已有连接，观察到 offline=0，因此该步骤不计为断网通过。有效的离线证据来自独立桌面进程停止和重启，不冒充普通 Wi-Fi 断网或发现通过。

证据根目录 D:/KDEBuild/unified17-e2e：

- logs/cache-icons-check.json、pages-prepare.json、vertical-pages-summary.json、horizontal-summary.json。
- logs/restart-execution-check.json、offline-process-restore-check.json、final-horizontal-summary.json、final-offline-process-check.json。
- logs/font-summary.json、font-restored.json、font-final-state.json、fixed-font2-check.json、fixed-offline-font2-check.json。
- logs/android-font-before-fix.log、android-font-focused-fix.log、android-full-fix.log、lint-summary.json、ctest.log / .xml。
- reports/font-before、font-focused-fix、android-full；ui 中的每次独立 XML 和截图。
- logs/initial-desktop-cleanup.json 保留首次异常退出；restart-desktop-cleanup.json、final-desktop-cleanup.json 和 cleanup.json 记录后续正常清理。
- 私有 profile、证书/私钥、D-Bus nonce、原始配对/进程日志仅留本地，不整体上传 GitHub。

## 仍未完成

普通 LAN、实体设备、当前真实多设备长时、所有原生动作/焦点和三角色音频写入/回滚、完整参考视觉/全部表盘/无障碍、原 KDE 全功能、剩余 Lint、干净 Windows 系统、完整发行依赖/对应源码材料及最终包继续保留。主机焦点/音频、桌面 fork 推送规则及共享无效后端保护的确认尚未收到，不擅自执行。

Android 新布局及桌面测试工具作中文提交；Android 可继续同步个人 fork，桌面最新本地提交在确认后再推送。公开体验版附件和标签保持不变。整体目标仍在进行，验收清单不因本批局部通过全部勾选。
