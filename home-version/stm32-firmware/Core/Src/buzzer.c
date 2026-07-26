/**
 * @file    buzzer.c
 * @brief   蜂鸣器驱动实现 — PWM频率/音量控制
 *
 * 硬件连接: PA0 (TIM2_CH1) → 2N2222三极管基极 → 集电极接蜂鸣器 → VCC
 *          基极串联1KΩ限流电阻, 蜂鸣器并联续流二极管
 */

#include "buzzer.h"

void Buzzer_Init(void)
{
    /* GPIO和TIM2初始化已在main.c中完成 */
    /* 确保蜂鸣器初始为静音状态 */
    g_device_state.buzzer_active = false;
    g_device_state.buzzer_freq_hz = 0;
    g_device_state.buzzer_end_tick = 0;

    /* 设置PWM占空比为0(静音) */
    __HAL_TIM_SET_COMPARE(&htim2, BUZZER_TIM_CHANNEL, 0);
}

void Buzzer_On(uint16_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz < 200 || freq_hz > 8000) {
        return; /* 频率超出范围 */
    }

    /* 停止当前蜂鸣 */
    Buzzer_Off();

    /*
     * 设置PWM频率:
     * TIM2时钟 = 72MHz / (PSC+1) = 72MHz / 72 = 1MHz
     * 目标频率 freq_hz = 1MHz / (ARR+1)
     * 所以 ARR = 1MHz / freq_hz - 1
     */
    uint32_t arr = (1000000UL / freq_hz) - 1;
    if (arr < 10) arr = 10;
    if (arr > 65535) arr = 65535;

    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);

    /* 50%占空比产生最大音量 */
    uint32_t pulse = arr / 2;
    __HAL_TIM_SET_COMPARE(&htim2, BUZZER_TIM_CHANNEL, pulse);

    /* 记录状态 */
    g_device_state.buzzer_active = true;
    g_device_state.buzzer_freq_hz = freq_hz;
    g_device_state.buzzer_end_tick = (duration_ms == 0)
        ? 0xFFFFFFFF
        : g_systick_ms + duration_ms;
}

void Buzzer_Off(void)
{
    /* PWM占空比设为0 (静音但保持信号) */
    __HAL_TIM_SET_COMPARE(&htim2, BUZZER_TIM_CHANNEL, 0);

    g_device_state.buzzer_active = false;
    g_device_state.buzzer_freq_hz = 0;
    g_device_state.buzzer_end_tick = 0;
}

void Buzzer_Update(void)
{
    if (!g_device_state.buzzer_active) {
        return;
    }

    /* 检查持续时间是否到期 */
    if (g_device_state.buzzer_end_tick != 0xFFFFFFFF &&
        g_systick_ms >= g_device_state.buzzer_end_tick)
    {
        Buzzer_Off();
    }
}

bool Buzzer_IsActive(void)
{
    return g_device_state.buzzer_active;
}
