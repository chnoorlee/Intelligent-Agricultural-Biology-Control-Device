# Gimbal Tracker - 云台图像追踪模块

## 功能说明
基于 OpenCV 的 360° 云台鸟类追踪系统，配合 ESP32 舵机控制实现自动瞄准与追踪。

## 硬件要求
- Raspberry Pi 4B 或 Jetson Nano（运行追踪算法）
- OpenMV 摄像头模组（图像采集）
- PCA9685 舵机驱动板
- 双轴舵机云台（SG90/MG996R）
- ESP32 开发板（舵机串口控制）

## 安装
`ash
pip install -r requirements.txt
`

## 使用
`ash
# 真实硬件模式
python -m src.tracker --camera 0

# 模拟模式（无硬件测试）
python -m src.tracker --mock
`

## 工作原理
1. OpenMV 摄像头每秒采集 30 帧图像
2. 每 30 帧调用 YOLO 模型进行鸟类重检测
3. 检测到害鸟后，初始化 CSRT 追踪器锁定目标
4. PID 控制器根据目标偏离画面中心的位置计算舵机移动量
5. 通过串口发送指令控制云台舵机跟随目标
6. 追踪丢失超过 2 秒自动触发重新检测

## 配置参数
见 src/config.py，支持调节 PID 增益、追踪算法、死区等参数。
