# STM32 智能农业驱离装置固件

## 概述

STM32F103C8T6 (Blue Pill) 微控制器固件，作为农业生物防控装置的主控板。

### 核心功能
- 📡 通过USART1接收上位机(YOLO检测结果)的串口指令
- 🔊 PWM无源蜂鸣器驱离 (200Hz-8KHz可调)
- 💡 高亮LED爆闪灯驱离 (1-20Hz可调, 继电器驱动)
- 📏 HC-SR04超声波测距
- 🌡️ DHT11/DHT22温湿度监测
- 🌞 光敏电阻光照检测
- 🛡️ IWDG看门狗(4秒超时) + 故障检测
- 🔄 支持超声波自动备份驱离(上位机失效时)

## 硬件要求

| 组件 | 型号 | 数量 | 说明 |
|------|------|------|------|
| 主控 | STM32F103C8T6 | 1 | Blue Pill开发板 |
| 蜂鸣器 | 无源蜂鸣器 3-12V | 1 | 通过三极管驱动 |
| 爆闪灯 | 12V LED灯带 | 1 | 通过继电器模块驱动 |
| 超声波 | HC-SR04 | 1 | 距离传感器 |
| 温湿度 | DHT11/DHT22 | 1 | 环境监测 |
| 继电器 | SRD-05VDC | 1 | 5V继电器模块 |
| 光耦 | PC817 | 2 | 隔离保护 |
| 降压模块 | MP1584 12V→5V | 1 | 电源转换 |

## 引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA0 | TIM2_CH1 PWM | 无源蜂鸣器驱动 |
| PA1 | GPIO OUT | 爆闪灯继电器控制 |
| PA4 | ADC1_IN4 | 光敏电阻分压采样 |
| PA9 | USART1_TX | 上位机通信 |
| PA10 | USART1_RX | 上位机通信 |
| PB0 | GPIO OUT | HC-SR04 Trig |
| PB1 | GPIO IN | HC-SR04 Echo |
| PC0 | GPIO OD | DHT11/DHT22 数据 |
| PC13 | GPIO OUT | 板载LED (心跳) |

## 编译

### 安装工具链
```bash
# Ubuntu/Debian
sudo apt install gcc-arm-none-eabi openocd stlink-tools

# macOS
brew install arm-none-eabi-gcc openocd stlink

# Windows
# 下载 ARM GCC: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain
# 下载 STM32CubeF1 HAL库
```

### 编译固件
```bash
make          # 编译, 生成 build/bio_control.bin
make clean    # 清理
```

### 烧录固件
```bash
# ST-Link (推荐)
make flash

# OpenOCD
make flash-ocd

# 串口 (需要boot0=1)
make flash-serial
```

## 串口通信协议

### 请求帧格式
```
[0xAA] [CMD] [LEN] [DATA...] [CHK] [0xBB]
```
- 校验和 = CMD ^ LEN ^ DATA[0] ^ ... ^ DATA[n-1]

### 命令列表
| 命令 | 值 | 参数 | 说明 |
|------|-----|------|------|
| BUZZER_ON | 0x01 | freq(2B) + dur(2B) | 启动蜂鸣器 |
| BUZZER_OFF | 0x02 | 无 | 停止蜂鸣器 |
| STROBE_ON | 0x03 | freq(1B) + dur(2B) | 启动爆闪灯 |
| STROBE_OFF | 0x04 | 无 | 停止爆闪灯 |
| FULL_DETER | 0x05 | 无 | 紧急驱离 |
| STOP_ALL | 0x06 | 无 | 停止全部 |
| PING | 0x07 | 无 | 心跳检测 |
| GET_SENSORS | 0x0A | 无 | 查询传感器 |

## 自动备份驱离

超声波传感器持续监测前方区域：
- 当检测到物体 < 50cm 时(可能是鸟类降落)
- 且距离上次驱离超过5秒
- 自动启动 2KHz蜂鸣 + 3Hz爆闪, 持续3秒

此功能作为上位机(YOLO)失效时的备份保护。

## 状态LED指示

| LED状态 | 含义 |
|---------|------|
| 慢闪(1Hz) | 空闲正常 |
| 快闪(5Hz) | 正在驱离中 |
| 连续闪烁N次 | 错误码N |
