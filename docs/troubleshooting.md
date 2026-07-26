# 硬件组装指南

## 家用版硬件组装

### 物料清单
| 物料 | 规格 | 数量 | 单价(元) |
|------|------|------|----------|
| STM32F103C8T6 | 最小系统板 | 1 | 15 |
| USB摄像头 | 720P 免驱 | 1 | 35 |
| 蜂鸣器模块 | 有源 3.3V | 1 | 3 |
| 爆闪灯模块 | 绿色 LED 高亮 | 1 | 8 |
| USB-TTL | CH340G | 1 | 5 |
| 电源适配器 | 5V 2A | 1 | 10 |
| 杜邦线+面包板 | - | 1套 | 10 |
| 外壳 | 3D打印防水盒 | 1 | 20 |
| **合计** | | | **~106** |

### 组装步骤
1. 将 STM32 固定在外壳底板上
2. 摄像头安装在外壳顶部开孔处
3. 蜂鸣器和爆闪灯安装在外壳前方
4. 按接线图连接所有模块
5. 通电测试

## 小型农场版硬件

详见 small-farm-version/esp32-firmware/README.md 和 small-farm-version/solar-system/README.md

## 大型农场版硬件

详见 large-farm-version/flight-control/README.md
"@ | Out-File -FilePath "C:\Users\Chnoor\Desktop\tmp-bio-control\docs\hardware_guide.md" -Encoding utf8

# docs/troubleshooting.md
@"
# 常见问题排查

## 家用版

### 摄像头无法识别
- 检查 USB 连接
- Linux: ls /dev/video*
- Windows: 设备管理器确认驱动
- 尝试更换 USB 端口

### 蜂鸣器不响
- 检查 STM32 PA0 引脚电平
- 确认蜂鸣器为有源型（自带振荡电路）
- 串口助手发送 BUZZER_ON 测试

### 误报率高
- 降低 conf_threshold 为 0.6-0.7
- 检查摄像头安装角度（避免逆光）
- 排除树枝晃动等动态背景

## 小型农场版

### 云台抖动
- 增大 PID 死区 dead_zone_px
- 降低 PID 增益 pid_pan_kp
- 检查舵机供电（独立 5V 2A）

### 无人机通信中断
- 检查 ESP-NOW 配对 MAC 地址
- 确保视距内无遮挡
- 降低通信频率

### 太阳能充电不足
- 清洁太阳能板表面
- 检查追光舵机是否卡住
- 检查电池健康度

## 大型农场版

### GPS 信号丢失
- 远离高压线、金属建筑
- 等待冷启动 (首次定位需 2-5 分钟)
- 检查 GPS 天线朝向

### 飞控姿态解算异常
- 重新校准 IMU（水平静置）
- 检查震动隔离（减震球老化）
- 更新 Madgwick 滤波器 beta 参数
