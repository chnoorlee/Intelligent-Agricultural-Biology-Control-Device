# 模型权重文件目录

## 下载预训练权重

### YOLOv8 预训练权重

| 模型变体 | 参数量 | mAP50 | 下载链接 |
|---------|--------|-------|---------|
| YOLOv8n | 3.2M   | 37.3  | [yolov8n.pt](https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8n.pt) |
| YOLOv8s | 11.2M  | 44.9  | [yolov8s.pt](https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8s.pt) |
| YOLOv8m | 25.9M  | 50.2  | [yolov8m.pt](https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8m.pt) |

### 训练后权重命名规范

```
best.pt        — 训练后的最佳模型 (PyTorch)
best.onnx      — 导出的ONNX模型 (用于ONNX Runtime推理)
best.tflite    — 导出的TFLite模型 (用于树莓派等边缘设备)
best.engine    — TensorRT引擎 (用于NVIDIA Jetson)
best_openvino_model/ — OpenVINO IR格式 (Intel设备)
last.pt        — 最后一个epoch的checkpoint
```

## 使用方式

```python
# 方式1: 使用训练脚本自动下载
python scripts/train.py --config config/bird_detect.yaml

# 方式2: 手动下载
wget https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8n.pt -O weights/yolov8n.pt

# 方式3: Python API
from ultralytics import YOLO
model = YOLO('yolov8n.pt')  # 自动下载
```

## 注意

- 权重文件较大（>5MB），请勿提交到Git仓库
- 训练后的权重保存到此目录
- 生产环境建议使用TFLite/ONNX以减少推理延迟
