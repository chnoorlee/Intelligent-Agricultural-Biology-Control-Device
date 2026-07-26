# ESP32 主控固件 — 小型农场版

## 概述

本固件运行于 ESP32 DevKit V1，作为小型农场版智能生物防控系统的**中央控制单元**，负责：

- 与上位机/摄像头端通过 UART 通信接收目标追踪指令
- 控制 PCA9685 双轴舵机云台指向目标
- 通过 ESP-NOW/UART 与 Mini 无人机通信，派发驱离任务
- 监测 HC-SR04 超声波、DHT22 温湿度、太阳能电池状态
- 通过 MQTT 协议将传感器数据与事件上报云平台

## 硬件依赖

| 模块           | 接口       | 引脚          | 说明                    |
|---------------|-----------|---------------|-------------------------|
| PCA9685       | I2C       | SDA=21, SCL=22| 16路 PWM, 驱动云台舵机   |
| HC-SR04       | GPIO      | Trig=5, Echo=18 | 超声波测距              |
| DHT22         | GPIO      | GPIO4         | 温湿度传感器             |
| 太阳能板ADC   | ADC1      | GPIO34         | 电压分压后采样          |
| 电池ADC       | ADC1      | GPIO35         | 电压分压后采样          |
| 无人机UART    | UART2     | RX=16, TX=17  | 备用有线通信             |
| 无人机ESP-NOW | WiFi      | 内建           | 无线通信 (2.4GHz)        |
| Status LED   | GPIO      | GPIO2          | 板载LED状态指示          |

## 引脚分配总图

```
ESP32 DevKit V1
┌──────────────┐
│   EN         │
│   GPIO36(VP) │── (ADC1_CH0)
│   GPIO39(VN) │── (ADC1_CH3)
│   GPIO34     │── 太阳能板电压 (ADC)
│   GPIO35     │── 电池电压 (ADC)
│   GPIO32     │
│   GPIO33     │
│   GPIO25     │
│   GPIO26     │
│   GPIO27     │
│   GPIO14     │
│   GPIO12     │
│   GND        │
│   VIN (5V)   │── 5V电源输入
│   GPIO13     │
│   GPIO15     │
│   GPIO2      │── Status LED (板载)
│   GPIO4      │── DHT22 DATA
│   GPIO16/RX2 │── 无人机UART RX
│   GPIO17/TX2 │── 无人机UART TX
│   GPIO5      │── HC-SR04 Trig
│   GPIO18     │── HC-SR04 Echo
│   GPIO19     │
│   GPIO21/SDA │── PCA9685 SDA
│   GPIO22/SCL │── PCA9685 SCL
│   GPIO23     │
│   3.3V       │
│   GND        │
└──────────────┘
```

## 编译与烧录

### 前置条件

- [PlatformIO IDE](https://platformio.org/) (VS Code 插件) 或 PlatformIO Core CLI
- USB 数据线连接 ESP32

### 步骤

```bash
# 1. 进入固件目录
cd small-farm-version/esp32-firmware

# 2. (首次) 安装依赖
pio pkg install

# 3. 修改 WiFi 与 MQTT 配置
#    编辑 platformio.ini 中的 build_flags:
#    -DWIFI_SSID=\"你的WiFi名\"
#    -DWIFI_PASSWORD=\"你的WiFi密码\"
#    -DMQTT_BROKER=\"你的MQTT服务器IP\"
#    -DMQTT_PORT=1883

# 4. 编译
pio run

# 5. 烧录 (选择对应的串口)
pio run --target upload

# 6. 查看串口输出
pio device monitor
```

## 通信协议

### 上位机 → ESP32 (UART, 115200bps)

| 指令格式                 | 含义                          |
|-------------------------|-------------------------------|
| `TRACK:pan,tilt,conf`   | 追踪目标: 水平角度, 俯仰角度, 置信度 |
| `DRIVE:1`               | 激活驱离                       |
| `DRIVE:0`               | 停止驱离                       |
| `CENTER`                | 云台归中                       |
| `PING`                  | 心跳检测 → 回复 PONG           |
| `STATUS`                | 查询系统状态                    |

### ESP32 → MQTT (周期性上报)

| 主题                     | 内容                      | 频率    |
|-------------------------|---------------------------|---------|
| `biocontrol/farm/sensor`| JSON 传感器数据             | 5秒     |
| `biocontrol/farm/state` | 系统状态 (idle/tracking/...) | 状态变更时 |
| `biocontrol/farm/event` | 事件 (drive_success/failed等) | 事件触发时 |
| `biocontrol/farm/alert` | 告警信息                   | 告警触发时 |

### MQTT 传感器数据格式

```json
{
  "t": 28.5,
  "h": 65.2,
  "bat_v": 3.85,
  "bat_pct": 72,
  "solar_v": 5.2,
  "dist": 156.3,
  "ts": 12345678
}
```

## 状态机说明

```
        ┌─────────┐
        │  INIT   │ 系统启动初始化
        └────┬────┘
             ↓
        ┌─────────┐
   ┌────│  IDLE   │ 空闲等待检测
   │    └────┬────┘
   │         │ TRACK 指令
   │         ↓
   │    ┌──────────┐
   │    │ TRACKING │ 追踪目标，云台随动
   │    └────┬─────┘
   │         │ DRIVE:1 指令
   │    ┌────┴────────────┐
   │    │ 距离 < 50cm?     │
   │    │ Y        N       │
   │    ↓         ↓        │
   │ ┌───────┐ ┌──────────┐│
   │ │DRIVING│ │DRONE_DISP││
   │ │(声光)  │ │(派发无人机)││
   │ └───┬───┘ └────┬─────┘│
   │     │           │ DRONE_ARRIVED
   │     │     ┌─────▼────┐│
   │     │     │ DRIVING  ││
   │     │     │(超声驱离) ││
   │     │     └─────┬────┘│
   │     │           │ DRIVE_DONE
   │     │     ┌─────▼────┐
   │     │     │RETURNING │ 返航中
   │     │     └─────┬────┘
   │     │           │ DRONE_RETURNED
   └─────┴───────────┘
                    ↓
            ┌─────────┐
            │  IDLE   │
            └─────────┘
```

## 文件结构

```
esp32-firmware/
├── src/
│   ├── main.cpp              # 主程序入口，状态机调度
│   ├── wifi_manager.cpp/h    # WiFi AP/STA 双模式管理
│   ├── gimbal_control.cpp/h  # PCA9685 双轴舵机控制
│   ├── drone_comm.cpp/h      # ESP-NOW/UART 无人机通信
│   ├── ultrasonic.cpp/h      # HC-SR04 超声波测距
│   ├── solar_manager.cpp/h   # 太阳能电池管理
│   └── mqtt_client.cpp/h     # MQTT 云平台上报
├── platformio.ini            # PlatformIO 项目配置
└── README.md                 # 本文件
```
