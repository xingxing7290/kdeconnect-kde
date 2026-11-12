# 阶段十一：离线进程恢复修复、图标附件与多页联调

日期：2026-11-12。本阶段继续原完整目标，只修改 Android 控制面板模块；桌面仅扩展测试工具，不改变其他插件、配对实现或共享网络代码。

## 实际缺口与修复

Android Device.reloadPluginsFromSettings 只在设备可达时加载插件。原 ControlPanelActivity 仅从已加载的插件取得会话；保留在内存中的会话能够离线展示，但电脑离线且 Android 进程已重建时，插件不存在，已持久化的面板没有恢复入口。

先新增真实 Activity 回归，旧代码得到 config=null，单项失败证据保存在 D:/KDEBuild/logs/android-restore-before-fix.log 及 reports/android-restore-before-fix。随后作最小修复：

- 新增 PanelDisplayCache，沿用原 Plugin 的按设备 SharedPreferences 命名，恢复既有 panel_config / panel_weather，不迁移或重写数据。
- Activity 在已配对但无插件时恢复展示专用会话；其连接状态始终为 DISCONNECTED，无 sessionId、待执行队列或网络请求。
- 重连后改用生产插件会话，必须收到本次完整配置才重新启用动作。
- 未配对设备拒绝打开控制页；打开期间取消配对，清空绑定并关闭 Activity。
- 对错误类型的缓存字段忽略恢复，不让损坏的缓存引发启动崩溃。配置与天气仍由原解析器验证。
- 未修改 Device、Plugin 基类、SystemVolume、Run Command、LAN、Payload 或权限。取消配对没有删除用户已保存的展示缓存；后续读取仍要求当前设备已配对。

恢复入口本轮验证的是系统最近任务重建 ControlPanelActivity，不表示新增了所有离线导航入口。离线恢复使用缓存配置中的合法页面；重连后接收桌面当前状态，不把它描述成持久化了所有瞬时界面状态。

## 自动回归与新包

| 检查 | 结果与证据 |
| --- | --- |
| 定向回归 | 新增 14 项全部通过：Activity 5 项、缓存 2 项，均覆盖 SDK 28/37；logs/android-restore-focused1.log，reports/android-restore-focused1 |
| Android 完整单测 | 257 项通过，0 failures / errors / skipped；reports/android-restore-full1 |
| 单测 + APK + 完整 Lint 任务 | 6 分 49 秒结束；logs/android-restore-full1.log。退出成功不等于 Lint 无错误 |
| Lint 实际报告 | 477 errors（457 Error + 20 Fatal）、211 warnings、1 hint，与阶段九一致；主要为翻译和 TV 入口，reports/lint-restore-full1。没有新增 NewApi 或 MissingDefaultResource |
| APK 测试库隔离 | 定义包中无 androidx.compose.ui.test / androidx.test，logs/android-restore-apk-packages.txt |
| APK 签名 | 最低 API 23 的 v1/v2 校验通过，使用既有调试身份，不是正式签名；logs/android-restore-apk-signature.txt |
| Windows | 测试工具目标构建通过，21/21 CTest 通过，35.09 秒；logs/desktop-emulator-pages-build2.log、ctest-emulator-pages.xml |

路径均相对于 D:/KDEBuild。新 APK 为：

~~~text
D:/KDEBuild/artifacts/control-panel-restore-20261112/kdeconnect-controlpanel-debug.apk
SHA256 A88F99EB9CCCD8E98BC449E7435D82832BEE89C9367B8C4FCEF2B3FE29C3A200
~~~

桌面仍使用 controlpanel-buttons-win64-20261112-r1 候选包，其运行 EXE 未改；测试探针单独放在 emulator-e2e/tools，不打入产品包。Android 新包只在专用 AVD 上以保留数据方式更新，未安装实体华为手机。

## 真实双端验证

继续使用阶段十的 Android 34 AVD、独立 ADB 5038、emulator-5580 和独立桌面 profile。普通模拟器 LAN 发现问题尚未解决；本轮经 ADB reverse 的定向发现与正常生产 TLS 配对联调，不算普通 Wi-Fi 验收。图标附件另将专用通道 tcp:1739 映射到桌面 tcp:1739。

桌面测试工具新增 --extend-pages，只接受由 --prepare 创建的原专用单页探针配置，复用生产 PanelEditor 添加图标、第二页及外观选项。重复扩展被拒绝，配置哈希不变。该工具不是正式配置导入器，不应用于日常 profile。

为避免测试期间收取日常媒体、剪贴板或通知，只在这个独立桌面 peer 配置中关闭对应插件；产品默认值和日常配置不变。未测试或操作主机音量、日常快捷键、短信或文件关联。

实际完成：

1. 桌面生成原创蓝底黄圆测试图，经正式导入、附件传输及 Android 校验写入缓存。桌面文件和 Android 缓存 SHA-256 均为 b446bc8a2128046718a10c209f18330d1f2a09732953965a550c22e1ed7ec42c。
2. revision 11 的两页、背景 #DDEBFF、隐藏音量栏及纵向滑动同步。向上滑动到第二页，按钮一次执行专用探针；向下滑动返回首页。
3. 关闭独立桌面 LAN，在 Android 回到桌面后以该调试包自己的 UID 结束其进程。原 PID 3650，系统服务重启后的新 PID 4687；不是只重建内存中的 Compose 组件。
4. 从系统最近任务恢复，生产 Activity 在新进程中仍显示缓存图标、两页和外观，且显示离线。实际触摸无执行。
5. 重连前后执行记录保持不变，证明本次离线触摸未排队补发；随后一次新的在线触摸只增加一条正确记录。
6. 读取本面板专用缓存核对 revision、页数、图标与外观，确认公共展示数据不包含 target、程序路径、工作目录或参数。
7. 独立桌面取消这个测试配对后，Android 退出控制页并显示未配对提示；重新正常配对后控制页恢复。

Android 集成工具增加受 AVD 校验约束的 Swipe、Home、Recents。滑动范围取当前唯一节点的实际边界，不使用历史坐标；不匹配的临时系统提示会中止输入，不重试点击。最近任务截图用于重新进入的任务预览，恢复成功证据则取自新进程的实际 Activity、UI 树和缓存文件，不用任务快照冒充运行界面。

## 证据与边界

本地证据根目录 D:/KDEBuild/emulator-e2e：

- logs/pages-prepare.json、pages-fixture-safety.json：准备结果、资源 hash、页 ID 与保护检查。
- logs/pages-open-ui.json、pages-swipe-up-ui.json、pages-swipe-down-ui.json、pages-second-action-ui.json。
- logs/restore-process-death.json、restore-offline-after-process-ui.json、restore-offline-tap-ui.json、restore-online-ui.json、restore-online-action-ui.json。
- logs/restore-cache-check.json、restore-unpaired-ui.json、restore-repaired-panel-ui.json、restore-android-final.log。
- 每个 UI JSON 引用本地 ui/*.xml 和 screen-*.png。所有原始运行日志、配对文件和整个 profile 只留本地，不上传证书、密钥或 D-Bus nonce。
- 本轮第二页新 token 的执行记录一条；首页旧 token 从阶段十的两条基线增加到三条。离线恢复点击无新增，重连也无自动新增。

本轮未新增 Windows 正式功能，也未完成原生 GUI 点击、普通局域网发现、全部动作/快捷键/lnk、十页上限的真实设备联调、跟随焦点、音频全部角色、多设备长时稳定性、全套视觉/无障碍、剩余 Lint、干净系统安装和最终发行签名。系统最近任务首次显示教学提示，提示自动消失后重新采样才选择任务；进程结束后的立即 PID 读取为空，最终以恢复后的新 PID 与实时界面确认，不将暂时空值当作失败或恢复成功。

完整目标及验收门槛不变，原整体清单保持未勾选。
