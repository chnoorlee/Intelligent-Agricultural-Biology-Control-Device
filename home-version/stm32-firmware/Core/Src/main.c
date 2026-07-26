/**
 * @file    main.c
 * @brief   STM32 智能农业驱离装置 — 主控程序
 * @author  BioControl Team
 * @version 2.0.0
 *
 * 硬件平台: STM32F103C8T6 (Blue Pill) @ 72MHz
 * 功能概要:
 *   1. 通过USART1接收上位机(YOLO检测结果)的串口指令
 *   2. 根据指令控制蜂鸣器、爆闪灯、超声波等驱离设备
 *   3. 采集温湿度、光照、超声波距离传感器数据
 *   4. 周期性发送传感器数据和设备状态到上位机
 *   5. 看门狗 + 故障检测确保设备稳定运行
 *
 * 引脚分配:
 *   PA0  - TIM2_CH1 PWM → 无源蜂鸣器
 *   PA1  - 推挽输出 → 光耦 → 继电器 → 爆闪LED
 *   PA4  - ADC1_IN4 → 光敏电阻
 *   PA9  - USART1_TX → 上位机通信
 *   PA10 - USART1_RX → 上位机通信
 *   PB0  - 推挽输出 → HC-SR04 Trig
 *   PB1  - 浮空输入 → HC-SR04 Echo
 *   PC0  - 开漏输出/输入 → DHT11/DHT22
 *   PC13 - 推挽输出 → 板载LED(心跳)
 */

#include "main.h"
#include "buzzer.h"
#include "strobe.h"
#include "serial_comm.h"
#include "sensor.h"

/* 全局变量 */
DeviceState g_device_state = {0};
SensorData g_sensor_data = {0};
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2;
ADC_HandleTypeDef hadc1;

/* SysTick 中断计数 (用于非阻塞延时) */
volatile uint32_t g_systick_ms = 0;
volatile uint32_t g_systick_10ms = 0;
volatile uint32_t g_systick_1s = 0;

/* IWDG 看门狗句柄 */
IWDG_HandleTypeDef hiwdg;

/* 局部函数声明 */
static void IWDG_Init(void);
static void LED_Heartbeat(void);
static void System_Error(uint8_t error_code);


/* ================================================================
 * 系统初始化
 * ================================================================ */

/**
 * @brief  HAL库初始化
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /* HSE 8MHz → PLL x9 → 72MHz SYSCLK */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* AHB=72MHz, APB1=36MHz, APB2=72MHz */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                   RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 |
                                   RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

    /* ADC时钟预分频 */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6; /* 12MHz */
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

/**
 * @brief  GPIO初始化
 */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PA1 - 爆闪灯输出 (推挽) */
    GPIO_InitStruct.Pin = STROBE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(STROBE_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(STROBE_PORT, STROBE_PIN, GPIO_PIN_RESET);

    /* PB0 - 超声波Trig */
    GPIO_InitStruct.Pin = ULTRASONIC_TRIG_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ULTRASONIC_TRIG_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_RESET);

    /* PB1 - 超声波Echo (输入) */
    GPIO_InitStruct.Pin = ULTRASONIC_ECHO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(ULTRASONIC_ECHO_PORT, &GPIO_InitStruct);

    /* PC0 - DHT数据 (开漏, 用于双向通信) */
    GPIO_InitStruct.Pin = DHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);

    /* PC13 - 板载LED */
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET); /* 低电平点亮 */
}

/**
 * @brief  USART1初始化 (与上位机通信)
 */
void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = CMD_UART_BAUDRATE;
    huart1.Init.WordLength = CMD_UART_WORDLENGTH;
    huart1.Init.StopBits = CMD_UART_STOPBITS;
    huart1.Init.Parity = CMD_UART_PARITY;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/**
 * @brief  TIM2初始化 (蜂鸣器PWM + 1ms时基)
 */
void MX_TIM2_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 72 - 1;      /* 72MHz / 72 = 1MHz → 1us tick */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000 - 1;        /* 1KHz默认, ARR在Buzzer_On中更改 */
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim2);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);

    /* PWM通道1配置 */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 500;               /* 50%占空比 */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1);

    /* 启动PWM输出 */
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

/**
 * @brief  ADC1初始化 (光照传感器)
 */
void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&hadc1);

    /* 配置通道4 (PA4) */
    sConfig.Channel = LIGHT_SENSOR_CHANNEL;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    /* 启动ADC连续转换 */
    HAL_ADC_Start(&hadc1);
}

/**
 * @brief  看门狗初始化 (4秒超时)
 * @note   如果主循环卡死超过4秒，系统自动复位
 */
static void IWDG_Init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;   /* 40KHz / 64 = 625Hz */
    hiwdg.Init.Reload = 2500;                    /* 2500 / 625 = 4秒 */
    HAL_IWDG_Init(&hiwdg);
}

/**
 * @brief  看门狗喂狗
 */
static inline void IWDG_Feed(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}


/* ================================================================
 * 系统错误处理
 * ================================================================ */

void Error_Handler(void)
{
    __disable_irq();

    /* LED快闪表示错误 */
    while (1)
    {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        for (volatile uint32_t i = 0; i < 1000000; i++) {
            __NOP();
        }
    }
}

static void System_Error(uint8_t error_code)
{
    g_device_state.error_code = error_code;

    /* 停止所有输出 */
    Buzzer_Off();
    Strobe_Off();

    /* LED快闪错误码 */
    for (int i = 0; i < error_code; i++) {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
    }
    HAL_Delay(1000);
}


/* ================================================================
 * LED心跳指示
 * ================================================================ */

static void LED_Heartbeat(void)
{
    static uint32_t last_toggle = 0;
    uint32_t now = g_systick_1s;

    if (g_device_state.error_code != 0) {
        return; /* 错误状态由Error处理 */
    }

    if (now - last_toggle >= 1) {
        last_toggle = now;

        if (g_device_state.buzzer_active || g_device_state.strobe_active) {
            /* 驱离状态: 快闪 */
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        } else {
            /* 空闲状态: 慢闪(1Hz) */
            HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        }
    }
}


/* ================================================================
 * SysTick中断 (1ms)
 * ================================================================ */

void SysTick_Handler(void)
{
    g_systick_ms++;

    /* 10ms时基 */
    static uint32_t cnt_10ms = 0;
    if (++cnt_10ms >= 10) {
        cnt_10ms = 0;
        g_systick_10ms++;
    }

    /* 1s时基 */
    static uint32_t cnt_1s = 0;
    if (++cnt_1s >= 1000) {
        cnt_1s = 0;
        g_systick_1s++;
    }

    HAL_IncTick();
}


/* ================================================================
 * USART1 中断处理 (接收上位机命令)
 * ================================================================ */

void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart1.Instance->DR & 0xFF);
        SerialComm_ProcessByte(byte);
    }
    HAL_UART_IRQHandler(&huart1);
}


/* ================================================================
 * 主函数
 * ================================================================ */

int main(void)
{
    /* HAL库初始化 */
    HAL_Init();

    /* 系统时钟配置 */
    SystemClock_Config();

    /* 外设初始化 */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_TIM2_Init();
    MX_ADC1_Init();

    /* 子系统初始化 */
    Buzzer_Init();
    Strobe_Init();
    SerialComm_Init();
    Sensor_Init();

    /* 看门狗初始化 */
    IWDG_Init();

    /* 启用USART1中断 */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);

    /* 启动信号：LED闪烁3次 */
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
    }

    /* 初始化完成，设置上电时间 */
    g_device_state.uptime_ms = 0;

    /* ============================================================
     * 主循环
     * ============================================================ */
    while (1)
    {
        /* ---- 喂看门狗 ---- */
        IWDG_Feed();

        /* ---- 更新系统运行时间 ---- */
        g_device_state.uptime_ms = g_systick_ms;

        /* ---- 更新驱离设备状态 ---- */
        Buzzer_Update();
        Strobe_Update();

        /* ---- 更新串口通信 ---- */
        SerialComm_Update();

        /* ---- 传感器数据采样 (每2秒) ---- */
        static uint32_t last_sensor_update = 0;
        if (g_systick_1s - last_sensor_update >= 2) {
            last_sensor_update = g_systick_1s;
            Sensor_UpdateAll();

            /* 周期性发送传感器数据到上位机 */
            SerialComm_SendSensorData();
        }

        /* ---- LED心跳 ---- */
        LED_Heartbeat();

        /* ---- 检测超声波如有物体靠近 (>50cm认为是鸟类), 自动启动声光驱离 ---- */
        static uint32_t last_auto_deter = 0;
        if (g_sensor_data.distance_cm < 50.0f &&
            g_sensor_data.distance_cm > 2.0f &&
            g_systick_1s - last_auto_deter >= 5)  /* 最低5s冷却 */
        {
            last_auto_deter = g_systick_1s;

            /* 自动驱离模式 (可作为上位机失效后的备份) */
            if (!g_device_state.buzzer_active && !g_device_state.strobe_active) {
                /* 温和驱离: 2KHz蜂鸣 + 3Hz闪光, 持续3秒 */
                Buzzer_On(2000, 3000);
                Strobe_On(3, 3000);
            }
        }
    }
}
