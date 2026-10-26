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

PDF 展示的是完整 update 固件升级，没有提供我们 128MiB boot 分区镜像的定点写回流程。不能把 original-boot.img 当完整固件、从扇区 0 写入。已记录分区起始扇区 32768（512 字节单位），但具体工具地址单位、目标存储及 loader 流程还需进一步核对，本文不提供写入命令。
