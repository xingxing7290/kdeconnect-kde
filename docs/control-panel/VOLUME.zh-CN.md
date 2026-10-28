# 双端音量控制

## 使用

- 电脑预览右侧显示真实音量百分比；拖动滑杆或按方向键调整，底部按钮切换静音。下方“音频输出”选择当前输出设备。
- 手机右侧上方扬声器按钮打开输出列表；当前默认项带勾。竖向滑杆向上增大、向下减小，下方按钮切换静音。
- 手机拖动期间最多每 80 毫秒发一次音量更新，松手补发最终值。一次手势只针对开始时的设备 ID；中途默认输出变化不会把旧手势写到新设备。
- 音量区域与页面手势区分开；选择上下翻页也不应让音量拖动切页。
- 没有默认输出时禁用音量与静音，不擅自选择列表中的第一个设备。有可用输出时仍可打开列表明确选择。

## 实现与兼容

Windows 的电脑预览和既有 System Volume 网络插件复用 systemvolume/VolumeController。每个进程内共用一个控制器，每 500 毫秒读取原生音频状态，只在状态变化时通知；手机最终显示取决于电脑回传及网络延迟。音频服务暂不可用时清除过期可控状态，后续读取成功可恢复。

设备 name 为 Windows 音频端点 ID，description 为友好名称。同名设备也使用不同 ID，不因名称相同串控。沿用 kdeconnect.systemvolume 与 kdeconnect.systemvolume.request，旧客户端读取新 sinkList 后可继续控制。macOS/Pulse 原有插件未改写；新电脑预览的本地后端当前只实现 Windows，其他平台明确不可用。

Windows 只接受整数 0..100、布尔静音/启用和当前存在的设备 ID；整条请求先验证类型，再调用系统。COM 接口和系统分配字符串使用受控释放，不保留旧实现的逐设备回调链。轮询并不承诺零延迟。

## 验证边界与待办

- 默认输出切换沿用原 KDE Windows 实现的 eMultimedia 角色。尚需核对 eConsole 和不同应用实际播放路径；本轮不能推断所有 Windows 音频角色都被切换。没有主动请求改变通话角色。
- 电脑侧原生写入失败有 error 属性，界面显示错误；手机原协议没有逐次写入结果确认，失败提示仍需完善，不能把当前状态回读当成所有错误场景的完整反馈。
- 已实测真实音量、外部变化、静音、手机选择设备，以及换设备后的目标隔离；设备拔插、无输出硬件状态、休眠/唤醒、长期运行和完整多平台回归仍需补证。
- 当前 Samsung 真机上，部分右侧控件的 UIAutomator 边界为零，但可见图像、触摸和原生回读正常；它与窗口裁切/已有分辨率覆盖的关系尚未确定，无障碍验收仍未通过。

## 测试入口

- volumecontrollertest：假音频系统下的同名设备隔离、默认切换、移除、错误恢复、请求类型和范围。
- volumeuitest：加载实际 ControlDashboard.qml，以假音频系统执行鼠标滑杆、键盘调节、静音、输出选择、外部默认变化和无输出禁用。
- Windows volumeintegration：默认只读；--snapshot 保存基线，--exercise 做原生测试后恢复，--restore 显式恢复。测试可短暂改变实际音量/静音/多媒体默认设备，运行前应了解影响，不属于安装包。
- Android SystemVolumePluginTest 与 PanelVolumeDragTest：列表替换、默认状态、监听器生命周期、请求、实时拖动节流、最终值及设备切换/离线边界。

原生接口参考：[音量标量](https://learn.microsoft.com/en-us/windows/win32/api/endpointvolume/nf-endpointvolume-iaudioendpointvolume-setmastervolumelevelscalar)、[默认音频端点与角色](https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdeviceenumerator-getdefaultaudioendpoint)、[COM 初始化配平](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex)。

完整项目验收继续以 PLAN.zh-CN.md 为准，本页不是最终完成声明。
