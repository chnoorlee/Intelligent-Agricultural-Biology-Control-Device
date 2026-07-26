# YOLO轻量化鸟类检测模块

## 概述

基于 YOLOv8 的轻量化鸟类检测系统，专为智能农业生物防控设计。可实时检测农田中的鸟类，区分害鸟/益鸟，并自动触发驱离设备。

### 核心功能
- 🐦 **7类鸟类识别**：麻雀、乌鸦、喜鹊、鸽子(害鸟)，燕子、猫头鹰、啄木鸟(益鸟)
- ⚡ **实时检测**：30+ FPS 在树莓派4上
- 🔌 **多平台导出**：ONNX、TFLite、TensorRT、OpenVINO
- 📡 **串口控制**：通过UART协议控制STM32驱离设备
- 🚨 **智能决策**：根据害鸟数量和种类自动选择驱离策略

## 目录结构

```
yolo-lightweight/
├── config/
│   └── bird_detect.yaml      # YOLO模型配置文件
├── models/
│   └── bird_classes.py        # 鸟类分类定义
├── scripts/
│   ├── train.py                # 训练脚本
│   ├── detect.py               # 实时检测推理
│   ├── export_onnx.py          # 模型导出
│   └── dataset_prep.py         # 数据集预处理
├── utils/
│   ├── preprocess.py           # 图像预处理
│   ├── postprocess.py          # 后处理/NMS
│   └── camera.py               # 摄像头采集
├── weights/                    # 模型权重目录
├── requirements.txt
└── README.md
```

## 快速开始

### 1. 安装依赖

```bash
pip install -r requirements.txt
```

### 2. 准备数据集

```bash
# 从原始图像和标注创建YOLO格式数据集
python scripts/dataset_prep.py \
    --images ../../dataset/raw/images \
    --labels ../../dataset/raw/labels \
    --output ../../dataset \
    --split 0.7 0.2 0.1 \
    --stats
```

### 3. 训练模型

```bash
python scripts/train.py --config config/bird_detect.yaml
```

### 4. 导出模型

```bash
# 导出ONNX和TFLite
python scripts/export_onnx.py --model weights/best.pt --formats onnx,tflite

# 带性能测试
python scripts/export_onnx.py --model weights/best.pt --benchmark
```

### 5. 实时检测

```bash
# USB摄像头检测
python scripts/detect.py --source 0 --model weights/best.pt

# 检测并控制STM32驱离设备
python scripts/detect.py --source 0 --model weights/best.pt --serial COM3

# 视频文件检测
python scripts/detect.py --source birds.mp4 --model weights/best.onnx --save

# 无头模式（树莓派）
python scripts/detect.py --source 0 --model weights/best.tflite --no-display
```

## 硬件要求

| 设备 | 最低配置 | 推荐配置 |
|------|---------|---------|
| PC训练 | CPU + 8GB RAM | NVIDIA GPU (≥6GB VRAM) |
| 边缘推理 | 树莓派4B (4GB) | Jetson Nano / Orin |
| 摄像头 | USB 720p | USB 1080p / CSI |

## 串口通信协议

与STM32驱离板的通信格式：

```
帧格式: [0xAA] [命令] [参数长度] [参数...] [校验和] [0xBB]

命令列表:
  0x01 - 启动蜂鸣器 (参数: 频率(2B) + 时长(2B))
  0x02 - 停止蜂鸣器
  0x03 - 启动爆闪灯 (参数: 频率(1B) + 时长(2B))
  0x04 - 停止爆闪灯
  0x05 - 紧急驱离(全开)
  0x06 - 停止全部
```

## 模型性能

| 模型 | mAP50 | 推理速度(RPi4) | 模型大小 |
|------|-------|---------------|---------|
| YOLOv8n | ~85% | 8 FPS | 6.2 MB |
| YOLOv8n ONNX | ~85% | 12 FPS | 6.0 MB |
| YOLOv8n TFLite | ~84% | 15 FPS | 3.1 MB |

*注：mAP数据取决于训练数据集，需要使用实际数据训练后验证*
