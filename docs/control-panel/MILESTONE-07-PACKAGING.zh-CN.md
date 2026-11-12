# 阶段七 A：Windows 独立目录候选包

日期：2026-11-12。本阶段只推进交付工程，不代表阶段七完整回归或整体目标完成。Android 运行代码和所有已有 Windows 功能模块均未改动。

## 实现范围

桌面仓库新增 packaging/windows-controlpanel：CMake 安装、Qt windeployqt、递归 PE 运行依赖解析、相对 qt.conf、包内 QML/KDE 插件与数据、私有 D-Bus 配置、会话启动器和生命周期自检。不是仅把 EXE 从开发目录复制出来。

- 保留 28 个 KDE Connect 插件；递归依赖检查解析 149 个非系统运行库。动态 Qt/KF/QML/KIO 支持另行按目录部署。
- 同名依赖内容不一致必须报错；只有 windeployqt 正常修补路径的 Qt6Core 使用部署版本。
- 启动器仅为子进程设置包内运行路径、专用配置和 D 盘临时/缓存路径，不依赖 Craft PATH。
- nonce-tcp D-Bus 仅监听本机回环；采用本次专用配置与设备身份，不接管用户原有 D-Bus。
- 每个配置目录一个会话；关闭主界面后清理本包的后台进程，不终止外部动作程序或其他 KDE 实例。
- 新建配置目录设置 Windows ACL，仅当前账户、管理员、SYSTEM 可访问；已有非空且非本启动器管理的目录拒绝使用。
- 不修改 Windows 注册表、系统服务、防火墙、开机启动、用户证书信任或系统环境变量。

## 真实检查证据

本机证据根目录 D:/KDEBuild。所有当前华为手机的安装/配对操作均未执行。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| 从现有完整工程安装到新目录 | 通过 | logs/windows-install-stage1.log |
| 新目录自动收集依赖 | 通过，149 个非系统运行库 | logs/windows-package5.log；候选包 runtime-dependencies.tsv |
| 同名 DLL 内容检查 | 通过 | logs/windows-dependency-audit4.log |
| 中文与空格路径、无窗口平台、实际主窗口截图 | 通过 | logs/windows-portable-smoke4.log；logs/windows-portable-r4.png |
| 原生 Windows 平台、启动器 CMD、身份 ACL 和实际 DLL 来源 | 通过 | logs/windows-portable-native4d.log；portable-profile native r4c/logs/loaded-modules.json |
| 中文与空格路径生命周期 | 5 项通过 | logs/windows-package-lifecycle4b.log；package-lifecycle-r4b/result.json |
| 最终候选目录生命周期 | 5 项通过 | logs/windows-package-lifecycle5.log；package-lifecycle-r5/result.json |
| 实际窗口 | 可读截图已查看；不是参考图或模拟 UI | logs/windows-portable-native-r4c.png；package-lifecycle-r5/restart.png |
| 退出后残留进程 | 本候选包无残留；未停止既有 Craft D-Bus | 生命周期检查及进程路径核验 |

生命周期检查为：本次进程原生窗口启动；重复配置拒绝且原会话保留；正常 WM_CLOSE 退出和清理；重启身份不变且自检通过；结束后无包内进程。模块检查只证明启动阶段观测到的模块来自包内/Windows 系统目录，不冒充所有后续插件路径和动作都已验证。

本轮未重新编译 Android 或修改其代码；上轮 189 项全绿仍作为阶段六历史证据。本轮也没有把 Windows 20 套 CTest 的阶段六结果改称新打包环境中的全部功能已通过。

## 中途失败与修正

1. 初次依赖扫描发现已修补 Qt6Core 与 Craft 原库冲突，打包报错停止；补路径规范化及有条件的冲突检查后重跑成功。
2. 初次 D-Bus 地址读取遇到输出尚未出现，修正空值与期限检查。
3. Qt 默认只部署 windows 平台，初次无窗口检查失败；明确部署 qoffscreen 后重测。
4. Windows PowerShell 的进程对象未保存句柄，退出码为空；保留原生句柄后核验真实退出码。
5. CMD 在本机缩减 PATH/PowerShell 7 模块路径下启动失败；使用 Windows PowerShell 绝对路径与仅内置模块路径，限制在 setlocal 会话中。
6. 隐藏启动的原生 Qt 窗口不出现在 MainWindowHandle 中，首次生命周期测试失败。失败实例已按精确进程路径清理；测试改为按本次 PID 枚举原生窗口、发送正常 WM_CLOSE，不强制抢焦点。重跑两目录均 5 项通过。

失败日志保留 windows-package2.log、windows-portable-smoke3*.log、windows-portable-native4*.log、windows-package-lifecycle4.log。不删失败记录、不把首次失败算通过。

## 产物与使用

内部候选目录：D:/KDEBuild/packages/controlpanel-win64-20261112-r5。

入口：Start-ControlPanel.cmd；完整使用/备份/排障说明：候选目录 README.zh-CN.md，源码 packaging/windows-controlpanel/README.zh-CN.md。

候选包不含本机配对密钥或测试个人数据。测试数据明确放在外部 D:/KDEBuild/package-lifecycle-* 和 portable-profile * 目录。Windows 二进制来自阶段六桌面源码 72525cc22cbab3d6615f916c0201940ae45b1e72，本阶段增加打包文件而未修改/重编译其运行模块。Android APK 和 d22aa16069be80322972ab386a096c1b232dc97c 不变。

该包只供内部本地验证，尚非最终安装程序或公开发行物。已有 licenses 文本不等于完整第三方版权/对应源码合规审计。

## 已知边界与下一步

- 只验证当前开发主机的隔离搜索路径，不等于没有开发工具的干净 Windows VM/普通账户验证。
- Windows Qt 标准路径不会随 APPDATA 环境变量全部重定向；已明确配置本项目、D-Bus 认证目录、TEMP、QML/着色器缓存，但不声称所有第三方库绝不访问系统盘。
- 上游 Qt 文件权限检查仍报告 privateKey/certificate 过宽；未压制警告，另行核验实际 Windows ACL 并在 SelfTest 中拒绝意外授权主体。后续仍须普通账户/干净机器验证。
- 强制结束启动器可能绕过 finally；本阶段验证正常退出，不宣称崩溃/系统断电也已覆盖。
- 真正配对/重连、多设备、全部插件/平台动作、完整参考视觉/信息表盘、焦点切换、全部音频角色、无障碍/长期测试、Android Lint、最终签名和安装/卸载、第三方声明与对应源码归档仍未完成。
- 用户仍选择不向华为手机安装。后续本地自动测试继续推进；需要真机授权时另行确认，不借本次打包扩大操作范围。

完整原目标与验收清单保持不变，不把本阶段启动与生命周期通过替代全部功能交付。
