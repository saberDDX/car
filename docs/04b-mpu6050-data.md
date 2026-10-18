# MPU6050 第二步：六轴采集与 misc/read

## 已确认与本次范围

板卡第一阶段已通过：I2C7 client 为 7-0068，15 次 WHO_AM_I=0x68，三轮加载/绑定/卸载正常。接线、启动 overlay 保持现状，本次不重新部署设备树。

本次扩展同一个驱动，加入唤醒、配置与读回校验、六轴及内部温度批量读取、24 字节 misc/read 接口、用户态单位换算和 CSV。按应用请求同步读取，不使用 INT、FIFO、DMP，不实现 poll。目标主机读取 20Hz，芯片内部采样 100Hz，不宣称硬实时或逐帧保存。

## 板卡操作

此前日志有 Makefile 和编译产物在未来、Clock skew 警告。先启用系统网络校时，再更新代码并测试：

```sh
sudo timedatectl set-ntp true && cd ~/car-driver-dev && git -c http.version=HTTP/1.1 pull --ff-only && sh scripts/test-mpu6050-data.sh
```

`check-build-clock.sh` 只读检查相关源文件、生成文件和已出现警告的 headers 文件；若仍在未来，会给 NTP 最多 30 秒，再停止。此时查看 `timedatectl status`，确认实际当前时间。若系统不支持 NTP 或网络校时不可达，依据可信当前时间手动校时后重试，不把源文件或 headers 批量 touch 成旧时间。

校时检查通过后脚本清理并重编译。保持接线，前 3 秒静置，随后轻轻倾斜和转动模块，不拉扯或插拔杜邦线。第一轮默认 10 秒、后两轮各 1 秒，屏幕约每秒一行，全部样本保存到 `build/mpu6050/本次run_id/round1.csv` 等文件。

测试包括原有 15 次身份读取和三轮重载，以及 misc 父设备、完整采样、短读/零读/大读约定、错误用户指针、不可 seek、两个并发读者。第三轮持有旧 fd 进行 sysfs unbind/rebind，确认旧 fd 一直返回 ENODEV，新 fd 正常工作。测试仅操作本次 run_id 对应模块和已核对的 client，不抢占已有实例。失败保留完整输出，不重复部署 overlay。

短测试通过并检查物理变化后，再运行 5 分钟版本：

```sh
cd ~/car-driver-dev && sh scripts/test-mpu6050-data.sh 300
```

确认首轮持续约 300 秒、errors=0，记录实际样本数和 Host intervals 的 min/mean/max、actual_hz。日志打印的 PASS 验证通信和软件行为；物理合理性需要查看真实值，不能仅凭 PASS 认定精度达标。脚本结束时模块已卸载。

## 配置与数据含义

| 寄存器 | 设置 | 作用 |
| --- | --- | --- |
| PWR_MGMT_1 0x6B | 先 0x80，后 0x01 | 复位、等待、唤醒；X 陀螺 PLL 时钟 |
| PWR_MGMT_2 0x6C | 0 | 六轴开启 |
| CONFIG 0x1A | 3 | DLPF：加速度 44Hz，陀螺 42Hz 带宽 |
| SMPLRT_DIV 0x19 | 9 | DLPF 开启时 1kHz/(1+9)=100Hz 内部采样 |
| GYRO_CONFIG 0x1B | 0 | ±250°/s，131 LSB/(°/s)，关闭自检 |
| ACCEL_CONFIG 0x1C | 0 | ±2g，16384 LSB/g，关闭自检 |
| FIFO_EN / INT_ENABLE / USER_CTRL | 0 | 不启用 FIFO、中断、DMP 或辅助主机 |

从 0x3B 一次连续读 14 字节，顺序是 AX、AY、AZ、TEMP、GX、GY、GZ，每个值两字节、高字节在前。有符号 raw 值转成 s16，内核保留整数；应用换算：

```c
double ax = sample.accel_x / 16384.0;        /* g */
double gx = sample.gyro_x / 131.0;           /* deg/s */
double temperature = sample.temperature / 340.0 + 36.53; /* 芯片内部温度 */
```

静置时合加速度 `sqrt(ax*ax+ay*ay+az*az)` 应约 1g，0.8～1.2g 可做初步功能筛查，不能当作精度标定。具体哪个轴接近 ±1g 取决于摆放方向。静止角速度通常有零偏，不要求严格为零。缓慢倾斜改变重力分量，绕某轴转动改变相应角速度，反向转动可看到符号变化。内部温度不作为环境温度。

## 核心文件、代码与面试点

### 1. I2C 组合传输与返回值

`drivers/mpu6050/car_mpu6050.c`，`car_mpu6050_read_regs()`：

```c
struct i2c_msg messages[] = {
    { .addr = client->addr, .len = 1, .buf = &reg },
    { .addr = client->addr, .flags = I2C_M_RD,
      .len = length, .buf = buffer },
};
ret = i2c_transfer(client->adapter, messages, ARRAY_SIZE(messages));
return ret == ARRAY_SIZE(messages) ? 0 : (ret < 0 ? ret : -EIO);
```

两条消息在一次 transfer 内完成寄存器寻址和读，之间是 repeated START。返回 2 才表示两条消息都完成；返回 1 也属于不完整传输。`i2c_master_send()` 写寄存器时返回的是字节数，不要混淆两者。我们写的是外围传感器 client，RK3X 控制器驱动仍由厂家提供，负责控制器寄存器、中断和实际总线传输。

### 2. 批量读、端序和时间戳

同文件，`car_mpu6050_read()`：

```c
ret = car_mpu6050_read_regs(mpu->client, MPU6050_ACCEL_XOUT_H,
                          bytes, sizeof(bytes));
if (ret)
    goto unlock;
sample.timestamp_ns = ktime_get_ns();
sample.accel_x = (s16)get_unaligned_be16(bytes + 0);
sample.accel_y = (s16)get_unaligned_be16(bytes + 2);
```

一次读取覆盖一组传感器寄存器，比逐轴独立读减少传输开销和数据跨更新的风险。`get_unaligned_be16` 处理大端字节对，转 s16 还原负值；不能直接按 CPU 原生字节序强转指针。时间戳是主机传输完成附近的单调时间，不能称为芯片采样时刻。20Hz 的按需读取也不保证每次都是内部 100Hz 的相邻样本。

### 3. misc/read 与用户空间

同文件，`car_mpu6050_fops` 和 `car_mpu6050_read()`；共享定义在 `include/car_mpu6050_uapi.h`：

```c
static const struct file_operations car_mpu6050_fops = {
    .owner = THIS_MODULE,
    .open = car_mpu6050_open,
    .release = car_mpu6050_release,
    .read = car_mpu6050_read,
    .llseek = no_llseek,
};
```

`misc_register()` 简化字符设备注册，节点为 `/dev/car_mpu6050`。一次 read 返回一个 24 字节样本；请求不足 24 字节返回 EINVAL，较大请求仍只返回一个样本，零字节请求返回 0。结构体使用固定宽度字段与对齐 64 位时间戳，预留字段清零。它是本机 native endian 接口，不是网络协议。`copy_to_user()` 失败返回 EFAULT，不能直接解引用用户指针。权限只读，驱动也拒绝带写权限打开。

### 4. 互斥锁与设备生命周期

同文件，`struct car_mpu6050`、`car_mpu6050_open/release/remove()`：

```c
misc_deregister(&mpu->misc);
sysfs_remove_groups(&client->dev.kobj, car_mpu6050_groups);
mutex_lock(&mpu->lock);
car_mpu6050_write_reg(client, MPU6050_PWR_MGMT_1, 0x40);
mpu->client = NULL;
mutex_unlock(&mpu->lock);
kref_put(&mpu->refs, car_mpu6050_free);
```

I2C 传输可能睡眠，采用 mutex，不能持自旋锁执行。`.owner` 保护模块代码，打开文件时模块不能正常卸载；但 sysfs 解绑仍可能发生。驱动对象用 kref 保留到最后一个 fd 关闭，解绑先阻止新打开、移除 sysfs，再同步在途传输并清空 client。旧 fd 再读返回 ENODEV，不能访问已经释放的 client。不能简单用 devm 分配整个 fd 私有状态后在 remove 返回时全部释放。

### 5. 用户态周期、换算与日志

`tools/mpu6050-monitor/mpu6050-monitor.c` 使用绝对 CLOCK_MONOTONIC deadline，错过周期后跳过补读，避免追赶导致突发访问。每次读检查完整长度和时间戳，失败停止，不把上一帧当新值。终端每秒显示一次，CSV 保存所有成功样本；已有 CSV 文件拒绝覆盖。Ctrl+C 输出统计并关闭资源，以 130 退出。工具报告实测读取间隔，不把配置频率直接当作实际频率。

misc 便于本次学习 read、锁和生命周期；Linux 传感器常用标准框架是 IIO，其 channel、scale、buffer、trigger 值得后续对照学习。当前没有把这个接口宣称为标准 IIO 驱动。

## 验证状态

- 已有板卡证据：身份读取和三轮模块生命周期。
- 新增云端证据：Linux 6.1.190 x86_64 headers 下 W=1 编译；用户态 -Werror 编译；20 项测试通过，覆盖既有 overlay 保护，以及 CLI 的负值换算、完整 64 位时间戳、短数据、重复时间戳、保留位、参数和输出文件保护。
- 云端没有 MPU6050，不能验证真实采样或内核动态生命周期。板卡脚本会重新编译 ARM64 模块；六轴物理行为、并发/解绑和五分钟持续采集仍待用户实测。
