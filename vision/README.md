# RK3576 行人／车辆检测：图片与单线程摄像头

当前交付：C++ Runtime 加载检查、YOLOv5n 导出与 RKNN 转换脚本、
两份 RK3576 模型、C++ 图片/摄像头推理、云端正确性测试和基础计时。
**板端 INT8 静态图推理已通过；摄像头真实运行、非量化模型对照、多线程和量化精度评估待完成。**

## 已收到的板端结果

用户在 `fa203eb` 版本回传 Runtime 2.1.0、driver 0.9.8，模型初始化成功，
INT8 检出 3 person + 1 bus；类别与数量对应云端参考。
一次预热、三次测量的 rknn_run 均值 11.473 ms，processing 均值 22.933 ms，
output_get 均值 7.931 ms。详细数据见 [原始范围归档](reports/board-int8-smoke.json)。
这是单图功能 smoke test，不能作为稳定性能、摄像头 FPS 或数据集精度结论。
结果图已经由程序保存，用户视觉核对尚未回传。

首次摄像头尝试在模型初始化后自动发现 `found 0`，未采集任何帧，
见 [失败记录](reports/board-camera-attempt-1.json)。当时诊断未保留每个设备的打开错误，
所以具体原因尚未确定。代码审查发现原 Qt 启动器提供临时摄像头 ACL，而首版 NPU 启动器遗漏了此流程。
现已加入逐设备诊断和临时授权；是否解决实际板端问题仍需再次运行确认。

## 下一步：单线程 USB 摄像头

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

摄像头代码仅云端构建及辅助测试通过；真实采集、预览、Ctrl+C、重开和断开行为均待板端验收。
启动器有 10 项 host mock 测试，覆盖已有权限、临时授权/恢复、失败回滚、设备替换、
恢复失败保留备份、仅在权限错误时请求提权，以及拒绝整个推理以 root 运行。
这些测试不执行真实 sudo、不改变设备 ACL，板端临时权限流程仍待验证。

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

首次复现需要先取得示例图（可先运行板端脚本，或按其固定 URL 和摘要下载）。
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

先验收单线程摄像头，包括结果图、正常结束/停止后重开；随后做 FP/INT8 的板端对照与性能定位。
