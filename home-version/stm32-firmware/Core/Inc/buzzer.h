/**
 * @file    buzzer.h
 * @brief   蜂鸣器驱动模块 — PWM频率/音量控制
 *
 * 功能: 通过TIM2 PWM控制无源蜂鸣器，可设置频率和持续时间
 * 硬件: PA0 → TIM2_CH1 → 三极管驱动 → 无源蜂鸣器
 */

#ifndef __BUZZER_H
#define __BUZZER_H

#include "main.h"

/**
 * @brief  初始化蜂鸣器PWM输出
 * @note   使用TIM2_CH1 (PA0), 初始频率1000Hz, 占空比50%
 */
void Buzzer_Init(void);

/**
 * @brief  启动蜂鸣器
 * @param  freq_hz    频率 (Hz), 范围 200-8000
 * @param  duration_ms 持续时间 (ms), 0表示手动停止
 * @note   不同频率对鸟类有不同的驱离效果:
 *         1000-2000Hz: 温和驱离 (适用于初期预警)
 *         2000-4000Hz: 强力驱离 (适用于中度威胁)
 *         4000-8000Hz: 紧急驱离 (适用于严重威胁)
 */
void Buzzer_On(uint16_t freq_hz, uint32_t duration_ms);

/**
 * @brief  停止蜂鸣器
 */
void Buzzer_Off(void);

/**
 * @brief  蜂鸣器状态更新 (在main循环或定时器中调用)
 * @note   检查持续时间是否到期, 自动停止
 */
void Buzzer_Update(void);

/**
 * @brief  获取蜂鸣器当前状态
 * @return true=正在蜂鸣, false=已停止
 */
bool Buzzer_IsActive(void);

#endif /* __BUZZER_H */
