# 新内核启动证据与外设回归

## 已验收：真实启动

用户在 activate/reboot、重新连接后返回：

```text
6.1.99-rk3576-car-bsp
{
  "running_kernel": "6.1.99-rk3576-car-bsp",
  "phase": "active",
  "uenv": "experiment"
}
```

这证明本次构建内核已在实板运行，启动配置与实际运行 release 一致。证据存为 `bsp/boot-result.json`。运行配置、设备树、模块绑定和外设功能需继续用实际结果验证，不能只靠 uname 判定整阶段完成。

## 回归命令

保持原摄像头、MPU6050 和按键接线，关闭车载 Qt 程序，在板卡以 cat 用户运行：

```sh
cd ~/car-driver-dev &&
git -c http.version=HTTP/1.1 pull --ff-only origin dev/driver-baseline &&
sh scripts/test-bsp-runtime.sh
```

不使用原来的模块编译测试脚本：实验包没有新 headers，旧 headers 不能替代。当前脚本只编译两个原生用户态 C 监视器，加载或复用 `/lib/modules/6.1.99-rk3576-car-bsp/` 的匹配模块。

### 自动检查

- `scripts/check-bsp-runtime.py` 从 `/proc/config.gz` 核对本次构建的完整配置 SHA256 和 CONFIG_CAR_MPU6050=m。
- 检查运行设备树的 GPIO4_A6、active low、KEY_CAMERA、20ms、GPIO pinmux/上拉，以及 I2C7 M1、100kHz、0x68 的引用与属性。
- 检查两个模块的安装位置/vermagic，并要求 MPU 模块带 intree=Y，核对本次 Kbuild 集成产物。
- 验证真实 I2C/platform 绑定、who_am_i=0x68、misc 节点归属、唯一按键 input 节点。
- 在新内核下采集 10 秒、目标 20Hz 的六轴数据，保存 CSV 并显示实际时间间隔与读取错误。
- 最后要求按 K1 三对按下/松开，其中一次保持约 3 秒；复用已验证的事件检查器确认状态配对、零重复和长按。

运行时已有的模块会复用且不卸载。脚本新加载的模块带唯一 run_id，结束或失败只清理归属于本次运行的实例。不会更改启动配置、设备树、接线或全局设备权限。

输出路径为 `build/bsp/car-bsp-runtime-.../`，内有 `kernel-tree.json` 和 `mpu6050.csv`。返回终端完整输出即可；此 10 秒回归不等同于此前原内核上的五分钟稳定性测试。

## 摄像头与 Qt 的验收边界

JSON 中的 usb_video_nodes 只表示 uvcvideo 枚举，不能证明帧采集成功。完成上述回归后，还需在板卡 VNC/图形桌面终端执行：

```sh
cd ~/car-driver-dev && sh scripts/run-car-gui.sh
```

新内核分支会直接使用已安装的匹配按键模块，不调用旧 headers 编译；Qt 本身仍按原流程构建。实际观察 K1 打开实时画面、长按保持、退出后重开，才验收新内核下摄像头/Qt 联动。

当前尚未收到本轮回归或新内核 GUI 的实板结果。可恢复的设备匹配排障实验也尚未执行，不能提前标记整个 BSP 衍生完成。

## 云端检查

`tests/test_bsp_runtime.py` 的 6 项检查通过，覆盖正常设备树、大端 cell、错误地址、GPIO 极性/上拉、I2C pin group、截断属性和非目标环境拒绝。shell 脚本语法检查通过，GUI 变更仅为模块选择与 vermagic 校验。云端没有实际 GPIO/I2C/摄像头，功能验收依赖用户后续实板结果。

另外从实际编译的 ARM64 Image 使用 extract-ikconfig 导出配置，确认其哈希与运行检查器要求一致；从真实合并的 `merged-mpu6050.dtb` 提取全部受检属性，构造成临时只读树后通过同一 audit_tree，核对 phandle 和节点路径规则。这仍是产物交叉检查，不替代运行树的实板检查。
