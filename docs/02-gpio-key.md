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

完成接线和设备树部署后，使用：

```sh
sudo ~/car-driver-dev/tools/key-monitor/key-monitor
```

程序按设备名 `car-gpio-key` 查找节点，不假设 event0。按下、松开 20 次后 Ctrl+C，预期按下 20 次、松开 20 次、重复 0 次；运行前让按键处于松开状态。零事件不算通过。队列溢出或设备移除会返回失败。

还需验证长按不自动重复，以及驱动卸载、重载 3 次之后仍能读取事件。具体部署和测试脚本将在引脚确定后补充。

## 验证状态

- `car_hello` 编译、加载和卸载：板卡已通过。
- 按键驱动：板卡编译已通过，生成 `car_gpio_key.ko`，vermagic 与运行内核版本一致；绑定 GPIO 和硬件验证待执行。
- 按键事件监视程序：云端及板卡编译已通过；云端检查了错误路径，真实事件待板卡验证。
- 设备树和接线：排针映射、无源模块电路已确认；overlay 已在云端使用测试基础树完成编译和离线合并校验，板卡实际符号、引脚占用和启动部署待执行。
- Qt 按键联动：后续实现。

板卡实际型号已由运行设备树确认：`EmbedFire LubanCat-3`，compatible 为 `embedfire,rk3576-lubancat-3` 和 `rockchip,rk3576`。模块中的 OF alias 与自定义 compatible 对应，仍需设备树节点描述硬件才能触发 probe。

用户的按键模块引脚为 K1–K4 和 GND，尚未接线，有母对母杜邦线。模块图片给出的原理图确认四个按键分别连接公共地，没有 VCC 接口；本阶段只使用 K1 和 GND 两根线。

云端的离线检查覆盖：正常合并、错误板卡型号、缺少 gpio4 符号、错误上下拉属性、重复 overlay，以及 7 种引脚占用输出。测试基础树用于验证脚本和 overlay 逻辑，不代替实际 RK3576 板卡验收。

## 厂商 overlay 加载方式

对照 [LubanCat 3 配置](https://github.com/LubanCat/kernel/blob/lbc-develop-6.1/arch/arm64/boot/dts/rockchip/uEnv/rk3576/uEnvLubanCat3.txt) 和 [boot.cmd](https://github.com/LubanCat/kernel/blob/lbc-develop-6.1/arch/arm64/boot/dts/rockchip/uEnv/boot.cmd)，厂商启动脚本读取启动分区的 `/uEnv/uEnv.txt`，使用 `enable_uboot_overlays=1` 和 `dtoverlay=/dtb/overlay/文件名.dtbo` 加载覆盖层。Linux 中通常对应 `/boot/uEnv/uEnv.txt`。

这确定了厂商支持的配置路径，但还需检查板卡实际配置和设备树符号。脚本已增加这些字段的采集；原输出只有 uname_r，是旧的字段过滤规则未包含 dtoverlay，不能据此认定所有 overlay 都未启用。

正式部署时需先确认目标物理引脚、当前占用、活动配置路径和符号支持，验证 overlay 能正确合并，再备份并追加配置。保留已有 overlay 和启动参数；当前尚未修改板卡启动配置。
