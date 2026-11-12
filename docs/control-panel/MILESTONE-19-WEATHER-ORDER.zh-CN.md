# 阶段十九：天气配置顺序修复与小版本冻结

日期：2026-11-12。用户要求先输出功能小版本，当前冻结为 0.1.0-alpha.2；不继续扩展功能，不代表原目标全部验收完成。

## 已确认的问题与最小修复

真实生产 controlpanel 插件在全局天气对象存活、连接插件重建或第二控制端加入后，可能按 STATE、DIAL、CONFIG、STATE 顺序发送同一新 revision。Android 严格修订校验会丢弃先到的天气。问题已用实际 PluginLoader 加载生产插件的隔离回归复现，修复前失败记录保留。

仅修改 controlpanelplugin.cpp：每次 CONFIG 成功进入发送队列后，重推当前天气快照；删除旧的连接/显式请求额外重推，保留天气变化订阅。成功入队不等于网络送达确认。Android 的会话、城市和 revision 校验不放宽，共享连接、音量后端及其他插件不改动。

Windows 回归使用合成天气和进程内记录设备，覆盖插件重建、第二控制端、连续三次仅文本更新且 fetchedAt 不变、不可达/取消配对不发包、恢复连接和无效请求。它不是普通 LAN 或真实双手机测试。Android 增加未来天气先到、配置生效后仍须显式重发的契约测试。

## 实际验证

| 项目 | 结果 |
| --- | --- |
| Windows 完整构建 | 退出 0 |
| Windows CTest | 22/22，通过，29.62 秒 |
| 真实插件修复回归 | 修复前失败；修复后单套通过，直接 QtTest 3 个测试阶段均通过 |
| Android 本地测试 | 44 套件、274 项，失败/错误/跳过 0 |
| Android 构建任务 | testDebugUnitTest、assembleDebug、lintDebug；2 分 19 秒，退出 0 |
| Lint | lintReportDebug 复用已有报告；493 错误、211 警告、1 提示，仍未通过 |
| Windows 当前内部包 | 打包退出 0，149 个非系统运行库；独立配置无窗口 SelfTest/截图退出 0 |
| APK | 与阶段十八运行产物相同；本轮仅新增 Android 测试；v1/v2 签名再次通过 |

QtTest 仍报告旧 Windows 私钥/证书权限警告；启动器独立配置检查实际 ACL。APK 签名工具仍有 META-INF 不受 JAR 签名保护警告，不宣称零警告或生产签名。

旧包在专用 AVD 取得真实长沙天气并保存初始界面；用户要求先交小版本后，未完成旧/新包同条件 AVD 对照，不能用插件单测代替该项。阶段十八五表盘、字体和触摸证据继续有效，但不升级为本轮新桌面包完整联调通过。

## 本地证据与清理

证据在 D:/KDEBuild/unified19-weather：
- logs/plugin-before-qtest.txt 保存实际失败与包顺序；plugin-fix-qtest.txt/.xml 保存修复后结果。
- logs/ctest-full1.log/.xml、desktop-full-build.log、android-full.log、package.log。
- logs/release-smoke-output.log、release-smoke-error.log 和 release-smoke.png；独立 release-smoke-profile 只留本地。
- before-fix/logs 和 ui 保存实际旧包界面，logs/real-observation.json 保存真实天气，与合成测试分开。
- 空闲选项已恢复 digital、disabled、空城市/文本、5000 毫秒；AVD 应用语言恢复空列表；专用 AVD 和 ADB 5038 已停止，测试包进程为 0。用户 ADB 5037 与其他 D-Bus 不动。

私有配置、证书、私钥、D-Bus nonce 和原始身份日志不上传。源代码在 E 盘，构建、缓存和交付物在 D 盘；C 盘空间不足，未在 C 盘新增开发目录。

## 未完成范围

本版不完成普通 LAN、真实多控制端长时、全部原生按钮/快捷键/跟随/提权兼容、实际音量三角色写入回滚、所有表盘大字体与无障碍、系统手势、原 KDE 全功能、剩余 Lint、干净 Windows 安装/卸载及第三方发行材料验收。

共享无效后端输入保护、主机可见窗口/焦点及音频写入尚待用户确认，不擅自执行。GitHub 仅个人 fork，不提交 KDE 官方 MR。桌面推送遵守仓库的明确风险确认要求。
