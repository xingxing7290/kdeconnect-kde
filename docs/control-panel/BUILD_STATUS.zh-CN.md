# 构建与测试记录

更新：2026-10-28。本记录是开发里程碑，不是完整交付验收。

## 本阶段实现

- 两端配置模型与同一份 JSON 样本：18 个固定槽位、1..10 页、稳定 ID、模式/外观/空闲参数、修订号和大小/深度边界校验。
- 桌面公开配置白名单投影：不向 Android 下发私有路径、动作 target 或未知私有字段。
- 桌面原子存储、上次有效备份、修订计数、并发编辑冲突检测、跨进程文件变更监听、显式恢复并保留损坏文件。
- Android 会话状态：离线/同步时禁止动作、不缓存离线点击、拒绝旧修订、重复点击等待确认、超时释放、断线清空待执行状态。
- 修复已有全屏命令页面在官方 Kotlin 单例改动后的 3 处 Java 调用。

网络插件、动作执行、控制面板界面、图标传输、智能跟随和表盘尚未接入；不能从本阶段测试推断这些功能已经完成。

## Android：通过

环境：JDK 17.0.14、Gradle Wrapper 9.6.0、Android SDK 37、AGP 9.4.1。

执行任务：`testDebugUnitTest assembleDebug`（未禁用或跳过测试）。

- 143 项单元测试：失败 0、错误 0、跳过 0。
- 其中新增 PanelConfigTest 6 项、PanelSessionTest 6 项。
- 调试 APK 构建成功，applicationId 为 org.kde.kdeconnect_tp.debug。
- 本阶段 APK 尚未安装到手机，不是最终控制台交付包。

本机证据：

- 日志：D:/KDEBuild/logs/android-baseline-retry5.log
- 测试报告：E:/git/kdeconnect-android/build/reports/tests/testDebugUnitTest/index.html
- XML：E:/git/kdeconnect-android/build/test-results/testDebugUnitTest/
- APK：E:/git/kdeconnect-android/build/outputs/apk/debug/kdeconnect-android-debug.apk
- APK SHA-256：B80D1801A1BEE671A3AFCE869EBF24193043932F6161559BCC5079351BBD71C9

首轮发现的真实失败及修复：

1. 原全屏命令 Java 页面直接调用 getInstance，官方 Kotlin companion 未导出静态方法；改为 Companion.getInstance。
2. JSON 库允许字符串布尔值，未知字段的深层嵌套未被自动限制；新增显式原始类型和容器深度检查，保留失败用例后重跑全部单元测试。

完整日志还含上游弃用 API/翻译资源警告；此处没有将警告描述为全部解决。

## Windows 控制面板模块：通过

环境：Windows x64、MSVC 19.35、Windows SDK 10.0.22000.0、Craft Qt 6.12.0。

使用本机独立 CMake 入口编译仓库实际 controlpanel 源码及 tests/paneldocumenttest.cpp、tests/panelstoretest.cpp。这不是整个 KDE Connect 的构建。

- 编译成功。
- CTest：2/2 测试套件通过，单项超时 60 秒。
- 再次直接执行并生成 JUnit XML：PanelDocumentTest 20 项、PanelStoreTest 6 项，失败/错误均为 0；计数包含各套件的初始化与清理项。
- 覆盖：共享样本、类型/版本/ID/大小/深度错误、私有字段隔离、并发修改、原子替换监听、损坏配置恢复。

本机证据：

- D:/KDEBuild/logs/panel-tests-configure.log
- D:/KDEBuild/logs/panel-tests-build4.log
- D:/KDEBuild/logs/panel-tests-ctest3.log
- D:/KDEBuild/logs/paneldocumenttest.xml
- D:/KDEBuild/logs/panelstoretest.xml
- 本机测试入口：E:/git/kdeconnect-tooling/controlpanel-tests/CMakeLists.txt
- 构建目录：D:/KDEBuild/panel-tests

首轮独立编译发现变量名 slots 与 Qt 宏冲突，已改名并重编译测试。完整项目中的 tests/CMakeLists.txt 也注册了这两个测试，待其余桌面依赖安装完成后从完整工程再次运行。

新增 C++ 文件按 ECM 的 C++ 格式规则整理并通过格式检查。Craft 提供的 clang-format 20 在本机启动异常，故使用已有 Visual Studio clang-format 15 和相同的 ECM C++ 配置段；未修改第三方工具文件。

## 存储位置与环境修复

源码在 E 盘，不在 C 盘开发：

- E:/git/kdeconnect-android
- E:/git/kdeconnect-kde

Gradle 缓存 D:/gradle；SDK D:/Android/SDK；Craft D:/KDECraft；工具缓存、Java/Android 用户目录、临时文件和构建日志 D:/KDEBuild。Android build 产物目前在 E 盘。

本机辅助入口 E:/git/kdeconnect-tooling/build-android.ps1 和 run-craft.cmd 只调整子进程环境，不修改系统全局环境变量。Android 旧 ANDROID_SDK_HOME 与新 ANDROID_USER_HOME 冲突已在构建进程中消除。Craft 日志默认写 C 盘的问题已改为 D 盘，先前任务日志已保留到 D 盘。Windows SDK 是本机已有系统组件，只读取，不迁移或删除。

C 盘剩余空间仍紧张，项目目录迁移不能控制操作系统和其他程序的写入；未删除其他文件。

## 待完成验证

- 桌面其余 KF/多媒体等依赖仍在安装，尚未完成完整 CMake 构建、完整 CTest、打包或安装。
- Android Lint、真机安装/界面、横屏/生命周期/无障碍测试尚未完成。
- 双端协议、真实动作、断网重连、音量、智能跟随、全部表盘及视觉对照尚未完成。
- 最终包、完整 README 和需求逐项验收继续按 PLAN 执行。
