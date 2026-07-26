/**
 * @file    sensor.h
 * @brief   传感器接口模块 — 超声波/温湿度/光照传感器数据采集
 *
 * 传感器列表:
 *   HC-SR04 超声波测距模块 (Trig: PB0, Echo: PB1)
 *   DHT11/DHT22 温湿度传感器 (Data: PC0)
 *   光敏电阻 + ADC1_CH4 (PA4)
 */

#ifndef __SENSOR_H
#define __SENSOR_H

#include "main.h"

/**
 * @brief  初始化所有传感器
 * @note   包括超声波引脚、DHT数据引脚、ADC初始化
 */
void Sensor_Init(void);

/**
 * @brief  读取超声波测距值
 * @return 距离 (cm), 返回999.9f表示超时(无回波)
 * @note   测量范围: 2cm - 400cm, 精度: ±0.3cm
 *         触发 → 10us脉冲 → 等待ECHO高电平 → 计算时间 → 距离 = 时间*0.034/2
 */
float Ultrasonic_GetDistance(void);

/**
 * @brief  读取DHT11/DHT22温湿度
 * @param  temperature 输出温度值 (°C)
 * @param  humidity    输出湿度值 (%)
 * @return true=读取成功, false=校验失败/超时
 * @note   DHT数据协议: 18ms起始信号 → 响应 → 40bit数据
 */
bool DHT_Read(float *temperature, float *humidity);

/**
 * @brief  读取光照传感器
 * @return ADC原始值 (0-4095), 值越大光照越强
 * @note   使用ADC1_CH4 (PA4), 12位精度
 */
uint16_t LightSensor_Read(void);

/**
 * @brief  获取光照百分比 (0-100%)
 * @return 光照百分比, 0=完全黑暗, 100=强烈阳光
 */
uint8_t LightSensor_GetPercent(void);

/**
 * @brief  更新所有传感器数据
 * @note   在主循环中周期性调用 (建议每1-2秒)
 */
void Sensor_UpdateAll(void);

#endif /* __SENSOR_H */
