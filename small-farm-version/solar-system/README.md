# Solar Power System - 太阳能供电控制方案

## 功能
- 四象限光敏电阻追光算法（双轴舵机）
- 3S 锂电池充放电管理与 SOC 估算
- 低电压自动保护与负载切换
- 串口 JSON 状态上报

## 硬件
- ESP32 开发板
- 4× 光敏电阻 (GL5516)
- 2× 舵机 (MG996R)
- 3S LiPo 电池 (12.6V / 6000mAh)
- 太阳能板 (21V / 20W)
- 电压分压模块

## 编译烧录
`ash
pio run -t upload
pio device monitor
`

## 串口指令
| 指令 | 功能 |
|------|------|
| STATUS | 获取电池状态 JSON |
| LOAD_ON/LOAD_OFF | 开关负载 |
| RESET_COULOMB | 重置库仑计数 |
