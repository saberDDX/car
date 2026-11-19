# RK3576 视觉检测扩展：M0 平台基线

## 已确认的范围

2026-11-19 开始，基于 `dev/driver-baseline` 的
`c5f6da1c1b0721087499ae03ba0a89d29de891b8` 创建 `dev/npu-baseline`。
保留已有 BSP/GPIO/MPU6050/V4L2 功能，当前只进行平台盘点。

后续按顺序完成：平台官方样例 → 单模型导出/正确性 → 非量化与 INT8
小样本对比 → C++ 静态图片推理 → 摄像头 → 分段计时与一项有依据的改进。
多线程选做；不新增语音、推流、录像、Qt 页面或零拷贝。
初始采用用户先尝试、助手审查的方式；随后用户明确调整协作方式：
助手负责实现、构建、Git 同步，用户执行板端命令并回传结果，阶段完成后讲解关键代码。

## 当前事实与待验证内容

- 云端终端正常：Debian 13 / x86_64 / Python 3.12；不是板卡运行环境。
- 初始云检出为 `work`，与旧 main 相同，未发现用户未提交修改。
- 当前没有发现板卡 SSH 配置或连接变量，尚未连接板卡。
- 最初只有用户此前报告的配置、库与节点线索；以下盘点已由
  2026-11-19 用户回传的板端命令输出补充，不是云端实测。
- 当前内核：`6.1.99-rk3576-car-bsp`，AArch64；Ubuntu 22.04.5 LTS。
- `renderD128` 绑定 `rockchip-drm`，`renderD129` 绑定 `RKNPU`。
  节点为 root:render、0660，另有 ACL；当前用户实际可访问性仍待确认。
- `/lib/librknnrt.so` 是 AArch64 ELF，大小 7155496 字节；strings 提取到
  `librknnrt version: 2.1.0 (967d001cc8@2024-08-07T19:28:19)`。
  后续用户回传的真实模型 SDK 查询也确认了同一版本字符串。
- `/sys/module/rknpu/version` 返回 `0.9.8`；debugfs 版本路径权限不足。
  后者本身不是 NPU 故障证据，暂不修改权限或挂载 debugfs。
- 板端 gcc、g++、cmake、pkg-config、python3 存在；OpenCV pkg-config 版本 4.5.4。
- 用户已回传 INT8 模型真实初始化、SDK/driver 查询和推理成功，输出 3 person + 1 bus。
  [详细记录](../vision/reports/board-int8-smoke.json)仅覆盖一张图片和 3 次测量。
  没有安装或替换板端运行库/驱动；结果图视觉核对、FP 模型对照和摄像头验收待完成。

## 第一项操作与知识点

在板端执行 `sh scripts/check-npu-env.sh`，保存终端输出。
脚本仅查询：系统、当前内核配置、DRM 节点绑定、运行库元数据和构建工具。
如果尚未传到板端，可先按会话中的短命令盘点，无需为此同步整个仓库。

需要解释：

1. `uname -r` 是当前运行内核；旧构建日志或 boot 文件不能替代它。
2. `CONFIG_ROCKCHIP_RKNPU=y` 表示内建，不要求 lsmod 中出现 rknpu。
3. renderD128/129 编号不固定用途，要读取 sysfs 中的 driver 链接。
4. ELF/AArch64 只证明二进制架构，不证明 API、驱动或模型兼容。
5. strings 输出仅作版本线索；实际样例初始化后应通过
   `rknn_query(..., RKNN_QUERY_SDK_VERSION, ...)` 核对 API 与 driver 版本。
6. 权限不足、缺少工具与硬件未就绪是不同结果，不直接修改设备权限。

M0 通过条件：真实板端成功加载匹配的 RKNN 模型、查询 SDK/driver 信息，
完成一次推理并核对输出。脚本输出本身不能使 M0 通过。

## 官方参考（先盘点，再选匹配版本）

- [Toolkit2 固定审阅提交](https://github.com/airockchip/rknn-toolkit2/tree/2c723177a94fac65cbc036d722907ddbc91980cf)：
  README 当前为 v2.5.0，列明 RK3576；未据此安装最新版或判定兼容。
- [C API 头文件](https://github.com/airockchip/rknn-toolkit2/blob/2c723177a94fac65cbc036d722907ddbc91980cf/rknpu2/runtime/Linux/librknn_api/include/rknn_api.h)：
  SDK_VERSION 查询命令与 rknn_sdk_version 结构。
- [Model Zoo 固定审阅提交](https://github.com/airockchip/rknn_model_zoo/tree/837db8e992815ff8647eadb709f07067f4bc3a71)：
  明确列出 RK3576 支持；没有用其标称性能作为本项目实测结果。

原始参考文件保存在云端 `/workspace/npu-reference`，不作为板端 SDK。
根据板端库版本，优先检查官方 Toolkit2 v2.1.0，而非直接安装最新版。
已确认官方 tag 对应 `deaba85fc437a28db0b0c29f27d8929f4c5816a1`；
是否与当前驱动及具体模型配套仍须实际运行验证。
后续 YOLO 版本和转换环境尚未定版。

## 当前交付与下一项板端操作

根据用户授权，已实现 Runtime 加载检查、C++ 图片推理、匹配的模型转换流程。
两份 RK3576 模型已转换，云端编译、后处理与 PyTorch/ONNX 对照通过。
详细证据与运行步骤见 [vision/README.md](../vision/README.md)。
用户已执行 `sh scripts/run-npu-image.sh`，M0 的模型加载、实际版本查询与样例推理条件通过。
接下来执行 `sh scripts/run-npu-camera.sh`；单线程 V4L2 接入代码已云端编译/测试，尚未实板运行。
FP/INT8 板端效果/性能比较待进行，尚不将整个视觉模块写为验收完成。
