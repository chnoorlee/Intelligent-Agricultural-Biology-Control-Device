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
   `ash
   cd home-version/yolo-lightweight
   pip install -r requirements.txt
   `
2. **下载模型权重**
   - 下载预训练权重到 weights/ 目录
   - 或使用 scripts/train.py 自行训练
3. **STM32 固件烧录**
   `ash
   cd home-version/stm32-firmware
   make flash  # 需要 arm-none-eabi-gcc + st-flash
   `
4. **硬件接线** (见 home-version/hardware/wiring.md)
5. **启动检测**
   `ash
   python scripts/detect.py --camera 0 --threshold 0.5
   `

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
   `ash
   cd small-farm-version/esp32-firmware
   pio run -t upload
   `
2. **云台追踪**
   `ash
   cd small-farm-version/gimbal-tracker
   pip install -r requirements.txt
   python -m src.tracker --camera 0
   `
3. **太阳能控制**
   `ash
   cd small-farm-version/solar-system
   pio run -t upload
   `

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
   `ash
   cd large-farm-version/flight-control
   pio run -t upload
   `
2. **路径规划服务**
   `ash
   cd large-farm-version/cruise-planner
   pip install -r requirements.txt
   python -m src.path_planner
   `
3. **后端分析服务**
   `ash
   cd large-farm-version/backend-analysis
   pip install -r requirements.txt
   uvicorn src.app:app --host 0.0.0.0 --port 8000
   `
"@ | Out-File -FilePath "C:\Users\Chnoor\Desktop\tmp-bio-control\docs\installation.md" -Encoding utf8

# docs/user_manual.md
@"
# 用户使用手册

## 家用阳台版使用指南

### 开机与自检
1. 接通电源，系统自动启动
2. LED 指示灯：绿色常亮 = 正常运行，红色闪烁 = 检测到鸟害
3. 系统自动校准，约30秒完成

### 日常使用
- 全自动运行，无需人工干预
- 摄像头 24 小时监测
- 检测到害鸟自动触发声光驱离

### 参数调节
在 home-version/yolo-lightweight/config/bird_detect.yaml 中调整：
- conf_threshold: 识别置信度阈值 (默认 0.5)
- deter_interval: 驱离间隔时间 (默认 30s)

---

## 小型农场版使用指南

### 启动
1. 开启太阳能供电系统
2. ESP32 自动连接 WiFi
3. OpenMV 云台开始 360° 扫描

### 工作流程
1. 云台识别鸟情 → 确认害鸟
2. 向 Mini 无人机发送目标坐标
3. 无人机飞抵目标 → 超声波驱赶
4. 驱赶成功 → 自动返航充电
5. 驱赶失败 → APP 推送告警

### 监控
访问 Web 面板 (ESP32 IP:80) 查看：
- 实时云台画面
- 驱赶日志
- 电池/太阳能状态

---

## 大型农场版使用指南

### 双模式操作

**自动巡航模式**（低密度鸟群）
1. 预设巡航路线（KML/GPX 导入）
2. 固定翼无人机按路线自动飞行
3. 机载超声波常态化驱鸟
4. 遥测数据实时回传

**人工介入模式**（高密度鸟群）
1. 后端检测到高密度鸟群
2. 系统自动告警并建议切换
3. 操作员手动接管飞控
4. 结合 DeepSeek 分析结果优化驱鸟路线

### 数据分析
登录后台 (http://server:8000) 查看：
- 鸟群活动热力图
- 驱避效果统计
- 设备运行状态
