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

## 回退通道的收敛方向

Windows boot 备份校验、MASKROM 识别和退出后旧系统启动已通过。本次不修改 U-Boot、分区表或根文件系统，优先调查官方 Recovery 按键进入的 LOADER 通道：若现有 U-Boot 可提供 eMMC 访问，boot 分区恢复可能不需要另找 MASKROM 下载用 loader。此方向还未实测，不能标为可用。

下一步先取得用户 RKDevTool v3.37“高级功能”界面截图，核对实际按钮、存储选择、扇区地址和读回能力。打开页面本身不写入设备，无需为截图断电。随后再安排 Recovery/LOADER 识别及只读分区检查。MASKROM 作为独立的 ROM 识别通道已验证，但要让它访问 eMMC 仍需合适的下载 loader。

已有 boot 分区几何为起始 LBA 32768（0x8000）、长度 262144（0x40000）个 512-byte 扇区。该换算是板卡布局事实，不是授权用户直接按这些数值写入：必须先核对 Windows 工具的单位、当前目标存储和只读结果。压缩备份必须恢复为原始分区镜像才能用于原始块写回；不能写入整个磁盘的 LBA 0。

## 当前状态

- 已完成：ARM64 构建、离线模块/DTB/overlay 检查、boot 备份、Windows SHA256 校验、MASKROM 识别、返回原系统、上述部署输入检查。
- 进行中：initrd 内容核对、Recovery/LOADER 定点回退方法和最终空间预算。
- 未执行：新内核安装、启动切换、实板回归或分区恢复。
