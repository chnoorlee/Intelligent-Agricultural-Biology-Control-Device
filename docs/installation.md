# 安装部署指南

## 家用阳台版 安装 (Home Version)

### 环境要求
- Python 3.9+
- STM32F103C8T6 开发板
- USB-TTL 串口模块
- 国产 USB 摄像头 (720P+)
- 蜂鸣器模块 (有源 3.3V)
- 绿色 LED 爆闪灯模块

### 部署步骤
1. **Python 环境配置**
   ```bash
   cd home-version/yolo-lightweight
   pip install -r requirements.txt
   ```
2. **下载模型权重**
   - 下载预训练权重到 `weights/` 目录
   - 或使用 `scripts/train.py` 自行训练
3. **STM32 固件烧录**
   ```bash
   cd home-version/stm32-firmware
   make flash  # 需要 arm-none-eabi-gcc + st-flash
   ```
4. **硬件接线** (见 `home-version/hardware/wiring.md`)
5. **启动检测**
   ```bash
   python scripts/detect.py --camera 0 --threshold 0.5
   ```

---

## 小型农场版 安装 (Small Farm Version)

### 环境要求
- Raspberry Pi 4B 或 Jetson Nano
- ESP32 开发板
- OpenMV Cam H7+
- Mini 无人机 (ESP32-Drone)
- PCA9685 舵机驱动板
- HC-SR04 超声波模块
- 太阳能板 (20W) + 3S LiPo 电池

### 部署步骤
1. **ESP32 固件**
   ```bash
   cd small-farm-version/esp32-firmware
   pio run -t upload
   ```
2. **云台追踪**
   ```bash
   cd small-farm-version/gimbal-tracker
   pip install -r requirements.txt
   python -m src.tracker --camera 0
   ```
3. **太阳能控制**
   ```bash
   cd small-farm-version/solar-system
   pio run -t upload
   ```

---

## 大型农场版 安装 (Large Farm Version)

### 环境要求
- 固定翼无人机 (Titan Bumblebee Y3 VTOL 或类似)
- Pixhawk / Matek 飞控
- GPS + 罗盘模块
- 数传电台 (433MHz / 915MHz)
- 4G 图传模块
- 后端服务器 (4核8G, Ubuntu 22.04)

### 部署步骤
1. **飞控固件**
   ```bash
   cd large-farm-version/flight-control
   pio run -t upload
   ```
2. **路径规划服务**
   ```bash
   cd large-farm-version/cruise-planner
   pip install -r requirements.txt
   python -m src.path_planner
   ```
3. **后端分析服务**
   ```bash
   cd large-farm-version/backend-analysis
   pip install -r requirements.txt
   uvicorn src.app:app --host 0.0.0.0 --port 8000
   ```

## 常见问题
详见 `docs/troubleshooting.md`
