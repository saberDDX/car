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
4. 确认 boot 备份的 original-boot.img.gz 和 manifest.txt 已下载到 Windows，副本 SHA256 应与板卡报告一致。此项尚未收到用户确认。

## 识别步骤（依据 PDF 第 10 页）

1. 若板卡 Linux 仍运行，先在板卡终端执行 `sudo poweroff`，等待关机。
2. 断开所有可能给板卡供电的连接，使板卡断电。
3. 用 USB-A 转 Type-C 数据线连接电脑 USB 与板卡 OTG 烧录口，并打开 RKDevTool。
4. 按住 MASKROM 按钮，再接入原来的独立供电线。
5. 等工具底部显示“发现一个 MASKROM 设备”后松开按钮。只记录该状态，不继续 PDF 后面的固件升级步骤。

没有识别时先返回完整断电状态，再核对接口、数据线和驱动；若数据线接入本身已让板卡启动，重试时应在任何可能上电的连接接入前按住 MASKROM，保证启动采样时已按下。无法确认状态时停止在识别阶段，不通过擦除或写镜像尝试解决。

验证结束后关闭工具，断开 OTG 数据线与供电，确认按键均已松开，再只接原来的独立供电启动。确认旧系统能正常启动并恢复 SSH。MASKROM 识别本身没有写入 eMMC。

## 当前验收状态与后续缺口

- 已确认：官方接口位置、工具版本要求、按键进入方式。
- 待用户验证：Windows 备份副本校验、驱动安装、MASKROM 识别、退出后旧系统正常启动。
- 未完成：boot 分区定点恢复的工具操作、必要的 loader 文件及回退演练。

PDF 展示的是完整 update 固件升级，没有提供我们 128MiB boot 分区镜像的定点写回流程。不能把 original-boot.img 当完整固件、从扇区 0 写入。已记录分区起始扇区 32768（512 字节单位），但具体工具地址单位、目标存储及 loader 流程还需进一步核对，本文不提供写入命令。
