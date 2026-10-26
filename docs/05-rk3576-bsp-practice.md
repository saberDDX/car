# RK3576 BSP 衍生实践：任务与当前进度

## 目标和边界

在现有 LubanCat-3 项目上，使用厂家源码完成内核构建、MPU6050 的 Kconfig/Kbuild 集成、设备树核对和板卡启动验证。保留现有 Ubuntu 根文件系统，复用已验收的外设驱动。此阶段不等同于从零移植 BSP，不修改 U-Boot，不新增外设或 Qt 页面。

最初时间预算为约 8～12 小时主动操作与整理，下载和编译等待另计。实际没有连续工时记录，不能将这个估算写成已验证耗时。当前限定主线已完成并归档，后续优先学习和投递，不新增必做硬件。

## 已确认的条件

**最终状态：**构建、驱动集成、独立安装、实验内核真实启动、运行配置/树/绑定和外设功能回归完成。[匹配练习](05g-bsp-key-match-debug.md)按分阶段证据归档：首次运行到恢复后的物理验收但收到额外按下而失败；后续只复查取得 3.055900 秒长按、1 对事件、零重复和静默 PASS，未重新注入故障。首次额外事件原因未确定，单次完整命令全 PASS 报告未取得。完整清单、学习重点与简历边界见 [最终归档](05h-bsp-acceptance-and-study.md)。实际分区恢复仍未执行。

完整运行配置与启动脚本已导出并完成分析，具体历史调查见 [板卡调查结果](05a-bsp-board-findings.md)。最新部署检查确认 `/boot` 可用 65,904,640 bytes，原 initrd 为带 legacy U-Boot 头的 8,436,090-byte 镜像；详细预算和后续输入见 [部署布局调查](05d-bsp-deployment-layout.md)。

- 实板：LubanCat-3 / RK3576、aarch64、Ubuntu 22.04.5；原内核 6.1.99-rk3576，现已运行实验内核 6.1.99-rk3576-car-bsp。
- 初始只有板卡和现有连接；现已具备 Windows RKDevTool、MASKROM 识别和电脑上的 boot 备份。尚未完成定点恢复验证，SSH/VNC 可用本身不代表新内核启动失败后可以恢复。
- 当前执行环境是云端 x86_64 工作区，不能直接进入板卡终端。仓库更新通过 GitHub 传给板卡。
- 云端观察到约 5 个可用逻辑 CPU、32GiB 可用内存、30GiB 工作区可用空间；这是本次观察值，完整源码、输出目录和依赖仍需预算。
- 实际构建源码：[LubanCat/kernel b73c060](https://github.com/LubanCat/kernel/tree/b73c060dbe3576f056c197d7f2f7cc11f65e2aec)，来自 `lbc-develop-6.1` 历史。早期调查的 639af5a... 不是最终构建版本。
- Makefile 为 Linux 6.1.99；存在 `arch/arm64/configs/lubancat_linux_rk3576_defconfig` 和板级 DTS。实际以导出的完整运行 `.config` 为配置基线，不能拿 defconfig 简单替代。
- 工具链为 GCC 12.2 AArch64 / binutils 2.40，与原厂 Arm GCC 10.3.1 明确不同。实验 LOCALVERSION 为 `-rk3576-car-bsp`，MODULES/I2C_RK3X/BLK_DEV_INITRD/EXT4_FS 保留，CAR_MPU6050=m。

**相同版本号、时间接近或 DTB 相同不证明原厂旧镜像恰由此提交构建。** 本次证明的是固定该来源派生出的实验内核已实际启动并通过本项目功能回归，不宣称原厂二进制重现或从零移植整板。

## 分工与阶段验收

| 阶段 | 助手直接完成 | 用户参与 | 验收／状态 |
| --- | --- | --- | --- |
| 0. 可行性 | 源码与候选配置调查、只读采集脚本 | 将 boot 备份保存到 Windows 并校验、确认恢复条件 | 板卡 128MiB boot 备份、Windows 副本校验和 MASKROM 识别已完成；定点恢复未验证 |
| 1. 构建 | 固定版本、准备工具链及可重复构建命令，整理产物 | 看懂配置来源、架构与交叉编译参数 | ARM64 Image、256 个模块、DTB 已构建和离线验证 |
| 2. 集成 | 准备可审阅补丁，处理目录和头文件路径 | 对照 `CONFIG_CAR_MPU6050=m` 与 Kconfig/Makefile | 完整构建及新内核配置/intree/绑定/采集检查通过 |
| 3. 设备树 | 核对基础树与现有 overlay，避免重复节点 | 对照一次 reg/compatible/pinctrl | 离线合并与新内核运行树检查通过 |
| 4. 启动 | 准备独立文件安装、uEnv 两行切换、Linux 回退和 Windows 预案 | 返回运行版本，执行外设回归 | 新内核运行、配置/运行树、MPU/GPIO 与摄像头视觉回归全部通过 |
| 5. 排障 | 准备 platform driver_override 故障/恢复及时间戳复核工具 | 判断匹配与 probe、解释首次多余事件 | 已按首次尝试与独立物理复查分阶段归档，未伪装成单次全 PASS |
| 6. 归档 | 更新验收记录、知识点与项目清单 | 学习代码并按实际范围写简历 | 最终归档完成；首个额外事件定因、实际分区写回均未纳入完成声明 |

## 已执行的板卡只读检查（无需重复）

```sh
cd ~/car-driver-dev && git -c http.version=HTTP/1.1 pull --ff-only && python3 scripts/inspect-bsp-deployment.py
```

脚本读取当前 boot 空间、Image/initrd 大小与链接、initrd 内模块版本、initramfs 工具和相关配置，并在有限目录中寻找已有 loader 文件名。只在项目 `build/bsp/deployment-inputs.txt` 保存报告，不写 `/boot`、不加载驱动、不安装软件、不重启。

上述为已经执行的调查命令。后续独立 initrd、Windows 读回、stage/activate、真实启动和功能回归均已推进完成；最新结论集中在 [最终归档](05h-bsp-acceptance-and-study.md)，无需照历史步骤重新安装或切换。

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

已对固定的厂家源码生成并应用集成补丁。初期 x86_64 检查之外，现已完成 ARM64 内核/modules/DTB 构建及实板启动，运行配置、intree、设备绑定与采集检查通过。没有要求用户实际完成交互式 menuconfig 练习，不把它写成已做动作。

新的板卡采集脚本做语法检查和云端输出自检；云端缺少的板卡路径会明确报告 unavailable，不当作板卡事实。

## 启动与时间限制

当前已确认 MASKROM/LOADER 和 eMMC 定点读回，电脑保存最初 gzip 备份及最新 raw boot 快照。Linux 配置回退和 Windows boot 定点写回预案已明确；实际恢复尚未执行。部署保留原 Image/initrd/modules，通过独立动作才切换 uEnv。

实板曾有时钟回退，NTP 服务不可用；应核对真实时间，不能重复执行之前对话里的固定旧日期命令。内核构建需正确时间，避免 Clock skew 和错误增量结果。

## 最终验收与简历边界

本次构建版本运行、运行树、集成模块绑定/采集和外设回归已验收；排障材料按两阶段的真实结果保存。可表述为“基于厂家 BSP 的内核构建、设备树配置与自编驱动集成”，不能写成 U-Boot、Buildroot 或整板从零移植。单次完整匹配工具 PASS、首次额外按下定因、实际新旧内核往返/分区写回不在已完成声明中。
