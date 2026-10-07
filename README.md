# car
一个基于rk3576板卡的功能相对齐全的linux车载系统

驱动开发使用 `dev/driver-baseline` 分支，目标板卡为野火 LubanCat-3（RK3576、Linux 6.1.99、Ubuntu 22.04、Qt 5.15）。

- [模块编译、加载与卸载基线](docs/01-driver-baseline.md)：板卡已通过。
- [GPIO 按键驱动、设备树与硬件验收](docs/02-gpio-key.md)：20 次按下/松开、长按及卸载重载已通过。
- [K1 打开 Qt 摄像头及对应知识点](docs/03-qt-camera-key.md)：云端构建和离屏测试已通过，板卡画面待验收。

原 Qt 项目位于 `car/`，驱动位于 `drivers/`。运行新 Qt 程序时，在板卡 VNC 桌面的终端执行 `sh scripts/run-car-gui.sh`；编译输出放在 `build/qt/`。MPU6050 为后续阶段，DHT11 与大模型语音模块暂缓。
