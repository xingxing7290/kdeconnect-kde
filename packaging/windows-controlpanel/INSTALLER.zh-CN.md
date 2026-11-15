# 个人 fork Windows Release 安装器

此安装器仅包装现有控制面板启动器，不修改原 KDE 功能、系统音频或配对逻辑。
先单独配置 CMake `CMAKE_BUILD_TYPE=Release` 并完成构建，使用 package.ps1
收集运行依赖；再以 Inno Setup 7 编译 installer.iss，指定 PackageRoot 和 OutputRoot。

目标 Windows 10/11 x64，不请求管理员权限；默认 D:/Apps/KDE-ControlPanel，
可选择其他有写入权限的本地目录。安装器创建开始菜单入口与卸载项，不设置
开机启动、服务或防火墙例外，不自动启动应用。卸载只移除安装文件，保留
profile 中的个人身份、配对与配置。不要把多个版本安装到同一正在运行的目录。

目前没有 Windows 代码签名证书，安装器和个人应用未经发行者代码签名，系统
可能显示未知发布者或 SmartScreen 警告；不要为了运行而关闭系统安全保护。

公开二进制发布的前置检查仍未完成：依赖的完整版本/版权/许可证声明及所需
对应源码、干净 Windows 普通用户安装卸载与全部功能验收。现有依赖目录中仅
有 SDL2 许可证，不能视为完整材料。未补齐前只生成本地安装候选或个人仓库
的未发布草稿，不公开该 Windows 二进制。源码可同步个人 fork，不提交 KDE 官方 MR。

安装权限说明：https://jrsoftware.org/inno-setup/documentation/topic_setup_privilegesrequired.htm
