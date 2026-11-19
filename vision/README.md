# RK3576 行人／车辆检测：图片与单线程摄像头

当前交付：C++ Runtime 加载检查、YOLOv5n 导出与 RKNN 转换脚本、
两份 RK3576 模型、C++ 图片/摄像头推理、云端正确性测试和基础计时。
**板端 INT8 静态图已通过；摄像头虽完成 100 帧运行，但用户反馈结果图全白，图像质量验收未通过。先定位采集问题，暂缓量化性能对比。**

## 当前优先事项：白色画面诊断

用户反馈 `result-camera.jpg` 为白色，[问题记录](reports/board-camera-white-frame.json)保留该反馈。
此前日志只能证明帧长度和接口调用通过，不能证明获得了有效场景。
目前没有证据直接归因于曝光、YUYV 转换或显示工具，不修改曝光参数来碰运气。

在板端关闭其他摄像头程序，镜头对准有明显轮廓的物体，执行：

```sh
git pull --ff-only
sh scripts/diagnose-npu-camera.sh
```

此程序不加载 RKNN 模型，读取已有结果 JPG 的像素统计，再直接采集 60 帧。
第 1、30、60 帧各保存推理前的 PNG、原始 `.yuyv` 和尺寸/stride/统计文本。
输出位于新建的 `build/vision/camera-diagnostics/capture-*/`，不会覆盖原结果图。
`build/vision/camera-diagnostics/latest/` 指向这一轮目录，便于直接打开 `latest/frame-60.png`。
控制参数只执行 QUERYCTRL/G_CTRL/QUERYMENU，不写曝光、增益或白平衡值。
摄像头格式仍为原先的 YUYV 640×480 请求，按返回的尺寸和 stride 解析。
权限与其他摄像头脚本相同，仅在需要时临时授权并恢复。

重点看：

- `mean_BGR` / `std_BGR` / `near_white_percent`：确认实际图像是否接近均匀白色。
- `raw_channel=Y/U/V`：检查原始字节的亮度和色度是否与转换结果相符。
- `controls_before_capture/after_capture`：检查当前曝光模式、曝光时间、增益等可读取值。
- `frame-1.png` 与 `frame-60.png`：观察启动后是否逐渐恢复正常画面。

白色原始画面可能来自场景或摄像头设置；若原始 YUYV 有正常变化而转换图发白，
则继续检查格式/stride/转换。若文件像素正常而查看器仍白，则排查实际打开文件和显示路径。
这些是后续诊断分支，不是已确认原因。诊断代码的合成白/黑图及 raw 统计测试通过，实板结果待回传。

## 已收到的板端结果

用户在 `fa203eb` 版本回传 Runtime 2.1.0、driver 0.9.8，模型初始化成功，
INT8 检出 3 person + 1 bus；类别与数量对应云端参考。
一次预热、三次测量的 rknn_run 均值 11.473 ms，processing 均值 22.933 ms，
output_get 均值 7.931 ms。详细数据见 [原始范围归档](reports/board-int8-smoke.json)。
这是单图功能 smoke test，不能作为稳定性能、摄像头 FPS 或数据集精度结论。
结果图已经由程序保存，用户视觉核对尚未回传。

首次摄像头尝试在模型初始化后自动发现 `found 0`，未采集任何帧，
见 [失败记录](reports/board-camera-attempt-1.json)。当时诊断未保留每个设备的打开错误，
当时具体原因未确定。用户随后明确说明当时没有连接摄像头。
代码审查另发现原 Qt 启动器提供临时摄像头 ACL，而首版 NPU 启动器遗漏了此流程，现已补齐。
后续成功运行记录为 `camera_access=existing`，不能将这次问题写成权限故障或声称实测了临时授权恢复。

接上摄像头后，用户回传 `/dev/video0`、UVC/YUYV 640×480、stride=1280、4 个 MMAP buffer，
5 帧预热后完成 100 帧测量：约 3.368 秒、29.687 completed detection FPS，sequence gaps/anomalies 均为 0。
rknn_run 均值 11.442 ms、processing 均值 23.085 ms，驱动 MONOTONIC/SOE 时间戳到后处理均值 57.642 ms。
最后一帧检测数为 0；用户后续反馈图片全白，不能作为有效行人/车辆检测性能结果。
见 [摄像头短跑记录](reports/board-camera-smoke.json)。这没有覆盖长时内存、反复重开、断开或预览测试。

## 下一步：同图 FP/INT8 对比

在板卡当前分支执行：

```sh
git pull --ff-only
sh scripts/compare-npu-models.sh
```

脚本使用同一张校验过的示例图、相同阈值/类别/预处理，按 FP→INT8→INT8→FP 顺序，
每轮 10 次预热、100 次测量，减轻固定先后顺序的影响。不设置频率，不启动摄像头或 GUI。
每轮保留 Runtime/driver 查询、各段耗时、检测框和输出图；每轮超时为 120 秒。
只有运行成功且测量次数、必需字段一致时才生成 COMPLETED 报告，失败保留退出码、日志和不完整报告。

终端打印两组的运行均值及模型文件大小。报告位于
`build/vision/comparisons/comparison-*/report.json`，具体路径由脚本打印。
跨轮只汇总均值，P50/P95 保留各轮结果，不把分位数的平均冒充总体 P95。
框按同类别且 IoU≥0.5 做一对一贪心匹配，展示置信度和位置变化、未匹配项；
**FP 也不是人工标注，匹配数量不是准确率，不能由单图给出 mAP 或量化精度损失结论。**
本脚本的 8 项 host fixture 测试通过，覆盖日志解析、未完成测量拒绝、非法耗时、
检测数不一致、类别匹配、ABBA 顺序与原始失败退出码保留；未据此宣称板端对比已经通过。

## 单线程 USB 摄像头运行

先关闭原 Qt 程序里的摄像头窗口，避免与新程序争用摄像头。在板卡仓库内执行：

```sh
git pull --ff-only
sh scripts/run-npu-camera.sh
```

默认只在可访问的 YUYV UVC 摄像头恰好有一个时自动选择，申请 640×480，
使用驱动实际返回的尺寸与 bytesperline。5 帧预热后测量 100 帧，保存最后一帧
`build/vision/result-camera.jpg`。默认没有 GUI、录像或推流，运行完自动释放资源。

脚本以普通用户运行。若打开设备被拒绝，会打印 `open_errno` 和原因，
在需要时通过可见的 `sudo` 密码提示进行只读发现（仅 QUERYCAP/ENUM_FMT）。
确定唯一合适设备后，若当前用户仍无读写权限，先保存该节点完整 ACL 和设备身份，
只给当前用户临时读写权限；NPU 推理进程始终不使用 sudo。
正常退出、推理失败或中断后恢复原 ACL；设备已替换或恢复失败则保留备份并报告路径，
不把旧权限恢复到新设备。不使用 chmod 777、不修改用户组、不卸载已有驱动。
此流程需要 Python3 和已有的 sudo/acl 工具；原 Qt 启动脚本已使用相同 ACL 工具。
请直接执行脚本，不要在整条命令前加 sudo。

若为多设备、格式不支持、占用等问题，脚本保留具体诊断，不自动改变格式筛选或关闭其他程序。
可以指定正确设备，例如：

```sh
sh scripts/run-npu-camera.sh /dev/video0
```

这里 `/dev/video0` 只是例子，以实际枚举为准，不把第一个 video 节点当成摄像头。
在已有桌面终端、OpenCV highgui 可用时，可选择预览 300 帧：

```sh
sh scripts/run-npu-camera.sh auto 300 1
```

`q`/Esc 或 Ctrl+C 可停止；停止后仍保存最后成功处理的画面。
没有 DISPLAY/WAYLAND_DISPLAY 时拒绝打开预览，不改变桌面配置。

当前实现沿用原采集流程的要点：能力检查 → S_FMT → REQBUFS → QUERYBUF/
MMAP → QBUF → STREAMON → poll/DQBUF → YUYV 转为自有 BGR 图像 → QBUF → 推理。
归还缓冲区之后，推理不再引用 MMAP 地址。结束/异常时清理 STREAMOFF、映射和 fd。
没有增加采集线程、零拷贝、热插拔自动重连，也没有修改已有 Qt 摄像头或按键驱动。

新增统计：

- `capture_wait_convert_ms`：poll/DQBUF 等待、YUYV 转换及 QBUF 的合计耗时。
- `completed_detection_fps`：测量窗口内完成检测的帧数/墙钟时间，包含等待、
  输出释放和可选预览；不是传感器实际采集 FPS，也不是 rknn_run 耗时的倒数。
- `driver_timestamp_to_postprocess_ms`：仅在驱动声明 MONOTONIC 时，统计驱动时间戳
  到后处理结束的间隔；记录 SOE/EOF/default 来源，不称为摄像头到屏幕的总延迟。
- `sequence_gaps`：已取出帧的 sequence 间隙；不等于应用精确统计了所有硬件丢帧原因。

真实 100 帧采集/推理与正常结束已由用户回传；预览、Ctrl+C、重开和断开行为仍待板端验收。
启动器有 10 项 host mock 测试，覆盖已有权限、临时授权/恢复、失败回滚、设备替换、
恢复失败保留备份、仅在权限错误时请求提权，以及拒绝整个推理以 root 运行。
这些测试不执行真实 sudo、不改变设备 ACL；本次板端无需临时授权，其真实恢复流程仍待验证。

## 板卡运行

在已经切换到 `dev/npu-baseline` 的 `/home/cat/car-driver-dev` 内执行：

```sh
git pull --ff-only
sh scripts/run-npu-image.sh
```

`pull --ff-only` 同步当前分支，遇到分叉会停止。若本地修改或文件冲突，
保留输出，不使用 reset/强制覆盖。

运行脚本依次：

1. 本机编译 `check_runtime`，检查 `/lib/librknnrt.so` 及七个 API 符号。
2. 下载固定版本的 SDK 头文件到被忽略的 `build/`，验证 SHA-256。
   SDK 头文件保留官方声明，不重新分发进本项目源码。
3. 使用板端 OpenCV/CMake 编译 `detect_image`，执行后处理测试。
4. 验证模型 SHA-256，下载固定示例图并验证摘要。
5. 初始化模型，查询实际 API/driver 版本与张量信息，执行 1 次预热、
   3 次测量，打印检测结果并保存 `build/vision/result-int8.jpg`。

板端需要 g++、CMake、OpenCV 开发包、curl、sha256sum。
已知用户回传环境提供前三项、OpenCV 4.5.4、Runtime 2.1.0、driver 0.9.8。
不替换系统库、不设置定频、不改设备权限、不要求 sudo。
若 SDK 头文件无法下载，可以用已有匹配 SDK：

```sh
RKNN_INCLUDE_DIR=/实际SDK/include sh scripts/run-npu-image.sh
```

单独运行加载检查：

```sh
sh scripts/run-npu-check.sh
```

它打印 `model_inference=NOT_RUN`，即使加载检查通过，也不代表 NPU 推理通过。

切换非量化模型或自选图片：

```sh
sh scripts/run-npu-image.sh build/vision/bus.jpg fp
sh scripts/run-npu-image.sh /实际路径/image.jpg int8
```

更充分的静态图片计时示例（仍不是摄像头性能）：

```sh
build/vision/detect_image --model vision/models/yolov5n-rk3576-int8.rknn \
  --image build/vision/bus.jpg --output build/vision/benchmark.jpg --warmup 10 --repeat 100
```

## 模型与输入输出约定

- 官方 YOLOv5n v6.2 权重，Rockchip `rk_opt@v6.2.1` 的三输出 sigmoid-head 适配。
- 固定 640×640，batch=1，80 类 COCO；支持本项目转换产物，不是通用 YOLO 加载器。
- 图像：保持比例 resize + 黑色 letterbox + BGR→RGB。
- CPU 参考输入：float32 NCHW /255。
- RKNN 运行时输入：uint8 NHWC，`pass_through=0`；/255 已配置进模型转换，
  C++ 不再重复归一化。
- 输出为三组 `[1,255,H,W]`，H/W=80、40、20；已经 sigmoid，不能再次 sigmoid。
- 标准 YOLOv5 COCO anchors；运行时检查张量数量、尺寸与布局，拒绝其他模型契约。
- `want_float=1` 交给 Runtime 反量化输出；解码后按类别 NMS、去 letterbox 映射回原图。
- 默认保留 person/car/motorcycle/bus/truck（类别 0,2,3,5,7）；
  可传 `--classes all` 或明确类别编号。模型本身仍预测全部 80 类。

## 资源管理与计时边界

`Runtime` 管理 dlopen/dlclose；`Context` 管理模型内存与 rknn_init/destroy；
`Outputs` 在成功获取输出后负责 outputs_release，异常路径也执行析构。
类禁止复制，避免重复释放；模型缓冲存活至 context 销毁。

分段记录 preprocess、inputs_set、rknn_run、outputs_get、postprocess，
预热样本不进入统计，输出 mean/P50/P95/max。
`processing` 不包含初始化、图片文件读取、输出释放、绘图、保存和显示，
**不是摄像头端到端延迟，也不是板端实测报告**。
3 次测量仅用于首轮 smoke test，不能据此认定稳定性能。

## 云端已经执行的检查

2026-11-19，Debian 13 x86_64，g++ 14.2、OpenCV 4.10：

- CMake 构建检查器和图片推理程序成功。
- 摄像头改版后的 CTest 2/2 通过：anchor/grid 解码、NCHW 索引、置信度、类别过滤、跨类别 NMS，
  另包括 YUYV 行填充/长度/内存所有权及 50 次非摄像头设备初始化失败的 fd 清理。
  后一测试不模拟完整 MMAP/STREAMON 路径，不能替代真实摄像头验收。
- 加载器的 host fixture 检查：库缺失、符号缺失、正常符号集合、参数错误，全部通过。
  fixture 不实现 NPU，没有用它替代板端推理验收。
- 一张官方示例图上，适配后的 PyTorch 与 ONNX Runtime 的输出一致，
  三个 head 最大绝对误差分别见 [export-reference.json](reports/export-reference.json)，均小于 4e-6。
- 真实 ONNX head 经 C++ 与官方 Python 后处理得到相同 4 个结果，框最大绝对差
  约 3.4e-5 像素；[参考检测](reports/onnx-detections.json)为 3 person + 1 bus。
- 非量化与 INT8 模型转换成功。INT8 使用官方 Model Zoo 的 20 张 COCO 子集图片，
  先按相同策略 letterbox；[来源摘要](reports/calibration-sources.json)。
- 以上为云端证据；板端 INT8 图片结果另行归档，量化精度、稳定性能与摄像头链路待验收。

## 转换环境与复现

此部分在 x86_64 转换机器执行，不在 RK3576 上安装 Toolkit2。

- Toolkit2 v2.1.0：`deaba85fc437a28db0b0c29f27d8929f4c5816a1`
- Model Zoo v2.1.0：`c2b7d00714b4e5d21266ab3003f3ca687ba0d57b`
- Rockchip YOLOv5：`23a20ef3e5fa648e765e4a70b30edbfd3aaf8c8c`
- Python 3.10、CPU Torch 2.1.0、ONNX 1.14.1、ORT 1.16.0；其余版本固定于
  `scripts/npu/requirements-host.txt`。

安装脚本需要预先有 `uv`，所有依赖写到项目的 `build/npu-host`：

```sh
sh scripts/npu/setup-host.sh
```

本次云端实际工作目录为 `/workspace/npu-sdk`；使用
`NPU_HOST_ROOT=/workspace/npu-sdk` 重跑安装脚本和依赖/import 检查通过。
旧 ORT 1.16 的 wheel 请求可执行栈，在本云端 glibc 2.41 被拒绝。
用 Debian patchelf 0.18 的 `--clear-execstack` 将该扩展的 GNU_STACK 从 RWE
收紧为 RW 后恢复；保留了原摘要，没有启用可执行栈或关闭包/TLS 校验。
安装脚本遇到该错误时支持通过 `PATCHELF` 指定具有此选项的工具。
Ubuntu 22.04 板端不使用这份 Python ORT 环境。

下载可信官方权重（脚本校验固定 SHA-256）：

```sh
mkdir -p build/npu-models
curl -fL https://github.com/ultralytics/yolov5/releases/download/v6.2/yolov5n.pt \
  -o build/npu-models/yolov5n.pt
build/npu-host/venv/bin/python scripts/npu/export_yolov5.py \
  --source build/npu-host/yolov5 --weights build/npu-models/yolov5n.pt \
  --output build/npu-models/yolov5n.onnx --image build/vision/bus.jpg
```

首次复现先执行 `sh scripts/fetch-npu-sample.sh` 取得并校验示例图，该步骤不需要 NPU。
导出脚本使用 fork 的 `RKNN_model_hack=npu_2` 产生通用 sigmoid heads；
**具体 RK3576 目标由下面 Toolkit 配置指定**，没有把 RK3588 模型改名当作 RK3576。

准备官方小样本校准集并转换：

```sh
git clone --depth 1 --branch v2.1.0 https://github.com/airockchip/rknn_model_zoo.git build/rknn_model_zoo
build/npu-host/venv/bin/python scripts/npu/prepare_calibration.py \
  --source-list build/rknn_model_zoo/datasets/COCO/coco_subset_20.txt \
  --output-directory build/npu-models/calibration
build/npu-host/venv/bin/python scripts/npu/convert_rknn.py \
  --onnx build/npu-models/yolov5n.onnx --dtype fp --output build/npu-models/yolov5n-rk3576-fp.rknn
build/npu-host/venv/bin/python scripts/npu/convert_rknn.py \
  --onnx build/npu-models/yolov5n.onnx --dtype i8 --output build/npu-models/yolov5n-rk3576-int8.rknn \
  --dataset build/npu-models/calibration/calibration.txt
```

校准 20 张图片仅作为最小部署实验，不代表充分覆盖车辆实际使用环境。
当前一张图的正确性检查没有替代独立标注验证集或完整 mAP 测试。

## 来源与许可

模型来自 [Ultralytics YOLOv5 v6.2](https://github.com/ultralytics/yolov5/tree/v6.2)，
上游声明 GPL-3.0；[Rockchip 适配源码](https://github.com/airockchip/yolov5/tree/23a20ef3e5fa648e765e4a70b30edbfd3aaf8c8c)
同样保留 GPL-3.0 声明。模型目录保留许可全文、原始权重来源与转换记录；
没有将整仓库重新声明为其他许可证。
RKNN SDK 和 Runtime 受 Rockchip 自身条款约束，未将其头文件或运行库复制进 Git。
示例图片按 [Model Zoo 固定文件](https://github.com/airockchip/rknn_model_zoo/blob/c2b7d00714b4e5d21266ab3003f3ca687ba0d57b/examples/yolov5/model/bus.jpg)
下载并注明来源，本仓库不重新分发该图片，也不宣称拥有其版权。

## 接下来

先解决白色画面并核对摄像头可见目标，再完成 FP/INT8 板端对照；按实测数据决定后续优化。
