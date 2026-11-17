# 个人 fork Windows Release 安装器

此安装器仅包装现有控制面板启动器，不修改原 KDE 功能、系统音频或配对逻辑。
先单独配置 CMake `CMAKE_BUILD_TYPE=Release` 并完成构建，使用 package.ps1
收集运行依赖；再以 Inno Setup 7 编译 installer.iss，指定 PackageRoot 和 OutputRoot。
可额外通过 `AppVersionName` 和 `OutputFilename` 宏为本地候选指定版本与文件名；
未指定时保留旧 alpha.4 默认值。更改显示版本不会改变 AppId，测试安装到同一目录
仍可能影响已安装版本，须使用隔离环境和独立目录；不应将本地候选误作正式发行。

目标 Windows 10/11 x64，不请求管理员权限；默认 D:/Apps/KDE-ControlPanel，
可选择其他有写入权限的本地目录。安装器创建开始菜单入口与卸载项，不设置
开机启动、服务或防火墙例外，不自动启动应用。卸载只移除安装文件，保留
profile 中的个人身份、配对与配置。不要把多个版本安装到同一正在运行的目录。

目前没有 Windows 代码签名证书，安装器和个人应用未经发行者代码签名，系统
可能显示未知发布者或 SmartScreen 警告；不要为了运行而关闭系统安全保护。

公开包使用独立的可再分发 FFmpeg、匹配的应用私有 CRT，以及归档的完整对应源码、
配方/补丁、82 个开源组件版权声明和 Microsoft CRT/SDK 许可。package.ps1 强制
传入这些输入，并核验实际 DLL，不从共享 Craft 前缀悄悄保留 nonfree 构建。
干净 Windows 普通用户、全部功能验收和代码签名仍是预发布限制，不伪称已完成。
源码仅同步个人 fork，不提交 KDE 官方 MR。

安装权限说明：https://jrsoftware.org/inno-setup/documentation/topic_setup_privilegesrequired.htm
