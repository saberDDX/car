# 第一阶段：板卡内核模块编译、加载和卸载

## 已知环境

- 板卡：野火 LubanCat 3，RK3576，aarch64。
- 系统：Ubuntu 22.04.5 LTS。
- 运行内核：`6.1.99-rk3576`。
- Qt：5.15.3。
- headers 链接：`/lib/modules/6.1.99-rk3576/build`。

这些信息来自板卡终端输出。链接存在不代表目标目录完整，也不代表模块能够加载。以下操作在板卡的 SSH 终端运行，不在云端或电脑本机运行。

## 取代码

保留原来的 `/home/cat/car`。本阶段使用新的普通克隆目录，不创建 Git worktree。若下面的目录已经存在，不覆盖它，换一个目录名。

```sh
git clone --branch dev/driver-baseline https://github.com/saberDDX/car.git ~/car-driver-dev
cd ~/car-driver-dev
sh scripts/check-board-env.sh
```

检查失败时先保留并反馈完整输出，不要安装 Ubuntu 通用内核的 headers 来替代厂商内核 headers，也不要禁用模块签名或强制加载不匹配的模块。

## 编译

```sh
cd ~/car-driver-dev/drivers/hello
make -j2
modinfo ./car_hello.ko
```

编译必须正常结束，且生成本次构建的 `car_hello.ko`。`modinfo` 的 vermagic 应包含运行内核的版本。vermagic 相符只是必要条件，仍需检查加载结果。

## 加载和卸载

```sh
sudo insmod ./car_hello.ko
lsmod | grep '^car_hello '
sudo dmesg | tail -n 20
sudo rmmod car_hello
sudo dmesg | tail -n 20
```

逐条执行。`insmod` 失败时停止后续加载步骤，把命令报错及最新内核日志反馈回来。不要卸载已经存在、但不是本次测试加载的模块。

预期出现：

```text
car_hello: loaded (driver baseline)
car_hello: unloaded
```

验收：编译成功，加载成功，模块列表可见，卸载成功，并且内核日志没有此次操作引起的 Oops。完整完成一次后可重复两次，验证重载；本模块不访问 GPIO、I2C 或摄像头。

加载外部模块可能记录 out-of-tree taint；非强制签名的内核也可能提示模块缺少签名。这些记录需要与拒绝加载、Oops 或未知符号等错误区分。若内核强制要求签名，采用厂商支持的签名流程，先反馈错误，不绕过验证。

## 这个模块教什么

- `.ko` 是内核模块，运行在内核空间，而不是普通用户程序。
- `module_init` 注册加载入口；返回非零表示初始化失败。
- `module_exit` 注册卸载入口；真正的设备驱动在这里或框架回调中清理资源。
- `pr_info` 写入内核日志，用 `dmesg` 查看。
- `MODULE_LICENSE` 声明许可证；这里的 GPL 标记对应文件的 GPL-2.0 声明。
- Makefile 中的 `obj-m` 交给 Kbuild 生成模块；`M` 指定外部模块源码目录。
- 编译器、内核构建配置、符号版本与运行内核需匹配；不能用版本号相近的 headers 任意替换。

这个模块是环境验证材料，不是项目的 GPIO 驱动。下一阶段才实现 platform 匹配、GPIO 中断、消抖和 input 事件。

## 当前 Qt 工程审查结果

- `car/v4l2thread.cpp` 直接使用 V4L2 和 mmap 采集，OpenCV 负责 YUYV 到 RGB 转换。
- `car/mainwindow.cpp` 引用了未提交的 `ailistenerthread.h`。
- `car/camera.h` 引用了未提交的 `VoiceThread.h`。
- 第一版需要单独移除语音依赖；现有 `.o` 和可执行文件不证明源码可重新构建。
- 摄像头线程的阻塞退出、跨线程状态和错误处理需要修正，再验证反复开关。
- 云端只验证了两个摄像头测试程序能编译；尚未完成板卡采集验证。

Qt 整理保持低优先级，驱动首先通过命令行验证。

## 首次板卡构建失败记录：正在诊断

用户已在板卡执行 `make -j2`，构建在内核 headers 的 `prepare` 阶段失败，尚未进入 `car_hello.c` 编译：

- 缺少 `include/generated/asm-offsets.h`。
- 编译器版本比较命令出现重复双引号，导致 shell 在版本字符串的括号处报语法错误。
- 内核构建时使用 GCC 10.3.1，板卡当前 GCC 为 11.4.0；版本差异本身通常产生警告，不能据此认定必须更换编译器。

对照 Rockchip `develop-6.1` 源码，ARM64 的 `asm-offsets.h` 还用于获得栈保护相关的结构体偏移，不能创建空文件、填猜测值或关闭栈保护来绕过。

Kconfig 生成的 `.config` 和 `include/config/auto.conf` 对字符串的格式不同：前者带引号，后者写给 Makefile 的值通常不带引号。重复引号可能来自 headers 中配置文件的生成或打包，需要查看板卡实际文件确认。不要全局删除配置文件中的引号。

诊断命令（只读）：

```sh
KDIR=/lib/modules/$(uname -r)/build
sed -n '1880,1910p' "$KDIR/Makefile"
awk '/^(CONFIG_CC_VERSION_TEXT|CONFIG_CC_IS_GCC|CONFIG_GCC_VERSION|CONFIG_MODULES|CONFIG_MODVERSIONS)=/ {print}' "$KDIR/include/config/auto.conf"
ls -l "$KDIR/include/generated" "$KDIR/scripts/mod/modpost" "$KDIR/kernel/bounds.c" "$KDIR/arch/arm64/kernel/asm-offsets.c"
dpkg-query -W -f='${Package} ${Version}\n' 'linux-headers*'
```

板卡反馈进一步确认：`auto.conf` 的编译器字符串确实带引号；生成头文件目录只有 `autoconf.h`、`utsrelease.h` 和 `uapi`；`scripts/mod/modpost`、`kernel/bounds.c`、`arch/arm64/kernel/asm-offsets.c` 都不存在。安装包版本为 `linux-headers-6.1.99-rk3576 6.1.99-rk3576-6`。

结论：当前安装的 headers 树不能直接支持本次外部模块构建；只修复字符串引号不足以恢复环境。需要区分软件包本身没有打包必要文件，还是安装后的文件损坏或丢失。

```sh
dpkg -V linux-headers-$(uname -r)
dpkg -L linux-headers-$(uname -r) | awk '/(asm-offsets.h|bounds.h|modpost|Module.symvers)$/ {print}'
apt-cache policy linux-headers-$(uname -r)
```

`dpkg -V` 没有输出一般表示它能校验的文件未发现变化，但不证明软件包自身包含构建所需的全部文件。结合文件清单和可用包版本，选择恢复匹配的软件包或获取厂商匹配的内核构建产物。不能用任意一个标为 6.1.99 的源码树替代；补齐构建资料时仍需核对配置和厂商版本。

检查脚本已增加生成头文件、modpost 和编译器字符串格式检查。模块环境尚未通过验收。

源码依据：

- [Rockchip 内核 Kbuild](https://github.com/rockchip-linux/kernel/blob/develop-6.1/Makefile)。
- [ARM64 构建规则](https://github.com/rockchip-linux/kernel/blob/develop-6.1/arch/arm64/Makefile)。
- [Kconfig 配置文件生成逻辑](https://github.com/rockchip-linux/kernel/blob/develop-6.1/scripts/kconfig/confdata.c)。
- [LubanCat 6.1 内核源码](https://github.com/LubanCat/kernel/tree/lbc-develop-6.1)。
- [LubanCat headers 打包脚本](https://github.com/LubanCat/kernel/blob/lbc-develop-6.1/scripts/package/builddeb)：包括生成头文件、构建工具和 Module.symvers；当前分支源码不是运行镜像精确版本的证明。
