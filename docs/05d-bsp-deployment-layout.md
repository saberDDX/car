# 部署布局调查：Image、initrd 与回退通道

## 2026-10-26 实板只读检查结果

用户在原内核 `6.1.99-rk3576` 上运行 `inspect-bsp-deployment.py`，完整输出已返回。没有安装新模块或修改启动文件。

| 项目 | 实测结果 |
| --- | --- |
| `/boot` | `/dev/mmcblk0p2`，ext2，可用 65,904,640 bytes |
| 项目所在分区 | `/dev/mmcblk0p3`，ext4，可用 36,702,605,312 bytes |
| 原 Image | `/boot/Image-6.1.99-rk3576`，43,123,200 bytes |
| 原 initrd | `/boot/initrd-6.1`，8,436,090 bytes |
| initrd 前 8 字节 | `27051956253e87a3`，前 4 字节为 legacy U-Boot image magic |
| 基础 DTB | `/boot/rk-kernel.dtb` → `dtb/rk3576-lubancat-3.dtb`，306,930 bytes |
| 当前 uEnv | `uname_r=6.1.99-rk3576`、`initrd=initrd-6.1`，两个项目 overlay 均启用 |
| 工具 | depmod、modinfo、gzip、sha256sum、mkimage、fw_printenv 可找到；mkinitramfs、lsinitramfs 不可找到 |
| initramfs 配置文件 | MODULES=most、BUSYBOX=auto、COMPRESS=zstd；文件存在不证明当前 initrd 由该配置生成 |
| 根存储配置 | MMC、SDHCI、SDHCI_PLTFM、SDHCI_OF_DWCMSHC、EXT4_FS 均为 y |
| kexec | KEXEC/KEXEC_FILE 均未启用，不能使用 kexec 绕过启动文件切换 |
| loader 文件名候选 | 限定目录和遍历数量内未发现；不证明其他位置也没有 |

仅凭 magic 不能确认 initrd 类型字段、校验和、压缩格式和内部文件，需要进一步读取完整镜像。`lsinitramfs` 不可用是工具状态，不是本次检查失败。

## 空间预算

本次新 Image 43,057,664 bytes，独立 DTB 306,930 bytes。若新 initrd 与原镜像同样大小，则新增文件后预计还余：

```text
65,904,640 - 43,057,664 - 306,930 - 8,436,090
= 14,103,956 bytes，约 13.45 MiB
```

该值尚未扣除 ext2 文件和目录分配开销；也未证明新 initrd 会与旧文件同样大小。脚本预留的 16 MiB 是初始规划余量，不是 U-Boot 或内核的硬性要求。因此输出的 initrd budget 5,762,830 bytes 不应直接解读为“必须删除旧内核”或“无法部署”。

最终安装前按实际新文件的分配大小重新核算，保留原 Image、原 initrd 和原 modules，不自动删除旧文件来腾空间。模块放在根分区的独立 `/lib/modules/6.1.99-rk3576-car-bsp/`，不占 boot 分区预算。

## initrd 处理前的输入

先将原镜像复制到项目 build 目录，用 VS Code 下载并提供给助手。复制不修改 `/boot`：

```sh
cd ~/car-driver-dev &&
mkdir -p build/bsp &&
cp /boot/initrd-6.1 build/bsp/original-initrd-6.1.img &&
sha256sum build/bsp/original-initrd-6.1.img
```

助手检查 legacy image 头、CRC、实际压缩格式、cpio 内容和 init 脚本，不执行其中程序。重点判断是否带旧版本模块、硬编码版本或厂商启动逻辑，再决定复用还是生成配套 initrd。不能直接把整个带 U-Boot 头的文件当普通 gzip/zstd 解压，也不能仅因配置文件写 COMPRESS=zstd 就认定实际 payload 为 zstd。

启动脚本会先尝试 `initrd-${uname_r}`，之后再尝试 `${initrd}`。最终选择应让两次读取一致，或明确避免后一次覆盖；不只改 `uname_r`。

## 原 initrd 实际分析与新产物

用户上传的 `original-initrd-6.1.img` 与板卡输出 SHA256 完全一致：

```text
a866ace9754b86b9fd552ecfe16f14de5386246f8bf2e537b28634916d55768a
```

- legacy image 头部 CRC、payload 长度与 CRC 均通过；type=3（ramdisk），compression=1（gzip）。实际不是当前 initramfs 配置文件里的 zstd。
- gzip 解压后为 18,505,216-byte newc cpio，包含 357 项。检查采用内存解析，没有运行镜像中的脚本或二进制。
- `/init` 及 `/scripts` 使用 Ubuntu initramfs-tools 的挂载根分区、udev 和 run-init 流程；所检查的 init/scripts/conf 内容没有硬编码 `6.1.84`。
- `/lib` 链接到 `usr/lib`。模块目录为 `usr/lib/modules/6.1.84`，仅包含一个可加载模块 `kernel/drivers/net/usb/cdc_eem.ko`，其余为模块索引。发现旧目录本身不能证明原系统启动有故障，也不能靠改目录名解决模块 ABI。
- 新内核的 CDC_EEM=m，依赖 USB_USBNET 等均内建；完整 modules.dep 核对该模块没有其他可加载模块依赖。使用本次新内核实际构建并剥离的 AArch64 cdc_eem.ko，再用 depmod 生成新的最小模块索引。

新增 `scripts/prepare-bsp-initrd.py`：只接受上述已审核 SHA256 的原镜像，完整保留 339 项非模块记录的内容和 cpio 元数据，替换旧模块子树，重算 gzip 与 legacy image 头。保留原头的 ARM 架构字段作为该 BSP 的封装惯例；不会据此把实际 AArch64 用户程序或模块改为 32 位。

云端 SDK 准备好后可复现（输出已存在时拒绝覆盖）：

```sh
. bsp/cloud-env.sh
python3 scripts/prepare-bsp-initrd.py \
  build/bsp/original-initrd-6.1.img \
  build/bsp/stage/lib/modules/6.1.99-rk3576-car-bsp \
  build/bsp/initrd-6.1.99-rk3576-car-bsp \
  --depmod /workspace/bsp-sdk/root/usr/bin/depmod
```

实际新产物：

| 项目 | 结果 |
| --- | --- |
| 新 initrd | `build/bsp/initrd-6.1.99-rk3576-car-bsp`，8,464,152 bytes |
| 新 SHA256 | b3d8ef3de3ef0fbf32630f3165c426f5a8fe36dca6d1912be77d5511dbaa6c4b |
| 内部记录 | 358 项，仅保留新 release 的早期模块目录，原非模块记录逐字节比对一致 |
| 新预算 | 65,904,640 − 43,057,664 − 306,930 − 8,464,152 = 14,075,894 bytes，约 13.42 MiB，未扣额外文件系统开销 |
| 检查 | CRC、序列化读回、gzip/newc、AArch64 模块/vermagic、原记录保留通过 |

新增包含配套 initrd 的离线包 `build/bsp/car-bsp-artifacts-with-initrd.tar.gz`，29,094,301 bytes，SHA256 为 `a7e612fe27e8151abd5d85c37accc67f24045d02891b9ab09658a503ddeee6e5`。原先不含 initrd 的包仍保留，未改写。新包有 257 个匹配模块，包内文件哈希和模块架构/vermagic 已核对；既不是安装器，也不包含启动切换配置、headers 或恢复工具，不能直接覆盖解压到板卡 `/`。

`tests/test_bsp_initrd.py` 检查镜像读写、损坏 CRC、cpio 路径/截断/隐藏数据拒绝、解压大小限制和仅替换模块子树；连同已有 BSP 输入检查共 9 项通过。此结果只证明离线构造与检查，不等同于新 initrd 已在实板启动。

## 回退通道的收敛方向

Windows boot 备份校验、MASKROM 识别和退出后旧系统启动已通过。本次不修改 U-Boot、分区表或根文件系统，优先调查官方 Recovery 按键进入的 LOADER 通道：若现有 U-Boot 可提供 eMMC 访问，boot 分区恢复可能不需要另找 MASKROM 下载用 loader。此方向还未实测，不能标为可用。

已取得用户 RKDevTool v3.37“高级功能”界面截图，确认存在获取当前存储、读取Flash信息、导出镜像以及起始扇区/扇区数控件；此时底部没有发现设备，尚不能据此认定能读取 eMMC。下一步按 [Recovery/LOADER 只读步骤](05c-windows-usb-recovery-check.md) 核对模式与目标存储，再安排扇区读取。MASKROM 作为独立的 ROM 识别通道已验证，但要让它访问 eMMC 仍需合适的下载 loader。

已有 boot 分区几何为起始 LBA 32768（0x8000）、长度 262144（0x40000）个 512-byte 扇区。该换算是板卡布局事实，不是授权用户直接按这些数值写入：必须先核对 Windows 工具的单位、当前目标存储和只读结果。压缩备份必须恢复为原始分区镜像才能用于原始块写回；不能写入整个磁盘的 LBA 0。

## 当前状态

- 已完成：ARM64 构建、离线模块/DTB/overlay 检查、boot 备份、Windows SHA256 校验、MASKROM 识别、返回原系统、上述部署输入检查。
- 已进一步完成：initrd 内容分析、匹配模块替换和独立新 initrd、含 initrd 的离线包、实际文件大小预算。
- Recovery/LOADER 已实测识别，FlashInfo 读取成功，0x7550000 个扇区与约 58.7GiB eMMC 容量相符；待当前存储返回与小范围 GPT 导出进一步核对目标和扇区输入规则。
- 进行中：定点回退方法和最终部署时的可用空间复核，尚未验证实际分区恢复。
- 未执行：新内核安装、启动切换、实板回归或分区恢复。
