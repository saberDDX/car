# 第二阶段：GPIO 按键驱动

## 当前目标

先让一个物理按键通过自己的驱动上报标准 Linux input 事件，再接入摄像头界面。Qt 在最后一步做少量集成，学习重点是驱动与调试。

模块编译和加载基线已经在板卡通过：用户提供了 `car_hello` 的本次加载、卸载日志和脚本 PASS。这只证明基线工作，不代表按键或 MPU6050 已验证。

## 先编译

```sh
cd ~/car-driver-dev
git -c http.version=HTTP/1.1 pull --ff-only && sh scripts/prepare-gpio-key.sh
```

脚本自动检查环境、编译 `car_gpio_key.ko` 和事件监视程序，读取板卡型号及启动配置中的设备树选择信息。此步骤不加载按键驱动，也不修改设备树或 GPIO。

## 驱动数据流

1. `compatible = "saberddx,car-gpio-key"` 匹配 platform 驱动，进入 `probe`。
2. `devm_gpiod_get(dev, "button", GPIOD_IN)` 从 `button-gpios` 获取输入引脚。
3. GPIO 转换成 IRQ，申请上升沿和下降沿中断。
4. 中断只重新安排延迟工作；默认最后一次边沿后约 20ms 采样。
5. 工作队列读取逻辑电平，只在状态改变时上报 `EV_KEY`，随后 `input_sync`。
6. 用户程序通过 `/dev/input/eventN` 接收按下和松开事件。

按下是 value=1，松开是 value=0；value=2 表示自动重复。此驱动不启用 EV_REP，所以长按不应反复触发打开摄像头。

`GPIO_ACTIVE_LOW` 会让描述符接口把低电平转换为逻辑值 1。驱动不要再手动取反。

软件消抖会忽略比消抖窗口更短的脉冲，适用于本项目的人手按键，不作为高速脉冲采集方案。

## 设备树所需信息

| 属性 | 用途 |
| --- | --- |
| compatible | 固定为 `saberddx,car-gpio-key` |
| button-gpios | 确认后的 GPIO 控制器、引脚及有效电平 |
| pinctrl-names / pinctrl-0 | 切换到 GPIO 输入并配置合适的上下拉 |
| linux,code | 默认 `KEY_CAMERA`，可配置其他有效键码 |
| debounce-interval | 默认 20，允许 1..1000，单位毫秒 |
| status | `okay` 启用节点 |

物理排针编号与 GPIO 编号不是一回事。用户提供的 LubanCat-3 官方排针表已确认物理 7 脚为 GPIO4_A6，物理 6 脚为 GND。GPIO4_A6 还能复用为 CAN0_RX_M2、I2C4_SDA_M1 和 UART6_RX_M0；是否空闲仍需在板卡读取实际占用，不能仅凭排针表判断。

## 接线方案与 overlay 预检

先保持模块未接线，运行一条命令：

```sh
cd ~/car-driver-dev && git -c http.version=HTTP/1.1 pull --ff-only && sh scripts/prepare-key-overlay.sh
```

脚本执行以下步骤：

1. 编译驱动和监视程序，输出板卡型号、实际启动配置字段及符号信息。
2. 缺少 `dtc`、`fdtoverlay`、`fdtget` 时，通过系统 apt 安装 `device-tree-compiler`，可能需要输入 sudo 密码。
3. 读取 debugfs 的 pinmux-pins，检查 pinctrl 编号 134（bank 4、offset 6，即 GPIO4_A6）。仅接受明确的 GPIO 和 mux 都未被申请的输出；占用、缺失或无法识别都会停止。必要时挂载 debugfs，不申请 GPIO 或改变电平。
4. 导出运行设备树，在 `drivers/gpio-key/build/` 编译 overlay、离线合并，核对 GPIO 引用、有效电平、上拉、键码、消抖和 pinctrl 引用。

此阶段不改 `/boot`，不加载按键模块，不重启。把完整输出反馈后，才能根据实际启动配置准备部署。`PREPARED` 只代表预检和离线合并通过，不代表物理按键已经工作。

预检通过后，断开板卡电源再按下表接线：

| 按键模块 | 板卡物理针脚 | 用途 |
| --- | --- | --- |
| GND | 6 | 公共地 |
| K1 | 7 | GPIO4_A6 输入 |
| K2、K3、K4 | 不接 | 后续再扩展 |

按板卡丝印的 1 脚标记确定方向，排针编号按 1/2、3/4、5/6、7/8 成对排列；不能凭照片左右方向猜编号。不接 3.3V 或 5V：该模块是无源按键，按下时把 K1 接到 GND。照片标注“不带电容”，仍需驱动的软件消抖。

`drivers/gpio-key/lubancat3-gpio4-a6-key.dts` 使用 `button-gpios = <&gpio4 6 1>`。其中 6 是 GPIO4 内的 offset，不是物理 6 脚；1 表示低电平有效。`rockchip,pins = <4 6 0 &pcfg_pull_up>` 选择 GPIO 功能并开启内部上拉。松开时为高电平，按下时为低电平。

`scripts/build-key-overlay.sh` 也可单独处理导出的基础 DTB。它会检查板卡型号和必要符号，并拒绝向已有本项目节点的设备树重复合并。输出的 `merged-key.dtb` 仅用于检查，不能直接替换板卡完整启动设备树。

没有匹配的设备树节点时，加载模块只会注册驱动，不会执行 `probe`，也不会出现本项目的 input 设备。模块存在和硬件绑定成功是两项不同的检查。

## 生命周期与知识点

- `devm_*` 在设备解绑或 probe 失败时释放资源。
- 本驱动按顺序注册 input、工作清理动作和 IRQ。释放时顺序相反：先停 IRQ，再同步取消工作，最后注销 input，避免工作访问已释放对象。
- 硬中断上下文不能执行可能睡眠的 GPIO 读操作，读取放到工作队列。
- `gpiod_to_irq` 完成 GPIO 到 IRQ 的映射，不硬编码中断号。
- `mod_delayed_work` 合并抖动边沿；状态比较进一步避免重复上报。
- `dev_err_probe` 保留具体错误并支持延迟探测，便于区分资源未准备好与无效配置。

## 后续硬件验收

板卡预检已通过：GPIO4_A6 的 pinmux 输出为 `(MUX UNCLAIMED) (GPIO UNCLAIMED)`，运行设备树具备 gpio4、pinctrl 和 pcfg_pull_up 符号，overlay 离线合并校验 PASS。当前选择文件 `/boot/uEnv/uEnv.txt` 指向 `/boot/uEnv/uEnvLubanCat3.txt`，`enable_uboot_overlays=1`。

### 部署并断电接线

保持未接线，运行下面的命令。只有部署成功才会执行关机，SSH 随关机断开属于正常现象：

```sh
cd ~/car-driver-dev && git -c http.version=HTTP/1.1 pull --ff-only && sh scripts/deploy-key-overlay.sh && sudo poweroff
```

脚本再次运行预检，再把 overlay 安装到 `/boot/dtb/overlay/car-lubancat3-gpio4-a6-key.dtbo`。Python 助手确认选择链接和内核版本，备份实际配置文件，在 `#overlay_end` 之前追加一行 `dtoverlay`。保持现有配置、其他 overlay、文件权限和选择链接。先安装 blob，再原子更新引用并读回检查；写入失败时尝试恢复旧文件。重复部署相同内容不会重复添加条目。

配置备份路径会输出，格式是 `/boot/uEnv/uEnvLubanCat3.txt.car-key-backup-时间戳`。如需恢复，使用输出的**确切备份路径**将备份复制回 `/boot/uEnv/uEnvLubanCat3.txt`，保留 `/boot/uEnv/uEnv.txt` 的链接，然后重启。若无法启动，在其他 Linux 环境挂载板卡启动分区后恢复同一文件即可。仅预检不生成这份配置备份。

确认关机后拔掉电源，接 GND→物理 6 脚、K1→物理 7 脚，再上电。没有自动加载模块的设置；本阶段由测试脚本加载和清理。

### 开机后的一条验收命令

```sh
cd ~/car-driver-dev && sh scripts/test-gpio-key.sh
```

等每一轮出现 `Verify` 提示再操作：

1. 第一轮按下、松开共 20 次，其中一次按住 3 秒再松开，其余按普通速度操作。长按也包含在这 20 次中。
2. 达到 20 次后停止操作，程序观察 1 秒并自动结束这一轮，不用 Ctrl+C。
3. 第二、三轮分别重新加载模块，各长按 3 秒、松开一次。等下一轮提示后再按。

脚本核实运行设备树节点、当前模块版本、platform 设备的 `driver` 和 `of_node` 链接。监视程序按设备名 `car-gpio-key` 查找 event 节点，不假设 event0；验证 KEY_CAMERA/212、交替的按下/松开、精确次数、零重复事件，以及至少一次达到 2 秒的长按。使用单调时钟计算时长；建议按住 3 秒留出余量。

重复按下、松开顺序错误、额外事件、SYN_DROPPED、设备移除、提前 Ctrl+C 或缺少长按都会失败。达到次数之后的观察窗口继续检查事件，避免刚好达到次数就退出而漏掉后续抖动。此测试反映本次人工操作，不宣称所有抖动条件都已覆盖。

每轮卸载后确认模块、driver 绑定和 input 设备消失，下一轮再次读取真实事件。失败或中断时也只清理本次 `run_id` 对应的模块，拒绝卸载已有实例。测试全部成功后模块处于卸载状态。

需要自由查看事件时，仍可运行：

```sh
sudo ~/car-driver-dev/tools/key-monitor/key-monitor
```

此模式使用 Ctrl+C 结束，只打印统计；正式验收使用 `test-gpio-key.sh`。本阶段验证按键驱动，摄像头打开功能将在 Qt 集成阶段接上。

### 云端可重复检查

```sh
python3 -B -m unittest discover -s tests -v
make -C tools/key-monitor check
```

已通过 8 项配置/文件测试，包括保留其他配置及 overlay、重复安装、选择链接与权限、旧 blob 备份、替换配置后模拟 fsync 失败的恢复、拒绝异常链接，以及启动配置长度限制。C 事件验证测试覆盖正常的 20 对事件、长按、重复/额外/乱序事件、错误键码和时间戳。用户态程序用 `-Wall -Wextra -Wpedantic -Werror` 编译通过。这些云端检查不代替板卡上的中断、GPIO 电平和真实 input 验收。

## 验证状态

- `car_hello` 编译、加载和卸载：板卡已通过。
- 按键驱动：新版本在板卡编译、绑定和真实事件验收已通过，含只读 run_id 参数的本次模块生命周期可核对。
- 按键事件监视程序：板卡第一轮 pressed=20、released=20、repeated=0，长按达到阈值，观察窗口通过；两次卸载重载后的单次长按验证也通过。
- 设备树和接线：运行节点触发本项目驱动 probe；K1→物理 7 脚、GND→物理 6 脚的硬件链路已由真实事件验证。
- Qt 按键联动：板卡实时画面、重复/长按保持单窗口、退出重开三次已由用户确认通过，见 [03-qt-camera-key.md](03-qt-camera-key.md)。

板卡实际型号已由运行设备树确认：`EmbedFire LubanCat-3`，compatible 为 `embedfire,rk3576-lubancat-3` 和 `rockchip,rk3576`。模块中的 OF alias 与自定义 compatible 对应，仍需设备树节点描述硬件才能触发 probe。

用户的按键模块引脚为 K1–K4 和 GND，尚未接线，有母对母杜邦线。模块图片给出的原理图确认四个按键分别连接公共地，没有 VCC 接口；本阶段只使用 K1 和 GND 两根线。

云端的离线检查覆盖：正常合并、错误板卡型号、缺少 gpio4 符号、错误上下拉属性、重复 overlay，以及 7 种引脚占用输出。测试基础树用于验证脚本和 overlay 逻辑，不代替实际 RK3576 板卡验收。

## 厂商 overlay 加载方式

对照 [LubanCat 3 配置](https://github.com/LubanCat/kernel/blob/lbc-develop-6.1/arch/arm64/boot/dts/rockchip/uEnv/rk3576/uEnvLubanCat3.txt) 和 [boot.cmd](https://github.com/LubanCat/kernel/blob/lbc-develop-6.1/arch/arm64/boot/dts/rockchip/uEnv/boot.cmd)，厂商启动脚本读取启动分区的 `/uEnv/uEnv.txt`，使用 `enable_uboot_overlays=1` 和 `dtoverlay=/dtb/overlay/文件名.dtbo` 加载覆盖层。Linux 中通常对应 `/boot/uEnv/uEnv.txt`。

厂商配置路径、选择链接、overlay 启用状态和所需设备树符号已经在板卡实际输出中核实。原输出只有 uname_r，是旧的字段过滤规则未包含 dtoverlay，不能据此认定所有 overlay 都未启用。

部署脚本在临时目录中验证配置保留、备份及失败恢复。用户后续反馈三轮硬件测试全部 PASS，确认重启后的 overlay 已生效，GPIO4_A6 上报 KEY_CAMERA；第 2、3 轮就绪日志显示 IRQ=104、消抖=20ms。IRQ 数值是本次运行的动态映射结果，不作为后续硬编码常量。验收结束时模块已卸载。
