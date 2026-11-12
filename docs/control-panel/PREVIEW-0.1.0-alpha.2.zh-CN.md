# 控制面板 0.1.0-alpha.2 功能体验版

版本标签：controlpanel-v0.1.0-alpha.2。日期：2026-11-12。个人 fork 体验版，非 KDE 官方发行，不代表完整开发和验收完成；旧 alpha.1 标签及附件不覆盖。

## 功能范围

包含 Android 横屏 6 x 3 控制屏、最多十页与应用跟随；Windows 配套编辑器、按钮编辑/拖放与图标同步；程序、快捷方式、文件/目录、网站、快捷键、系统动作及远程命令入口；背景、标签、音量栏和切页设置；数字、指针、天气、信息、汽车五种空闲表盘；沿用 KDE 配对与加密，离线禁用动作并显示缓存。

相对 alpha.1，本版包含后续选项/连接收口、默认图标同步、大字体按钮名称和信息表盘温度范围裁切修复，以及配置发送后重推天气的顺序修复。功能存在不等于所有系统和边界均已验收。

## 下载和使用

- [Android APK 与对应源码](https://github.com/xingxing7290/kdeconnect-android/releases/tag/controlpanel-v0.1.0-alpha.2)
- [Windows 对应源码](https://github.com/xingxing7290/kdeconnect-kde/releases/tag/controlpanel-v0.1.0-alpha.2)

Android 文件：kdeconnect-controlpanel-0.1.0-alpha.2-android-debug.apk。最低 Android 6.0 / API 23，应用 ID org.kde.kdeconnect_tp.debug；可与官方正式版并存，会替换同 ID、同签名旧调试版。上游应用版本仍为 1.37.0-debug，alpha.2 是本 fork 体验版本。使用仓库公开调试密钥，非生产签名；没有安装到实体手机。

Windows 文件：kdeconnect-controlpanel-0.1.0-alpha.2-win64-internal.zip，仅保留本地内部使用。第三方版权声明及对应源码归档未完成，因此不公开上传 Windows 二进制；GitHub 源码 ZIP 不能当作可直接运行的 EXE。内部包解压到 D:/Apps 等本地目录，运行 Start-ControlPanel.cmd，必须保留整个目录；无需管理员权限，不自动修改防火墙、安装服务或开机启动。

配套两端打开原 KDE 设备管理并在可信网络完成配对；Android 对应电脑页打开控制面板，Windows 在桌面控制中编辑。原官方客户端没有新增面板插件，需要本 fork 两端配套。

## 核验和已知限制

当前 Android 44 套件/274 项、Windows 22/22 通过；两端构建成功，APK v1/v2 签名和 Windows 独立配置无窗口启动通过。[阶段十九记录](MILESTONE-19-WEATHER-ORDER.zh-CN.md) 区分生产插件合成回归、历史真实模拟器联调及本轮未完成的旧/新包对照。

Lint 任务复用的实际报告仍有 493 错误、211 警告、1 提示，不算通过。普通 LAN、多设备长时、全部系统动作/焦点/快捷键、真实音频三角色写入及失败回滚、全部视觉/无障碍、干净系统和第三方发行材料尚未完成。Windows 未签名，Android 为调试包，不用于要求可靠性保证的环境。

## 产物校验与源码

| 文件 | 字节数 | SHA-256 |
| --- | --- | --- |
| Android APK | 30872225 | B429B09B7FF21B0986754C5732A99E0934A249786EB75CF1485F8320DABF84E4 |
| Windows 内部 ZIP | 181513149 | 0DEDA37803624F549535CB28DBD7D3F30F4B4DDA8ADE4068E107E232D0B28FAE |

构建源码基线：Android 6603bd6b334a53039c2681c228b6dcff0165117b（相对 161f84ea4 仅增加测试，APK 字节不变）；Windows df3791f4081c5b1c7af89062478ec1e657944b78。后续版本整理只改文档，不改运行代码。两仓库同名标签保留对应源码，发布附件附校验文件和 manifest，不上传私有配对数据。
