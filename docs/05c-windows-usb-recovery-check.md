# LubanCat-3 Windows USB 恢复通道识别

## 来源与范围

已逐页核对用户提供的 15 页《烧录.pdf》，原页面：

https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html

PDF 页脚 revision 为 3be85a6，更新时间 2026-10-23。该 PDF 以图像形式打印，读取时使用页面渲染核对，未执行其中的操作或外部程序。

文档第 3 页明确：RK3576 必须使用 RKDevTool v3.30 及以上版本，示例 v3.32；配套驱动工具为 DriverAssistant，截图提供 v5.13。工具从官方资源的“6-开发软件”目录获取，分享页提取码以官方当前页面为准（PDF 显示 hslu）。

官方文档介绍整机固件升级，本次只采用工具安装和设备进入模式的识别部分。不加载 update.img、不执行升级、下载、擦除或分区写入。现有离线产物包也不是 RKDevTool 的 update 固件。

## 接口与模式

以 PDF 第 5、8 页的普通 LubanCat-3 图为准，不使用 3BTB 或金手指板图片：网口、USB-A 接口在左边、40Pin 在下边时，右侧上方 Type-C 为 OTG 烧录口，下方 Type-C 为独立供电口。RECOVERY 与 MASKROM 两个按钮有不同丝印，必须按图确认。

- Recovery：PDF 第 5～7 页，进入后工具显示 LOADER，依赖已有引导程序。
- MASKROM：PDF 第 8～11 页，进入后工具显示 MASKROM，适用于没有可用系统等情况。
- 本次优先验证 MASKROM 识别，以检查不依赖当前 Linux 启动的 USB 通道。它不等于已经完成分区恢复验证。

## Windows 准备

1. 在官方资源“6-开发软件”中下载 DriverAssistant 和 RKDevTool（选 v3.32 或符合官方要求的新版本）。
2. 解压 DriverAssistant，运行 DriverInstall.exe，点击驱动安装。若已存在旧版或安装报冲突，再按官方说明处理旧版驱动，不默认卸载无关驱动。
3. 解压 RKDevTool，运行 RKDevTool.exe；无需额外安装该应用。
4. 将 boot 备份的 original-boot.img.gz 和 manifest.txt 下载到 Windows，计算副本 SHA256 并与板卡报告比较。用户已确认压缩镜像比较结果为 True，见下方实测记录。

## 识别步骤（依据 PDF 第 10 页）

1. 若板卡 Linux 仍运行，先在板卡终端执行 `sudo poweroff`，等待关机。
2. 断开所有可能给板卡供电的连接，使板卡断电。
3. 用 USB-A 转 Type-C 数据线连接电脑 USB 与板卡 OTG 烧录口，并打开 RKDevTool。
4. 按住 MASKROM 按钮，再接入原来的独立供电线。
5. 等工具底部显示“发现一个 MASKROM 设备”后松开按钮。只记录该状态，不继续 PDF 后面的固件升级步骤。

没有识别时先返回完整断电状态，再核对接口、数据线和驱动；若数据线接入本身已让板卡启动，重试时应在任何可能上电的连接接入前按住 MASKROM，保证启动采样时已按下。无法确认状态时停止在识别阶段，不通过擦除或写镜像尝试解决。

验证结束后关闭工具，断开 OTG 数据线与供电，确认按键均已松开，再只接原来的独立供电启动。确认旧系统能正常启动并恢复 SSH。MASKROM 识别本身没有写入 eMMC。

## 当前验收状态与后续缺口

- 已确认：官方接口位置、工具版本要求、按键进入方式，以及下述实板结果。
- 未完成：boot 分区定点恢复的工具操作、必要的 loader 文件及回退演练。

### 2026-10-26 用户验证

- Windows 使用 RKDevTool v3.37，用户截图底部明确显示“发现一个 MASKROM 设备”，设备列表显示 MASKROM。此版本满足官方要求，已证实当前数据线、OTG 接口与 Windows 驱动组合能识别设备。
- 用户运行 PowerShell 比较 Windows 压缩备份 SHA256，返回“与板卡报告一致: True”。期望值为 `6fa3cc6ca8ce04684c74d66897a9345e9e5333e101553eea84ec157fa1d9679a`。
- 用户退出 MASKROM、正常重新上电后，确认能再次通过 VS Code 连接板卡。
- 未执行分区写入或恢复。这些结果不能替代实际恢复验收。

### 下一步只读检查

在板卡运行 `python3 scripts/inspect-bsp-deployment.py`，读取当前 boot 空间、镜像/initrd 大小和链接、initrd 内模块版本、initramfs 工具与相关配置，以及有限目录中已有 loader 文件名。仅写项目内 `build/bsp/deployment-inputs.txt` 报告，不修改系统文件。

该报告用于判断新内核的独立 Image、模块与 initrd 布局；脚本不安装产物、不切换启动、不证明恢复流程已具备。若找到了 loader，也必须另行核对其来源与板卡兼容性。

用户已返回该脚本的实板报告，空间和 initrd 格式等结果见 [部署布局调查](05d-bsp-deployment-layout.md)。后续优先调查不依赖更换 U-Boot 的 Recovery/LOADER 回退通道；MASKROM 定点恢复并非唯一候选，但不能把尚未测试的 LOADER 通道当作已通过。

### 已核对的 RKDevTool v3.37 高级功能界面

用户补充截图已确认：存在“获取当前存储”“读取Flash信息”“读取FlashID”“读取Chip信息”“读取Capability”和“导出镜像”；导出区域有“起始扇区”“扇区数”两个输入框。存储类型列表列出 `2. EMMC`，但列表本身不证明当前目标就是 eMMC。截图底部为“没有发现设备”，尚未验证 LOADER 下的 eMMC 读取。

截图也有“下载”“切换存储”“擦除分区”“擦除所有”等会改变状态的操作，当前步骤不使用。尚未确认扇区输入框的数值解析规则，不直接给出导出或写回参数。

### 下一步：Recovery/LOADER 与存储信息读取

依据同一官方 PDF 第 6 页的文字流程，使用普通 LubanCat-3 上丝印 RECOVERY 的按键：

1. 原系统执行 `sudo poweroff`，等待关机，断开所有可能供电的连接。
2. Windows 打开 RKDevTool。为确保按键在任何上电前已生效，先按住 RECOVERY，再接电脑至板卡 OTG 的数据线和原独立电源。
3. 等底部显示“发现一个 LOADER 设备”再松开 RECOVERY；本次不按 MASKROM。
4. 在“高级功能”依次点“获取当前存储”和“读取Flash信息”。只读取现有设备，不点“下载”或切换存储。
5. 返回包含底部设备状态和右侧日志的截图。若不能进入 LOADER，记录实际状态，不通过擦除或下载来尝试解决。

这一步只验证模式识别与存储信息。后续还要确认扇区读取、导出镜像的实际位置与字节数，以及 boot 分区对应关系；不能把读出信息当作已完成恢复。

### Recovery/LOADER 实测已通过

用户按上述步骤操作，截图确认底部“发现一个 LOADER 设备”，接口状态为 LOADER，右侧日志为“获取FlashInfo开始”及“获取FlashInfo成功”。返回字段：

```text
Flash Size:    60064MB
Total Sector:  0x7550000
LBA Size:      1
Block Size:    512KB
Page Size:     2KB
ECC Bits:      0
Access Time:   40
Flash CS:      0
```

`0x7550000 = 123,011,072`，按已知 512-byte 逻辑扇区换算为 62,981,668,864 bytes，即 60,064 MiB / 58.65625 GiB，与板卡约 58.7GiB 的 eMMC 容量相符。日志的 Block Size/Page Size 不是本次 boot LBA 寻址单位；LBA Size=1 是工具返回字段，不能解读为单扇区 1 字节。

已证实当前 Recovery/LOADER 能响应 Flash 信息读取，不需要为这一步另下载 MASKROM loader。但用户本轮截图未包含“获取当前存储”的返回值，eMMC 目标与分区几何仍需结合当前存储返回和只读分区表核对。

### 下一步：小范围分区表导出

工具保持 LOADER 模式，先点“获取当前存储”，保留返回结果。在高级功能底部填写以下**原样文本**，只点“导出镜像”：

| 控件 | 输入文本 |
| --- | --- |
| 起始扇区 | `0` |
| 扇区数 | `40` |

这里故意先读磁盘起始的一小段，而非直接导出整个 boot 分区。起始 0 不受十进制/十六进制差异影响，长度文本 40 在两种常见解析规则下分别为 40/64 扇区，约 20/32 KiB，都应覆盖常见 128-entry GPT 的主表。以实际导出长度和日志核对工具的输入规则，再校验 GPT header/entry CRC、boot 分区起点和长度，以及总扇区数。不根据猜测下发写回地址。

若出现保存窗口，使用新文件名 `gpt-head.img`；若工具自动保存，按右侧日志或软件目录下的 Output 查找本次新文件，不覆盖已有备份。用户提供该小文件及右侧日志后，助手进行数据分析；无需上传全盘镜像。

这次操作只读 eMMC，写入的是 Windows 导出文件。没有执行 boot 分区写入、恢复或新内核启动。

### 小范围 GPT 实测结果

用户上传 `ExportImage.img`，20,480 bytes，SHA256 为 `6022cb43f828e7f337b6a84c090e075a66c245afe460709daeab0b9e4244c9e9`。结合输入的扇区数文本 `40`，该导出长度符合十进制 40 × 512 bytes；尚不能只由起始值 0 判断“起始扇区”输入框的进制。

- protective MBR 标记 `55aa` 与 0xee 分区类型存在。
- 主 GPT header 位于 LBA 1，大小 92 bytes，header CRC 校验通过。
- entry array 位于 LBA 2，128 个 128-byte entries，整个 entry array CRC 通过。
- backup GPT LBA 为 123,011,071，总扇区数 123,011,072，与 LOADER FlashInfo 相同；本次只检查主表，没有读到或验证磁盘末尾的备份 GPT。
- 用户同时补充的存储列表截图选中 `2. EMMC`。结合与 Linux 报告相同的容量和以下分区几何，目标存储对应关系已进一步确认。

| GPT 分区 | 名称 | 起始 LBA | 扇区数（512-byte） | 字节数 |
| --- | --- | --- | --- | --- |
| 1 | uboot | 16384 | 16384 | 8,388,608 |
| 2 | boot | 32768 | 262144 | 134,217,728 |
| 3 | rootfs | 294912 | 122716127 | 62,830,657,024 |

结果记录为 `bsp/gpt-readback-result.json`，不公开原始磁盘 GUID 或分区 UUID。

### 下一步：完整 boot 只读导出与 Windows 检查

保持 LOADER/EMMC，在高级功能中填写起始扇区 `32768`、扇区数 `262144`，只点“导出镜像”。完整导出应为 134,217,728 bytes，另命名 `boot-readback.img`。此举不写 eMMC，也不覆盖原始 boot 备份。起始输入是否按预期处理，将进一步用实际导出文件的 ext2 特征验证。

在 Windows 运行 `scripts/check-bsp-boot-readback.ps1`，通过两个文件选择窗口分别选新导出文件和最初备份。最初备份可用 original-boot.img.gz 或其未压缩的 original-boot.img；不再默认选择 Downloads 中的同名文件。脚本只读取这些 Windows 文件，并写一份 JSON 报告：

1. 核对导出精确为 128MiB，防止误选之前 20KiB 的 GPT 文件。
2. 输出实际所选备份路径、文件长度及文件 SHA256，根据 gzip magic 判断是否需要流式解压；无论是否压缩，都要求原始内容精确为 128MiB，且 SHA256 为已记录的 `0b4b656ab67b9365fe7b5e3de90940f45f8e95c293ad26823815a09a4c133444`。流式计算不落地整盘解压文件，最多读取 128MiB+1 字节以拒绝超长输入，并保留前 4096 bytes 用于几何检查。
3. 比较两个文件的 ext2 magic、文件系统 UUID 及固定几何字段，确认导出对应原 boot 文件系统。
4. 输出新完整导出的 SHA256 及其是否等于原始 raw 备份 SHA256。

重启和重新挂载会改变 ext2 的挂载时间、计数等元数据，因此新导出不必与较早快照哈希相同。若原始哈希不等但上述身份/几何相同，只能证明同一文件系统与读回定位，不能声称整个文件系统内容没变化。脚本会明确输出此边界；失败时不继续切换启动或写回。

脚本不调用 RKDevTool、不打开物理磁盘、不修改设备或执行上传文件中的程序。当前云端没有 Windows PowerShell，脚本完成代码核对，Windows 实际运行结果待用户返回。

### 首次 Windows 运行的阻塞与修正

用户首次运行旧版时在 `Original compressed backup SHA256 mismatch` 处停止。由于脚本已先检查新导出长度，这次至少通过了新导出文件的 134,217,728-byte 大小检查；原备份完整性和文件系统身份检查尚未通过。旧版没有打印自动选择的原备份路径或实际哈希，现有输出不足以判断选错副本、备份是否被改动或是否重新压缩，不能据此认定板卡故障。

已将脚本改为始终显式选择原备份（也保留命令行路径参数和旧参数别名），打印诊断信息，并接受未压缩原镜像。完整性判定改为强制匹配最初原始镜像的 SHA256，而不是仅匹配 gzip 外壳的 SHA256：重新压缩可能改变压缩文件哈希，但只要解压后内容仍与已记录原始快照完全相同就可接受；未放宽对原始备份数据的校验。

为避免浏览器保存同名脚本为 `(1)` 后又运行旧版，提供单独下载副本 `build/bsp/check-bsp-boot-readback-v2.ps1`，内容与仓库主脚本相同。用户无需重新导出板卡，仅用新版对现有两个 Windows 文件复核并返回完整输出。若原始 SHA256 也不符，继续保留阻塞并找回最初备份，不把新导出直接当成已验证原备份。

### 弹窗启动命令错误与自动启动入口

用户随后返回的新错误位于外层文件选择器命令：`Title` 被填入带两层双引号的路径，导致 ParserError；最后的 if 块又缺少结束右花括号，出现 `>>` 等待补完输入。这一轮尚未运行核心校验脚本，也没有新的备份哈希或文件系统检查结果。应先 Ctrl+C 退出未完成输入。

为避免继续手动编辑路径，提供一行自动下载并启动的命令，记录在云端 `build/bsp/windows-boot-check-command.txt`。入口将固定提交 `4826d94ccea36d6f31727e7a0984b2091b1b86d3` 的校验脚本下载到当前 Windows TEMP，先尝试 GitHub raw，失败时尝试 jsDelivr 对应同一公开提交的副本；两者均必须通过 SHA256 `e905c73ca2172911333cd091fc882ca7eaf33b017a3870a6a07e47e14130f73d` 才启动。失败时不运行未知内容，不要求复制到特定 Downloads 文件名或修改 PowerShell Title。随后仍用脚本内部两个文件选择器选真实导出和原始备份。

本轮在工作区内提取微软官方源的便携 PowerShell 7.4.19（未安装到系统目录），用 `tests/test_bsp_windows_readback.py` 对真实 PowerShell 引擎执行 8 项合成镜像检查：原 gzip、原 raw、重新压缩、错误备份及诊断、仅挂载时间改变、导出 magic/UUID 错误、GPT 大小误选、超长 gzip 拒绝，全部通过。测试只在临时脚本副本中替换为合成镜像的已知哈希，发布脚本仍严格保留真实板卡快照哈希。

同一自动入口的下载/哈希阶段也已在云端 PowerShell 实际通过；未在云端启动 Windows 弹窗，不能据此声称 Windows PowerShell 5.1 或用户网络已实测通过。实际 Windows 完整检查仍待用户返回。测试命令：

```sh
BSP_PWSH=/workspace/.powershell-check/root/opt/microsoft/powershell/7/pwsh \
  python3 -m unittest discover -s tests -p 'test_bsp_windows_readback.py' -v
```

### Windows 实际完整读回检查通过

用户随后返回自动入口的实际输出，明确选择的原备份压缩文件为 25,850,532 bytes。压缩 SHA256 为已知 `6fa3cc6c…`，解压长度 134,217,728 bytes，raw SHA256 为已知 `0b4b656a…`，两者均正确；此前默认路径选择导致的核对阻塞已解决，未取得之前误选副本的内容。

新 LOADER 导出精确 128MiB，ext2 magic、UUID 和几何匹配原快照；读回 SHA256 为 `c19d3031144055158deb63c7d41fe5731ab2bd57e5250584434e7982aeacb08b`，与旧 raw 不同。报告 `raw_hash_matches_original_backup=false` 是两快照不相等的记录，不是本次身份/几何检查失败，也不能反过来证明仅元数据变化。结果保存在 `bsp/boot-readback-result.json`，没有公开 Windows 用户目录或文件系统 UUID。

独立安装与回退布局见 [安装与回退](05e-bsp-stage-and-rollback.md)。本页 USB 调查之后，实验内核已完成真实启动及功能回归，见 [最终归档](05h-bsp-acceptance-and-study.md)；实际分区恢复写回仍未完成。

PDF 展示的是完整 update 固件升级，没有提供我们 128MiB boot 分区镜像的定点写回流程。不能把 original-boot.img 当完整固件、从扇区 0 写入。GPT 已验证 boot 几何，后续仍需完成完整读回核对并明确“下载镜像”页面的地址写入操作；高级功能导出框和下载页面的地址框不能仅凭相似外观就假定进制相同。本文目前不提供写入命令。
