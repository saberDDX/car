# 可恢复排障练习：platform 设备存在，但驱动未绑定

## 目的与故障边界

在已运行的实验内核上，使用本项目按键设备复现“设备存在、compatible 正确、模块已加载，但没有 driver 链接和 input 节点”的现象，再恢复绑定并验证物理按键。

本次故障是人为设置 platform 设备的 `driver_override`，不是修改了设备树 compatible，也不是传感器 I2C 通信失败。它验证 Linux 设备匹配层与驱动 probe/input 注册之间的关系，不等同于开发或修复了新的 GPIO 控制器驱动。

临时影响仅限本项目 `car-camera-key` 设备。不改 `/boot`、DTB/DTBO、MPU6050、网络或 U-Boot。先关闭车载 Qt 程序，避免它保留已移除 input 设备的旧文件描述符。

## 实板执行

已通过新内核回归，并确认摄像头画面后，关闭 Qt，在板卡终端运行：

```sh
cd ~/car-driver-dev &&
git -c http.version=HTTP/1.1 pull --ff-only origin dev/driver-baseline &&
sudo python3 scripts/test-bsp-key-match.py
```

工具打印 BEFORE、FAULT、RESTORED 三份状态。出现 Verify 后，按 K1 一次、保持约 3 秒再松开；收到最终 PASS 才算匹配故障与物理恢复均验证。报告保存到 `build/bsp/key-match-....json`。

目前脚本和模拟测试已完成，尚无本次练习的实板执行结果。不能将下文预期流程写成已经实测的故障经历。

## 具体流程与代码

文件：`scripts/test-bsp-key-match.py`。

开始时要求实验内核、LubanCat-3、正确兼容字符串、已绑定本项目驱动、一个 input 节点、空 driver_override。若模块原本未加载，使用已安装的匹配模块并附唯一 run_id；结束只卸载本工具自己加载的模块。

核心顺序：

```python
device.set_override(marker)  # 不存在的驱动名；先设置以阻止重扫描抢先重绑
device.unbind()
device.probe()
fault = device.snapshot()
```

预期 FAULT 仍有 platform 设备、原 compatible 和已加载模块，但 driver 为 null，events 为空，driver_override 为本次的错误名称。这里应定位匹配层，而非直接怀疑 GPIO 接线、IRQ 或 input_report_key。

恢复放在 finally 内：

```python
device.set_override("")  # 实际写一个换行，内核清除 override
if device.snapshot()["driver"] is None:
    device.probe()
restored = device.wait_ready()
```

写回空 override 只移除强制匹配限制；重新扫描才触发正常匹配。驱动再次 probe 后获取 GPIO、申请 IRQ、注册 input 设备。事件设备编号可能复用或变化，工具按真实 sysfs 父设备重新发现，不硬编码 event6。

finally 覆盖正常结束、一般异常、Ctrl+C；主入口将 SIGTERM/SIGHUP 转为可清理退出。对方并发改成其他 override 时拒绝覆盖；SIGKILL、断电或内核/存储故障不能靠 Python finally 保证恢复。

## 对应的真实内核代码

所构建厂家源码提交 b73c060d...，`drivers/base/platform.c`，`platform_match()`：

```c
/* When driver_override is set, only bind to the matching driver */
if (pdev->driver_override)
    return !strcmp(pdev->driver_override, drv->name);

/* Attempt an OF style match first */
if (of_driver_match_device(dev, drv))
    return 1;
```

driver_override 优先于正常 OF 匹配。因此 compatible 正确仍可能被 override 阻断。清空后才继续 OF → ACPI → id_table → 名称回退的正常规则。不能把匹配成功与 probe 成功混为一谈：前者决定是否尝试初始化，后者还可能因 GPIO 资源、IRQ 或分配错误而失败。

`drivers/base/driver.c` 的 `driver_set_override()` 明确将 `"\n"` 解释为清除 override。只在 shell 中用 `echo -n '' > file` 可能写入零字节而不调用有效 store，不能当作已清除。

`drivers/base/bus.c` 中 drivers_probe_store() 调用设备重扫描。此 vendor Linux 6.1 的 I2C match 路径没有同样的 driver_override 判断，所以本练习明确使用 platform 按键，不把此行为泛化到所有总线。

本项目 `drivers/gpio-key/car_gpio_key.c` 的正常匹配信息：

```c
static const struct of_device_id car_key_of_match[] = {
    { .compatible = "saberddx,car-gpio-key" },
    { }
};
MODULE_DEVICE_TABLE(of, car_key_of_match);
```

MODULE_DEVICE_TABLE 支持用户态模块自动加载，模块注册和设备匹配/绑定是不同阶段。lsmod 里有模块不证明对应设备已成功 probe。历史 dmesg 中曾出现 ready 也不证明当前仍绑定，排查要对照当前 driver 链接、input 节点和本次实验状态。

## 未完成时的定点恢复

若工具异常退出后显示仍残留它自己的错误 override，先确认当前内容确属本次 `car-intentionally-unmatched-...`，然后可在原平台设备上清空并重扫：

```sh
sudo sh -c 'printf "\n" > /sys/bus/platform/devices/car-camera-key/driver_override'
sudo sh -c 'printf "car-camera-key\n" > /sys/bus/platform/drivers_probe'
```

不清除别的驱动管理策略，不通过重刷固件解决本练习的匹配问题。恢复后重新观察绑定和真实按键事件。

## 云端验证范围

8 项状态机检查通过：正常故障/恢复、已有 override 不动、设置后失败恢复、解绑失败恢复、重扫失败恢复、Ctrl+C 路径、并发其他 owner 不覆盖、云端主入口拒绝。测试使用模拟设备，不写真实 sysfs，不作为实板完成证据。
