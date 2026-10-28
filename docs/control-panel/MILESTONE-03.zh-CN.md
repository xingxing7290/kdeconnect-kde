# 双端控制面板：阶段三智能跟随记录

更新：2026-10-28。属于开发进度，不代表原目标全部验收。

## 实现

- Windows 原生前台窗口/进程识别，按根所有者窗口处理所属对话框，只提取可执行程序名，不读取标题、文档内容或命令行。
- 每 200 毫秒采样、400 毫秒连续稳定去抖；忽略 KDE 自身、无效样本和暂时无法读取的窗口。
- 跟随模式按绑定程序选页，无匹配回首页；大小写不敏感，同名绑定按页面顺序取第一项。
- 手机手动选页保持到下一次稳定程序变化。无关配置编辑不取消手动覆盖；模式变化、页面删除和程序绑定变化按有效状态重新计算。
- 桌面识别弹窗、复制程序名、直接绑定当前页；页面显示名与程序绑定分开。
- Android 显示当前程序；拒绝路径、控制字符及过长的前台名称。电脑公共配置中的绑定也仅保留程序名。
- 使用与阶段二相同的配对连接和 state 包，前台切换不持续写配置、不改变 revision；多个控制器共享状态。

使用方法与边界见 [智能跟随说明](FOLLOW.zh-CN.md)。

## 验证

| 项目 | 结果 | 本机证据 |
| --- | --- | --- |
| Android 单元测试 + APK | 154 项，失败 0、错误 0、跳过 0；构建成功 | D:/KDEBuild/logs/android-follow-build2.log |
| 真机安装 | Samsung SM-G973F 调试包更新成功 | 本轮 adb install 返回 Success |
| Windows 干净重建 | 完整工程 576 项构建步骤成功，随后完成增量更新 | desktop-follow-build3.log、desktop-follow-build4.log |
| Windows 完整 CTest | 增补手动页面删除/模式切换断言后再次 14/14 通过，26.08 秒 | D:/KDEBuild/logs/desktop-follow-ctest3.log |
| 新自动化覆盖 | 连续去抖、瞬时/忽略窗口、时钟回退、停用采样、模式/匹配/回退、手动覆盖、无关编辑、删除页、改绑定、重复绑定、多控制器广播及私有路径投影 | panelforegroundtest、panelruntimetest 和扩展既有套件 |
| 原生窗口自动焦点检查 | **未通过**：测试程序退出码 3，8 秒内未识别到测试窗口成为前台 | desktop-follow-native-probe.log、desktop-follow-native-probe2.log |
| 原生当前程序到手机 | 电脑识别真实 notepad++.exe，手机状态与绑定匹配联调成功 | 以下 UI 树和截图 |

前台自动焦点检查最初使用隐藏启动，随后已改为可见子窗口；独立可见测试窗口有有效窗口句柄，但仍未自动成为前台。没有绕过系统焦点限制或操作用户现有窗口。根因尚未完全确定，不能把这项记为通过，也不能用其他单测替代“真实切换两个前台程序”的完整验收。

## 真机状态联调过程

1. 独立 D 盘配置启动测试服务，与前轮已配对的手机重连。
2. revision 101 使用不匹配的绑定，手机显示真实当前程序 notepad++.exe 与首页。
3. revision 102 将第二页绑定为 notepad++.exe，手机自动显示“跟随匹配成功”按钮。此处通过修改绑定验证真实识别结果的映射，不声称已完成两个真实前台窗口之间的切换。
4. revision 103 换成未匹配的程序名，手机回到首页；revision 104 恢复绑定，手机回到匹配页。
5. 手机手动点首页，在识别结果保持不变时经过后续采样仍停留首页，未被自动覆盖。
6. 读取手机公共缓存，确认 revision 104、mode=follow，既无 target，也无本机 KDEBuild 私有目录。

证据均在 D:/KDEBuild/logs：
- android-follow-native-match-ui.xml、android-follow-native-fallback-ui.xml、android-follow-native-rematch-ui.xml
- android-follow-manual-home-ui.xml、android-follow-manual-hold-ui.xml
- android-follow-match.png、desktop-follow-final.png（已打开检查）

## 本轮构建环境修复

本机仅有中文 MSVC 资源。CMake 记录的 /showIncludes 前缀曾产生编码错误，Ninja 依赖表为空，头文件变化没有触发全部重编译，导致第一轮 panelpeertest 运行崩溃。

本机辅助脚本固定 UTF-8 控制台代码页，重新探测编译器，顺序清理并完整重建。之后检查 panelruntime.cpp 等实际对象的 Ninja 依赖表非空，再运行整套 CTest，协议测试及其余套件全部通过。不以删除失败用例或重复碰运气运行替代修复。清理时只停止本次构建/测试产生且路径已核对的进程；没有删除用户数据。

## 当前产物与剩余项

APK：E:/git/kdeconnect-android/build/outputs/apk/debug/kdeconnect-android-debug.apk

SHA-256：7CCDF60F8E55E32091E6EB559D18D12986A4C39B8E59FDAEEF0144D4AE5DDA2E

Windows 仍为 D:/KDEBuild/desktop/bin 下的开发程序，依赖本机 Craft DLL/QML 环境，**不是最终可分发包**。

本阶段没有完成图标资源链路、Android 空闲表盘/天气、音量完整交互与外部设备切换、所有系统动作/快捷键实测、全部 UI 回归、Lint、长时间/多设备压力测试、最终安装打包及 README 交付。截图还暴露 Android 旋转音量滑杆布局需要修正。智能跟随仍须真实焦点切换、KDE 自身/系统弹窗、权限/UWP 宿主等交互场景验证。原验收矩阵继续保留未完成状态。
