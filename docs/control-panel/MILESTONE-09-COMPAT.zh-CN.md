# 阶段九：Android 兼容修复与正式触摸测试

日期：2026-11-12。用户已确认逐项核查后的最小兼容修复，以及仅测试配置引入 Compose UI 测试依赖。本阶段不代表完整参考复刻、全部 KDE 功能兼容或正式发行完成。

## 核查与修改范围

| 原诊断 | 核查结果与处理 | 保留行为 |
| --- | --- | --- |
| 剪贴板 TileService / Run Command ControlsProviderService，2 项 NewApi | 两个服务分别以 values-v24 / values-v30 的 enabled 布尔值控制，基线为 false；局部 tools:targetApi 记录其启用版本 | API 24/30 起恢复原入口、权限与导出约束不变；没有仅加 Lint 标签而遗漏运行开关 |
| SMSHelper 视频缩略图，1 项 NewApi | API 27 及以上继续使用 getScaledFrameAtTime；API 23..26 取帧后按比例缩到不超过 100 x 100，同尺寸复用，回收不再使用的原帧 | 空帧不伪造图像，现代版本的调用不变；外层 retriever.release 的 finally 不变 |
| 输入设备接收，1 项 NewApi | 旧系统沿原 release 路径交还输入，不调用 API 24 起的 MouseReceiver | 不提高整个插件的最低系统版本，不改变 API 24 及以上的坐标处理 |
| SFTP 属性，4 项 NewApi | 原工程已使用 desugar_jdk_libs_nio:2.1.5；APK 中泛型界限、返回值与四处转换均为随包 j$.nio 类型。仅在 getFileAttributeView 方法作局部误报标注 | 不删除 basic / posix 支持，不改属性及权限语义，不把主机 JVM 测试当作 Android 实机 SFTP 传输 |
| 偏好资源，1 项 MissingDefaultResource | 查验 AndroidX Preference 1.2.1 AAR：基线本已为 false。项目显式补相同默认值，保留宽屏 false，并声明有意覆盖私有资源 | 不是已证实的旧系统崩溃修复；窄屏/宽屏现有图标间距行为不变 |

服务启用机制、视频 API 和 NIO 回移依据：[service enabled](https://developer.android.com/guide/topics/manifest/service-element)、[MediaMetadataRetriever](https://developer.android.com/reference/android/media/MediaMetadataRetriever)、[NIO 支持表](https://developer.android.com/studio/write/java11-nio-support-table)。tools 属性只是构建工具元数据，不是运行版本检查，参见[工具属性说明](https://developer.android.com/studio/write/tool-attributes)。

本阶段获授权后才修改以上原功能模块。共享 NetworkPacket/Payload、其他插件和最低 Android 版本没有扩大改动，既有功能设计保留。

## Compose 正式测试规则

- ui-test-junit4 使用与现有 ui-tooling 相同的版本引用 1.12.1，仅 testImplementation。
- 使用 createEmptyComposeRule 的官方 v2 规则，测试中创建 ComponentActivity 作为生产 ControlPanelScreen 的宿主；不添加调试 APK 的测试 Activity 或 ui-test-manifest，不改原手工重组辅助类。
- Compose 传递引入旧 Espresso 3.5.0，导致 SDK 37 初始化输入注入时反射失败。按[AndroidX Test 官方修复说明](https://developer.android.com/jetpack/androidx/releases/test)，只在测试配置对齐 Espresso 3.7.0；本地字节码也确认现代系统改用 getSystemService。
- [v2 测试规则说明](https://developer.android.com/develop/ui/compose/testing/migrate-v2)对应排队执行的测试调度和显式同步。测试使用 waitForIdle / runOnIdle，不靠无界等待或跳过 SDK 37 来得到通过。

触摸检查含：实际触摸发送一次动作、请求未完成时重复点击不重发、离线禁止发送；隐藏长中文标签后仍保留完整可访问名称；横/竖滑动只请求翻页；2 倍字体，以及 800 x 360dp 横屏下全部 18 个目标可见且宽高至少 48dp。SDK 28、37 均覆盖。

这是本地宿主、模拟设备和原生绘制后端测试，不是当前华为手机、TalkBack 实机验收或完整 Activity/网络端到端验证。HWUI 输出的虚拟时间域数值不用于推断手机帧率或性能。既有真实 Activity 空闲唤醒测试也继续纳入全量回归。

## 检查与产物

证据根目录 D:/KDEBuild，源码 E:/git；当前华为手机没有安装本阶段程序。

| 检查 | 结果 | 原始证据 |
| --- | --- | --- |
| 全量单测、APK、Lint 任务 | 两轮全量均 243 项，失败/错误/跳过均 0；APK 构建成功；Lint 任务退出 0 但实际仍有错误 | logs/android-compat-full2.log（6 分 42 秒）、android-compat-full3.log（4 分 7 秒）；reports/android-compat-full2、android-compat-full3 |
| 新增测试 | 共 43 项：资源/服务 10、视频缩略图 16、输入设备 3、SAF 属性 6、触摸 UI 8 | 以上 XML 与 5 个新增测试类；既有 200 项也在全量中运行 |
| 服务及资源边界 | SDK 23/24/29/30/37，读取实际合并清单 ServiceInfo，核对 enabled、签名绑定权限与 exported；偏好资源在 sw320/359/360/600 均为 false | ServiceResourceCompatibilityTest |
| 视频回退 | SDK 23/26/27/37，宽/高/极窄帧、同尺寸复用、原帧回收、空帧；现代调用保持原参数 | SMSVideoThumbnailTest |
| 输入回退 | SDK 23 交还输入、不调新版鼠标动作；24/37 保持坐标转发 | InputDevicesReceiverCompatibilityTest |
| SAF 属性 | SDK 23/26/37 的宿主逻辑测试，保留根目录只读及缺失文件异常；不是实际网络 SFTP | SafAttributeCompatibilityTest |
| 触摸与基础可访问性 | SDK 28/37，每个 4 方法；重复点击/离线、长名称隐藏标签、两种滑页、2 倍字体、800 x 360dp 横屏的 18 个目标及至少 48dp 尺寸通过 | ControlPanelTouchUiTest 8 项；reports/android-compat-full3 中对应 XML |
| 实际 APK 的 NIO | 四次 FileAttributeView 转换与签名为 j$.nio，回移接口在包内，未发现对应平台独占类型引用 | logs/android-compat-sftp-dex-final.txt、android-compat-sftp-interface-final.txt |
| 仅测试依赖边界 | debugRuntimeClasspath 查不到 ui-test；APK 定义类列表无 Compose ui.test 与 Espresso 类 | logs/android-compat-app-test-dependency-check.log、android-compat-apk-defined-packages.txt |
| APK 身份和签名 | org.kde.kdeconnect_tp.debug，1.37.0-debug，最低 23、目标 37；最低 API 23 签名验证通过，v1/v2 为 true | logs/android-compat-apk-manifest.txt、android-compat-apk-signature.txt |

最新 Lint：477 errors（457 Error + 20 Fatal）、211 warnings、1 hint；分类为 MissingTranslation 456、ExtraTranslation 20、MissingLeanbackLauncher 1。NewApi 与 MissingDefaultResource 均为 0。没有改变既有 abortOnError=false，不能把任务 BUILD SUCCESSFUL 写成 Lint 通过。完整报告归档于 reports/lint-compat-final；局部工具元数据补齐前为 479 errors / 213 warnings，归档 reports/lint-compat-before-metadata，便于核对不是全局屏蔽。

失败记录保留：首轮专项 29 项中 2 项同尺寸帧身份假设失败，随后明确复用同尺寸帧，专项重跑 38 项通过；首次全量 241 项中 SDK 37 的 3 项因旧 Espresso 反射失败。升级仅测试驱动后 6 项触摸通过，再补手机尺寸用例，最终两轮全量 243 项通过。日志见 android-compat-tests1/2、android-compat-full1、android-compat-ui3 及对应 reports 目录。反汇编辅助调用的前几次启动/方法参数错误也未当作通过，最终以 -final 文件和接口定义为证据。

## 内部候选 APK

D:/KDEBuild/artifacts/control-panel-compat-20261112/kdeconnect-controlpanel-debug.apk

SHA256：`207A13D4EE4BA74473B476F6C0FEFD07D985D08B1917182845BE388D1C07FC60`。

沿用工程既有调试密钥，不是最终发行签名或正式发行身份。没有安装当前手机。桌面运行代码/候选包沿用阶段八的 561d6455b5042e3146673bedf494e68bb1cc0375，未因同步文档重新构建或宣称再次验收。

## 保留的完整目标与下一步

- TV 入口与全语言翻译仍按用户要求单独记录，不借兼容修复改变其设计；Lint 因此仍不通过。其他告警还需审阅，不能只看 Error 数量。
- 完整参考视觉、所有原生 .lnk/URL/文件/快捷键/系统动作、真实焦点和音频角色、生命周期/旋转/进程恢复、完整可访问性与长时间运行仍需验证。
- 当前手机不安装，后续可继续本地模拟器及双端真实传输验证；本地模拟设备的成功不替代配对、断网、重连、多设备和原 KDE 全功能回归。
- Windows 干净普通账户、最终安装/卸载、签名、第三方许可与对应源码、最终中文交付说明仍未完成。
- 两端 README / PLAN 保留原验收范围，不因新增 243 项本地测试完成整体验收。
