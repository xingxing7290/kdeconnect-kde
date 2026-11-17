# 构建与测试记录

2026-11-17 经用户确认的本机安装卸载测试：对本地 `0.1.0-alpha.5-local` 候选安装器（SHA-256 `1348CCE6E6905A80FB9A1C40B2F80F632F6B0FFA3433EB93AFF057E1BEEE0E90`）指定 E 盘独立目录，禁用快捷方式、自动关闭应用和重启，安装成功。对照 guarded 候选目录，5345 个文件逐一 SHA-256 相同；卸载登记指向该目录。建立测试用 `profile/uninstall-preservation-test.txt` 后，仅调用该目录的卸载器，确认应用文件和卸载登记移除、测试数据保留，原 D 盘 D-Bus 进程仍运行。卸载日志因保留数据目录非空而记录 Windows 错误 145，最终报告卸载成功；保留目录和测试文件以便核查。安装/卸载日志分别为 `E:/KDEConnectTestData/install-check-20261117.log` 和 `E:/KDEConnectTestData/uninstall-check-20261117.log`。本次没有启动应用或验证真实双端、干净系统、升级安装及所有功能；该候选仍未签名、未公开发行。

2026-11-16 打包前路径结构补充：此前结构检查会接受组件路径 `libs/../example` 和含目录分隔符的归档名，尽管独立源码校验器会拒绝它们。合成测试先复现组件路径被错误接受，修正打包检查后 10/10 场景通过，覆盖组件遍历、归档遍历、反斜线及路径碰撞；现有 r3 材料 82 项仍通过。夹具在 `E:/KDEConnectTestData/material-path-before-20261116` 和 `E:/KDEConnectTestData/material-path-after-20261116`；未重新打包或发布。

2026-11-16 打包前归档路径防错：将分卷内源码归档路径冲突检查加入现有材料结构校验，先以冲突夹具复现旧校验错误放行，再修复；7/7 结构检查通过，既有 r3 材料索引 82 项通过。调用实际 `package.ps1` 验证冲突在输出目录创建前被拒绝；失败与复测夹具分别在 `E:/KDEConnectTestData/material-collision-before-20261116` 和 `E:/KDEConnectTestData/material-collision-after-20261116`。此项只检查索引结构，归档内容仍需运行独立校验器；未重建或发行安装包。

2026-11-16 第三方源码校验歧义修复：组件路径经 `/` 替换为 `__` 后可能映射到同一 ZIP 成员（例如 `libs/example` 与 `libs__example`），此前不同组件可以错误地复用同一个源码归档。合成夹具先复现校验器错误放行，增加按分卷和归档路径去重后，9/9 合成测试通过；对既有 alpha.4 r3 索引和四个本地源码分卷重新核验 82/82 通过，报告为 `E:/KDEConnectTestData/source-bundle-collision-check-20261116.json`。仅修改打包材料校验工具及测试，未重打安装包或公开发布；此检查不证明源码可复现二进制。

2026-11-16 安装测试环境核查：未找到可用的 Windows Sandbox、Hyper-V 管理命令或常见虚拟机命令；`D:/Apps/KDE-ControlPanel` 不存在，按候选 AppId 查询当前用户及本机常见卸载注册表视图未发现对应记录，但这不能证明所有用户或非标准安装位置均不存在旧安装。当前有 `D:/KDECraft/bin/dbus-daemon.exe` 正在运行。候选安装脚本使用固定 AppId，会创建卸载信息和开始菜单项，并默认尝试关闭占用目标文件的应用；无隔离系统时不应将静默安装当成无副作用检查。本轮只读检查，没有执行安装或卸载。

2026-11-16 Inno Setup 许可核查：本机 `D:/KDEBuild/tools/InnoSetup7/LICENSE.TXT` 对使用和二进制再分发列出了条件；Inno Setup [官方商业许可说明](https://jrsoftware.org/inno-setup/purchase)请求商业用户购买许可，并说明非商业用户不在购买请求范围内。公开下载本身不确定项目用途是否属于商业用途，不能据此判断用户的许可身份；公开发行前仍需按实际用途核对官方定义及随工具提供的许可条件。此次仅查看文本和官方说明，未购买许可、修改安装器或发布。

2026-11-16 guarded 候选包路径与敏感文件名检查：扫描 5345 个文件，未发现名为 privateKey、`.p12`、`.pfx`、`.jks`、password.dpapi 或 local.properties 的文件（仅按文件名检查，不等于完整秘密扫描）。`runtime-dependencies.tsv` 的 152 行和 `source-index.json` 的 82 个 recipe 字段包含构建机 D 盘绝对路径；这些是记录的来源路径，不应被理解为另一台电脑上的安装路径。是否需要为最终公开包进一步去除本机路径及复核源码索引一致性仍待决定；本次未修改材料或对外发布。

2026-11-16 源码材料校验边界补充：分卷核验工具现在拒绝重复组件；新增缺失分卷、重复 ZIP 成员、损坏数据、重复索引测试，合计 8/8 合成测试通过。重新核验本地 r3 分卷 82/82 通过，报告 `D:/KDEBuild/controlpanel-e2e-current/source-bundle-check-dedup-20261116.json`。打包前结构检查也拒绝重复组件，合成夹具 6/6 通过，r3 索引仍通过；夹具目录 `D:/KDEBuild/controlpanel-e2e-current/material-validator-dedup-20261116`。这些检查不证明二进制可由源码复现，也未重新打包或发布。

2026-11-16 本地安装器静态检查：`dumpbin /headers` 可识别 PE 签名、x86 安装器引导程序及 Windows GUI 子系统；候选应用本身仍为 x64 构建。Windows Authenticode 状态为 NotSigned。尝试用本机 7-Zip 26.05 列出安装器内容时报告无法作为归档打开，不能据此证明安装器损坏，也没有完成包内内容完整性检查。未运行安装器；Inno Setup 的静默选项仍会执行实际安装，不能当作无副作用的静态检查。安装、卸载和隔离系统验证继续未完成。

2026-11-16 Windows 本地安装器候选：安装脚本增加可选版本名和输出名宏，未指定时保留 alpha.4 默认值；使用 guarded 候选目录和独立输出路径，Inno Setup 编译出 `D:/KDEBuild/controlpanel-e2e-current/installer-local-candidate-20261116/kdeconnect-controlpanel-local-test-20261116.exe`，125882504 字节，SHA-256 `1348CCE6E6905A80FB9A1C40B2F80F632F6B0FFA3433EB93AFF057E1BEEE0E90`，产品显示版本 `0.1.0-alpha.5-local`，未签名。该候选仍沿用原安装器 AppId，不能在日常安装目录试装；本次没有运行安装器、启动服务或发布，发行材料和完整安装/双端验收未完成。编译器显示非商业用途授权提示，公开发行前须核对工具许可。

2026-11-16 防错入口完整打包复核：新的材料索引检查接受 r3 的 82 项后，完整生成 `D:/KDEBuild/controlpanel-e2e-current/desktop-release-stage-guarded-20261116`，FFmpeg 许可检查通过；候选说明标记为未发行。与上一 r3 候选逐文件对照，新目录 5345 个文件中仅 README 和因路径变化生成的 runtime-dependencies.tsv 不同，EXE/DLL 无差异，源码索引散列与 r3 一致。旧版材料已验证在创建包目录前被拒绝；本次没有单独重跑媒体、实际安装或双端测试，不把相同二进制作为这些测试的替代证明。

2026-11-16 本地公开源码分卷核验：新增 `verify-source-bundles.py`，不解压地逐项读取 r3 源码索引指定的本地 ZIP 条目并比较 SHA-256；82/82 通过，报告 `D:/KDEBuild/controlpanel-e2e-current/source-bundle-check-20261116.json`。合成 ZIP 的正常、缺失、散列不符、分卷路径越界 4 项测试通过。另单独检查主源码包中的 FFmpeg 8.1.1 原始压缩包，SHA-256 与本地构建输入一致（`B6863ADDE98898F42602017462871B5F6333E65AEC803FDD7A6308639C52EDF3`）。这只验证本地归档与索引一致，不是本轮上游签名重验、完整构建可复现性或所有二进制对应性的证明；当前候选仍未获准发行。

2026-11-16 打包材料防错：新增打包前源码索引结构检查，要求非空组件/归档、64 位十六进制 SHA-256 和不带目录的 ZIP 分卷名；拒绝不完整材料时不创建输出目录。合成夹具 5/5 通过，位于 `D:/KDEBuild/controlpanel-e2e-current/material-validator-20261116`；实测早期材料被拒且无包目录，r3 的 82 条通过结构检查。只修改打包模块，尚未以新入口完整重新打包；该检查不核验分卷存在、归档散列或源码对应，不代表发行材料完成。

2026-11-16 发行材料纠正：此前本地候选 `desktop-release-stage-20261116` 误用了早期无后缀 `public-materials-20261115`，其 AMF 索引仍指原始压缩包且缺少最终分卷信息；这不是 alpha.4 实际公开的 r3 材料，不得发布。保留旧候选用于追溯，已使用 `public-materials-20261115-r3` 新建 `D:/KDEBuild/controlpanel-e2e-current/desktop-release-stage-r3-20261116`，其源码索引散列与 r3 一致。比较新候选 5345 个文件，EXE/DLL 与旧候选全部一致；其他变化为索引、通知、AMF 许可文件、依赖清单及明确标注非发行版的 README，旧候选还有 6 个未进入新候选的早期 AMF 说明文件。新候选再次通过 12/12 本地媒体检查，记录在 `D:/KDEBuild/controlpanel-e2e-current/media-release-r3-20261116`。修正打包文档示例为 r3；仍需检查新构建源码对应、安装和双端，不能公开分发。

2026-11-16 候选包未归属二进制补充核查：Craft 清单中 74 个未匹配的 EXE/DLL 路径里，69 个 KDE Connect 文件（包含 lib/plugins 与 plugins 等重复部署路径）均可按文件名和 SHA-256 匹配到本次独立 Release 构建的 bin；其余 5 个微软图形/运行库文件与本地已公开 alpha.4 候选目录对应文件散列相同，`vcruntime140_threads.dll` 也匹配指定 CRT 来源。上条提及的 `translations/qt_zh_CN.qm` 与该旧包同路径文件散列相同。这些是文件一致性证据，不证明全部源码、许可或当前候选包的安装/双端测试已完成；非二进制未归属文件仍需分类。

2026-11-16 Windows 候选包第三方材料初核：对候选目录生成 Craft 文件归属清单 `D:/KDEBuild/controlpanel-e2e-current/release-candidate-dependency-inventory-20261116.json`。识别 83 个组件，与上版清单组件、安装版本/revision/cache 标签一致；其中 82 个与既有第三方源码索引的对应字段一致，`libs/runtime` 按既有材料设计单独处理微软许可。3261 个匹配归属文件中 19 个与 Craft 数据库散列不同：7 个 FFmpeg、10 个运行库和自带 session.conf 经散列比对与指定覆盖源一致；余下 qt_zh_CN.qm 是部署生成的翻译文件，仍需单独核对。清单另有 2088 个未匹配 Craft 归属文件（含 1224 个许可证文件、应用及生成文件等）；这不是 2088 个已确认缺少源码的组件，但需要逐类核验。附带的 vcruntime140_threads.dll 与指定 CRT 散列一致。此检查未核验全部源码压缩包、二进制对应关系和安装器，不认定可公开分发。

2026-11-16 当前 Windows Release 本地候选目录：使用新构建、既有可再分发 FFmpeg/CRT 和既有第三方材料，独立生成 `D:/KDEBuild/controlpanel-e2e-current/desktop-release-stage-20261116`；应用与插件散列分别与上一条 Release 构建一致，7 个 FFmpeg DLL 的许可检查通过。使用既有合成媒体和独立探针检查 PCM、MP3、AAC、FLAC、Vorbis、Opus、H.264、HEVC、VP9、AV1、中文路径/控制、损坏文件，共 12/12 通过，结果在 `D:/KDEBuild/controlpanel-e2e-current/media-release-candidate-20261116`；音频解码到内存，不写主机音频。首次启动打包命令因包装环境 PATH 找不到 pwsh 而未开始，改用已核对的 PowerShell 完整路径后完成。临时媒体探针按脚本校验后移除，候选包其余内容保留；包内说明已标识非 alpha.4、不得公开分发。没有启动桌面服务或测试安装/双端，第三方源码材料与新构建的一致性、完整安装包及发行验收仍需核对。

2026-11-16 当前源码独立 Windows Release 构建：在 `D:/KDEBuild/controlpanel-e2e-current/desktop-release-candidate` 新配置 `Release`、`BUILD_TESTING=ON`，安装前缀指定独立 D 盘目录；全部 676 个构建步骤成功，CTest 26/26 通过（33.95 秒，日志在该目录 `Testing/Temporary/LastTest.log`）。`bin/kdeconnect-app.exe` SHA-256 `6C1BC4F9E927AB5F7B002E0912AC769306C962CD96F848A280004E959427414C`，控制面板插件 SHA-256 `3167AD6A2F1D9A6BAFC7A836DBC1D7BBDB105094EFE6BF0B9426A3ACA5A1E160`。配置时 digitizer 因缺依赖未编译；未安装、打包、启动桌面服务、修改共享依赖或发布，构建目录依赖本机 Craft，不能作为独立安装包。真实双端、完整功能和干净系统验收仍待完成。

2026-11-16 进度插件发送路径补充：通过实际桌面插件和两个进程内模拟已配对设备，验证文件进度源切换为 none 后，新 revision 的 waiting 被两个设备按递增序号接收；同时检查公开配置不含私有 source/path/url。完整 CTest 26/26 通过（34.30 秒）。该测试不建立真实网络或 Android 连接，不等同于双端联调。

2026-11-16 进度源重配置序号修复：此前重新配置时发布的 waiting 状态序号固定为 0，手机可能将其作为乱序旧包拒绝；现在该状态也使用发布器的递增序号。模拟网络测试先复现失败（`D:/KDEBuild/controlpanel-e2e-current/progress-reconfigure-first.txt`），修复后桌面完整构建及 CTest 26/26 通过（34.41 秒）。手机侧另有配置修订与序号接收定向测试；尚未做当前版本真实双端测试。

2026-11-16 网站图标回归补充：增加独立编辑器与独立资源目录，分别验证网络请求尚未返回时手动换图不会被覆盖，以及未修改时全新网络请求正常应用图标；使用本机回环 HTTP，不访问外网。`paneleditortest` 定向通过，随后完整 CTest 26/26 通过（34.28 秒）。仅增加测试，未重新进行双端联调。

2026-11-16 网站图标异步覆盖修复：使用本机回环 HTTP 夹具复现下载完成信号期间用户选择的图标被覆盖，首轮失败日志保存在 `D:/KDEBuild/controlpanel-e2e-current/favicon-race-first.txt`。现在仅在该按钮的图标仍为发起请求时的值时应用自动图标；测试同时检查手动选择保留和未修改时自动应用。定向测试通过，桌面完整构建成功，已注册 CTest 26/26 通过（39.58 秒）。这是本地测试，不代表实际双端或完整发行验收；本轮尚未提交或推送。

2026-11-16 信息表盘预览补充：离屏加载生产 QML，验证多条备注、WorkBuddy 阶段与百分比、隐藏预报、缺省百分比、过期及不可用状态。首次断言未计入备注项目前缀而失败，修正夹具后定向通过；随后完整 CTest 26/26 通过（35.21 秒），首轮、复测和完整日志保存在 `D:/KDEBuild/controlpanel-e2e-current/reports/desktop-info-dial`。这是桌面预览，不代表手机渲染或实际 WorkBuddy 双端链路通过。

2026-11-16 主题菜单补充：使用离屏 QQuickView 实际点击主题选择按钮和夜幕蓝选项，确认设置变更及重建后的持久化；测试配置使用隔离 INI。首次两次测试分别因夹具基准 URL 导致递归和未从弹层视觉树查找菜单项而失败，修正测试夹具后通过。为稳定定位选项，主题菜单仅新增 objectName。桌面完整构建成功，CTest 26/26 通过（35.17 秒）；失败、成功和完整回归日志保存在 `D:/KDEBuild/controlpanel-e2e-current/reports/desktop-theme-menu`。真实桌面窗口及双端联调仍未验证。

2026-11-16 主题 QML 绑定补充：离屏加载 `AppTheme.qml`，验证真实 Appearance 对象从浅色切到深色时，QML 的主题索引、背景颜色和 dark 标志同步更新。定向测试通过；随后完整 CTest 再次 26/26 通过（34.30 秒），日志保存在 `D:/KDEBuild/controlpanel-e2e-current/reports/desktop-qml-theme/LastTest.log`。该检查不加载整个窗口或验证点击弹出菜单，真实窗口和双端验收仍未完成。

2026-11-16 后续回归：新增桌面主题测试，隔离 INI 配置并离屏验证六种主题、调色板、切换信号、重建后的持久化及越界值处理；未修改用户设置。定向通过后，完整已注册 CTest 26/26 通过（34.64 秒），本轮日志保存于 `D:/KDEBuild/controlpanel-e2e-current/reports/desktop-ctest-26/LastTest.log`。构建目录中 `LastTestsFailed.log` 是 11 月 12 日的历史文件，不代表本次失败。未进行真实窗口、所有系统配色或完整双端验收。

2026-11-16 拉取后本地验证（基线 `6a3c04403`，包含尚未提交的工作区修改）：

- WorkBuddy 进度源不再发送空百分比；文件创建及原子替换后尝试恢复监听，网络读取限制大小和时间、拒绝重定向。新增文件监听和模拟网络用例。
- 首次全量构建发现 `panelportabletest` 的字符串拼接与 JSON 类型不兼容，修正测试夹具后重新构建成功。`D:/KDEBuild/desktop` 使用现有 RelWithDebInfo 配置；不是新的公开 Release 安装包。
- 全部已注册 CTest 25/25 通过，包括进度、配置导入导出和网站图标；测试时间 48.04 秒。该结果不代替真实 LAN、多设备长时、真实音频写入、完整视觉/辅助技术或干净系统安装验证。
- 旧天气双端脚本固定了历史包和证据目录；当前构建要单独验证实际运行的可执行文件及插件路径，并使用新证据目录，不能将旧包结果当作当前结果。
- Android 最终 Debug 构建及 49 套件/322 项本地测试通过，Lint 仍为 511 errors、214 warnings、1 hint；详见 Android 仓库的 `docs/control-panel/BUILD_STATUS.zh-CN.md`。实体手机未安装，公开 alpha.4 未更新。
- 本轮两端工作区修改尚未提交或推送；切换电脑前需先按仓库规则完成同步，不将本地构建缓存和个人密钥提交。

2026-11-15：[换机开发指南](DEVELOPER-HANDOFF.zh-CN.md)为当前接续入口，发布结果以[alpha.4 记录](RELEASE-0.1.0-alpha.4.zh-CN.md)和[校验摘要](release-evidence/RELEASE-CHECKS-alpha.4.json)为准。以下记录是各阶段当时的测试，不因本次文档同步而重新计数。

当前结果见[阶段二十二](MILESTONE-22-CONTRAST-STARTUP.zh-CN.md)：双端表盘文字对比度修复、Android 45 套件/298 项、Windows 22/22 与新包三轮无窗口自检通过。修复前颜色不足及 r1/r2 首次启动失败均保留，r3 包含限时身份等待和旧版 PowerShell 内置模块来源修复。Lint 新报告仍为 493 错误；这里只收口当前纯色表盘和交付自检子项，主控制页自定义背景、完整辅助技术与原整体验收不因此通过。alpha.2 不覆盖，下方旧状态为历史记录。

最新见[阶段二十一](MILESTONE-21-DIAL-ACCESSIBILITY.zh-CN.md)：仅 Android 表盘行高/响应式排布修复，45 套件/294 项通过，新内部 APK 的实际两倍字号对照与五表盘唤醒通过。Lint 本轮重新生成仍为 493 错误；TalkBack、颜色对比度及原整体验收范围继续保留。Windows 运行代码未改、全量未重跑；下方旧“当前”与测试数为历史节点，alpha.2 维持原版。

阶段二十补齐[真实双端天气重连对照](MILESTONE-20-WEATHER-E2E.zh-CN.md)：旧包错误已复现，新包同缓存立即显示；新包五表盘空闲/唤醒全部通过。未改运行代码或发布包，未重跑全量或 Lint；阶段十九 274/22 与 493 错误仍为原报告。下方“天气重推未验收”的历史描述由本轮子项证据补充，完整时区、大字体/无障碍、普通 LAN 和整体验收不因此勾选。

当前为 [阶段十九](MILESTONE-19-WEATHER-ORDER.zh-CN.md) 与 [0.1.0-alpha.2](PREVIEW-0.1.0-alpha.2.zh-CN.md)：Android 44 套件/274 项、Windows 22/22 通过，两端构建及 Windows 新内部包无窗口自检通过。Lint 报告本次复用，实际仍为 493 错误，不算通过；新桌面包完整双端/普通 LAN 与发行验收未完成。下方阶段十八及更早的“最新”是历史记录。

阶段十八当前记录见 MILESTONE-18-VISUAL-DIALS.zh-CN.md：裁切失败保留，修复后 Android 44 套件/273 项通过；Windows 全部 21/21 与额外真实快照显示检查通过。新 APK 已在专用 AVD 复跑五表盘，真实温度范围完整可见；Lint 493 错误仍未通过，天气重推时序和原完整验收仍保留。下述旧“最新”不覆盖本轮。

最新检查见 [阶段十七](MILESTONE-17-E2E-FONT.zh-CN.md)：先保留大字体裁切失败，再修复和完整复跑，Android 271 项通过，Windows 21/21 通过；当前包隔离双端联调及实际在线/离线 2.0 字体复核已执行。Lint 493 错误仍未通过，全部平台、普通 LAN、视觉和交付验收仍未完成。下文旧“最新”与测试数字属于对应历史阶段。

更新：2026-11-12。本记录是开发里程碑，不是完整交付验收。

最新为 [阶段十六统一验证](MILESTONE-16-UNIFIED.zh-CN.md)：两端编译、Android 269 项及 APK、Windows 两轮 21/21 和新内部包无窗口启动通过。Lint 本轮为 493 错误 / 211 警告 / 1 提示，不按任务退出值认定通过。功能复核见 DEVELOPMENT-GATE-16.zh-CN.md；后续完整平台、双端、视觉和发行验收仍待完成。

用户要求先输出小版本时，为 [0.1.0-alpha.1](PREVIEW-0.1.0-alpha.1.zh-CN.md) 做了最小包核验。随后 [表盘/外观及焦点边界](DEVELOPMENT-14-DIAL-OPTIONS.zh-CN.md) 和 [连接/多设备](DEVELOPMENT-15-CONNECTIONS.zh-CN.md) 当时只开发不运行；现已纳入阶段十六统一验证第一批。下述旧数字和阶段声明保留历史上下文，不代表当前整体验收通过。

阶段十二、十三当时按用户要求先开发后统一测试，仅静态检查，未构建或生成新包。最近一次完整测试历史节点见 [阶段十一](MILESTONE-11-RESTORE.zh-CN.md)：Android 257 项、Windows 21 套件通过，Lint 仍有 477 项错误。这些历史结果不覆盖当前原生动作、跟随反馈及三角色音量代码，开发内容见 [阶段十三](DEVELOPMENT-13-NATIVE.zh-CN.md)。下述早期“最新”文字仅保留历史上下文。

最新功能记录见 [阶段七 B：信息表盘与三日预报](MILESTONE-07-INFO.zh-CN.md)：Android 200 项单测与 APK、中文五类渲染、SDK 23/28/37 电量/语言检查通过；Windows 20/20 与新候选包 5 项生命周期检查通过。Lint 首轮 490 项错误，修复本模块 4 项代码错误后仍有 486 项错误、212 项警告、1 项提示，不按任务退出 0 认定通过。其余完整回归与发行仍未完成。

最新打包进度见 [阶段七 A：Windows 独立目录候选包](MILESTONE-07-PACKAGING.zh-CN.md)：脱离 Craft 搜索路径的原生/无窗口启动、实际 DLL 来源和五项生命周期检查通过，包含中文/空格路径。没有改动已有运行模块，也不是干净 Windows 或全部功能验收；最终安装、发行依赖声明及完整回归仍待完成。下述阶段六数字保留为原构建环境的历史测试结果。

最新进度见 [阶段六：基础表盘与天气](MILESTONE-06.zh-CN.md)：Android 189 项单测及 APK 通过，Windows 完整构建及 CTest 20/20 通过。图标见阶段五、音量见阶段四、智能跟随见阶段三、双端初版见阶段二。下方保留历史基线记录；同路径 APK/报告可能被后续构建覆盖，当前版本和校验值以最新阶段记录为准。

## 本阶段实现

- 两端配置模型与同一份 JSON 样本：18 个固定槽位、1..10 页、稳定 ID、模式/外观/空闲参数、修订号和大小/深度边界校验。
- 桌面公开配置白名单投影：不向 Android 下发私有路径、动作 target 或未知私有字段。
- 桌面原子存储、上次有效备份、修订计数、并发编辑冲突检测、跨进程文件变更监听、显式恢复并保留损坏文件。
- Android 会话状态：离线/同步时禁止动作、不缓存离线点击、拒绝旧修订、重复点击等待确认、超时释放、断线清空待执行状态。
- 修复已有全屏命令页面在官方 Kotlin 单例改动后的 3 处 Java 调用。

网络插件、动作执行、控制面板界面、图标传输、智能跟随和表盘尚未接入；不能从本阶段测试推断这些功能已经完成。

## Android：通过

环境：JDK 17.0.14、Gradle Wrapper 9.6.0、Android SDK 37、AGP 9.4.1。

执行任务：`testDebugUnitTest assembleDebug`（未禁用或跳过测试）。

- 143 项单元测试：失败 0、错误 0、跳过 0。
- 其中新增 PanelConfigTest 6 项、PanelSessionTest 6 项。
- 调试 APK 构建成功，applicationId 为 org.kde.kdeconnect_tp.debug。
- 本阶段 APK 尚未安装到手机，不是最终控制台交付包。

本机证据：

- 日志：D:/KDEBuild/logs/android-baseline-retry5.log
- 测试报告：E:/git/kdeconnect-android/build/reports/tests/testDebugUnitTest/index.html
- XML：E:/git/kdeconnect-android/build/test-results/testDebugUnitTest/
- APK：E:/git/kdeconnect-android/build/outputs/apk/debug/kdeconnect-android-debug.apk
- APK SHA-256：B80D1801A1BEE671A3AFCE869EBF24193043932F6161559BCC5079351BBD71C9

首轮发现的真实失败及修复：

1. 原全屏命令 Java 页面直接调用 getInstance，官方 Kotlin companion 未导出静态方法；改为 Companion.getInstance。
2. JSON 库允许字符串布尔值，未知字段的深层嵌套未被自动限制；新增显式原始类型和容器深度检查，保留失败用例后重跑全部单元测试。

完整日志还含上游弃用 API/翻译资源警告；此处没有将警告描述为全部解决。

## Windows 控制面板模块：通过

环境：Windows x64、MSVC 19.35、Windows SDK 10.0.22000.0、Craft Qt 6.12.0。

使用本机独立 CMake 入口编译仓库实际 controlpanel 源码及 tests/paneldocumenttest.cpp、tests/panelstoretest.cpp。这不是整个 KDE Connect 的构建。

- 编译成功。
- CTest：2/2 测试套件通过，单项超时 60 秒。
- 再次直接执行并生成 JUnit XML：PanelDocumentTest 20 项、PanelStoreTest 6 项，失败/错误均为 0；计数包含各套件的初始化与清理项。
- 覆盖：共享样本、类型/版本/ID/大小/深度错误、私有字段隔离、并发修改、原子替换监听、损坏配置恢复。

本机证据：

- D:/KDEBuild/logs/panel-tests-configure.log
- D:/KDEBuild/logs/panel-tests-build4.log
- D:/KDEBuild/logs/panel-tests-ctest3.log
- D:/KDEBuild/logs/paneldocumenttest.xml
- D:/KDEBuild/logs/panelstoretest.xml
- 本机测试入口：E:/git/kdeconnect-tooling/controlpanel-tests/CMakeLists.txt
- 构建目录：D:/KDEBuild/panel-tests

首轮独立编译发现变量名 slots 与 Qt 宏冲突，已改名并重编译测试。完整项目中的 tests/CMakeLists.txt 也注册了这两个测试，待其余桌面依赖安装完成后从完整工程再次运行。

新增 C++ 文件按 ECM 的 C++ 格式规则整理并通过格式检查。Craft 提供的 clang-format 20 在本机启动异常，故使用已有 Visual Studio clang-format 15 和相同的 ECM C++ 配置段；未修改第三方工具文件。

## 存储位置与环境修复

源码在 E 盘，不在 C 盘开发：

- E:/git/kdeconnect-android
- E:/git/kdeconnect-kde

Gradle 缓存 D:/gradle；SDK D:/Android/SDK；Craft D:/KDECraft；工具缓存、Java/Android 用户目录、临时文件和构建日志 D:/KDEBuild。Android build 产物目前在 E 盘。

本机辅助入口 E:/git/kdeconnect-tooling/build-android.ps1 和 run-craft.cmd 只调整子进程环境，不修改系统全局环境变量。Android 旧 ANDROID_SDK_HOME 与新 ANDROID_USER_HOME 冲突已在构建进程中消除。Craft 日志默认写 C 盘的问题已改为 D 盘，先前任务日志已保留到 D 盘。Windows SDK 是本机已有系统组件，只读取，不迁移或删除。

C 盘剩余空间仍紧张，项目目录迁移不能控制操作系统和其他程序的写入；未删除其他文件。

## 待完成验证

- 桌面其余 KF/多媒体等依赖仍在安装，尚未完成完整 CMake 构建、完整 CTest、打包或安装。
- Android Lint、真机安装/界面、横屏/生命周期/无障碍测试尚未完成。
- 双端协议、真实动作、断网重连、音量、智能跟随、全部表盘及视觉对照尚未完成。
- 最终包、完整 README 和需求逐项验收继续按 PLAN 执行。
