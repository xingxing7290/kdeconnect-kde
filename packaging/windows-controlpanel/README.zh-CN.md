# Windows 个人控制面板预发布包

这是个人 fork 的功能预发布，不是 KDE 官方发行版，也不是已通过完整产品验收的稳定版。不得把一次启动通过当作全部功能、真机配对或干净 Windows 安装通过。安装器不请求管理员权限，默认 D:/Apps/KDE-ControlPanel，可选其他可写本地目录；卸载保留 profile 数据。

## 许可证与对应源码

本次应用发行选择 GPLv3，FFmpeg 8.1.1 为 GPLv3-or-later 构建，未启用 --enable-nonfree。原有开源组件保留各自许可证；Microsoft CRT/SDK 可再分发文件保留微软许可。安装器未做发行者代码签名，可能显示未知发布者，不应为此关闭安全保护。

THIRD-PARTY-NOTICES.txt、source-index.json 和 licenses 目录包含通知、准确版本和来源。完整对应源码、Craft 配方/补丁、FFmpeg 构建脚本以及应用源码在同一公开 Release：
https://github.com/xingxing7290/kdeconnect-kde/releases/tag/controlpanel-release-v0.1.0-alpha.6

第三方源码主附件及全部 third-party-sources-part-N.zip 均需下载并解压到同一目录，无需拼接二进制文件。动态库可替换、重链接，不附加逆向工程限制。不要把本包的通知和许可证删除后再分发，也不要把用户私钥夹进再打包文件。

## 启动与数据

解压整个目录到本地 NTFS 磁盘，例如 D:/Apps/KDE-ControlPanel，双击 `Start-ControlPanel.cmd`。不要单独复制 bin 下的 EXE。无需管理员权限；启动器不安装服务、不注册开机启动、不改防火墙、证书信任或系统环境变量。

默认数据保存在候选包旁的 profile 子目录。也可用命令行指定一个新的专用目录：

```powershell
& 'D:/Apps/KDE-ControlPanel/Start-ControlPanel.cmd' -DataDirectory 'D:/KDEData/My Panel'
```

为避免碰到其他文件，首次指定的目录必须不存在或为空。每个数据目录只允许一个启动器会话。配置目录不能是盘符根目录或网络 UNC 路径。

启动器为本会话建立仅监听本机回环的 nonce-tcp D-Bus，启动承载后台服务的托盘程序和主界面。在 Windows PowerShell 5.1 下明确加载该宿主自带的 Security/CimCmdlets，避免从 PowerShell 7 启动时误选 Core 模块；不改 PSModulePath 或传给应用/外部脚本的环境。关闭主界面后停止本会话的包内后台进程，不停止其他 KDE Connect 实例，也不终止按钮打开的外部应用。不要直接关闭启动器控制台；强制终止启动器可能绕过 finally 清理。此时确认路径后，在任务管理器关闭该候选包目录下的 kdeconnect-indicator 和 dbus-daemon，再重新启动。

该候选包使用独立身份和配对配置，不自动复制已安装正式版的配对密钥。连接入口使用 KDE Connect 原有配对界面。需与配套 Android fork 连接才能使用新增控制面板协议；普通官方 Android 客户端不包含新增模块。当前连接的华为手机按用户要求未安装、未用于本轮配对测试。

## 备份与排障

关闭会话后备份整个专用数据目录，尤其是 kdeconnect（身份与配对）、panel（按钮与表盘配置/图标/天气缓存）。其中 privateKey.pem 是私钥，不上传 GitHub、不发给他人。新目录 ACL 仅允许当前账户、管理员和 SYSTEM。原 KDE Connect 的 Qt 文件权限检查在本机 Windows 上仍会打印过宽警告；本阶段另行核验实际 Windows ACL，未修改或压制上游检查。

日志位于数据目录 logs。TEMP、控制面板/KDE Connect 配置、QML/着色器缓存明确指向该目录。D-Bus 1.14 在 Windows 上用 HOMEDRIVE/HOMEPATH 定位认证 cookie，启动器仅为子进程设置它们。Windows Qt 的通用标准路径不会仅因 APPDATA 改变就重定向，因此这不是所有第三方框架都保证零系统盘写入的“完全便携版”。不修改系统路径或用户已安装开发环境。

出现依赖加载错误时，保留整个候选包和 logs，不从网上随机补 DLL。该包应仅加载包内及 Windows 系统 DLL；启动检查会保存实际模块路径。

## 本地自动启动检查

```powershell
& 'D:/Apps/KDE-ControlPanel/Start-ControlPanel.cmd' `
  -DataDirectory 'D:/KDETests/Native' -SelfTest `
  -Screenshot 'D:/KDETests/native.png'
```

追加 `-Offscreen` 可用无窗口平台。截图模式最多等候 45 秒；D-Bus 15 秒、后台注册 20 秒、单次 D-Bus 探测 2 秒、CLI 10 秒。官方后台先注册 D-Bus 再生成身份，自检另最多等候 20 秒，要求两个身份文件均非空且后台仍存活，再保留原有 ACL 和 CLI 核验，避免新目录首次启动的误报。它不执行用户按钮、不改音量、不模拟已完成手机配对。

`Test-IdentityReady.ps1 -EvidenceDirectory 'D:/KDETests/new-identity-check'` 只提取并检查生产启动器的就绪函数，使用合成文件验证延迟生成、已有文件、超时和后台退出；不加载完整启动器，也不创建原生界面或真实身份。它不替代下面的实际包生命周期检查。

源码中的 `Test-Package.ps1` 进一步自动检查隐藏启动的原生窗口、同配置重复启动拒绝且不干扰原会话、正常 WM_CLOSE、重启身份保持、自检截图、退出后无包内进程。通过进程 ID 枚举本次 Qt 窗口，不强制抢焦点或操作用户其他窗口：

```powershell
& './packaging/windows-controlpanel/Test-Package.ps1' `
  -PackageDirectory 'D:/Apps/KDE-ControlPanel' `
  -EvidenceDirectory 'D:/KDETests/new-lifecycle-run'
```

## 从已构建工程重新打包

在可运行 MSVC dumpbin、CMake 的开发环境中：

```powershell
& './packaging/windows-controlpanel/package.ps1' `
  -BuildDirectory 'D:/KDEBuild/desktop' -DependencyRoot 'D:/KDECraft' `
  -OutputDirectory 'D:/KDEBuild/packages/new-candidate' `
  -FFmpegRoot 'D:/KDEBuild/redistributable-ffmpeg/install' `
  -RuntimeRoot 'D:/KDEBuild/toolchains/runtime-14.44' `
  -DistributionMaterialsRoot 'D:/KDEBuild/public-materials-20261115-r3'
```

输出目录必须新建。脚本使用项目 CMake 安装规则、Qt windeployqt 及递归 PE 依赖收集；保留 KDE 原插件和动态 QML/KIO 支持。qt.conf 使用相对目录；私有 D-Bus 配置不包含 Craft 的绝对路径。除 windeployqt 已修补路径的 Qt6Core 外，同名但不同内容的依赖必须报错，不能悄悄选一个。

上面的 r3 路径对应 alpha.4 已公开的第三方材料；早期同名无后缀目录的 AMF 索引和材料不是最终版，不应误用于发行。每次新构建仍须重新核对实际依赖与对应源码，不能仅因路径相同就认定材料适用。

打包入口会先检查源码索引的组件名、归档名、SHA-256 格式和分卷文件名；未通过时不会创建包目录。可用 `Test-DistributionMaterials.ps1 -EvidenceDirectory 'D:/KDETests/new-material-check'` 验证这一结构检查。该检查不读取或核验分卷内容，不代表源码对应性或许可证审查完成。

另可用 `python verify-source-bundles.py --index <source-index.json> --parts <分卷目录> --report <新报告路径>` 逐项校验分卷内索引归档的 SHA-256；`test-verify-source-bundles.py` 使用合成 ZIP 检查有效、缺失、散列错误及非法路径。此工具不检查 FFmpeg 主包、上游签名或二进制与源码对应性。

## 预发布限制

干净 Windows VM/普通账户验证、配对/重连与全部原 KDE 功能、完整参考视觉/动作/无障碍/长期测试和发行者代码签名仍未完成。本次包必须通过实际 FFmpeg DLL 许可证核验、Qt 音视频兼容和安装/卸载检查才公开；这不构成覆盖所有司法辖区的法律意见，也不代表完整产品验收结束。
