# 第三阶段：物理按键打开 USB 摄像头

## 已完成与待验证

GPIO 驱动已在 LubanCat-3 通过 20 对按下/松开事件、零重复、长按以及两轮卸载重载验收。现在把标准 input 事件接到现有 Qt 的摄像头入口。

代码已清理缺失的 AiListenerThread、VoiceThread 接入和无用的 ZeroMQ 链接，保留现有地图、天气、音乐和视频页面。摄像头入口只维护一个窗口，K1 和界面按钮使用同一槽函数。没有重新设计 Qt 界面，也没有接回大模型模块。

云端 Qt 5.15.15/x86_64 原生构建成功，离屏测试通过：4 个功能测试，加 init/cleanup 共 6 passed、0 failed、0 skipped。覆盖按下触发/重复抑制、丢事件期间抑制操作、三次单窗口打开返回，以及初始化失败后五次重启无文件描述符泄漏。测试使用 `/dev/null` 触发真实 open/ioctl 失败，不模拟或声称 USB 摄像头已输出有效画面。

板卡 Qt 5.15.3/aarch64 的本次编译、画面显示、退出重开和拔插行为待实际验收。云端没有连接用户板卡或 USB 摄像头。

## 板卡一条启动命令

插好 USB 摄像头，关闭旧 Qt 程序。在 **VNC 桌面里的终端**运行，不用 sudo 启动整段命令：

```sh
cd ~/car-driver-dev && git -c http.version=HTTP/1.1 pull --ff-only && sh scripts/run-car-gui.sh
```

脚本按需安装 Qt/OpenCV 开发包和 ACL 工具，在 `build/qt/` 从源码重新编译，加载按键模块，按名称查找 input 设备，通过只读能力查询选择可用摄像头，再启动普通用户的 Qt 程序。保留原 `car/` 目录中的旧可执行文件和编译产物，运行新生成的 `build/qt/car`；以 `car/` 作为工作目录，方便原页面读取已有媒体文件。

权限不足时，脚本仅为当前用户临时添加本项目 event 节点的读权限和所选摄像头的读写权限，先保存原 ACL。退出时检查设备身份后恢复权限，清理本次 run_id 所有的模块；已有模块不会被此脚本卸载。ACL 恢复失败时保留原权限备份并输出路径。没有设置登录自启、开机永久加载或全局 chmod。

程序默认优先 `/dev/v4l/by-id/*-video-index0`，再扫描 `/dev/video*`，检查 UVC、单平面捕获、流式采集和 YUYV 格式。RK3576 的其他视频节点不会因为编号靠前就被选中。多摄像头的指定方式为：

```sh
CAR_CAMERA_DEVICE=/dev/videoN sh scripts/run-car-gui.sh
```

将 N 替换为确认过的设备编号。当前采集支持单平面 YUYV + MMAP；MJPEG-only 或多平面设备会明确报不支持，后续有实际需要再扩展。

## 验收动作

1. 启动后主界面显示“按键已连接”，按 K1 打开摄像头，确认看到实时画面。
2. 摄像头页面内再按 K1、长按 K1，不应出现第二个摄像头窗口或另一个采集线程。
3. 点击“退出倒车影像”返回主界面，再按 K1 重新打开，重复 3 次，确认退出流畅且每次均有画面。
4. 在采集期间拔掉摄像头，页面应显示断开/错误，仍可返回；重新插入后关闭并重新打开摄像头页面。
5. 关闭整个程序，使启动脚本恢复权限和清理本次模块。

终端日志 `K1 pressed: opening camera` 证明应用接收到事件；`V4L2 camera selected` 证明选择了设备；`V4L2 streaming` 证明 STREAMON 成功；`V4L2 first frame received` 证明采集、长度检查和图像转换完成。实际画面仍由用户观察确认。报错时提供终端输出，不把只弹出窗口当作图像链路通过。

## 本阶段必要修复

- `O_NONBLOCK` 打开设备，`poll` 每次最多等待 100ms，再执行 DQBUF。退出用原子标志通知采集线程并 wait，避免原实现的普通 bool 数据竞争和阻塞 DQBUF 卡住返回。
- 查询并核对设备能力、驱动返回的实际尺寸、格式和 bytesperline，转换时使用协商后的 stride。不能把摄像头返回的图像一律当成 640×480、无行填充。
- 检查 REQBUFS、QUERYBUF、mmap、QBUF、STREAMON/DQBUF 的结果及帧索引/长度。初始化失败也释放已成功申请的资源；结束时 STREAMOFF、munmap、close。
- OpenCV 完成 YUYV→RGB；QImage 深拷贝后通过信号发送，避免跨线程显示已被重新入队覆盖的内存。UI 更新留在主线程。
- 摄像头未返回帧或断开时显示错误，关闭后仍可重新打开。

## 对应知识点

| 模块/代码 | 面试时可解释的内容 |
| --- | --- |
| `drivers/gpio-key/car_gpio_key.c` | OF compatible 匹配 platform 驱动；probe 中申请 GPIO 描述符、映射 IRQ、注册 input 设备；devm 资源逆序释放 |
| GPIO4_A6 overlay | 物理针脚与 bank/offset 的区别；pinctrl 设置 GPIO 功能和内部上拉；GPIO_ACTIVE_LOW 让描述符读取结果直接代表“按下” |
| IRQ 与 delayed_work | 硬中断只安排工作，睡眠型 GPIO 读取放到工作队列；最后一个边沿后约 20ms 采样，实现软件消抖 |
| input/evdev | EV_KEY、KEY_CAMERA=212、按下=1/松开=0/重复=2，input_sync 对应事件帧；用户态 read/poll 获取事件 |
| `car/inputkeyreader.cpp` | 非阻塞读取与 QSocketNotifier 结合 GUI 事件循环；处理 EINTR/EAGAIN、设备消失和 SYN_DROPPED；只在新按下时触发 |
| `car/v4l2thread.cpp` | QUERYCAP/ENUM_FMT、S_FMT 协商、MMAP 缓冲区、QBUF/DQBUF 队列所有权与 STREAMON/OFF 生命周期 |
| 线程退出与图像寿命 | 原子停止标志、有限 poll 等待、wait 后释放对象；图像深拷贝隔离内核缓冲区和跨线程显示的寿命 |

本 GPIO 驱动使用 input 子系统，没有自行实现字符设备的 file_operations/read/poll 或自建等待队列。这些用户接口由 evdev 提供，简历和面试按实际实现说明。USB 摄像头底层 UVC 驱动由内核提供，本项目实现的是 V4L2 用户态采集和缓冲区管理。

当前已验证的 GPIO 简历表述可写为：“在 RK3576 Linux 6.1 平台实现设备树匹配的 GPIO 按键 platform 驱动，完成双边沿中断、延迟工作消抖和 input 事件上报，通过长按、20 次按下/松开及卸载重载测试。”Qt 摄像头联动完成板卡验收后再加入结果描述。

## 云端检查命令

使用系统 Qt/OpenCV 开发包时：

```sh
mkdir -p build/qt build/qt-tests
(cd build/qt && qmake ../../car/car.pro && make -j2)
(cd build/qt-tests && qmake ../../tests/qt/integration.pro && make -j2)
QT_QPA_PLATFORM=offscreen ./build/qt-tests/qt-integration-tests
```

本次云端 SDK 解包在 `/workspace/.qt5`，通过自定义 qt.conf 指定工具和库位置；具体启动命令保存到环境配置草稿。板卡启动脚本使用系统 qmake，不依赖云端 SDK。原音乐/视频页面仍有 Qt 弃用 API 警告，未影响本次构建；天气、地图和媒体播放的外部服务不属于本次验收。
