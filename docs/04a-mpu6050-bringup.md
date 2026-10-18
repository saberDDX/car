# MPU6050 第一步：身份验证与部署

## 当前范围

本文记录第一步身份验证，已通过板卡实测。驱动现已扩展六轴配置与 misc/read，新增内容见 [第二步采集与验收](04b-mpu6050-data.md)。原身份诊断接口和测试仍保留，不把身份验证等同于六轴物理数据验收。

## 已通过的板卡预检

用户实际输出确认：

- EmbedFire LubanCat-3，Linux 6.1.99-rk3576。
- CONFIG_I2C=y、CONFIG_I2C_RK3X=y、CONFIG_I2C_CHARDEV=y、CONFIG_IIO=y。
- i2c7 符号为 /i2c@2aca0000，当前 status=disabled。
- i2c7m1_xfer 和 pcfg_pull_none_smt 符号存在。
- pin 96 / GPIO3_A0、pin 97 / GPIO3_A1 均为 MUX UNCLAIMED 和 GPIO UNCLAIMED。

这是部署准备通过，还未证明传感器通信。Linux 的适配器编号以实际 of_node 对应关系为准，不硬编码 i2c-7。

## 部署、断电接线和测试

保持 MPU6050 未连接，在板卡运行：

```sh
cd ~/car-driver-dev && git -c http.version=HTTP/1.1 pull --ff-only && sh scripts/deploy-mpu6050-overlay.sh && sudo poweroff
```

脚本再次预检、编译模块、导出运行设备树并离线合并验证 overlay，随后备份并更新启动配置。只有这些步骤全部成功才执行关机。SSH 随关机断开是正常现象。错误时保留完整输出，不继续接线。

确认关机后拔掉电源，按板卡丝印确认第 1 脚及编号方向，再接线。适用于此前照片中的 HW-123 模块；更换不同供电设计时先重新核对。

| 模块引脚 | 板卡物理针脚 | 用途 |
| --- | --- | --- |
| VCC | 1：3.3V | 核对具体模块适用供电 |
| GND | 9：GND | 共地 |
| SDA | 3：GPIO3_A1 | I2C7 M1 数据 |
| SCL | 5：GPIO3_A0 | I2C7 M1 时钟 |
| AD0 | 14：GND | 选择 7 位地址 0x68 |
| XDA、XCL、INT | 不接 | 第一阶段不使用辅助总线或中断 |

原 K1→物理 7 脚、按键 GND→物理 6 脚保持不变。SDA/SCL 需要合适的上拉，常见模块自带，通信失败时核对模块电路和实际电平。

上电后运行：

```sh
cd ~/car-driver-dev && sh scripts/test-mpu6050.sh
```

测试脚本在板卡重新构建模块，检查运行节点、client 的 of_node 和实际 driver 绑定。每轮读取身份寄存器 5 次，卸载并检查绑定、诊断属性消失，共 3 轮。每次读取真实访问 I2C，不返回缓存的身份值。只清理本次 run_id 对应的模块，拒绝接管或卸载已有实例，不影响按键模块。

预期末尾：

```text
PASS: MPU6050 identity read 15 times and three load/unload rounds passed.
```

用户已提供真实输出：client 为 7-0068，15 次均读出 0x68，三轮绑定、身份读取与卸载通过。结束时模块已卸载。原日志有 Clock skew 警告，六轴测试前先校准板卡时钟并清理重编译。

## 核心代码和知识点

| 文件 / 函数 | 内容 |
| --- | --- |
| drivers/mpu6050/lubancat3-mpu6050.dts | I2C7 M1 pinctrl、100kHz、reg=<0x68>、自定义 compatible |
| car_mpu6050.c / car_mpu6050_probe | i2c_driver 匹配、i2c_client、适配器能力、Linux 6.1 probe_new |
| car_mpu6050_read_reg | 两个 i2c_msg 组成一次 i2c_transfer：写寄存器地址、repeated START、读取；短传输返回 -EIO |
| who_am_i_show | sysfs 只读诊断属性，每次实际读寄存器，错误传播到用户态 |
| scripts/build-mpu6050-overlay.sh | 离线合并并核对实际引脚组、地址、频率与驱动匹配 |

WHO_AM_I 寄存器地址为 0x75，标准 MPU6050 预期读出 0x68。AD0 接高会把总线地址改成 0x69，但身份寄存器预期值仍为 0x68。身份不符时先识别实际器件，不跳过校验。

## 启动配置保护和恢复

沿用 /boot/uEnv/uEnv.txt → /boot/uEnv/uEnvLubanCat3.txt。新增：

```text
dtoverlay=/dtb/overlay/car-lubancat3-mpu6050.dtbo
```

按键和其他 overlay 条目保持不变。安装器复用按键阶段的备份、原子写入、读回和失败恢复逻辑，使用同一把文件锁序列化配置写入。重复安装相同内容不重复追加。

备份路径会打印，格式为 /boot/uEnv/uEnvLubanCat3.txt.car-mpu6050-backup-时间戳。恢复时将**本次确切备份**复制回 /boot/uEnv/uEnvLubanCat3.txt，保留选择链接，再重启。无法启动时可在其他 Linux 环境挂载启动分区后恢复同一文件。离线生成的 merged-mpu6050.dtb 仅供检查，不能替换完整启动 DTB。

若 I2C7 已启用，部署预检会停止，先核对现有 pinctrl 和 client。身份验证使用 test-mpu6050.sh，不重复部署。

## 验证范围

- 板卡总线与引脚预检：用户实际输出已通过。
- 云端：Linux 6.1.190 x86_64 headers 下 W=1 模块编译通过；14 项 Python 测试全部通过，无跳过。覆盖原按键安装器回归、保留按键配置、重复部署、失败恢复、真实 dtc/fdtoverlay 合并，以及错误板型、已启用控制器、错误引脚、缺失配置和地址冲突的拒绝路径。
- 云端未连接板卡，也未实际访问 MPU6050；上述编译不等于 RK3576 板卡编译。板卡脚本会再次用其当前 headers 构建匹配架构和内核的模块。
- 板卡部署、芯片身份和真实卸载重载：已通过，15 次身份读取、三轮生命周期。
- 六轴数据、量程换算、misc/read：已通过板卡初版功能验收，含 5 分钟采集与生命周期测试，见 [实测记录](04c-mpu6050-acceptance.md)。
- Qt 数据页：后续可选。

厂商参考：[I2C7 M1 overlay](https://github.com/LubanCat/kernel/blob/lbc-develop-6.1/arch/arm64/boot/dts/rockchip/overlay/rk3576-lubancat-i2c7-m1-overlay.dts)、[RK3576 pinctrl](https://github.com/LubanCat/kernel/blob/lbc-develop-6.1/arch/arm64/boot/dts/rockchip/rk3576-pinctrl.dtsi)。
