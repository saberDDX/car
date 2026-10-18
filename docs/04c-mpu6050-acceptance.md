# MPU6050 初版板卡验收记录

## 结论与证据来源

驱动与命令行初版功能验收通过。结论依据用户提供的 LubanCat-3 实际终端输出；云端没有连接传感器。被测实现为 `78ee4bf`，Linux 6.1.99-rk3576，I2C client 为 `7-0068`，绑定自编写的 `car-mpu6050` 驱动。

本次覆盖设备树枚举、身份检查、量程配置、六轴批量读取、misc/read、用户态换算与 CSV、五分钟持续读取、并发和解绑生命周期。Qt 数据页为可选扩展。

## 持续采集

命令：

```sh
cd ~/car-driver-dev && sh scripts/test-mpu6050-data.sh 300
```

测试标识：`car-mpu-1792336755-5846`。第一轮原始统计：

```text
Samples=6000 errors=0 elapsed=300.001s target_hz=20.00
Host intervals: min=38.824 mean=50.000 max=60.999 ms actual_hz=20.000
```

CSV 保存在板卡：

```text
/home/cat/car-driver-dev/build/mpu6050/car-mpu-1792336755-5846/round1.csv
```

平均读取频率约 20Hz，单次间隔 38.824～60.999ms，符合本项目普通 Linux 用户态周期读取的实测结果；不表示每次严格间隔 50ms，也不表示硬实时或传感器全部内部样本均被保存。内部采样配置为 100Hz，主机按需读取目标为 20Hz。

第二、三轮各 20 组、零读取错误，耗时均为 1.001 秒，平均频率分别为 19.994Hz、19.996Hz。三轮均正常卸载和清理。

## 物理响应

此前 10 秒测试静置合加速度约 0.986～0.995g；最终五分钟测试的第二、三轮示例均约 0.994g。各轴分量由摆放方向决定，无需强求 Z 轴始终等于 +1g。

另一次动态观察中，用户提供了前 33.507 秒日志：671 组、零读取错误，之后 Ctrl+C 中断。这次不计作五分钟验收，但用于观察动作响应：

- 约 1 秒时加速度 `[+0.151, +0.055, +1.041]g`，约 11.4 秒时变为 `[+1.046, +0.047, +0.132]g`，显示重力分量随倾斜改变。
- 转动时角速度出现 `[+53.52, -67.89, +13.69]°/s` 等较大正负变化。
- 运动时合加速度出现 0.804g 等变化，不能用静置约 1g 的条件去要求所有运动样本。
- 静置时存在约 1°/s 量级的陀螺零偏；本次未做零偏补偿或精度标定。

这些日志支持初步功能合理性，不等同于各轴独立方向校准、动态精度评估或姿态解算验证。

## 自动检查

最终完整输出确认：

```text
PASS: 24-byte ABI, short/zero/large reads, EFAULT, seek rejection, two readers.
PASS: old fd returns ENODEV across unbind/rebind; new fd reads normally.
PASS: MPU6050 identity read 15 times and three load/unload rounds passed.
PASS: six-axis reads, ABI/concurrent reads and open-fd unbind/rebind passed.
```

每轮五次真实 WHO_AM_I 读取，共 15 次，均为 0x68。持有旧 fd 解绑后读取返回 ENODEV；重新绑定后，新 fd 正常，旧 fd 仍返回 ENODEV。脚本结束时模块已卸载。

被测版本末尾的 `Physical plausibility and 5-minute stability still require reviewing real results` 是通用人工复核提醒，不是失败判定。上述板卡输出已完成本次人工复核；后续脚本将提醒改为检查本轮读数和实际持续时间。

## 环境问题与后续范围

板卡时钟曾落后于源码和 headers 文件时间，导致 Clock skew。系统 `timedatectl set-ntp true` 返回 NTP not supported，此次通过手动校时后继续。永久网络校时或重启后的时间保持尚未验证，后续编译仍保留时钟检查。

以下未在本次验收中完成：硬件断线/断电故障注入、传感器精度与零偏标定、长期老化、硬实时、INT/FIFO/DMP/IIO 接口、姿态融合和 Qt 数据页。驱动已有 I2C 错误传播与解绑检查，不把这些等同于全部硬件故障路径均已实测。

## 对应简历表述

可依据实际完成内容表述：

> 在 RK3576 Linux 平台实现 MPU6050 I2C client 驱动，通过设备树完成设备枚举，完成芯片身份检查、量程配置和六轴数据批量读取；设计 misc/read 接口及用户态采集工具，实现单位换算与 CSV 记录，完成五分钟 6000 组零读取错误测试，并验证并发访问、模块重载及设备解绑后的资源生命周期。
