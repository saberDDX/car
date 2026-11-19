# RK3576 部署模型

本目录包含本项目生成的两份小型部署产物，方便板卡通过 Git 直接取得，
不需要重新下载外部网盘或在板卡运行转换工具。

- `yolov5n-rk3576-fp.rknn`：非量化构建，Toolkit 默认 float16 模式。
- `yolov5n-rk3576-int8.rknn`：使用 20 张示例校准图的 PTQ INT8 构建。
- `SHA256SUMS` 与同名 JSON：模型摘要、工具链与校准输入记录。

**两份模型均尚未在用户 RK3576 上完成真实推理验证。**
原始权重为官方 YOLOv5 v6.2 `yolov5n.pt`，SHA-256：
`4f180cf23ba0717ada0badd6c685026d73d48f184d00fc159c2641284b2ac0a3`。
来源：https://github.com/ultralytics/yolov5/releases/download/v6.2/yolov5n.pt 。

上游 YOLOv5 声明 GPL-3.0，保留其许可全文 `LICENSE-YOLOv5`。
相应源码和可复现导出/转换步骤见 [上级文档](../README.md)。
这些是预训练模型部署产物，不是本项目自行训练的模型。
