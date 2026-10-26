# RK3576 BSP 衍生实践：最终归档、代码重点与简历

## 1. 结论与完整项目范围

本次限定的 BSP 主线已完成：固定厂家源码与运行配置 → 自编 MPU6050 接入 Kconfig/Kbuild → ARM64 Image/modules/DTB 构建 → 配套 initrd → 独立安装 → 新内核真实启动 → 配置/运行树/模块绑定核对 → MPU6050、按键、摄像头功能回归。匹配排障使用分阶段证据归档，详见第 3 节。

这是一个主项目“基于 RK3576 的 Linux 车载终端与外设驱动”，BSP 是其中的深化模块。Quard Star RISC-V 第二项目尚未开始。

| 模块 | 已实现 | 主要代码/材料 |
| --- | --- | --- |
| 内核模块基线 | headers 环境定位、外部模块构建、加载/卸载与本轮日志核对 | [hello](../drivers/hello/car_hello.c)、[基线记录](01-driver-baseline.md) |
| USB 视频采集 | V4L2 YUYV、MMAP 缓冲区、队列/采集/释放、Qt 显示 | [v4l2thread.cpp](../car/v4l2thread.cpp)、[摄像头联动](03-qt-camera-key.md) |
| GPIO 按键驱动 | platform/OF、pinctrl、GPIO descriptor、双边沿 IRQ、delayed work 消抖、input | [car_gpio_key.c](../drivers/gpio-key/car_gpio_key.c)、[overlay](../drivers/gpio-key/lubancat3-gpio4-a6-key.dts) |
| Qt 按键联动 | evdev KEY_CAMERA、单窗口、返回后重开、长按保持 | [inputkeyreader.cpp](../car/inputkeyreader.cpp)、[mainwindow.cpp](../car/mainwindow.cpp) |
| MPU6050 驱动 | I2C client、身份/配置读回、六轴批量读、misc/read、mutex/kref | [car_mpu6050.c](../drivers/mpu6050/car_mpu6050.c)、[UAPI](../include/car_mpu6050_uapi.h) |
| 采集工具 | 目标 20Hz、物理量换算、CSV、时间间隔/错误统计 | [mpu6050-monitor.c](../tools/mpu6050-monitor/mpu6050-monitor.c) |
| BSP 构建与启动 | 固定源码/工具链、Kconfig/Kbuild、版本隔离、initrd、真实上板与回归 | [build-bsp-kernel.sh](../scripts/build-bsp-kernel.sh)、[deploy-bsp-kernel.py](../scripts/deploy-bsp-kernel.py) |
| 受控匹配排障 | platform override 阻断、解绑/重扫/恢复、事件复查与失败日志保留 | [test-bsp-key-match.py](../scripts/test-bsp-key-match.py)、[排障过程](05g-bsp-key-match-debug.md) |

原项目的地图、音乐、天气等界面继续保留；本次没有以新增 Qt 页面为重点。

## 2. 实测结果与证据范围

| 项目 | 验收结果 | 证据 |
| --- | --- | --- |
| 构建 | Image、256 个内核树模块、DTB；另有同 release 的外部 GPIO 模块，包内共 257 个模块 | [构建记录](../bsp/arm64-build-result.json) |
| 独立安装 | 原启动文件保留，新 Image/initrd/modules 独立命名并核对 | [stage](../bsp/stage-result.json) |
| 真实启动 | 运行 `6.1.99-rk3576-car-bsp`，phase=active，uenv=experiment | [boot](../bsp/boot-result.json) |
| 运行配置与绑定 | 完整配置哈希、CONFIG_CAR_MPU6050=m、intree=Y、运行树和两个驱动绑定检查通过 | [脚本及原理](05f-bsp-runtime-regression.md)、[结果摘要](../bsp/runtime-result.json) |
| 新内核六轴读取 | 10.001s / 200 组 / 0 读取错误，实际 19.999Hz | [结果摘要](../bsp/runtime-result.json) |
| 新内核宿主间隔 | min 49.683、mean 50.002、max 50.305ms | 同上；不是硬实时保证 |
| 新内核按键 | 3 对事件、零重复、长按和 1 秒静默通过 | 同上 |
| 新内核摄像头 | 用户确认实时画面、返回重开和长按保持正常 | 同上，人工视觉验收 |
| 最终按键复查 | 1 次按下/松开，保持 3.055900s，0 repeat，静默通过 | [复查记录](../bsp/key-verify-result.json) |
| 原内核深入验收 | MPU6050 五分钟 6000 组零读取错误，以及 ABI/并发/持有 fd 的解绑重绑 | [原内核记录](04c-mpu6050-acceptance.md)，不冒称已在新内核重跑五分钟 |
| 外部回退准备 | Windows 原备份双重哈希、MASKROM/LOADER、GPT 与完整 boot 只读导出已验证 | [读回记录](../bsp/boot-readback-result.json)；没有实际分区写回 |

首次额外按下的原因没有定论；后一次正常不能证明已经修复触点抖动。本次没有更改 GPIO 驱动或 20ms 消抖时间，没有放宽事件计数标准。

## 3. 匹配排障采用分阶段记录

第一次完整工具运行进入了恢复后的物理按键验收。依据工具控制流，这意味着此前故障状态与恢复检查已完成；但用户只贴回末尾，没有提供 BEFORE/FAULT/RESTORED 原始快照。物理部分收到 `1→0→1`，统计为 2 次按下、1 次松开，工具正确返回 FAIL。该失败保留在 [首次尝试](../bsp/key-match-attempt.json)。

随后使用 `--verify-only`，确认当前驱动绑定、空 override，并得到：

```text
code=212 value=1 (pressed) time=4018.580218
code=212 value=0 (released) time=4021.636118
Totals: pressed=1 released=1 repeated=0
PASS: 1 pairs, zero repeats, >=2-second hold, and 1-second quiet observation.
```

时间戳差为 `4021.636118 - 4018.580218 = 3.055900s`。新结果见 [独立复查](../bsp/key-verify-result.json)。

因此可描述为“受控复现匹配阻断、按工具检查恢复，之后独立复核物理输入通过”。不能描述成“同一次命令全程无失败”或“后一次又重新注入了故障”；也不证明两次之间未发生模块重新加载。无需为了美化记录覆盖首次失败。

## 4. 源码、配置与交叉编译：第一学习重点

实际构建源码固定为厂家提交 `b73c060dbe3576f056c197d7f2f7cc11f65e2aec`。更早调查过的 639af5a... 不是本次上板版本。工具链为 GCC 12.2 AArch64 / binutils 2.40，原厂内核使用 Arm GCC 10.3.1；没有声称二进制复现原厂镜像。

文件：`scripts/build-bsp-kernel.sh`，核心调用：

```sh
make -C "$source_dir" O="$output" ARCH=arm64 CROSS_COMPILE="$cross" CC="$compiler" LOCALVERSION= \
    HOSTCFLAGS="${BSP_HOSTCFLAGS:--O2}" HOSTLDFLAGS="${BSP_HOSTLDFLAGS:-}" "$@"
```

应会解释：ARCH 决定目标架构，CROSS_COMPILE 指定目标工具链前缀，CC 指定实际目标 C 编译器，HOSTCFLAGS/HOSTLDFLAGS 用于构建在云端主机运行的工具，O 隔离输出目录。主机工具与 ARM64 内核/模块不是同一种可执行文件。

同一文件对运行配置派生：

```sh
cp "$config" "$output/.config"
"$source_dir/scripts/config" --file "$output/.config" \
    --set-str LOCALVERSION '-rk3576-car-bsp' --disable LOCALVERSION_AUTO \
    --disable GCC_PLUGINS --module CAR_MPU6050
kmake olddefconfig
kmake -j"${BSP_JOBS:-2}" Image modules rockchip/rk3576-lubancat-3.dtb
```

应会解释 `.config` 与 defconfig、olddefconfig 如何补齐目标源码的配置项、LOCALVERSION 为什么避免与旧 modules 目录冲突。关闭 GCC_PLUGINS 与编译器特征引起的差异有明确记录，未批量关闭外设以绕过构建。未把交互式 menuconfig 操作写成已实际做过的步骤。

## 5. Kconfig/Kbuild 集成：第二学习重点

文件：`bsp/car-mpu6050.Kconfig`：

```kconfig
config CAR_MPU6050
    tristate "Car MPU6050 sensor driver"
    depends on I2C
```

文件：`scripts/make-bsp-integration-patch.py` 生成的父/子 Makefile 内容分别为：

```make
# drivers/misc/Makefile
obj-$(CONFIG_CAR_MPU6050) += car_mpu6050/
# drivers/misc/car_mpu6050/Makefile
obj-$(CONFIG_CAR_MPU6050) += car_mpu6050.o
```

完整补丁还在父 Kconfig 添加 source，将驱动和 UAPI 头移入该子目录并仅调整共享头文件路径。`m` 是可加载模块，`y` 是内建，未选中则不构建；“源码树内构建”不等于“内建”。

`intree=Y` 支持核对本次通过内核树 Kbuild 生成的模块，但不代表代码进入 Linux 上游主线。还需核对 ELF 架构、vermagic、配置和实际绑定。modinfo 是查看文件信息，lsmod 是查看已加载模块，两者都不能单独证明设备 probe 成功。

## 6. 设备树与 I2C 分层：第三学习重点

文件：`drivers/mpu6050/lubancat3-mpu6050.dts`：

```dts
target = <&i2c7>;
__overlay__ {
    status = "okay";
    clock-frequency = <100000>;
    pinctrl-names = "default";
    pinctrl-0 = <&i2c7m1_xfer>;
    #address-cells = <1>;
    #size-cells = <0>;
    mpu6050@68 {
        compatible = "saberddx,car-mpu6050";
        reg = <0x68>;
        status = "okay";
    };
};
```

应会解释 controller 与 client、reg 是该总线下的 7 位地址、compatible 用于外设驱动匹配、pinctrl 用于引脚复用/电气配置。我们开发的是 MPU6050 外设 client 驱动，复用厂商 rk3x I2C 控制器驱动。

基础 DTS 位于厂家源码 `arch/arm64/boot/dts/rockchip/rk3576-lubancat-3.dts`，板卡 `/boot` 中实际使用的是 DTB，不保证安装了完整 DTS 源码。新基础 DTB 与导出的原文件哈希相同，所以保持原选择链接；overlay 的离线合并与运行树属性分别核对。

本 BSP 由 U-Boot 在启动前合并 DTBO，所以 Linux 没启用 CONFIG_OF_OVERLAY 不等于该流程不可用。

## 7. 启动布局与模块版本：第四学习重点

文件：`scripts/deploy-bsp-kernel.py`，`experiment_uenv()` 只替换两个字段：

```text
uname_r=6.1.99-rk3576-car-bsp
initrd=initrd-6.1.99-rk3576-car-bsp
```

厂家 boot.cmd 先尝试版本化 initrd，再尝试 initrd 字段；只改 uname_r 可能再次载入旧通用 initrd。本次使两次引用指向同一个新文件。新 modules 按 release 放进独立目录，stage 只安装文件，activate 才修改选择，且都不在脚本内自动重启。

文件：`scripts/prepare-bsp-initrd.py` 审核原 legacy U-Boot/gzip/newc 镜像，保留非模块内容，替换旧早期模块与索引，并重算 CRC。最终实际启动证明该配套 initrd 可用于本次实验内核；未构建新的 Ubuntu 根文件系统。

应会解释 Image、DTB/DTBO、initrd/initramfs 与根文件系统的职责，insmod 与 modprobe、modules.dep 和 vermagic 的作用。vermagic 匹配是必要检查之一，不是所有 ABI/行为兼容的充分证明。

## 8. 匹配失败与 probe 失败：第五学习重点

文件：`scripts/test-bsp-key-match.py`，`exercise()`：

```python
device.set_override(marker)
device.unbind()
device.probe()
fault = device.snapshot()
```

本次厂家 `drivers/base/platform.c` 的 platform_match() 在 OF 匹配之前检查 driver_override。错误名称会直接阻断匹配，虽然 compatible 正确且模块仍在。清除 override 并重扫后才重新走 probe、GPIO/IRQ/input 初始化。

面试时按顺序说明：设备是否枚举 → compatible/modalias/override 与模块别名 → driver 链接是否建立 → probe 返回和资源错误 → input/misc 节点 → 应用读取。不要用一条旧 ready 日志替代当前绑定状态。

本练习使用 platform 总线；不能把其 override 规则泛化成此 vendor Linux 6.1 I2C 框架也必然相同。更详细的真实代码与恢复路径见 [排障说明](05g-bsp-key-match-debug.md)。

## 9. 简历表述与边界

可用项目名：**基于 RK3576 的 Linux 车载终端与外设驱动开发**。

可依据已经完成的内容压缩为四条：

- 实现 GPIO 按键 platform/input 驱动，结合设备树、pinctrl、双边沿中断和 delayed work 消抖，通过 evdev 事件控制 Qt 摄像头窗口。
- 实现 MPU6050 I2C client 驱动与 misc/read 接口，完成六轴批量读取、固定长度 UAPI、mutex/kref 生命周期管理及 CSV 工具；原内核五分钟采集 6000 组零读取错误，覆盖并发和解绑/重绑检查。
- 基于 V4L2 MMAP 完成 USB 视频采集及 Qt 显示，处理缓冲区队列、图像格式和异常退出；底层复用现有 UVC 驱动。
- 基于厂家 BSP 完成 ARM64 内核/modules/DTB 构建、自编驱动 Kconfig/Kbuild 集成、配套 initrd 与独立安装，实板启动实验内核并完成 GPIO、MPU6050 和视频回归；完成 platform 匹配阻断/恢复的分阶段排障记录。

不能写成完成 U-Boot 移植、Buildroot/rootfs 从零构建、I2C/GPIO 控制器驱动从零开发、USB 摄像头底层驱动开发、MIPI sensor/CSI/ISP、PHY 移植、AI/NPU、RTOS/RPMsg。这些没有在本次实现。

回退边界：备份、Windows SHA256、LOADER/GPT/boot 定点读回已实测；写回预案和 Linux 配置回退工具已准备，但真实 eMMC 恢复与新旧内核往返切换没有作为本次实板验收执行。

## 10. 接下来学习顺序

先看 Kconfig/两级 Makefile 和构建参数，再看 uEnv/Image/initrd/modules 的对应关系，最后对照 platform_match 与本次故障记录。已有 GPIO、MPU6050、V4L2 知识分别回到 [GPIO](02-gpio-key.md)、[MPU6050](04b-mpu6050-data.md)、[V4L2/Qt](03-qt-camera-key.md)。

优先掌握配置的原因、各子系统的职责，以及用本次实际证据区分编译成功、模块加载、设备绑定和功能正常的方法，同时准确说明尚未测试的范围。
