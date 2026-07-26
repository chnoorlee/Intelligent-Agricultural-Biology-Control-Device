/**
 * @file    strobe.h
 * @brief   爆闪灯驱动模块 — 继电器控制高亮LED爆闪
 *
 * 功能: 通过IO口控制继电器模块驱动高亮度LED爆闪
 * 硬件: PA1 → 光耦隔离 → 继电器模块 → 12V高亮LED灯带
 */

#ifndef __STROBE_H
#define __STROBE_H

#include "main.h"

/**
 * @brief  初始化爆闪灯控制引脚
 * @note   PA1配置为推挽输出, 初始低电平(关闭)
 */
void Strobe_Init(void);

/**
 * @brief  启动爆闪灯
 * @param  flash_freq_hz  闪烁频率 (Hz), 范围 1-20
 * @param  duration_ms    持续时间 (ms), 0表示手动停止
 * @note   鸟类对闪烁光非常敏感, 建议:
 *         1-5Hz:   缓慢闪烁 (常规驱离)
 *         5-10Hz:  快速闪烁 (强力驱离)
 *         10-20Hz: 高频闪烁 (紧急驱离，可能引起不适)
 */
void Strobe_On(uint8_t flash_freq_hz, uint32_t duration_ms);

/**
 * @brief  停止爆闪灯
 */
void Strobe_Off(void);

/**
 * @brief  爆闪灯状态更新 (在定时器中断中调用)
 * @note   实现PWM-like闪烁效果, 需要定期调用 (如每1ms)
 */
void Strobe_Update(void);

/**
 * @brief  获取爆闪灯当前状态
 * @return true=正在闪烁, false=已停止
 */
bool Strobe_IsActive(void);

#endif /* __STROBE_H */
