# 实验内核安装、启动切换与回退

## 当前证据与范围

2026-10-26 用户返回 Windows 检查结果，原始 gzip 和 raw 哈希均与板卡初次备份一致。LOADER 导出的文件精确为 134,217,728 bytes，ext2 magic、UUID 与几何匹配原备份；起始输入 `32768` 和长度 `262144` 已通过实际文件对应关系检查。

新读回快照 SHA256：`c19d3031144055158deb63c7d41fe5731ab2bd57e5250584434e7982aeacb08b`。它与较早原始快照哈希不同；身份/几何匹配不证明每个文件内容相同。该文件是在安装新内核之前、原系统正常启动后的 Recovery/LOADER 会话里读出，后续作为该时点 boot 的原始回退快照保留。最初的 `original-boot.img.gz` 也继续保留。

只读通道已实测，实际写回仍未测试。用户已完成下述 stage，真实启动配置尚未切换，新内核尚未启动。

## 独立文件布局

`scripts/deploy-bsp-kernel.py` 提供分离的动作，不自动重启：

| 动作 | 实际影响 |
| --- | --- |
| check-package | 只读取并核对固定包哈希、全部文件清单、Image、DTB、257 个 AArch64 模块及 vermagic |
| stage | 安装两个独立命名启动文件和独立 modules 目录；保存回退状态，不改当前 uEnv |
| activate | 验证已安装文件和原文件，再只修改当前 uEnv 的 uname_r 与 initrd 两行 |
| rollback | 校验旧启动文件并恢复 uEnv 原字节；下一次正常启动回到旧内核 |
| status | 输出当前运行内核、管理状态及 uEnv 选择，不能替代实际启动验收 |

新增位置：

```text
/boot/Image-6.1.99-rk3576-car-bsp
/boot/initrd-6.1.99-rk3576-car-bsp
/lib/modules/6.1.99-rk3576-car-bsp/
/var/lib/car-bsp/6.1.99-rk3576-car-bsp/state.json
```

原 Image、initrd、modules 不覆盖。原基础 DTB 与包内新 DTB 哈希相同，继续使用原来的 DTB 和选择链接，两个 overlay 也保留。boot.cmd、boot.scr 和 extlinux.conf 均不修改。

新的 uEnv 只改为：

```text
uname_r=6.1.99-rk3576-car-bsp
initrd=initrd-6.1.99-rk3576-car-bsp
```

这样 boot.cmd 中先读版本 initrd、再读通用字段的两条路径都会指向同一个新文件。uEnv 选择符号链接保持原样，写入实际目标文件。

## 第一阶段：回原系统并安装独立产物

退出 Windows 工具，断开 OTG 和独立供电，松开按键，按正常方式重新供电。恢复 VS Code/SSH 后，将云端提供的 `car-bsp-artifacts-with-initrd.tar.gz`（29,094,301 bytes）从 Windows 拖入 VS Code 远程资源管理器中的 `/home/cat/car-driver-dev/build/bsp/`。不自行覆盖解压到系统目录。

### Windows 桌面下载入口

聊天中的 `/workspace/...` 链接会在部分客户端进入不支持压缩包的云端预览，用户实测无法从那里下载。因此已发布独立产物标签 `artifacts-rk3576-bsp-a7e612fe`，指向无开发分支父提交的产物 commit `72289d5ea911f42e5945f72098dea6d19b308719`。标签只存储四段文件及清单；不将大文件加入开发分支，也不需要 GitHub Release API（本环境该接口返回 Forbidden）。最初用于发布的临时 artifacts 分支已转换为该标签，避免普通开发分支 fetch 默认拉取所有分支时附带下载整个产物。

`scripts/download-bsp-artifacts.ps1` 自动查找 Windows 的 DesktopDirectory，支持桌面重定向；从固定产物 commit 下载四段，分别验证大小和 SHA256，按序合并，再验证完整包的已知哈希。GitHub raw 与 jsDelivr 各有重试，正确分段缓存在用户 TEMP，失败后可复用。它只写用户桌面和 TEMP，不安装内核或操作板卡。桌面同名文件若内容不同，会先保留为 `.previous-...`，再保存已验证的新包。

用户在 Windows 原样粘贴助手提供的一行入口即可：固定提交下载脚本→核对脚本 SHA256→执行。无需手填用户名、下载目录或文件选择器标题。输出 `READY: <真实桌面路径>` 才表示本机已完成下载，助手不能从云端直接宣称已写入用户桌面。

实际检查：已在云端 PowerShell 7.4.19 完整联网下载四段、复用校验通过的缓存并合并。输出文件为 29,094,301 bytes，SHA256 为 `a7e612fe27e8151abd5d85c37accc67f24045d02891b9ab09658a503ddeee6e5`，与原产物一致。初测时修正了 PowerShell 在 Linux 上读取 dot 临时文件需 Get-Item -Force 的兼容问题。实际 Windows 桌面路径和网络由用户运行结果确认。

在板卡运行：

```sh
cd ~/car-driver-dev &&
git -c http.version=HTTP/1.1 pull --ff-only &&
sudo python3 scripts/deploy-bsp-kernel.py stage
```

stage 将审核固定包 SHA256、原 boot.cmd/boot.scr/initrd/DTB/uEnv、板型与分区布局；为新增文件计算空间，要求安装后至少预留 8MiB，不删除旧文件。已知正常旧布局才安装：新 modules 原子重命名，两个新启动文件分别写临时文件并 fsync/重命名。重复 stage 仅接受内容相同的已有文件，不重写完整新 Image 来额外占用 boot 空间。

输出 STAGED 后，当前 uEnv 仍为原配置，正在运行的内核仍是原内核。这一步不是已经上板启动新内核。脚本会检查并记录旧文件哈希，以便后续切换前确认未被修改。

### 实板 stage 已通过

用户在更新到 `46bb10f` 后返回：

```text
STAGED: Image, initrd and 257 modules for 6.1.99-rk3576-car-bsp; active uEnv unchanged.
Prepared uEnv changes: uname_r=6.1.99-rk3576-car-bsp; initrd=initrd-6.1.99-rk3576-car-bsp
State/rollback data: /var/lib/car-bsp/6.1.99-rk3576-car-bsp/state.json
```

这表示已通过脚本的真实板型/旧内核/分区检查、包哈希和全部清单检查、原启动文件检查、空间检查、独立文件安装及读回核对。没有切换 uEnv，也没有加载新内核模块。证据记录见 `bsp/stage-result.json`。

## 第二阶段：单独切换与正常回退

完成安装输出核对并保留 Windows 回退文件后，切换命令为：

```sh
cd ~/car-driver-dev &&
sudo python3 scripts/deploy-bsp-kernel.py activate &&
sudo reboot
```

重连后应看到 `uname -r` 为 `6.1.99-rk3576-car-bsp`，随后检查运行配置、MPU6050/按键绑定、采集与摄像头。不能直接在新内核上沿用原 headers 重编模块：本包没有新 headers，应先使用本包已安装的匹配模块。

先返回以下最小启动证据，再执行外设回归：

```sh
cd ~/car-driver-dev &&
uname -r &&
sudo python3 scripts/deploy-bsp-kernel.py status
```

预期 running_kernel 为 `6.1.99-rk3576-car-bsp`、phase 为 active、uenv 为 experiment。仅 phase/uenv 符合而 running_kernel 仍是旧值，不算成功启动新内核。

静态启动文件不能完全证明 U-Boot 优先选中了 boot.cmd。若重启后仍为原内核，先运行 status 并调查实际 boot target，不删除原 Image，也不盲目改更多启动入口。此次不为强制切换去重写 extlinux 或 U-Boot。

若新内核可正常 SSH，但要回原系统：

```sh
cd ~/car-driver-dev &&
sudo python3 scripts/deploy-bsp-kernel.py rollback &&
sudo reboot
```

rollback 保留独立新文件，只恢复 uEnv；运行内核的变化发生在重启之后。脚本拒绝覆盖用户另外修改过的 uEnv。配置替换或管理状态写入失败时会尝试恢复操作前配置；断电或存储故障不能靠 Python 异常回滚保证，因此仍保留外部 boot 快照。

## 无法 SSH 时的 Windows 回退预案

这是一份尚未实际执行的分区写回预案，不将其记为恢复已验收：

1. 完整断电后按住 RECOVERY，再接 OTG 和独立供电，确认 RKDevTool 显示 LOADER；当前练习未更改 U-Boot 或 GPT。
2. “获取当前存储”核对 EMMC。在 Windows 校验保留的 `boot-readback.img` 为 134,217,728 bytes 且 SHA256 为 `c19d3031144055158deb63c7d41fe5731ab2bd57e5250584434e7982aeacb08b`。
3. 打开“下载镜像”页。只勾选 Boot 行，路径选上述未压缩 raw 镜像；所有其他行（尤其 Loader、Uboot、Parameter）取消勾选。
4. Boot 地址填 `0x00008000`，勾选“强制按地址写”。这里使用该页显示的十六进制地址格式，不把高级功能的十进制文本直接填入。
5. 执行前完整核对这一页的设备模式、目标、勾选、地址与路径，确保没有额外条目。执行写入的范围应为 LBA 32768～294911；前面的 uboot 和后面的 rootfs 均不在该范围。
6. 写入成功后可再次通过高级功能从起始 `32768`、长度 `262144` 导出，要求此时 SHA256 与写入镜像完全一致，再正常断电重启。实际运行还要核对原内核和外设。

若改用最初的 original-boot.img.gz，必须先解压得到 128MiB 原始镜像，并核对 raw SHA256 `0b4b656ab67b9365fe7b5e3de90940f45f8e95c293ad26823815a09a4c133444`；不能把 gzip 文件直接当分区 raw 写入，也不能写到磁盘 LBA 0。

## 云端检查与验收边界

已用真实产物在临时模拟文件树执行 stage→activate→rollback，确认原文件和选择链接保留、重复运行不重写 Image，损坏模块/空间不足/非目标环境会拒绝，模拟替换后与状态写入失败会恢复配置。`tests/test_bsp_deployment.py` 共 7 项通过。测试不触碰真实板卡或云端系统启动目录，不能作为真实板卡启动/恢复成功的证据。

真实 stage 已由用户输出验收；切换、启动、外设回归与必要的恢复结果仍待后续实板输出。
