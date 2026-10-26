# BSP 板卡调查结果与下一步输入

本页保留初始板卡调查时点的事实与导出步骤。后续已完成实验内核构建、上板和功能回归；最新状态见 [最终归档](05h-bsp-acceptance-and-study.md)，无需重复当时的“下一步”。

依据用户运行 `a9d5d50` 的 collect-bsp-env.sh 后返回的报告。这里只记录实测信息与调查结论，不将候选源码当作已匹配发布源码。

## 已确认

| 项目 | 实测结果 |
| --- | --- |
| 板卡 | EmbedFire LubanCat-3 / RK3576，aarch64 |
| 系统 | Ubuntu 22.04.5 |
| 运行内核 | 6.1.99-rk3576，#7，构建时间 2026-01-10 15:35:04 CST |
| 原厂编译器 | aarch64-none-linux-gnu-gcc，Arm GNU 10.3-2021.07，GCC 10.3.1 |
| 存储 | mmcblk0，具有 boot0/boot1 区，符合 eMMC 设备特征 |
| 根分区 | /dev/mmcblk0p3，ext4，约 35GB 空闲 |
| 启动分区 | /dev/mmcblk0p2，ext2，分区标称 128MB，约 63MB 空闲 |
| 内存 | 约 7.7GiB，无 swap，本次约 5.3GiB available |
| uEnv 选择链接 | /boot/uEnv/uEnv.txt → uEnvLubanCat3.txt |
| 启动文件 | 存在 boot.cmd、boot.scr、extlinux/extlinux.conf；实际分支需读脚本确认 |
| 内核文件 | /boot/Image-6.1.99-rk3576 |
| DTB 链接 | /boot/rk-kernel.dtb → dtb/rk3576-lubancat-3.dtb |
| overlay | uEnv 已启用按键与 MPU6050 的自定义 overlay |
| 完整配置 | /proc/config.gz、/boot/config-6.1.99-rk3576 均可读 |
| image 包 | linux-image-6.1.99-rk3576，版本 6.1.99-rk3576-6 |
| headers 包 | linux-headers-6.1.99-rk3576，版本 6.1.99-rk3576-7 |

headers 与 image 的软件包修订号不同，不能仅据此判定 ABI 不兼容。此前自编模块已在该环境通过实测，headers 修订可能包含打包修复；仍需结合完整配置与来源核对。

## 正常现象与待核对事项

- `7-0068 driver=unbound`：上一轮测试结束会卸载模块，节点仍在而驱动未绑定符合预期，不需要重新部署设备树。
- `/boot/dtb/rockchip` 不存在：当前 BSP 将板级 DTB 放在 `/boot/dtb`，不能照搬内核构建输出目录作为安装目录。
- `dpkg-query` 找不到 `*lubancat*`：通配包名没有匹配，不是内核或硬件故障。
- `CONFIG_OF_OVERLAY` 未启用：此选项涉及 Linux 内核中的动态 overlay 支持；U-Boot 在内核启动前合并 DTBO 并不要求它开启。实际启动脚本仍要核对。
- `CONFIG_LOCALVERSION_AUTO=y`：新构建应明确实验 release，防止版本后缀变化不受控，或与原厂 modules 路径相撞。
- 当前 `/boot` 空闲约 63MB，不能假定足够并排存放新 Image、DTB、initramfs 和恢复文件。部署前必须按实际产物大小计算。
- 用户尚无独立恢复工具，因此当前不覆盖启动文件，不启动新内核。

## 源码版本调查

先前准备集成补丁的厂家提交 `639af5a858a073c3e968813138671740d2fec895`，提交时间为 2026-10-24，明显晚于当前运行内核的构建时间；它只是最初的代码集成验证基线。

通过加深厂家 Git 历史，找到构建时间之前最近的一条候选提交：

```text
b73c060dbe3576f056c197d7f2f7cc11f65e2aec
2026-01-10 15:31:40 +0800
dts rk3576 规范设备树插件名称，添加虚拟屏幕插件
```

**时间接近不能证明当前镜像由此提交构建。** 需要进一步核对完整运行配置、发布资料与设备树；不直接将现有内核归属到该 SHA。

原厂 Arm 10.3 工具链官方地址在本次云端网络访问中返回代理 403，目前未取得工具链。后续可核对厂家提供的下载来源，或评估可获得的编译器版本；不关闭 TLS 校验，不把不同工具链构建声称为原厂原样复现。

## 下一步：一次导出配置与启动文件

此操作只读取指定文件，并在项目 build/bsp 中保存配置和压缩包，不更改系统配置，不加载模块，不重启。

在板卡执行：

```sh
cd ~/car-driver-dev &&
mkdir -p build/bsp &&
gzip -dc /proc/config.gz > build/bsp/running.config &&
tar -czf build/bsp/board-inputs.tar.gz \
  -C "$PWD/build/bsp" running.config \
  -C / \
  boot/config-6.1.99-rk3576 \
  boot/boot.cmd \
  boot/boot.scr \
  boot/uEnv/uEnvLubanCat3.txt \
  boot/extlinux/extlinux.conf \
  boot/dtb/rk3576-lubancat-3.dtb &&
ls -lh build/bsp/board-inputs.tar.gz
```

将压缩包通过 VS Code 下载到电脑后上传到对话。文件中包含完整内核配置、指定启动脚本和基础 DTB，用于实际构建与启动分析。这里只导出列出的文件，不导出整个根文件系统。

收到后，助手可以直接完成配置对比、启动脚本分析、候选版本判断和构建准备。用户下一次核心参与点是理解并设置 `CONFIG_CAR_MPU6050=m`，不需要手动抄写驱动或逐文件查找配置。
