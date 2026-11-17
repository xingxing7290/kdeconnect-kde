# 换机开发与工作接续

2026-11-17：本机桌面候选安装卸载测试已完成，范围与保留的测试数据见[构建记录](BUILD_STATUS.zh-CN.md)。下面 2026-11-16 的未提交状态是当时的历史快照，当前是否同步请以 `git status`、远端提交与最新记录为准；安装测试不等于真实双端或干净系统验收。

2026-11-16 再次只读核对个人 GitHub 的 master：桌面为 `6a3c04403f67b27697480931775df19f036bcccb`，Android 为 `b36947d79b9a445c4348d739b4535b0cdc5542dd`，分别与本机 HEAD 相同；两端工作区仍有未提交文件，远端不包含这些修改。最近 Android 本地测试为 51 套件、343 项通过；桌面候选未做实际安装卸载，详情见构建记录。

2026-11-16 本机状态补充：两端工作区均有尚未提交或推送的代码、测试及文档；按下列命令从 GitHub 克隆不能获得这批未同步内容。最新本地回归与未完成项目见[构建记录](BUILD_STATUS.zh-CN.md)，下方 2026-11-15 基线保留为历史记录。

同日只读查询个人 GitHub 的 `master`：桌面为 `6a3c04403f67b27697480931775df19f036bcccb`，Android 为 `b36947d79b9a445c4348d739b4535b0cdc5542dd`，分别与本机当时 HEAD 一致；工作区未提交修改不包含在这两个提交内。远端会继续变化，后续同步前必须再次核对。

更新：2026-11-15。本文是两端接续入口，不是全功能验收通过声明。
本次只同步源码、文档与开发/发布入口，不重新编译，不改已发布标签和附件。

## 先取得正确代码

在自己的可写工作盘执行，路径可改；不要复制旧机器的构建目录、CMakeCache 或 local.properties。

```powershell
git clone https://github.com/xingxing7290/kdeconnect-kde.git
git clone https://github.com/xingxing7290/kdeconnect-android.git
git -C kdeconnect-kde switch master
git -C kdeconnect-kde pull --ff-only origin master
git -C kdeconnect-android switch master
git -C kdeconnect-android pull --ff-only origin master
```

继续开发用两端最新 master；需要复现已发布版本时，使用标签，不移动原标签：

| 平台 | 发布标签 | 当时源码提交 |
| --- | --- | --- |
| Windows x64 | controlpanel-release-v0.1.0-alpha.4 | 64efe8bbe01fa8b1ec7f1064368dd5168d4f1f9d |
| Android | controlpanel-release-v0.1.0-alpha.3 | b7c569ad500d601b636c7ccac03bcdad2ad97c43 |

[Windows 发布页](https://github.com/xingxing7290/kdeconnect-kde/releases/tag/controlpanel-release-v0.1.0-alpha.4)含安装器、应用源码、对应第三方源码、许可证和校验文件。
[Android 发布页](https://github.com/xingxing7290/kdeconnect-android/releases/tag/controlpanel-release-v0.1.0-alpha.3)含个人签名 Release APK；它与官方版、调试版的包名和配对数据不同。
文档整理提交位于这些功能基线之后。不要把较早阶段文档中的“当前”当成新进度。

## 优先阅读的现有资料

- [需求基线](REQUIREMENTS.zh-CN.md)、[架构](ARCHITECTURE.zh-CN.md)、[协议](PROTOCOL.zh-CN.md)。跨端字段变更先核对协议和 fixtures/config-v1.json。
- [计划与验收](PLAN.zh-CN.md)、[构建记录](BUILD_STATUS.zh-CN.md)。保留未通过项目，不用一次退出码或截图替代验收。
- [表盘](DIALS.zh-CN.md)、[图标与数据迁移](ASSETS.zh-CN.md)。配置和资源规则直接参考这些文件，不另造一套。
- [最新 Windows 发行记录](RELEASE-0.1.0-alpha.4.zh-CN.md)、[发布测试摘要](release-evidence/RELEASE-CHECKS-alpha.4.json)。其中源码/运行库/测试范围是发布时快照。
- [Android 接续与阶段二十三](https://github.com/xingxing7290/kdeconnect-android/blob/master/docs/control-panel/DEVELOPER-HANDOFF.zh-CN.md)。Android 的主控制页后续修复不应按桌面阶段二十二记录推断。

参考产品的公开功能已整理进需求分析。原始第三方接口响应、激活二维码、联系方式和整套参考图片不纳入公开 Git；不要把参考软件安装包当成本项目依赖或执行测试。

## 找到修改位置

| 范围 | 入口 |
| --- | --- |
| 桌面模型、配置、执行和会话 | controlpanel/ |
| 桌面 UI | app/qml/ 中的 ControlDashboard、Panel 系列文件 |
| 网络插件 | plugins/controlpanel/ |
| 桌面回归 | tests/panel*、tests/volume* 及 tests/CMakeLists.txt |
| Windows 打包/发行材料 | packaging/windows-controlpanel/ |
| FFmpeg/Qt 真实兼容检查 | packaging/windows-controlpanel/compatibility/ |
| Android UI、协议和缓存 | Android 仓库 src/main/java/org/kde/kdeconnect/plugins/controlpanel/ |

只改当前功能模块。确需改共享传输、音量后端、其他插件或构建依赖时，先说明影响并取得确认。

## Windows 开发环境

在 MSVC x64 Developer PowerShell/Native Tools 环境中构建，避免混入 MinGW。
需要 C++20、Windows SDK、CMake、Ninja、完整 Qt/KDE Frameworks 依赖，不是只安装一个 Qt SDK。
本次已验证依赖为 Qt 6.12.0、KF 6.31.0、CMake 4.4.4；项目最低声明不等于完整发行环境。
FFmpeg 的静态依赖涉及 MSVC LTCG，发行重建使用 19.44.35230，不能随意换成旧链接器。
准确依赖版本、配方、补丁和来源见发布页第三方源码主 ZIP 的 source-index.json。

下面变量仅为示例，可换成新电脑的路径。在新专用终端里设置，不写入系统环境变量：

```powershell
$workRoot = 'E:/git'
$buildRoot = 'D:/KDEBuild'
$depsRoot = 'D:/KDECraft'
$env:TEMP = Join-Path $buildRoot 'temp'
$env:TMP = $env:TEMP
$env:VSLANG = '1033'
New-Item -ItemType Directory -Path $env:TEMP -Force | Out-Null
chcp 65001 > $null
cmake -S "$workRoot/kdeconnect-kde" -B "$buildRoot/desktop" -G Ninja `
  -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$depsRoot" -DBUILD_TESTING=ON `
  -DKDE_INSTALL_QMLDIR=lib/qml -DKDE_INSTALL_PLUGINDIR=lib/plugins
cmake --build "$buildRoot/desktop" --parallel 4
ctest --test-dir "$buildRoot/desktop" --output-on-failure --timeout 90
```

CMake/编译/测试分开执行，每步核对退出码。运行超过十分钟且无明确进展应结束当前步骤，记录最后输出和下一步。
新机未实际验证；示例不是自动安装依赖脚本。不要复制旧机的代理端口、证书信任或构建缓存来“修复”网络/编译问题。

日常功能开发不必先重做全部 Windows 发行归档。需要发布时再按
[打包说明](../../packaging/windows-controlpanel/README.zh-CN.md)和
[安装器说明](../../packaging/windows-controlpanel/INSTALLER.zh-CN.md)准备独立 FFmpegRoot、RuntimeRoot、DistributionMaterialsRoot。
build-ffmpeg.sh 中保留原机复现路径，换机要显式调整 MSYS2/Craft 路径；不能原样假设 D 盘已有依赖。
GNU/MSYS2 sed 应优先于不兼容的 Windows shim，cl/link 则必须使用匹配工具集。

已把本机发布时修正的脚本归档为
[publish-personal-preview.ps1](../../packaging/windows-controlpanel/publish-personal-preview.ps1)，需要 PowerShell 7.4+ 与你自行配置的 GitHub 凭据。
它只面向 xingxing7290 的两个个人 fork；无 -Publish 时仍会创建草稿/上传附件，并非只读预演。
不要在功能开发过程中顺手运行发布命令；公开前仍需检查 DLL、材料、兼容和安装卸载。

## 从新电脑继续开发

1. 阅读仓库 AGENTS.md/CONTRIBUTING.md 和当前功能对应的协议、测试、阶段记录。
2. 建立 feature/<功能名> 分支；先记录任务范围、可复现问题、预期行为与完成标准。
3. 两端分批改动，先对应的定向测试，功能齐备后再统一回归。跨端协议变更要同时安排兼容策略。
4. 两台电脑交替工作前先提交并推送当前分支；另一台用 fetch 和 pull --ff-only 接续。
   不直接同步工作目录/构建缓存，不强推解决分叉；有分叉先查看双方日志并处理冲突。
5. 一次提交只放当前功能相关文件，中文说明并保留 Assisted-by: Codex（如确有 AI 辅助）。
   不向 KDE 官方自动提交 MR；上游对自动 MR 有封禁风险，官方贡献须按项目规则另行由人审核。

## 当前证据和剩余工作

Windows：22/22 CTest、12 个实际音视频场景、安装后相同场景复测、10 种合成编码样本及回读、安装卸载/身份保留通过。
这些重复部署场景不是新增几十种独立格式；仅无窗口软件解码与内存音频，不表示全 GPU/声卡矩阵通过。
Android 最近全量单元测试为 304 项通过；Release 安装/启动是专用模拟器检查，不是实体手机验收。
Android Lint 仍有 493 项错误。Windows 安装器未代码签名；干净 VM/普通账户、真实 LAN/多端长时、
完整原生动作/音量角色回滚、全部视觉和真实辅助技术等仍见计划，不自动勾选完成。

真机安装、主机原生可见窗口/焦点及真实音频写入需要相应用户确认；此前未批准的范围不因换机而扩大。

## 不上传的内容

正式签名 keystore/密码、DPAPI 密码文件、配对 privateKey.pem/证书、实际用户 profile、GitHub 凭据、
local.properties、CMakeCache、Gradle/SDK/Craft 缓存和大体积构建日志不进入 Git。
Android 调试开发不需要正式密钥；续发可覆盖升级的 Release 必须使用原发布密钥并提升 versionCode。
DPAPI 密码文件绑定原 Windows 账户，仅复制它到新机并不能恢复；密钥和可恢复的密码应另走可信安全备份渠道。
不要为接续开发读取或发布密钥，不要用新生成的密钥假冒旧版本的可覆盖升级。
