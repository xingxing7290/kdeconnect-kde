# 双端控制面板：阶段二验证记录

更新：2026-10-28。当前为开发版本，全部原始需求仍按 PLAN 验收。

## 本轮实现

- Windows 新主窗口：四个主要入口、黑色圆角手机预览、6 x 3 固定网格、快捷库、页签及显示选项；原 KDE 设备管理窗口保留。
- Windows 编辑模型：按钮增删改、交换槽位、外部文件/文件夹/程序识别、多页增删、名称和应用绑定字段、外观与表盘参数、上次有效配置恢复。
- 双端插件：已配对连接上的配置请求/发布、会话、动作请求/结果、手动页状态；手机重连先同步。
- Windows 动作层：程序、lnk、文件、文件夹、HTTP(S)、结构化快捷键、系统库和既有 Run Command 引用；仅接受电脑私有配置目标。
- Android 横屏入口：18 槽位、名称显示、分页按钮、方向手势、同步/离线状态、错误反馈、确认等待，以及已有音量接口的接入。
- 请求按会话与 sequence 去重，缓存淘汰后的旧请求也不能再次执行。错误配置不替换上一份有效显示。
- Android 多网卡修复：身份包解析不提前消耗设备级连接限流；TCP 建立后再限制重复握手，连接尝试增加 5 秒超时。每 IP 限流和可信网络校验仍保留。
- Windows 可通过 KDECONNECT_CONFIG_DIR、KDECONNECT_PANEL_CONFIG_DIR 指定绝对配置目录，便于本次全部在 D 盘联调。

## 编译与测试

| 项目 | 结果 | 本机证据 |
| --- | --- | --- |
| Android testDebugUnitTest + assembleDebug | 152 项，失败 0、错误 0、跳过 0；APK 成功 | D:/KDEBuild/logs/android-controlpanel-build4.log |
| Windows 完整 CMake 工程 | 所有目标编译链接成功 | D:/KDEBuild/logs/desktop-build10.log |
| Windows 完整 CTest | 12/12 套件通过 | D:/KDEBuild/logs/desktop-ctest4.log |
| Android 真机安装 | 已更新到连接的 Samsung SM-G973F 调试包 | 本轮 adb install 返回 Success |
| Windows 控制页渲染 | 原生 Windows 平台实际生成截图并查看 | D:/KDEBuild/logs/desktop-controlpanel-final.png |
| Android 控制页渲染 | 真机截图并查看 | D:/KDEBuild/logs/android-controlpanel-final.png |

Windows 新套件覆盖配置、存储、协议、动作校验和编辑模型；整套还运行原有通知、插件加载、文件传输、SMS 辅助、LAN 限流/IPv6 和 mDNS 测试。这里的 CTest 通过不等于所有界面和平台动作已实测。

当前 APK：E:/git/kdeconnect-android/build/outputs/apk/debug/kdeconnect-android-debug.apk

SHA-256：54DA3EFA0BE581E8A3C8F0AB9E62BF9400A38F35B4027A24F8CA1A5DE253CE22

Windows 当前开发程序：D:/KDEBuild/desktop/bin/kdeconnect-app.exe、kdeconnectd.exe。运行仍依赖 D:/KDECraft 的 DLL/QML 环境，尚不是最终可分发包。

## 真实联调证据

1. 以独立 D 盘配置启动 Windows 服务，与已连接手机配对，核对双方配对码一致后接受。
2. 手机收到 revision 100 的两页配置，显示“联调测试文件”；读取手机缓存确认没有私有 target 字段。
3. 手机点击该文件按钮后，电脑默认关联编辑器打开 D:/KDEBuild/integration-data/kdeconnect-action-check.txt；点击前没有该测试文档窗口标题，点击后存在。
4. 点击第二页后测试按钮消失且保留 18 个空槽；右滑后回到首页并再次显示按钮。这些页面变化来自电脑回传状态。
5. 停止本次测试服务后，手机出现离线提示，实际可点击节点 enabled=false；恢复服务后自动重新连接并恢复按钮。相关树记录为 android-final-offline-ui.xml、android-final-reconnected-ui.xml。
6. 此电脑有多个实体/虚拟网卡。日志证实手机曾先尝试不可达虚拟地址并超时；完成连接限流和超时修复后，实际服务重启恢复成功。

联调配置、证书、配对信息、截图和日志都只保留在 D:/KDEBuild，不提交仓库。联调目录的 NTFS 权限已校正为当前用户与 SYSTEM 可访问；没有删除原有应用或配对配置。

## 本轮修复的测试与界面问题

- Qt JSON 槽位交换最初保存了 QJsonValueRef，交换后引用随数组改变导致按钮丢失；改为独立 QJsonValue，保留回归测试。
- 完整 KDE 编译启用 QStringBuilder，深度测试中的字符串需要先转换为 QString；已修复并在完整工程运行。
- Windows 临时文件测试改用 QTemporaryFile.remove，使底层句柄正确关闭，并继续断言输入文件确实不存在。
- mDNS 测试在服务启动后明确发送查询，验证实际单播回复，避免依赖发现器未监听端口上的组播公告。
- 保留 ECM 生成的插件搜索路径后追加无窗口测试配置，避免原测试环境覆盖。
- 无窗口渲染最初缺少系统字体，因此界面视觉验证改用真实 Windows 平台。新界面使用 Basic 控件样式并初始化 Breeze 图标资源。

## 尚未完成

- 自定义 JPG/PNG、程序默认图标提取、资源传输/缓存与缺失图标恢复。
- 前台程序识别、自动跟随、去抖、复制程序名称。
- Android 空闲进入表盘、首次触摸只唤醒、全部表盘及真实天气；当前只有桌面画廊与参数配置。
- Windows 预览侧音量滑杆目前禁用；Android 已接现有接口，但音量/静音/默认输出切换仍需完整实测。
- 全部系统动作、快捷键、lnk、异常路径、长时间连接与多设备压力实测。
- 全部弹窗、拖放、导航、旋转/重建、无障碍与视觉自动化回归。
- Android Lint、Windows 安装/可运行包、安装验证、最终 README 与完整验收矩阵。

这些事项继续作为原目标的一部分，不把本轮编译、截图或文件按钮联调当作最终完成。
