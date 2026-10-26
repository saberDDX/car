# RK3576 BSP 衍生实践：任务与当前进度

## 目标和边界

在现有 LubanCat-3 项目上，使用厂家源码完成内核构建、MPU6050 的 Kconfig/Kbuild 集成、设备树核对和板卡启动验证。保留现有 Ubuntu 根文件系统，复用已验收的外设驱动。此阶段不等同于从零移植 BSP，不修改 U-Boot，不新增外设或 Qt 页面。

时间预算为约 8～12 小时主动操作与整理，下载和编译等待另计；环境可行性先调查最多约 2 小时。超出预算且主要卡在环境时重新评估，不无限扩展。已有项目可以继续投递。

## 已确认的条件

**最新进度：**ARM64 构建、驱动集成、独立安装、实验内核真实启动以及新内核配置/运行树/绑定检查已通过。新内核 MPU6050 200 组零读取错误、约 20Hz，GPIO 3 对事件/长按/零重复通过；用户也确认摄像头实时画面、重开和长按正常，见 [外设回归](05f-bsp-runtime-regression.md)。剩余 [可恢复匹配排障](05g-bsp-key-match-debug.md)实测与最终归档；实际分区恢复仍未执行。

完整运行配置与启动脚本已导出并完成分析，具体历史调查见 [板卡调查结果](05a-bsp-board-findings.md)。最新部署检查确认 `/boot` 可用 65,904,640 bytes，原 initrd 为带 legacy U-Boot 头的 8,436,090-byte 镜像；详细预算和后续输入见 [部署布局调查](05d-bsp-deployment-layout.md)。

- 实板此前信息：LubanCat-3 / RK3576、aarch64、Ubuntu 22.04.5、6.1.99-rk3576。
- 初始只有板卡和现有连接；现已具备 Windows RKDevTool、MASKROM 识别和电脑上的 boot 备份。尚未完成定点恢复验证，SSH/VNC 可用本身不代表新内核启动失败后可以恢复。
- 当前执行环境是云端 x86_64 工作区，不能直接进入板卡终端。仓库更新通过 GitHub 传给板卡。
- 云端观察到约 5 个可用逻辑 CPU、32GiB 可用内存、30GiB 工作区可用空间；这是本次观察值，完整源码、输出目录和依赖仍需预算。
- 厂家候选源码：[LubanCat/kernel](https://github.com/LubanCat/kernel/tree/639af5a858a073c3e968813138671740d2fec895)，分支 `lbc-develop-6.1`，本次调查固定提交 `639af5a858a073c3e968813138671740d2fec895`。
- 该提交 Makefile 为 Linux 6.1.99；存在 `arch/arm64/configs/lubancat_linux_rk3576_defconfig` 和 `arch/arm64/boot/dts/rockchip/rk3576-lubancat-3.dts`。
- 候选配置包含 `CONFIG_LOCALVERSION="-rk3576"`、IKCONFIG/IKCONFIG_PROC、MODULES、I2C_RK3X、BLK_DEV_INITRD、EXT4_FS。defconfig 是初始输入，不能与展开后的 `.config` 简单逐行等同。

**相同版本号不证明与已安装 BSP 完全匹配。** 厂家构建发布流程、完整运行配置、启动文件和恢复路径还需结合板卡报告确认。上面源码不是已验收部署版本。

## 分工与阶段验收

| 阶段 | 助手直接完成 | 用户参与 | 验收／状态 |
| --- | --- | --- | --- |
| 0. 可行性 | 源码与候选配置调查、只读采集脚本 | 将 boot 备份保存到 Windows 并校验、确认恢复条件 | 板卡 128MiB boot 备份、Windows 副本校验和 MASKROM 识别已完成；定点恢复未验证 |
| 1. 构建 | 固定版本、准备工具链及可重复构建命令，整理产物 | 看懂配置来源、架构与交叉编译参数 | ARM64 Image、256 个模块、DTB 已构建和离线验证 |
| 2. 集成 | 准备可审阅补丁，处理目录和头文件路径 | 对照 `CONFIG_CAR_MPU6050=m` 与 Kconfig/Makefile | 完整构建及新内核配置/intree/绑定/采集检查通过 |
| 3. 设备树 | 核对基础树与现有 overlay，避免重复节点 | 对照一次 reg/compatible/pinctrl | 离线合并与新内核运行树检查通过 |
| 4. 启动 | 准备独立文件安装、uEnv 两行切换、Linux 回退和 Windows 预案 | 返回运行版本，执行外设回归 | 新内核运行、配置/运行树、MPU/GPIO 与摄像头视觉回归全部通过 |
| 5. 排障 | 准备 platform driver_override 故障和恢复工具 | 判断“有设备无绑定”与 probe 失败的区别，按一次键确认恢复 | 工具及 8 项模拟检查完成，待实板执行 |
| 6. 归档 | 更新验收记录、知识点与项目清单 | 口述构建—启动—绑定流程 | 进行中，不能提前写为完成 |

## 已执行的板卡只读检查（无需重复）

```sh
cd ~/car-driver-dev && git -c http.version=HTTP/1.1 pull --ff-only && python3 scripts/inspect-bsp-deployment.py
```

脚本读取当前 boot 空间、Image/initrd 大小与链接、initrd 内模块版本、initramfs 工具和相关配置，并在有限目录中寻找已有 loader 文件名。只在项目 `build/bsp/deployment-inputs.txt` 保存报告，不写 `/boot`、不加载驱动、不安装软件、不重启。

用户已返回报告及原始 initrd。匹配新内核的独立 initrd 和含 initrd 的新离线包已完成，见 [initrd 分析与实际产物](05d-bsp-deployment-layout.md)。Windows 工具与 Recovery/LOADER 只读通道已核对，下一步恢复正常 Linux 后传入包并执行 stage；该动作保持当前 uEnv 不变。

报告中的 unavailable/找不到软件包可以是正常调查结果，保留原输出。不会输出整个环境变量、完整内核命令行或硬件序列号。文件存在不证明启动加载器真的使用它，需要后续与启动日志或厂家脚本对照。

## 集成补丁准备工具

`scripts/make-bsp-integration-patch.py` 从当前项目驱动和 UAPI 头文件生成补丁，目标位置为 `drivers/misc/car_mpu6050/`：

- 添加独立 Kconfig 和 Makefile。
- 在父级 Kconfig/Makefile 中接入配置项。
- 沿用自定义 compatible、misc/read 和已验证的寄存器流程。
- 仅调整共享头文件路径，不更改驱动功能。
- 生成后运行 `git apply --check`，不自动修改目标内核。
- 已存在配置项或目录时停止，避免重复追加。

在准备好的内核源码 checkout 上使用（路径由实际环境确定）：

```text
python3 scripts/make-bsp-integration-patch.py <kernel-checkout>
```

默认补丁保存到 `build/bsp/car-mpu6050.patch`。完整 ARM64 构建命令与实际验证结果见 [启动分析与构建记录](05b-bsp-boot-and-build.md)。最初用于语法检查的 x86_64 模块不能复制到 ARM64 板卡；本次产物包是单独构建并验证过架构的新内核及模块。

## 当前验证边界

已对固定的厂家源码生成并应用集成补丁。初期仅做过 x86_64 模块检查；现已进一步完成整棵厂家 ARM64 内核、模块和板级 DTB 的构建，olddefconfig 接受 `CONFIG_CAR_MPU6050=m`。这验证了 Kconfig/Kbuild 构建路径，但没有在实板上启动新内核，也未要求用户完成交互式 menuconfig 练习。

新的板卡采集脚本做语法检查和云端输出自检；云端缺少的板卡路径会明确报告 unavailable，不当作板卡事实。

## 启动与时间限制

当前已确认 MASKROM/LOADER 和 eMMC 定点读回，电脑保存最初 gzip 备份及最新 raw boot 快照。Linux 配置回退和 Windows boot 定点写回预案已明确；实际恢复尚未执行。部署保留原 Image/initrd/modules，通过独立动作才切换 uEnv。

实板曾有时钟回退，NTP 服务不可用；应核对真实时间，不能重复执行之前对话里的固定旧日期命令。内核构建需正确时间，避免 Clock skew 和错误增量结果。

## 最终验收与简历边界

必须有本次构建版本的运行证据、运行设备树证据、集成驱动绑定和采集结果、一次可复现排障记录，才标记整阶段完成。届时可表述为“基于厂家 BSP 的内核构建、设备树配置与自编驱动集成”，不写成完成 U-Boot、Buildroot 或整板从零移植。
