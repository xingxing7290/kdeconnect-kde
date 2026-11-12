# 双端控制面板：阶段五图标实现与验证

更新：2026-11-12。整体目标未完成，仍为开发版。

## 实现

Windows 支持本地 JPG/PNG 导入、规范化 PNG、SHA-256 内容寻址、exe/lnk 原生图标提取。程序提取不启动程序。编辑窗口预览/保存/取消已接通，只有保存才改变配置。按配对会话、当前配置引用、修订号授权图标请求，并限制有效请求速率。

Android 严格校验元数据、大小、PNG、哈希、尺寸及实际解码，原子保存到私有 files 目录；限制磁盘/内存缓存，顺序接收及两次重试。图片失败使用类别占位，不禁用其他正常动作；断线保留已缓存图标。

用户已明确批准 Payload 按次读取超时接口，仅图标模块调用。此前影响全部 LAN 附件连接的 5 秒超时已撤回。使用及安全边界见 [图标说明](ASSETS.zh-CN.md)。

## 自动化证据

证据根目录 D:/KDEBuild：

| 检查 | 结果与证据 |
| --- | --- |
| Android 全部单测与 APK | 172 项、失败 0、错误 0、跳过 0；build6 完整重建 4 分 7 秒成功；logs/android-assets-build6.log 及工程 test-results/testDebugUnitTest XML |
| Windows 完整构建 | 成功；logs/desktop-assets-build5.log |
| Windows 完整 CTest | 18/18 通过，41.21 秒；logs/desktop-assets-ctest3.log |
| 图标导入/存储 | PNG/JPEG 规范化、元数据去除、超大/损坏输入拒绝、损坏资源修复及持久化通过 |
| 原生图标 | 中文、空格、# 路径 exe 通过；实际 lnk 与 exe 规范化哈希相同；logs/assets-link.json |
| 生产 QML 弹窗 | 真实鼠标取消不提交，重新打开不沿用取消图标，保存提交，不存在资源拒绝保存 |
| 电脑资源授权 | 未握手、旧 revision、非配置引用及路径 ID 拒绝；第 17 个有效请求返回 asset_busy |
| 手机缓存及接收 | 新实例离线读取、损坏/删除重取、配额淘汰、错误会话/大小/格式/哈希、晚到附件关闭、重试上限及超时通过 |
| 超时隔离 | 仅选定 payload socket 变为 3000 毫秒；其他 socket 保持原值；越界参数拒绝 |

logs/desktop-assets-preview.png 是实际开发窗口截图，显示首页通用图标，不作为原生图标页面的视觉证据。两组图片测试使用 SDK 28 的原生图片运行库，其余原测试保留原环境。

## 真实双端证据与范围

此前在 Galaxy S10 上安装本阶段早期 APK，从独立测试电脑端成功获取自定义图片和原生程序图标。手机缓存 SHA-256 与电脑内容 ID 完全一致：

- 自定义测试图：3ffcdb3dc5f406452510981a05805407d8c9ae1ac1cea01c5fb558c02da5a6c5
- 程序图标：2a34586debb7ddfc759ae8ef86a84f6e6c01db9649f360659327fa097d45c1f9

已查看 logs/android-assets-online.png，两类图标实际显示。关闭本任务独立 daemon/indicator 后，logs/android-assets-offline.png 显示缓存图标仍在、按钮灰化及离线提示。未启动图标对应程序，未修改音量。

真机检查早于撤回共享 LAN 连接超时，图标显示/传输实现相同，但不能代替最终 APK 全部重装/回归。当前 ADB 已换成另一台华为手机，尚未向它安装本 APK。离线冷启动入口没有据此判定通过。

## 失败与恢复

最初 168 项测试有 4 项图片缓存相关失败。Android 6 测试运行库下，仅指定 NATIVE 模式仍不足以得到这些图片测试所需的原生实现；将两组图片测试明确指定 SDK 28 后通过。生产校验未放宽。

SDK 28 自动下载一度在 0 字节停留约 8 分钟，已结束对应测试进程并保留日志，没有无限等待。续传完整运行库后与主仓 SHA-512 核对一致，文件保存在 D 盘。一次被中断的构建没有成功终态，不算通过；以最终成功日志及 XML 为准。

后续发现 Kotlin 守护进程的小量运行标识仍默认写系统用户目录，已在本机构建入口指定 D:/KDEBuild/kotlin-daemon；新进程命令行已核对该路径。源码继续 E 盘，依赖、临时目录、缓存及日志使用 D 盘，未删除用户其他文件。

## 产物与待办

本阶段调试 APK：D:/KDEBuild/artifacts/control-panel-assets-20261112/kdeconnect-controlpanel-debug.apk。

最终本机环境核对后再次执行 testDebugUnitTest + assembleDebug 成功，1 分 28 秒，仍为 172 项零失败/错误/跳过。日志 logs/android-assets-build7.log；上述 Kotlin D 盘运行目录亦从新进程命令行确认。最终 APK SHA-256：918EC54D937D85A8ABF4D6BCCB9C796661E6332FB8D31EAC5918A1C4B39DB707。

Windows 仍是 D:/KDEBuild/desktop/bin 中依赖本机 Craft 的开发程序，未完成可分发安装包。

待办：图标多设备压力/长时间/后台生命周期、实际文件选择对话框完整回归、离线冷启动入口、特殊程序图标资源、最终 APK 重装；另有全部表盘/真实天气、真实焦点切换、其他音频角色/错误反馈、所有动作/兼容性/Lint/打包安装与最终 README。不能据本阶段单测或一次真机检查宣称整体完成。
