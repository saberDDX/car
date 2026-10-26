# car
一个基于rk3576板卡的功能相对齐全的linux车载系统

驱动开发使用 `dev/driver-baseline` 分支，目标板卡为野火 LubanCat-3（RK3576、Linux 6.1.99、Ubuntu 22.04、Qt 5.15）。

[完整项目清单](docs/00-project-inventory.md) · [BSP 最终验收、代码重点与简历](docs/05h-bsp-acceptance-and-study.md) · [BSP 任务与范围](docs/05-rk3576-bsp-practice.md)

- [模块编译、加载与卸载基线](docs/01-driver-baseline.md)：板卡已通过。
- [GPIO 按键驱动、设备树与硬件验收](docs/02-gpio-key.md)：20 次按下/松开、长按及卸载重载已通过。
- [K1 打开 Qt 摄像头及对应知识点](docs/03-qt-camera-key.md)：板卡实时画面、单窗口及三次关闭重开已通过。
- [MPU6050 与 I2C 开发](docs/04-mpu6050.md)：[六轴采集与 misc/read](docs/04b-mpu6050-data.md)已通过板卡功能验收；[实测记录](docs/04c-mpu6050-acceptance.md)包括 5 分钟 6000 组、零读取错误、三轮重载及持有 fd 的解绑/重绑。
- [RK3576 BSP 构建与启动](docs/05h-bsp-acceptance-and-study.md)：固定厂家源码完成 ARM64 内核构建与自编驱动 Kconfig/Kbuild 集成，实板运行 `6.1.99-rk3576-car-bsp`，配置/运行树/绑定、MPU/GPIO/实时画面回归通过。受控匹配练习以首次尝试和独立物理复查分阶段归档，保留首次失败及未定原因；实际分区写回未测试。

原 Qt 项目位于 `car/`，驱动位于 `drivers/`。运行新 Qt 程序时，在板卡 VNC 桌面的终端执行 `sh scripts/run-car-gui.sh`；编译输出放在 `build/qt/`。MPU6050 驱动与命令行初版已验收，Qt 数据页为可选扩展；DHT11 与大模型语音模块暂缓。

实验内核日常回归使用 `sh scripts/test-bsp-runtime.sh`，复用已安装的匹配模块；包未包含新 headers，另行重编外部模块需要与本次内核匹配的构建目录/headers。保留原内核文件及 Windows 上的 boot 备份。BSP 产物发布时的离线记录保留原状，后续真实运行结果以最终验收文档为准。
