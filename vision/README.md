# RK3576 行人／车辆检测：静态图片阶段

当前交付：C++ Runtime 加载检查、YOLOv5n 导出与 RKNN 转换脚本、
两份 RK3576 模型、C++ 图片推理/后处理、云端正确性测试和基础计时。
**板端真实推理待执行；摄像头整合、多线程和量化精度评估尚未完成。**

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
- 现有新增 CTest 1/1 通过：anchor/grid 解码、NCHW 索引、置信度、类别过滤、跨类别 NMS。
- 加载器的 host fixture 检查：库缺失、符号缺失、正常符号集合、参数错误，全部通过。
  fixture 不实现 NPU，没有用它替代板端推理验收。
- 一张官方示例图上，适配后的 PyTorch 与 ONNX Runtime 的输出一致，
  三个 head 最大绝对误差分别见 [export-reference.json](reports/export-reference.json)，均小于 4e-6。
- 真实 ONNX head 经 C++ 与官方 Python 后处理得到相同 4 个结果，框最大绝对差
  约 3.4e-5 像素；[参考检测](reports/onnx-detections.json)为 3 person + 1 bus。
- 非量化与 INT8 模型转换成功。INT8 使用官方 Model Zoo 的 20 张 COCO 子集图片，
  先按相同策略 letterbox；[来源摘要](reports/calibration-sources.json)。
- 以上没有证明 NPU 效果、板端耗时、量化精度或摄像头链路通过。

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

先回传真实板端 API/driver 版本、张量信息、检测结果、错误和结果图。
确认推理契约正确后再接现有 V4L2 数据源，最后做 FP/INT8 的板端对照与性能定位。
