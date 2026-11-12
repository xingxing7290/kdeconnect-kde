# 阶段十：隔离模拟器与桌面候选包端到端验证

日期：2026-11-12。本阶段只新增测试工具和记录，不修改两端运行模块，不代表整体目标完成。

## 环境与隔离

- Android 34 x86_64 AVD：KDEPanel34_E2E_20261112，数据在 D:/KDEBuild/emulator-e2e，源码仍在 E:/git。
- 使用独立 ADB 5038，只操作 emulator-5580，并核对 ro.kernel.qemu 和 AVD 名称。原 ADB 5037 与连接的华为手机没有安装、卸载或输入操作。
- 桌面使用 controlpanel-buttons-win64-20261112-r1 内部候选包和独立 desktop-profile-r2，独立 D-Bus、配置、证书、日志，不沿用日常配置。
- APK 仍为阶段九 a525f74 的包；桌面运行文件仍为阶段八 561d6455 的候选包。本阶段未替换发布包中的 EXE 或 APK。

## 实际结果与边界

| 检查 | 结果 |
| --- | --- |
| 普通模拟器网络发现 | 未通过。mDNS 将模拟器服务解析到宿主机网卡地址，桌面出现连回自身、连接限流和反向 UDP 失败；没有修改共享网络代码或防火墙来掩盖问题 |
| 隔离通道 TLS | 通过。ADB reverse tcp:1717 tcp:1716，仅注入定向发现包；两端生产 TLS 身份校验、加密身份交换和配对逻辑照常执行 |
| 配对 | Android 点击 Request pairing，独立桌面 CLI 接受；CLI 显示 paired and reachable，真实 controlpanel 插件加载 |
| 配置同步 | 桌面 PanelEditor 通过原存储接口创建 revision 3 测试配置，Android 生产面板显示测试按钮、18 槽位和系统音量状态 |
| 触摸到程序执行 | 通过。Android 一次触摸启动专用测试 EXE，输出的中文/空格参数与中文/空格工作目录完全匹配 |
| 离线保护 | 关闭独立桌面 LAN 后端，Android 显示离线；在原按钮位置触摸，测试执行记录数不增加 |
| 重连 | 两轮断开/重新启用后，以定向发现恢复真实 TLS；无需重新配对，面板保留，最终一次触摸只新增一条正确执行记录 |
| Windows 回归 | 新测试目标构建成功，现有 CTest 21/21 通过，31.74 秒 |
| 测试工具保护 | 不匹配 AVD、非法桌面 ID 被拒绝；未标记目录和已有按钮的配置被拒绝，配置哈希不变 |

这是通过 ADB 通道的生产双端联调，不是普通 Wi-Fi 发现、实体手机、跨电脑网络或全部功能验收。没有发送短信、操作日常应用、测试系统快捷键或修改主机音量。界面读取到 45% 不代表完成音频角色切换验证。

Android 运行源代码与 APK 未改，本阶段没有重跑 243 项 Gradle 单测或 Lint；阶段九两轮 243 项通过和 Lint 477 errors / 211 warnings / 1 hint 仍是最新对应报告，不能把本阶段端到端结果记成 Lint 通过。

## 工具与复现

Android 仓库新增 tests/integration/Invoke-ControlPanelEmulatorUi.ps1。需要 PowerShell 7.4，所有命令固定传入独立 ServerPort 和 emulator 序列号，并再次检查 AVD。每次操作先取新 UI 树，唯一匹配后才点击，输入不自动重试；只读 UI 采样最多三次。命令 30 秒截止，保留 GUID XML、截图和 dump 结果，拒绝使用旧 XML。

示例（仅适用于专用 AVD，不能把日常手机替换进来）：

~~~powershell
$ui = @{
    AdbPath = 'D:/Android/SDK/platform-tools/adb.exe'
    Serial = 'emulator-5580'
    ExpectedAvd = 'KDEPanel34_E2E_20261112'
    EvidenceDirectory = 'D:/KDEBuild/emulator-e2e/ui'
}
& ./tests/integration/Invoke-ControlPanelEmulatorUi.ps1 @ui -Action State
# 先确认独立桌面身份与 reverse 路由，不能使用日常 D-Bus/配对身份：
& ./tests/integration/Invoke-ControlPanelEmulatorUi.ps1 @ui -Action DiscoverDesktop -DesktopId '<独立桌面 CLI --my-id>'
& ./tests/integration/Invoke-ControlPanelEmulatorUi.ps1 @ui -Action Tap -Description 'E2E 参数验证'
~~~

DiscoverDesktop 要求已存在的 reverse tcp:1717 tcp:1716，不创建/覆盖路由，不接受或自动配对。发现包使用测试桌面真实 ID，后续加密身份仍由两端校验。PowerShell 直接管道到 adb nc 曾持续等待，改为关闭子进程标准输入；该工具没有无界等待。

Windows 仓库新增测试目标 panelemulatorintegration，仅在测试构建中使用，不安装到产品包：

~~~text
cmake --build D:/KDEBuild/desktop --target panelemulatorintegration --parallel 2
panelemulatorintegration --prepare <专用空白候选包 profile>
ctest --test-dir D:/KDEBuild/desktop --output-on-failure --timeout 90
~~~

prepare 必须有候选包私有 profile 标记和单页全空配置，通过原 PanelEditor 保存按钮。probe 只在该 profile 的 e2e-probe 写独立 UUID JSON，不执行 shell、不打开用户程序。每条记录含 token、参数及工作目录，可据此计数。测试 EXE 不得移入日常配置或作为最终产品入口。

## 原始证据

本地根目录 D:/KDEBuild/emulator-e2e：

- logs/probe-prepare.json：预期参数、工作目录、revision 和测试 token。
- desktop-profile-r2/e2e-probe/*.json：两次在线触摸对应两条记录；离线触摸无新增。
- logs/offline-tap-ui.json、reconnect2-ui.json、reconnected-tap-ui.json，及其中指定的 ui/*.xml、screen-*.png。
- logs/ctest-after-emulator.xml、fixture-safety.json、adb-helper-safety.json。
- logs/android-network-before-reboot.log、android-network-final.log 和独立桌面诊断日志。原始运行日志及配对身份只保留本地，不上传私人证书、密钥、D-Bus nonce 或整个 profile。

产物哈希仍为：

- APK SHA256：207A13D4EE4BA74473B476F6C0FEFD07D985D08B1917182845BE388D1C07FC60
- 桌面 app SHA256：E3A3F31699F118025260262D26EDB803D60F2A43904A70D7E34C539BB00F7E26

## 失败、环境异常和后续

- 模拟器初次启动及重启都出现 System UI ANR，清除系统弹窗后才继续。未把它当成 KDE 应用崩溃或性能基准。
- 旧 emulator 的 redir 删除后仍出现监听残留，通过仅重启专用 AVD 清除，AVD 数据保留。
- 最初将 UIAutomator 的 enabled/clickable 字段直接当作 Compose Surface 状态，断言不成立；改用生产离线提示、实际触摸和测试进程记录核验，不改应用代码制造通过。
- Windows 测试 EXE 继承 GUI 子系统，直接执行时 PowerShell 的 LASTEXITCODE 为空；保护检查改用显式 Process.WaitForExit(5000) 和 ExitCode。
- Windows 原生 GUI 自动化运行环境初始化报 failed to write kernel assets / 系统找不到指定的路径，重置后仍失败。本阶段桌面操作仅使用 CLI/D-Bus，原生 GUI 点击验收未完成。

仍需普通局域网发现、图标附件、全部按钮动作、多页/跟随、全部音频角色、多设备、长时重连、进程恢复、完整视觉/无障碍、剩余 Lint 与干净 Windows 安装发行验收。原计划整体验收项保持未勾选。
