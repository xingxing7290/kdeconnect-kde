# 阶段十八：参考视觉、五表盘联调与信息区裁切修复

日期：2026-11-12。完整目标继续进行。本批对照结果与明确设计差异见 [VISUAL-18-COMPARISON.zh-CN.md](VISUAL-18-COMPARISON.zh-CN.md)。

## 本批修改范围

- Android 只改 PanelInformationDial 的当前天气区：图标和温度占用上部，状态与当日温度范围并排放在底部，减少继承行距造成的挤压，保留原字体大小、缩放、数据和完整温度数值。
- 新增真实缓存/天气尺寸的布局用例，使用 TextLayoutResult 检查温度、状态、范围的完整可见行高与天气区边界，覆盖 SDK 28/37。测试数据是显式合成样本，不是在线天气。
- Android 集成 UI 工具增加受范围约束的节点内触摸比例，默认仍为中心，旧参数位置不变；实际输入前仍核验专用 AVD、唯一节点和新 UI 树。用从控制页观察到的按钮中心核验唤醒穿透，而非只触摸空位置。
- Windows 运行 QML 仅增加三个 objectName；新增两尺寸的四入口、五预览、设置/确认/编辑弹窗自动检查与截图。使用假音频后端，不改实际输出。
- 桌面隔离测试工具新增 --unified-dial，沿用既有十页/十八槽及探针路径/参数保护，只经 PanelEditor 修改本测试 profile。disabled 恢复原空闲设置；没有扩大共享网络、音频或其他插件范围。

## 失败、修复与回归

先前的位图测试能证明“有绘制内容”，不能证明最低/最高温完整可见。真实 AVD 首次五表盘运行中，信息表盘当日温度范围只剩上沿。新增回归先在旧布局失败：panel-info-today-range must retain every displayed line，原始报告已保存。

修复后定向两项通过，随后运行 testDebugUnitTest、assembleDebug、lintDebug：

| 检查 | 实际结果 |
| --- | --- |
| 裁切回归修复前 | 精确筛选的默认 SDK 用例 1 失败，56 秒；不删除失败证据 |
| 修复后定向 | SDK 28/37 共 2 项通过，59 秒 |
| Android 完整回归 | 44 套件、273 项，失败/错误/跳过 0；任务退出 0，3 分 51 秒 |
| Lint 实际报告 | 473 Error + 20 Fatal = 493 错误，211 Warning、1 Hint；NewApi=0、MissingDefaultResource=0，仍未通过 |
| Windows 完整构建 | 当前源码构建退出 0；有既有测试拖放构造器弃用警告，不称零警告 |
| Windows 全部 CTest | 21/21，失败/跳过 0，29.39 秒 |
| 桌面真实天气快照显示测试 | 单套额外通过，1.64 秒；不代替全量 |
| 工具保护 | 非法表盘返回 32，配置哈希不变；非法触摸比例在实际输入前拒绝 |
| APK | v1/v2 签名通过，安装到专用 AVD 的文件哈希与产物一致，未安装实体手机 |

Lint 错误数未变化，不把任务退出 0 当作绿色。其他原测试断言未放宽；未加入假翻译、忽略规则或改变 APK 的测试资源设置。

## 五表盘实际联调

使用已有专用 Android 34 AVD KDEPanel34_E2E_20261112、ADB 5038 / emulator-5580，以及独立 desktop-profile。Windows 运行包仍为 controlpanel-unified16-win64-r1（运行基线 e1153b466），没有把测试探针放进产品包；本批源码完整构建不等于重新发行 Windows 二进制。

先使用阶段十七 APK，再使用本批新 APK，分别完整执行数字、指针、天气、信息和汽车五种表盘：

1. 通过生产 PanelEditor 保存 dialId、城市、文本及默认 5000 毫秒设置；核对 Android 生产缓存已收到对应选项。
2. 观察真实空闲画面，保存新 XML/截图。等待约 6.2 秒加 UI 采样，只证明超过阈值后进入，不当作精确计时或性能基准。
3. 触摸原首页探针按钮中心。该点从进入表盘前实际 UI 边界取得，并核对位于当前表盘节点中且旋转未变；不沿用跨页面历史坐标。
4. 第一次触摸只唤醒，五次均不新增执行；完成后新的在线触摸才新增一次。前一轮记录从 3 到 4，后一轮从 4 到 5，旧记录保留。
5. 信息表盘稳定刷新后，以 Android 缓存的真实天气核对：当前 14°C，当日 13° / 16°，完整范围行高 46 像素；三天预报、本机电量、城市、来源和获取时间均在实际截图中。

真实数据来自 Open-Meteo，未发送 GPS、按钮路径或主机窗口标题。首次快照温度 14.2，后续模型刷新为 14.0；保留不同获取/模型时间，不用旧快照冒充新查询。单位为摄氏度，屏幕按原规则取整。

有一项现象尚未收口：新 APK 五表盘复跑的一次信息画面显示天气暂不可用，稍后在稳定信息页取得刷新后数据。桌面仍有先前有效缓存；需要在下一批独立复现并核对配置 revision、重连与天气重推的先后，尚未确认根因，不能直接归因于外部网络，也不把五次唤醒成功当作天气所有状态通过。本批未为此修改插件或共享连接实现。

## 内部候选与原始证据

~~~text
D:/KDEBuild/artifacts/controlpanel-unified18-20261112/kdeconnect-controlpanel-unified18-debug.apk
30872225 字节
SHA256 B429B09B7FF21B0986754C5732A99E0934A249786EB75CF1485F8320DABF84E4
~~~

它是新内部候选，不覆盖已发布的 controlpanel-v0.1.0-alpha.1 标签、APK 或 Windows ZIP。

证据根目录 D:/KDEBuild/unified18-visual：

- logs/info-before-fix.log、info-focused-fix.log、android-full-fix.log、lint-summary.json；reports/info-before、info-focused-fix、android-full。
- logs/desktop-full-build.log、navigation-test.log / .xml、ctest.log / .xml、desktop-live-dial-test.log / .xml；desktop 中保存常用与最小尺寸截图。
- logs/dial-scenario.log、dial-steps.json、dial-summary.json：修复前 APK 的五表盘联调。
- logs/dial-scenario-after-fix.log、after-fix/logs/dial-summary.json 和对应 ui：新 APK 五表盘联调，包含暂不可用状态，不只保留最终成功图。
- logs/info-final-ui.json、info-android-weather.json、info-final-range-check.json：稳定刷新后真实数据与完整温度范围检查。
- logs/live-observation.json 为实际快照；Android 本地渲染图片在 android-render，桌面对应快照图片在 desktop/live-dials。合成布局测试与真实数据分开标识。
- 本机编排入口 E:/git/kdeconnect-tooling/run-unified18-dials.ps1，仅调用仓库内工具，不进入产品包。
- logs/restoration-and-apk-check.json、cleanup.json：空闲开关/城市/文本恢复原值，revision 正常从 38 递增至 65；应用语言恢复原空列表，启动器退出 0，专用模拟器/ADB 5038 已停止，包进程 0；用户 ADB 5037 和无关 Craft D-Bus 保留。
- 整个私有 profile、证书/私钥、D-Bus nonce 与原始身份日志仅留本地，不上传 GitHub。

## 下一步与原门槛

优先复现并判断天气缓存/配置推送时序，再继续全部表盘大字体、长城市/文本、无障碍与系统手势。普通 LAN、真实多设备长时、原生焦点/动作和实际三角色音频、原 KDE 全功能、剩余 Lint、干净 Windows 环境、发行依赖/对应源码与最终包仍未完成。

主机原生测试、共享无效后端名称保护和桌面 fork 推送的确认尚未收到，继续不擅自执行。Android 本批中文提交可同步个人 fork；桌面本地提交待确认再推送。整体目标保持进行，不勾选全部验收。
