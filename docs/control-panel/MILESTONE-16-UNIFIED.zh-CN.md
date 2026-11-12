# 阶段十六：统一验证第一批结果

日期：2026-11-12。功能代码覆盖复核见 [DEVELOPMENT-GATE-16.zh-CN.md](DEVELOPMENT-GATE-16.zh-CN.md)。本批开始统一验证，不表示全功能、真实双端、原生动作或最终交付已经通过。

## 本批修改

- 补齐发现的默认图标同步缺口：GUI 保存空图标按钮时，将程序/固定主题图标转为现有规范 PNG；手机复用原哈希附件和离线缓存，无需接收私有动作目标。
- “补齐默认图标”只处理旧按钮的空图标，保留已有自定义图标与槽位；批量一次原子保存，重复操作无额外 revision，生成失败不提交半批配置。自动默认图标不可用时仍可使用类别占位，不伪造成功传输。
- 新生产 QML 用例检查不同系统图标、256 x 256 PNG、公共哈希且无 target、保留自定义图标及补齐重试/幂等。
- 图标取消用例改为检查原按钮实际图标，不再错误假设默认图标一定为空；取消仍必须保留全部原配置。
- 表盘测试初始化与正式程序相同的 Breeze 资源/主题，补图标可用断言；仅测试链接现有项目依赖，不增加运行依赖。
- 修正空闲 Activity 测试前置条件：诊断显示模拟时钟在测试失焦前已距上次输入 5115 毫秒，表盘按 5000 毫秒阈值正确出现。明确唤醒后再开始未松键/失焦场景；不放宽时间、无动作或恢复断言，不改生产空闲逻辑。清理一个测试夹具的无用表达式警告。

## 已执行的统一检查

| 检查 | 实际结果 | 边界 |
| --- | --- | --- |
| Android 全量第一轮 | 269 项，1 失败，构建任务非零，5 分 22 秒 | 失败为上述 Activity 场景前置条件，报告已保留 |
| Android 定位复跑 | 原场景再次失败，记录 showing/时钟/输入门控；修正后单项通过 | 只用于定位，不代替全量 |
| Android 全量第二轮 | 43 个报告套件，269 项，失败/错误/跳过均 0；testDebugUnitTest、assembleDebug、lintDebug 任务退出 0，3 分 33 秒 | JVM/Robolectric/Compose，不是真机或网络联调 |
| Windows 完整构建 | 两轮退出 0 | MSVC / Qt / KF 当前环境，不代表干净系统 |
| Windows 全部 CTest 第一轮 | 21/21，失败/跳过 0，30.28 秒 | 并行 2；包含生产 QML 和原 KDE 已注册测试 |
| Windows 全部 CTest 第二轮 | 21/21，失败/跳过 0，35.80 秒 | 串行；补齐正式图标初始化后完整复跑 |
| Windows 原生音频只读 | 3 个输出、3 个默认角色可读取，error 为空 | 没有调整音量/静音/输出，不验证三角色写入与回滚 |
| Windows 内部包 | 安装/依赖收集退出 0，收集 149 个非系统运行库；独立新配置无窗口自检退出 0 | D-Bus、CLI、DLL 来源及主界面截图；未点击按钮或写音频 |
| Windows 清理 | 应用错误日志 0 字节，退出后此包进程 0 | 后台仍有原 Windows 权限检查警告，CLI 仍有退出消息派发警告，不称已修复 |
| APK | 独立 .debug 标识、API 23 起、版本 1.37.0-debug，v1/v2 签名核验通过 | 调试签名，未向华为手机安装 |

第二轮 Android XML 全部在本轮生成，没有混入旧报告。首次失败、定位失败和修复后单项证据均保留；不删除失败后只展示成功。Windows QML 截图来自实际组件；包截图来自实际 app 的无窗口平台，不能当作真实原生窗口/焦点证据。

## Lint 实际未通过

本轮报告为 705 项：473 Error + 20 Fatal（合计 493 错误）、211 Warning、1 Hint。错误分类为：

- MissingTranslation 472 项；
- ExtraTranslation 20 项；
- MissingLeanbackLauncher 1 项。

比阶段十一 477 错误增加 16 项，差异是控制面板新增提示的缺译，不能统称全部旧问题。NewApi=0、MissingDefaultResource=0。项目既有 abortOnError=false 使任务返回 0，不等于 Lint 绿色；本轮没有修改策略、静默忽略、降低严重度或假翻译来清零。TV 和全语言范围尚未扩展，完整验收仍保留。

## 原始证据与内部候选

- Android 日志：D:/KDEBuild/logs/unified16-android-full1.log、unified16-android-idle-diagnostic.log、unified16-android-idle-fix1.log、unified16-android-full2.log。
- Android 归档：D:/KDEBuild/reports/unified16-android-full1、unified16-idle-diagnostic、unified16-idle-fix1、unified16-android-full2（最后一项含本轮 Lint XML/HTML/文本/SARIF）。
- Windows：D:/KDEBuild/logs/unified16-desktop-build1.log / build2.log、unified16-ctest-full1.log / .xml、unified16-ctest-full2.log / .xml。
- UI：D:/KDEBuild/unified16-ui1 与 unified16-ui2；包含按钮、多页、小窗口和五类表盘组件图。
- 包：D:/KDEBuild/packages/controlpanel-unified16-win64-r1。自检专用配置和原始日志：D:/KDEBuild/unified16-package-smoke1；不上传身份、私钥、D-Bus nonce、原始音频端点 ID 或整个 profile。
- APK：D:/KDEBuild/artifacts/controlpanel-unified16-20261112/kdeconnect-controlpanel-unified16-debug.apk，30872225 字节，SHA-256 A68B22BE013E4213BAF75D4AFFB4DD33A3E6471CF002E41731ABB486D94658B1。
- 包内 app：2739200 字节，SHA-256 306AAAA657B9A6CBF1F2E1BB50F4A4169DF18175DCE18ADB617AD67E466F0704。
- 包截图、签名和只读音频证据均保存在 D 盘。内部候选不是新公开发行；0.1.0-alpha.1 标签、APK、ZIP 及 GitHub 附件不覆盖。

## 仍未完成

实际程序/lnk/文件/网站/快捷键与系统动作、真实焦点/宿主窗口、三角色音频写入及回滚失败、拔插/无输出、普通 LAN、当前代码的双端/多设备/长期联调、全部参考视觉与无障碍、原 KDE 全功能、干净系统、发行依赖/对应源码材料与最终包均继续保留。未对手机安装。

为避免干扰用户，原生测试窗口取焦点及实际音频写入已提出范围确认，未确认前只做不干扰当前使用的检查。其他不受该确认影响的验证继续推进。整体目标不完成、不缩减；上述单测和启动通过不能代替这些待验收项。
