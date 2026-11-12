# 双端控制面板：阶段六基础表盘与天气

更新：2026-11-12。整体目标仍未完成，本阶段为开发版基础实现与本地验证。

## 实现

- Android 实际 ControlPanelActivity 接入单调时钟空闲状态机。持续触摸/按键不进入空闲；整次首次唤醒手势与键盘组合不送给底层按钮；后台暂停、恢复重新计时。
- 实现五类原创画面：数字、椭圆指针、圆钟加天气、浅色信息、汽车示意图；真实本地时间每秒更新，城市/文字取配置。汽车不冒充车辆遥测。
- 电脑表盘页具有推荐、当前、更多预览及确认/取消；设置城市/文本原子保存，取消不提交。
- Open-Meteo 城市查询与当前模型天气获取、限量验证、缓存、按配置修订同步给手机。显示来源、解析城市、模型时刻与获取时刻，区分加载/缓存/过期/关闭/不可用。
- 拒绝未知代码、错误单位、数值类型错误、过期/明显未来数据、超大响应及旧模型时刻倒退。失败不生成温度，也不把失败时刻伪装成新获取时刻。
- 详细限制、隐私、服务使用条款与行为见 [表盘说明](DIALS.zh-CN.md)。

## 范围确认

用户确认了正式 Android 资源纳入本地单测，以及旧 Run Command 测试设备缺少 ID 的一行夹具修复。Run Command 运行代码和断言均未改动。测试之外的共享传输/其他插件行为没有本阶段改动。

Java 21、Robolectric SDK 37 运行库在 D 盘安装/缓存。仅本机构建 JVM 使用 --add-opens=java.base/jdk.internal.access=ALL-UNNAMED，未改变 APK Java 字节码目标或 Android 最低版本。

用户明确暂不向当前华为手机安装调试包。本阶段没有手机安装/真机联调操作，下面的“渲染”和“Activity 事件”均为本地测试，不冒充真机证据。

## 最终检查

| 项目 | 结果 | 证据（D:/KDEBuild） |
| --- | --- | --- |
| Android 全部 testDebugUnitTest + assembleDebug | 189 项，失败 0、错误 0、跳过 0；构建成功，1 分 57 秒 | logs/android-weather-build7.log；工程 build/test-results/testDebugUnitTest XML |
| Windows 完整构建 | 成功 | logs/desktop-weather-build7.log |
| Windows 完整 CTest | 20/20 通过，32.40 秒 | logs/desktop-weather-ctest4.log |
| 实际 Activity 输入/生命周期 | 首次确认按钮可点；空闲后首触不发动作、第二触才发；后台 10 秒不计入恢复后的空闲时长 | ControlPanelIdleActivityTest，1 项通过 |
| 计时与角度 | 期限、长按、多点唤醒、重复/组合键、后台恢复、开关/时长修改、无障碍唤醒与钟面角度 | PanelIdleControllerTest，8 项通过 |
| 手机天气模型与协议 | 类型/大小/深度、来源/城市/修订、旧数据、缓存保留、一天恢复限制、配置变化清理、错误不禁用动作 | PanelWeatherSnapshotTest 5 项；ControlPanelPluginTest 合计 10 项通过 |
| Android 五类画面 | 正式资源、原生图片运行库实际绘制，分别截图；额外读取已验证的真实天气快照渲染 | PanelDialScreenTest；logs/dials/android-dial-*-live.png |
| 电脑实际 QML | 鼠标选择确认/取消、参数保存/取消、重启保持；五种预览和真实快照缓存显示 | paneldialsuitest；logs/dials/desktop-dial-*.png |
| 天气获取与缓存边界 | 本地模拟 HTTP 验证类型/单位/未知代码、旧数据、超大响应、城市切换取消、缓存与失败保留，不访问外网 | panelweathertest |
| 实时接口 | 独立工具取得真实长沙市模型快照，退出码 0 | logs/weather-live.json、weather-live-error.log；panelweatherintegration |

实际 Activity 事件测试使用模拟设备记录发送包，不连接真实电脑执行程序。渲染测试可选读取已取得的真实 JSON 快照，不在普通 CTest 中联网；实时接口工具是单独的一次检查。

真实快照示例：模型时刻 1794449700，获取时刻 1794449830，温度 14.9°C，代码 51，解析为长沙市 / 中国。数值仅描述该次记录，不作为之后的当前天气。界面将该温度四舍五入为 15°C；已查看两端截图的来源/时间/缓存标签。

## 失败处理记录

1. 初次 Compose 编译暴露嵌套布局作用域错误，已修正。最初空闲逻辑 8 项及 APK 构建通过。
2. 新 Android 画面测试缺少字符串资源，未删/跳过测试。获准开启正式资源后，五类渲染通过。
3. 正式资源使原测试按目标 SDK 37 运行，原 Java 17 不满足要求。使用官方校验过的 D 盘 JDK 21，未降低测试 SDK。
4. SDK 37 测试框架需访问 JDK 内部文件描述符接口；补本地 JVM 参数后，由 72 个环境失败收敛到 4 个旧夹具失败。
5. 4 个旧 Run Command 测试暴露模拟设备缺 ID，取得用户确认后只补固定 ID；最终 189 项全通过。原资源缺失曾遮住该缓存分支，不把旧绿色结果当作新环境已通过。
6. Windows 无窗口测试原来找不到字体，截图出现方框。只为本测试设置系统字体目录并读取已安装字体，不复制字体、不改生产字体策略。QML 警告会使本测试失败；最终可读截图已查看。
7. C++ 首轮编译发现 QStringBuilder 到 JSON 的转换、sendPacket 临时引用和整数钳制重载问题，均在目标模块修复并重新完整构建/CTest。

中间失败日志保留 logs/android-dials-build*.log、android-weather-build1..6.log、desktop-dials-*.log、desktop-weather-build*.log。未将失败、未运行、被中断当作通过。

## 产物与下一步

调试 APK：D:/KDEBuild/artifacts/control-panel-dials-weather-20261112/kdeconnect-controlpanel-debug.apk。

SHA-256：7C077F74C026E6354BF4645D22AD26F347C0A7A675908371DA686597F6223170。

Windows 仍为 D:/KDEBuild/desktop/bin 下依赖 Craft 的开发程序，不是最终安装包。源码 E，工具/依赖/运行库/缓存/日志 D；系统盘仍满，未删除用户其他文件。

仍须完成：逐张参考视觉对照及信息表盘细节、更多生命周期/文字缩放/无障碍/长时间/多设备/网络切换、真实焦点切换、音频其他角色/错误反馈、全部动作与 KDE 原功能回归、Lint、最终打包安装和完整 README。对当前华为手机仍遵循不安装的用户选择。原目标和验收清单不缩减，不能以本阶段全绿声明整体完成。
