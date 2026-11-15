# Windows 0.1.0-alpha.4 可再分发 Release 构建

2026-11-15，用户确认替换不可再分发 FFmpeg、执行兼容性测试、补齐发行材料后，
直接向个人 GitHub 公开。仅更改 Windows 打包和测试入口，不改应用业务逻辑、
共享 Craft 前缀、系统运行库、系统音量或 Android。不是 KDE 官方发行版。

发布标签 controlpanel-release-v0.1.0-alpha.4；配套 Android 继续使用已经公开的
controlpanel-release-v0.1.0-alpha.3（独立个人包名，正式签名 Release 构建）。
两端控制面板协议没有因本次依赖替换发生变化。

## 依赖与对应源码

旧包实际 avcodec DLL 返回 nonfree and unredistributable，原草稿不得公开。
新 FFmpeg 来自官方签名核验通过的 8.1.1 源码，保留原编解码依赖，使用原 Craft
的 MSVC LAME 名称与旋转元数据补丁，不开启 --enable-nonfree。所有 7 个生产
DLL 实际返回 GPL version 3 or later；本次应用发行选择兼容的 GPLv3 许可选项。

编译器 cl 19.44.35230、应用私有 CRT 14.44.35211、SDK 10.0.22000.0。
工具与 CRT 取自微软官方清单并核验 SHA-256，只放在隔离 D 盘目录；不升级共享
Visual Studio 或系统 CRT。打包脚本仅允许显式选择并核对散列的 FFmpeg/CRT
覆盖项，其他同名不同内容的依赖仍报错。Windows 文件名大小写归一化后再比较。

发行材料包含 82 个开源组件的对应源码/完整所用头文件、版本、来源、散列、
版权/许可证、Craft/配方快照和补丁。静态链接组件也纳入清单。AMF 只需完整
public/include 头文件，SDK 中未使用的预编译 FFmpeg、PDB 和示例排除；原始
上游包与所用头文件快照的散列分开记录，不冒称快照获得上游签名。
Microsoft CRT 和 SDK D3D 文件未修改，保留原许可与第三方通知。

第三方源码主 ZIP 与全部 part-N ZIP 都是独立普通 ZIP，解压到同一目录即可，
不是需要二进制拼接的分卷。主 ZIP 含索引、配方、构建说明及许可证；part ZIP
含 source-index.json 所指向的源代码压缩包。应用源码作为独立附件一并提供。
安装器显示 GPLv3 与第三方通知，并提供开始菜单中的许可证入口。

## 实际测试

- 桌面 CTest：22/22 通过，31.68 秒。
- 旧包基线：同一套 12 个用例，验证解码功能正常且旧许可检查确实拒绝 nonfree。
- 新包：12/12 Qt 实际解码/控制检查通过。
- 安装后：再次运行同一套 12/12，通过实际 DLL 路径确认使用安装目录中的
  avcodec 和 vcruntime，而不是误用开发环境。不是额外 12 种独立格式。
- 格式：PCM/WAV、MP3、AAC/M4A、FLAC、Vorbis、Opus、H.264、HEVC、VP9、AV1；
  另有中文/空格路径下暂停、跳转、恢复/变速，以及损坏文件错误处理。
- 新旧测试的音频帧数、视频帧数和首帧图像散列一致。全部媒体为两秒合成样本，
  不据此宣称所有真实文件或长时播放已通过。
- 包内首次启动、同身份重启、中文路径、自检截图、退出清理通过。
- EXE 静默安装后核对应用、7 个 FFmpeg DLL 和 CRT 散列；HKCU 卸载项存在；
  安装后再次检查多媒体与两轮启动；卸载移除程序/登记，保留测试配对身份。
  临时兼容检查 EXE 已移除，不进入交付包。

音频通过 QAudioBufferOutput 解码到内存，没有创建 QAudioOutput，不写主机音频。
视频为无窗口软件解码，不代替真实 GPU、声卡或所有驱动组合验证。

## 保留的失败记录和边界

原 MSVC 太旧不能链接缓存的 LTCG 库；已改用隔离工具集。错误 PATH 下的 sed
曾生成空导出表，已修正，失败生成物移到专用目录留档。原始构建警告、可选
configure 探测失败和所有最终通过日志均保留在 D:/KDEBuild/logs/redistributable-*。
未把可选探测失败伪装成运行时崩溃，也未通过关闭断言压制兼容性失败。

发布脚本还修正 PowerShell 对 REST 数组的封装，强制目标为单一数值 Release ID，
避免多条 Release 被拼成错误上传地址；上传后检查长度、状态和 GitHub 摘要。
旧超大源码附件只存在于未发布草稿时，已移除，本地原件保留；未公开其中无关二进制。

Windows 安装器尚未发行者代码签名；普通账户/干净 VM、真实 LAN 多设备、全部
原生动作与音频角色切换、长期稳定性、完整视觉/辅助技术验收仍未结束。
Android 既有 493 项 Lint 错误仍保留，本次没有重新编译或安装实体手机。
本版本可作为公开功能预发布，不是稳定版或覆盖全部司法辖区的法律保证。

## 下载与校验

Windows：https://github.com/xingxing7290/kdeconnect-kde/releases/tag/controlpanel-release-v0.1.0-alpha.4

安装器 kdeconnect-controlpanel-0.1.0-alpha.4-windows-x64-setup.exe：
125706019 字节；SHA-256
F04971C6F7CEA31440BB4B239F9DF1A4CBEB5F7C1A17E273D67E4D8635F04791。

仅同步 xingxing7290 个人 fork，不向 KDE 官方提交 MR。此前的公开标签/附件保持不变。
