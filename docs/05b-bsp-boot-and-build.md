# 运行配置、启动链路与云端构建

## 输入验证

用户提供的 `board-inputs.tar.gz` 已完成数据检查，未执行其中的启动脚本。原始输入保留在云端项目 `build/bsp/board-inputs/`，不提交整个压缩包或原始板卡配置到公开仓库。

- `/proc/config.gz` 导出的运行配置与 `/boot/config-6.1.99-rk3576` 字节完全相同。
- 两者 SHA256：`f1f1dad7b2987c425b2807919952c57176a56f660cb1b5a5dd662893513191ee`。
- `boot.scr` 为 legacy U-Boot script image，头部和数据 CRC 均通过。
- 内嵌脚本 3437 字节，与 `boot.cmd` 完全一致。
- uEnv 启用按键与 MPU6050 的两个自定义 overlay。

可重复分析：

```sh
python3 scripts/analyze-bsp-inputs.py build/bsp/board-inputs
```

## 启动脚本的实际含义

在该脚本被 U-Boot 选中执行的前提下：

1. 从启动分区读取 `/uEnv/uEnv.txt` 并导入环境。
2. 查找名为 rootfs 的分区，构造根设备启动参数。
3. 若存在，先加载 `/initrd-${uname_r}`；随后若通用 `${initrd}` 存在，还会再加载它到同一地址。
4. 加载 `/Image-${uname_r}`，当前为 `/Image-6.1.99-rk3576`。
5. 固定加载 `/rk-kernel.dtb`，当前链接到 LubanCat-3 基础 DTB。
6. 若启用 overlay，执行厂家 `dtfile` 命令处理 uEnv 中的 overlay。
7. 执行 `booti`，传入内核、ramdisk 和设备树地址。

这里的 `/Image...` 是从 U-Boot 选择的分区根目录看的路径，在当前 Linux 中对应 `/boot/Image...`，不能混淆。

静态文件分析并不能证明上电时 U-Boot 优先选择了哪个 boot target；`extlinux.conf` 也存在，需要启动日志或引导环境进一步确认。运行树已有自定义节点，与启动前合并 overlay 的方案相符。

**不能只改 uname_r 就认为实验启动已隔离。** 基础 DTB 仍由固定链接决定；当前 uEnv 的 `initrd=initrd-6.1` 也可能覆盖前面加载的版本化 initrd。后续应同时设计镜像、DTB、模块、initramfs 与回退路径。

## 构建输入与明确差异

- 实验源码固定为厂家提交 `b73c060dbe3576f056c197d7f2f7cc11f65e2aec`，提交时间接近运行内核构建时间，但尚无证据证明是原厂镜像的准确构建提交。
- 编译器采用 Debian GCC 12.2 AArch64 交叉工具链、binutils 2.40；原厂为 Arm GCC 10.3.1、binutils 2.36。
- 以用户导出的完整运行配置为输入，不直接使用 defconfig 替代。
- 显式设置 `CONFIG_CAR_MPU6050=m`。
- 实验后缀 `-rk3576-car-bsp`，关闭 LOCALVERSION_AUTO 并显式传递空 LOCALVERSION，预期 release 为 `6.1.99-rk3576-car-bsp`。
- 关闭 GCC_PLUGINS；原配置并未启用其中的具体插件。其他由工具链检测引起的配置差异保存在 config-diff.txt。
- 保留原功能配置和 WERROR，没有为了绕过编译错误批量关闭外设。

此构建是基于厂家源码的实验构建，不宣称二进制重现原厂内核。

## 构建步骤

源码目录需要是固定提交的完整 checkout，先应用 `make-bsp-integration-patch.py` 生成的补丁。使用独立的干净输出目录：

```sh
sh scripts/setup-bsp-cloud.sh
. bsp/cloud-env.sh
python3 scripts/make-bsp-integration-patch.py /workspace/bsp-kernel
git -C /workspace/bsp-kernel apply "$PWD/build/bsp/car-mpu6050.patch"
BSP_JOBS=4 sh scripts/build-bsp-kernel.sh \
  /workspace/bsp-kernel \
  "$PWD/build/bsp/board-inputs/running.config" \
  "$PWD/build/bsp/kernel-b73" \
  /workspace/bsp-sdk/root/usr/bin/aarch64-linux-gnu-
```

上述为新源码、未应用补丁且输出目录不存在配置时的流程，不是要求在当前已集成目录重复执行。脚本会拦截重复集成或误用已有输出目录。

SDK 使用 Debian 签名索引下载固定版本软件包，提取到工作区，不覆盖系统编译器。依赖基础云端已有 native gcc/make、OpenSSL 开发文件与 Debian 签名密钥。包清单见 `bsp/cloud-packages.txt`。相同包清单的已准备 SDK 会跳过重新提取；改变工具链前必须结束正在使用它的构建。未来镜像若移除指定版本，需要审核更新清单，不能关闭签名验证。

首次复验 SDK 时曾与构建并行重写 cc1，导致 `Text file busy`；这属于云端操作顺序问题。安装复验结束后使用相同配置进行增量重试，保留初次及重试日志。原始 Git ref 抓取曾发生在 archive 对应的 Git 元数据刷新结束前，初始文件保留为 build-inputs.initial.txt，已在最终输入记录中纠正并注明来源；实际源码来自固定 b73 archive。

## 验证与部署边界

### 本次实际结果

完整 ARM64 构建已完成，release 为 `6.1.99-rk3576-car-bsp`。Image、256 个内核模块和板级 DTB 通过离线验证；另为新内核编译了已有 GPIO 按键外部模块，打包共 257 个 AArch64 模块。

| 产物 | 字节数／结果 |
| --- | --- |
| Image | 43,057,664 bytes，ARM64 Linux Image |
| 原始 MPU6050 .ko | 84,192 bytes，AArch64、intree=Y、匹配实验 vermagic |
| LubanCat-3 DTB | 306,930 bytes |
| 基础 DTB SHA256 | ad6dffba0a66463f6b9bb6fdc9a857c51d059904f636f45043e6e353f3729b96 |
| 原厂 DTB 对照 | 与用户导出的原 DTB 字节哈希完全相同 |
| overlay | 按键后再合并 MPU6050，GPIO4_A6/KEY_CAMERA/20ms、I2C7 M1/100kHz/0x68 均通过 |

DTB 完全相同提高了候选源码对应关系的可信度，但不证明整个原厂内核准确来源于该提交。原始产物 SHA256 与大小见 `bsp/arm64-build-result.json`。

`build/bsp/car-bsp-artifacts.tar.gz` 为离线产物包，含独立命名 Image、基础 DTB、剥离调试信息的模块、模块依赖索引和来源信息；不包含 initramfs、内核开发 headers、启动切换配置或安装脚本。包内 SHA256SUMS 校验打包后的文件，原始构建记录中的模块哈希则对应剥离前文件。

**不要将该压缩包直接覆盖解压到板卡根目录。** 这不是已验收的系统升级包，也不能在旧内核上加载新版本模块。

### 下一步

用户电脑为 Windows，有 USB 转 Type-C 线，尚未验证数据连接和恢复工具识别。先在板卡运行 `scripts/backup-bsp-boot.sh`，只读备份已经确认的 mmcblk0p2，同时记录分区起始扇区、大小与校验和。将备份下载到电脑保存，不要求再上传整个镜像。

该脚本不写块设备、不修改 /boot，云端验证了非目标环境会拒绝运行。它对已挂载分区做同步后的快照，备份期间不要同时更新内核或修改启动文件；它不是完整 eMMC 备份，也没有验证恢复成功。

Windows USB 恢复流程必须核对板卡的准确接口、工具和分区写入方式后再执行。官方文档站本次云端访问返回 HTTP 403，不能把通用 Rockchip 流程直接当作该板卡已验证步骤。

构建日志、配置差异、来源记录和原始输出保存在 `build/bsp/`。离线产物验证使用 `verify-bsp-build.py`，检查 Image 格式、模块 AArch64 架构与 vermagic、DTB 板型、modules.order 中全部模块是否存在，并生成大小和 SHA256 清单。再对新基础 DTB 离线合并两个项目 overlay，核对设备属性。

用户没有恢复工具，当前不修改板卡 `/boot` 或模块目录。新内核启动、外设回归和故障恢复在实际执行并提供证据后才可验收。
