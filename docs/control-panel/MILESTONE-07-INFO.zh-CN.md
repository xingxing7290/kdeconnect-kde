# 阶段七 B：电子墨水信息表盘与三日预报

日期：2026-11-12。个人 fork 的开发里程碑；不代表完整参考复刻、整体验收或正式发行完成。

## 对照与实现

重新对照 Kn2hdY3fkodIznx0UbycNddPnIe、LXzXd2tfwo7iEaxYOxTcmc8yn5b 等本地参考图。原信息表盘只是大时钟和文字，与参考的分区信息布局有明显差距。本轮增加单色边框分区、本机电量、时分秒、当前天气和当日温度范围、个性化文本、三行预报、底部日期/城市/来源时间。

- Android 新增 PanelInformationDial、PanelWeatherGlyph、PanelBatteryState；Qt 新增对应信息/天气图形组件并注册到应用 QML 模块。图形为自行绘制路径，不使用参考截图中的素材。
- 电量只读手机本机 ACTION_BATTERY_CHANGED。非法 level/scale 显示 --%，不默认为 80%。只在信息表盘组合存在时注册，在销毁时注销。电脑预览不假设某一手机电量，明确用 --%。
- 桌面天气同一固定 HTTPS 请求追加三日最高/最低温和 WMO 码，按返回的城市 UTC 偏移生成日期；不读取 GPS。观察值、预报、城市、时区、原模型/获取时间一起校验和缓存。
- 保持 v1，新增字段可选。新端接受旧端只有当前天气的快照，缺预报不编造；新联网端会补取旧缓存缺失的预报。失败保留原数据和原时间，过期隐藏温度/预报，跨城市清理不变。
- Qt 的个人文字和城市按纯文本显示，不把 HTML 标记解释为图片等外部内容。Android Text 本身使用纯文本。
- 日期格式读取可观察的 Android 配置语言，运行中改变语言可以重组，不依赖不可观察的默认 Locale。
- Lint 发现控制面板音量栏的 BoxWithConstraints 没有使用约束，改为相同尺寸/对齐的 Box；未改变滑杆参数、音量协议或共享 System Volume 插件。

源码修改限定控制面板功能与其测试、资源、QML 注册及文档；未修改共享网络类、主清单或其他插件。没有向当前华为手机安装程序。

## 数据来源与边界

[Open-Meteo 预报接口](https://open-meteo.com/en/docs) 规定 daily 变量、Unix 秒、UTC 偏移和模型日期边界。日预报是预测，不是已经发生的实测高低温；免费端点非商业使用边界、隐私和缓存规则继续见 DIALS.zh-CN.md。

本轮单独运行网络检查工具，取得长沙市快照：observedAt=1794455100、fetchedAt=1794455500、14.3°C，预报日期 2026-11-12..14，最高 15.4/18.0/20.5、最低 12.0/11.5/12.3。这里只记录该次归档数据，不称它是之后的当前天气。真实数据进入本地两端渲染检查；普通测试仍使用模拟网络，不把网络访问作为 CTest 的必需条件。

## 检查结果

证据根目录 D:/KDEBuild。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| Android 全量单测与 APK | 200 项，失败/错误/跳过均 0；连续两轮全量通过 | logs/android-info-full11.log（1 分 46 秒）、logs/android-info-full12.log（1 分 55 秒）；reports/android-info-full11、reports/android-info-full12 |
| Android 电量/语言跨版本 | SDK 23、28、37；电量 2 方法 x 3、语言 1 方法 x 3，9 项通过 | PanelBatteryStateTest、PanelDialLocaleTest；logs/android-info-config-sdks10.log |
| Android 天气 | 旧快照、严格日期/上下界/类型、不同城市偏移、预报缓存与过期保持 | PanelWeatherSnapshotTest 7 项 |
| Android 中文五类画面 | SDK 28 原生绘制，断言配置语言为 zh，真实快照渲染并查看截图 | logs/forecast-dials-zh/android-dial-*-live.png |
| Windows 完整构建/CTest | 20/20 通过，23.40 秒 | logs/desktop-info-fullbuild4.log、desktop-info-fullctest4.log |
| Windows 真实 QML | 画廊选择/设置/重启、真实预报范围、电量占位；无数据、旧端、缓存、过期和纯文本检查通过，警告会使测试失败 | paneldialsuitest；logs/desktop-info-qt3.txt 为较早一次明细，最终以 fullctest4 为准 |
| Windows 天气解析 | 数组长度/null/单位/范围/代码/偏移/时区、城市切换、缓存与模型倒退、6 种城市偏移 | panelweathertest；logs/desktop-forecast-ctest1.log 及最终 fullctest4 |
| 独立目录包 | 收集 149 个非系统库；重新生成并通过 5 项原生生命周期检查 | logs/windows-info-package2.log、windows-info-lifecycle2.log；package-lifecycle-info-r2/result.json |
| 实际天气接口 | 单独工具退出 0，输出当前条件与三日数组 | logs/weather-forecast-live-clean.json、weather-forecast-live-clean-error.log |

本地模拟设备、广播、截图不是手机真机或完整 KDE 配对传输验收。电量测试验证真实注册的接收器更新和注销，不把模拟的 43% 当作硬件测量。Qt 预览无手机数据始终显示 --%。

## Lint 与失败处理

首轮 lintDebug 任务退出 0，但报告是 490 errors、212 warnings、1 hint，不能算通过。控制面板有 3 个 NonObservableLocale 和 1 个未使用约束作用域，已在本模块修复；翻译缺项和其他模块项目没有全局屏蔽。

最终复查仍不通过：486 errors（465 Error + 21 Fatal）、212 warnings、1 hint。错误分类为 MissingTranslation 456、ExtraTranslation 20、NewApi 8、MissingDefaultResource 1、MissingLeanbackLauncher 1；本模块仍有 41 项 MissingTranslation 和 5 项警告，没有遗留上述两类代码错误。任务本身退出 0 是原 abortOnError=false 设置的结果，未修改或利用该设置宣称全绿。两轮报告已归档 reports/lint-info-before、reports/lint-info-after；最终日志 logs/android-info-lint2.log，2 分 57 秒。

其他模块的 API 兼容、资源默认值等只读核查结果已经提出范围确认，未得到确认前不擅自修改。仅有中文和英语的控制面板资源仍存在其他语言的 MissingTranslation；未用虚构译文或全局 suppress 伪造全绿。

中途失败与恢复：

1. AndroidX 在本地 SDK 28 接收器测试中检查动态权限失败，合并清单实际含该权限。模块改用仅受保护系统广播的平台注册形式，不扩大清单；符合 [Android 系统广播专用例外](https://developer.android.com/about/versions/14/behavior-changes-14)。
2. 新 QML 初次跨状态更新出现未定义 code 警告，补空值保护；Repeater 视觉子项按 QQuickItem 树查找，不用错误的 QObject 父树假定。
3. 旧空闲 Activity 测试有一次计时断言失败，增加实际单调时钟/输入时间诊断；未改计时运行代码或削弱唤醒/动作断言。后续单独和两轮全量通过，长期稳定性仍待验收，不据此宣称所有抖动原因都已定位。
4. 新状态测试的默认窗口重绘/后台 Snapshot 调度不稳定，保留预期电量和语言断言。测试改为专用 Recomposer、Unconfined 测试作用域与显式 BroadcastFrameClock；用 SideEffect 在提交后发布观测值，并在结束时取消作用域。未添加生产依赖或修改 APK 行为。9 项和两轮全量均通过。
5. 失败日志 android-forecast-build1/2、android-info-full4/7、android-info-config-sdks5/6/8、desktop-info-ctest2 等保留，没有删除或跳过失败测试。

## 产物

- 调试 APK：D:/KDEBuild/artifacts/control-panel-info-forecast-20261112/kdeconnect-controlpanel-debug.apk。
- SHA-256：A148F87127B38269D0453F0535A570194A8C0316C08A682E4ABA3AC1597193B3。
- Windows 内部候选目录：D:/KDEBuild/packages/controlpanel-info-forecast-win64-20261112-r2；入口 Start-ControlPanel.cmd。
- APK 为独立 debug 包名，不覆盖正式包；本轮没有手机安装操作。
- Windows 仍为内部目录候选包，不是已完成干净系统、签名、安装/卸载与发行依赖合规审计的公开成品。

## 下一步不变

仍需逐张参考视觉对照和更多信息/风况/汽车艺术细节、所有按钮/拖入/库/页面与平台动作验证、真实焦点切换、音频其他角色/失败反馈、原 KDE 功能、无障碍/字体缩放/长时间/多设备/断网重连、干净 Windows/普通用户、最终签名安装及对应源码/第三方说明。当前华为手机继续不安装；后续真机操作需要新的明确同意。

原计划和完整交付目标没有缩减，不能把 200/20 的开发测试通过当作整个软件已经完成。
