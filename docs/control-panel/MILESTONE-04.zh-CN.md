# 双端控制面板：阶段四音量实现与验证

更新：2026-10-28。当前仍为开发版本，整体目标未完成。

## 本轮改动

- 提取共用 Windows 音量后端，接通电脑预览滑杆、静音、输出选择和真实百分比。
- 用稳定端点 ID 区分同名输出；修正旧实现的 COM 资源和设备状态生命周期；500 毫秒读取外部变化。
- Windows 请求严格验证整数范围、布尔类型、当前设备和字段，错误/无输出不沿用过期可控状态。
- 手机使用细轨道、圆形滑块，修正旋转测量，并增加百分比和输出菜单。横屏在 Activity 声明中确定，翻页手势仅绑定按钮区。
- 实时拖动以 80 毫秒节流，松手发送最终值；手势固定初始设备，默认输出中途变化和离线不会补发到其他目标。
- Android 列表以新映射整体替换；默认切换清理旧标记，监听器以快照通知，支持回调中移除监听。
- 不改写其他平台原音量插件，不删除用户配置。功能与边界见 [音量说明](VOLUME.zh-CN.md)。

## 编译与自动化

| 项目 | 结果 | D 盘证据 |
| --- | --- | --- |
| Android testDebugUnitTest + assembleDebug | 162 项，失败 0、错误 0、跳过 0；APK 构建成功 | logs/android-volume-build3.log |
| Samsung SM-G973F 安装 | 最终 APK 安装成功 | 本轮 adb install 返回 Success |
| Windows 完整工程 | 构建成功 | logs/desktop-volume-build7.log |
| Windows 完整 CTest | 最终重建后 16/16 通过，17.05 秒 | logs/desktop-volume-ctest3.log |
| 实际 QML 自动交互 | 鼠标滑杆、方向键、静音、输出选择、外部默认变化、无输出禁用通过；不改真实音量 | logs/desktop-volume-ui-test3.log；volumeuitest |
| 原生音频检查 | 音量、外部轮询、静音、多媒体默认切换、基线恢复全部 true | logs/volume-exercise-output.json |

上表 D 盘证据根目录为 D:/KDEBuild。前三次 Windows 编译暴露旧 PolicyConfig.h 依赖包含顺序，已补齐其自身 Windows/COM/端点类型依赖；最终构建不靠偶然包含顺序通过。

## 真实双端联调

1. 先保存三个实际输出的音量、静音和多媒体默认状态，不播放测试音频。
2. 原生测试只降低当前音量；切到备用输出前先将其静音并设为零，结束后恢复基线。脚本在测试后另做恢复，原生回读核对一致。
3. 手机拖动将当前耳机从 45% 改到 15%，电脑原生读值同为 15%。在上下翻页模式中仍停留原匹配页。
4. 电脑端独立写入 5%，手机回传显示 5%；手机点静音后，原生读到 muted=true。
5. 手机打开输出菜单选择已静音且为零的扬声器；电脑默认 ID 确实变化，手机显示 0%。再次拖动后只将扬声器改为 10%，原耳机仍为 5%，没有串控。
6. 最终 APK 加入实时节流后再次实测 45% -> 15%，手机显示、原生回读和页面保持都通过。
7. 结束后恢复原记录：扬声器 19%、耳机 45%、数字输出 29%，均未静音，多媒体默认恢复耳机。测试配置/端点 ID 只存 D 盘，不入 Git。

联调证据位于 D:/KDEBuild/logs：
- android-volume-drag-ui.xml、android-volume-external-five-ui.xml、android-volume-new-muted-ui.xml
- android-volume-output-menu-ui.xml、android-volume-switched-ui.xml、android-volume-switched-drag-ui.xml
- android-volume-final-drag-ui.xml、volume-final-drag-state-output.json
- volume-final-baseline-restored-output.json、volume-baseline-phase4.json
- android-volume-new.png、android-volume-final.png、desktop-volume-new.png（已查看视觉效果）

## 当前产物

APK：E:/git/kdeconnect-android/build/outputs/apk/debug/kdeconnect-android-debug.apk

SHA-256：AE2D53D7625B59E0E887A114228BA7521D6D5F96BC1592FCAFB20C389F9D52C8

Windows 仍是 D:/KDEBuild/desktop/bin 下依赖本机 Craft 环境的开发程序，不是最终可分发包。

## 尚未完成

音量仍有多音频角色、手机写入失败反馈、硬件拔插/休眠、长期运行和无障碍边界待验证或完善，因此 VOL-01 未直接勾为全完成。实际 UI 点击测试通过不等于完整 UI 回归。

此前智能跟随的自动焦点检查仍未通过。图标资源链路、Android 空闲表盘与天气、全部动作/安全/多设备测试、Lint、发布打包和最终 README 仍保留在原目标中。

系统盘在本轮后段一度无空闲，末次检查仅约 180 MB；本项目源码、工具、缓存、构建和日志均在 E/D 盘，未擅自清理其他文件。当前非系统盘构建与测试仍成功，已提示用户腾出系统空间以保障后续系统/应用运行。
