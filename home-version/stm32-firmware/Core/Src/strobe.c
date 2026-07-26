/**
 * @file    strobe.c
 * @brief   爆闪灯驱动实现 — IO控制继电器驱动高亮LED
 *
 * 硬件连接: PA1 → PC817光耦(输入侧:1KΩ限流) → 
 *           光耦输出侧 → 2N2222 → SRD-05VDC继电器 → 12V LED灯带
 *           
 * 闪烁逻辑: 通过定时更新实现精确的闪烁周期
 *           flash_period_ms = 1000 / flash_freq_hz
 *           每半个周期翻转IO电平 (50% duty cycle)
 */

#include "strobe.h"

/* 闪烁时间控制 */
static uint32_t g_strobe_period_ms = 0;   /* 闪烁周期 = 1000/freq */
static uint32_t g_strobe_last_toggle = 0; /* 上次翻转时间 */

void Strobe_Init(void)
{
    /* GPIO已在main.c中初始化 */
    HAL_GPIO_WritePin(STROBE_PORT, STROBE_PIN, GPIO_PIN_RESET);

    g_device_state.strobe_active = false;
    g_device_state.strobe_flash_freq = 0;
    g_device_state.strobe_end_tick = 0;
    g_strobe_period_ms = 0;
    g_strobe_last_toggle = 0;
}

void Strobe_On(uint8_t flash_freq_hz, uint32_t duration_ms)
{
    if (flash_freq_hz < 1 || flash_freq_hz > 20) {
        return; /* 频率超出范围 */
    }

    /* 停止当前闪烁 */
    Strobe_Off();

    /* 计算闪烁周期 */
    g_strobe_period_ms = 1000U / flash_freq_hz;
    if (g_strobe_period_ms < 50) g_strobe_period_ms = 50;
    g_strobe_last_toggle = g_systick_ms;

    /* 立即点亮一次 */
    HAL_GPIO_WritePin(STROBE_PORT, STROBE_PIN, GPIO_PIN_SET);

    /* 记录状态 */
    g_device_state.strobe_active = true;
    g_device_state.strobe_flash_freq = flash_freq_hz;
    g_device_state.strobe_end_tick = (duration_ms == 0)
        ? 0xFFFFFFFF
        : g_systick_ms + duration_ms;
}

void Strobe_Off(void)
{
    HAL_GPIO_WritePin(STROBE_PORT, STROBE_PIN, GPIO_PIN_RESET);

    g_device_state.strobe_active = false;
    g_device_state.strobe_flash_freq = 0;
    g_device_state.strobe_end_tick = 0;
    g_strobe_period_ms = 0;
}

/**
 * @brief  爆闪灯状态更新
 * @note   需要在主循环或高频定时器中调用 (每1-10ms)
 *         每半个周期翻转一次IO电平 = 50%占空比方波
 */
void Strobe_Update(void)
{
    if (!g_device_state.strobe_active) {
        return;
    }

    /* 检查持续时间是否到期 */
    if (g_device_state.strobe_end_tick != 0xFFFFFFFF &&
        g_systick_ms >= g_device_state.strobe_end_tick)
    {
        Strobe_Off();
        return;
    }

    /* 闪烁逻辑: 每半个周期翻转一次 */
    uint32_t half_period = g_strobe_period_ms / 2;
    if (half_period < 25) half_period = 25;

    if (g_systick_ms - g_strobe_last_toggle >= half_period) {
        g_strobe_last_toggle = g_systick_ms;
        HAL_GPIO_TogglePin(STROBE_PORT, STROBE_PIN);
    }
}

bool Strobe_IsActive(void)
{
    return g_device_state.strobe_active;
}
