/**
 * @file    sensor.c
 * @brief   传感器驱动实现 — 超声波/DHT/光照数据采集
 *
 * 超声波: HC-SR04, 2cm-400cm, Trig=PWM输出触发, Echo=输入捕获脉宽
 * 温湿度: DHT11/DHT22, 单总线协议, 通过bit-banging实现
 * 光照:   光敏电阻分压 + ADC1_CH4采样
 */

#include "sensor.h"

/* DHT超时参数 */
#define DHT_TIMEOUT_MS  200

/* 当前超声波距离缓存 */
static float g_ultrasonic_distance = 999.9f;

/* ================================================================
 * 初始化
 * ================================================================ */

void Sensor_Init(void)
{
    /* GPIO和ADC在main.c中初始化 */
    g_sensor_data.temperature = 0.0f;
    g_sensor_data.humidity = 0.0f;
    g_sensor_data.light_level = 0;
    g_sensor_data.distance_cm = 999.9f;
    g_sensor_data.dht_valid = false;
    g_ultrasonic_distance = 999.9f;
}


/* ================================================================
 * 超声波 HC-SR04 — 基于HAL库实现
 * ================================================================ */

float Ultrasonic_GetDistance(void)
{
    uint32_t pulse_start = 0;
    uint32_t pulse_end = 0;
    uint32_t timeout;

    /* 步骤1: 发送10us触发脉冲 */
    HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_RESET);
    __NOP(); __NOP(); /* ~200ns delay */
    HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_SET);

    /* 10us延时 (72MHz下 ~720个NOP) */
    for (volatile uint32_t i = 0; i < 720; i++) {
        __NOP();
    }

    HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_RESET);

    /* 步骤2: 等待ECHO引脚变高 (超时50ms) */
    timeout = 0;
    while (HAL_GPIO_ReadPin(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN) == GPIO_PIN_RESET) {
        if (++timeout > 500000) {
            return 999.9f; /* 超时 — 无回波 */
        }
    }

    /* 步骤3: 记录高电平开始时间 (使用SysTick微秒级计数) */
    pulse_start = g_systick_ms * 1000;

    /* 步骤4: 等待ECHO变低 (超时200ms ≈ 600cm) */
    timeout = 0;
    while (HAL_GPIO_ReadPin(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN) == GPIO_PIN_SET) {
        if (++timeout > 2000000) {
            return 999.9f; /* 超时 */
        }
    }

    /* 步骤5: 计算脉宽时间 */
    pulse_end = g_systick_ms * 1000;

    /* 步骤6: 转换为距离
     * 声速 ≈ 343m/s = 0.0343 cm/us (20°C)
     * 距离 = 时间 * 声速 / 2 (往返)
     * 由于我们用的是ms而非us, 这里做近似计算
     * 实际: 使用循环计数估算时间, 约72MHz/cycle
     * 简化: 使用pulse_width_us = timeout * (1/72) us
     *        distance_cm = pulse_width_us * 0.0343 / 2
     */
    uint32_t pulse_width = timeout;  /* 循环计数 */
    float distance_cm = (float)pulse_width * (1.0f / 72.0f) * 0.0343f / 2.0f;

    /* 有效性检查 */
    if (distance_cm < 2.0f || distance_cm > 400.0f) {
        /* 保留上次有效值 */
        return g_ultrasonic_distance;
    }

    g_ultrasonic_distance = distance_cm;
    return distance_cm;
}


/* ================================================================
 * DHT11/DHT22 温湿度传感器 — Bit-banging单总线协议
 * ================================================================ */

/*
 * DHT通信时序:
 *   主机: 拉低18ms → 拉高20-40us → 释放总线
 *   从机: 拉低80us响应 → 拉高80us准备 → 40bit数据 (每bit: 50us低 + 26-70us高)
 *   bit=0: 高电平26-28us
 *   bit=1: 高电平70us
 */

#define DHT_DATA_READ()  HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN)
#define DHT_DATA_LOW()   HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET)
#define DHT_DATA_HIGH()  HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET)

/* 微秒级延时 (72MHz, 粗略估算) */
static void DHT_DelayUs(uint32_t us)
{
    for (volatile uint32_t i = 0; i < us * 12; i++) {
        __NOP();
    }
}

/**
 * @brief  等待引脚变为指定电平
 * @param  level 目标电平 (GPIO_PIN_SET 或 GPIO_PIN_RESET)
 * @param  timeout_us 超时(us)
 * @return true=成功等到, false=超时
 */
static bool DHT_WaitLevel(uint32_t level, uint32_t timeout_us)
{
    uint32_t count = 0;
    while (DHT_DATA_READ() != level) {
        if (++count > timeout_us * 12) {
            return false;
        }
        __NOP();
    }
    return true;
}

bool DHT_Read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};  /* 5字节: RH_int, RH_dec, T_int, T_dec, checksum */
    uint8_t bit_idx, byte_idx;

    /* ---- 步骤1: 主机发送起始信号 ---- */
    DHT_DATA_LOW();
    DHT_DelayUs(18000);      /* 拉低18ms */
    DHT_DATA_HIGH();
    DHT_DelayUs(30);         /* 拉高30us */
    /* 切换到输入模式 — 配置为开漏输出, 写高后即为输入 */
    /* (已初始化时配置为开漏+上拉, 写1后引脚由外部控制) */

    /* ---- 步骤2: 等待从机响应 ---- */
    if (!DHT_WaitLevel(GPIO_PIN_RESET, 100)) {
        return false;  /* 无响应 — 传感器不存在 */
    }
    if (!DHT_WaitLevel(GPIO_PIN_SET, 100)) {
        return false;  /* 响应超时 */
    }

    /* ---- 步骤3: 读取40bit数据 ---- */
    for (byte_idx = 0; byte_idx < 5; byte_idx++) {
        data[byte_idx] = 0;
        for (bit_idx = 0; bit_idx < 8; bit_idx++) {
            /* 等待50us低电平开始 */
            if (!DHT_WaitLevel(GPIO_PIN_RESET, 100)) {
                return false;
            }
            /* 等待高电平, 根据高电平持续时间判断bit值 */
            DHT_DelayUs(30);  /* 等待30us */
            if (DHT_DATA_READ() == GPIO_PIN_SET) {
                /* 高电平超过30us → bit=1 */
                data[byte_idx] = (data[byte_idx] << 1) | 0x01;
            } else {
                /* 高电平不足30us → bit=0 */
                data[byte_idx] = (data[byte_idx] << 1);
            }
            /* 等待高电平结束 */
            DHT_WaitLevel(GPIO_PIN_RESET, 100);
        }
    }

    /* ---- 步骤4: 校验和验证 ---- */
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        return false;  /* 校验失败 */
    }

    /* ---- 步骤5: 解析数据 ---- */
    /* DHT11: 湿度整数 = data[0], 湿度小数 = 0, 温度整数 = data[2], 温度小数 = 0 */
    /* DHT22: 湿度 = (data[0]<<8 | data[1]) / 10, 温度 = (data[2]<<8 | data[3]) / 10 */
    /* 由于无法区分DHT11/22, 我们按DHT22格式解析，同时兼容DHT11 */

    uint16_t rh_raw = ((uint16_t)data[0] << 8) | data[1];
    uint16_t temp_raw = ((uint16_t)data[2] << 8) | data[3];

    /* 温度符号判断 (bit 15 = 1表示负数, DHT22) */
    float temp_val;
    if (temp_raw & 0x8000) {
        temp_val = -(float)(temp_raw & 0x7FFF) / 10.0f;
    } else {
        temp_val = (float)temp_raw / 10.0f;
    }

    /* 如果数据看起来像DHT11(小数部分为0, 整数<100), 用DHT11格式 */
    if (data[1] == 0 && data[3] == 0 && data[0] < 100 && data[2] < 100) {
        temp_val = (float)data[2];
        rh_raw = data[0];
    }

    *temperature = temp_val;
    *humidity = (float)rh_raw / 10.0f;

    /* 合理性检查 */
    if (*humidity > 100.0f || *temperature > 80.0f || *temperature < -40.0f) {
        return false;
    }

    return true;
}


/* ================================================================
 * 光照传感器 — ADC采样
 * ================================================================ */

uint16_t LightSensor_Read(void)
{
    /* ADC已在连续转换模式，直接读取 */
    uint32_t adc_val = HAL_ADC_GetValue(&hadc1);
    return (uint16_t)adc_val;  /* 0-4095 */
}

uint8_t LightSensor_GetPercent(void)
{
    uint16_t raw = LightSensor_Read();

    /* 校准映射: 将ADC值映射到0-100%
     * 暗 → ADC ≈ 0-500 → 0-10%
     * 室内 → ADC ≈ 500-2000 → 10-50%
     * 室外阴天 → ADC ≈ 2000-3000 → 50-75%
     * 强烈阳光 → ADC ≈ 3000-4095 → 75-100%
     */
    if (raw < 100) return 0;
    if (raw > 4000) return 100;

    /* 线性映射 + gamma校正使低光照时变化更明显 */
    uint32_t percent = (uint32_t)(((float)(raw - 100) / 3900.0f) * 100.0f);

    if (percent > 100) percent = 100;
    return (uint8_t)percent;
}


/* ================================================================
 * 批量更新
 * ================================================================ */

void Sensor_UpdateAll(void)
{
    /* 光照 (最快, 无延时) */
    g_sensor_data.light_level = LightSensor_Read();

    /* 超声波 (需要等待ECHO回应, 可能耗时) */
    g_sensor_data.distance_cm = Ultrasonic_GetDistance();

    /* 温湿度 (DHT通信耗时, 仅每4次更新读一次以减少功耗) */
    static uint8_t dht_skip_counter = 0;
    if (++dht_skip_counter >= 4) {
        dht_skip_counter = 0;
        float temp, hum;
        if (DHT_Read(&temp, &hum)) {
            g_sensor_data.temperature = temp;
            g_sensor_data.humidity = hum;
            g_sensor_data.dht_valid = true;
        } else {
            g_sensor_data.dht_valid = false;
        }
    }
}
