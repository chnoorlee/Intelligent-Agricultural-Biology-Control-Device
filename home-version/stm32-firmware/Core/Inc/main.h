/**
 * @file    main.h
 * @brief   STM32智能农业驱离装置 — 主控头文件
 * @author  BioControl Team
 * @version 2.0.0
 * @date    2024
 *
 * 硬件平台: STM32F103C8T6 (Blue Pill)
 * 功能: 接收上位机串口指令，控制蜂鸣器/爆闪灯/超声波等驱离设备
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes -----------------------------------------------*/
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 板载外设定义 ------------------------------------------*/

/* LED 引脚 (PC13 板载LED) */
#define LED_PORT            GPIOC
#define LED_PIN             GPIO_PIN_13

/* 蜂鸣器引脚 (PA0 - TIM2_CH1 PWM输出) */
#define BUZZER_PORT         GPIOA
#define BUZZER_PIN          GPIO_PIN_0
#define BUZZER_TIM          TIM2
#define BUZZER_TIM_CHANNEL  TIM_CHANNEL_1

/* 爆闪灯控制引脚 (PA1 - 高电平触发继电器模块) */
#define STROBE_PORT         GPIOA
#define STROBE_PIN          GPIO_PIN_1

/* 超声波模块引脚 */
#define ULTRASONIC_TRIG_PORT    GPIOB
#define ULTRASONIC_TRIG_PIN     GPIO_PIN_0
#define ULTRASONIC_ECHO_PORT    GPIOB
#define ULTRASONIC_ECHO_PIN     GPIO_PIN_1

/* 温湿度传感器 DHT11/DHT22 (PC0) */
#define DHT_PORT            GPIOC
#define DHT_PIN             GPIO_PIN_0

/* 光敏传感器 (PA4 - ADC1_IN4) */
#define LIGHT_SENSOR_ADC    ADC1
#define LIGHT_SENSOR_CHANNEL ADC_CHANNEL_4

/* 扩展接口 (SPI/I2C预留) */
#define I2C_SCL_PORT        GPIOB
#define I2C_SCL_PIN         GPIO_PIN_6
#define I2C_SDA_PORT        GPIOB
#define I2C_SDA_PIN         GPIO_PIN_7

/* UART配置 (与上位机通信) */
#define CMD_UART            USART1
#define CMD_UART_BAUDRATE   115200
#define CMD_UART_WORDLENGTH UART_WORDLENGTH_8B
#define CMD_UART_STOPBITS   UART_STOPBITS_1
#define CMD_UART_PARITY     UART_PARITY_NONE

/* 协议常量 */
#define FRAME_HEADER        0xAA  /* 帧头 */
#define FRAME_FOOTER        0xBB  /* 帧尾 */
#define CMD_BUF_SIZE        64    /* 命令接收缓冲区大小 */

/* 命令类型定义 */
typedef enum {
    CMD_BUZZER_ON       = 0x01,  /* 启动蜂鸣器 */
    CMD_BUZZER_OFF      = 0x02,  /* 停止蜂鸣器 */
    CMD_STROBE_ON       = 0x03,  /* 启动爆闪灯 */
    CMD_STROBE_OFF      = 0x04,  /* 停止爆闪灯 */
    CMD_FULL_DETER      = 0x05,  /* 紧急驱离(全开) */
    CMD_STOP_ALL        = 0x06,  /* 停止全部 */
    CMD_PING            = 0x07,  /* 心跳检测 */
    CMD_ULTRASONIC_ON   = 0x08,  /* 启动超声波 */
    CMD_ULTRASONIC_OFF  = 0x09,  /* 关闭超声波 */
    CMD_GET_SENSORS     = 0x0A,  /* 查询传感器数据 */
    CMD_SET_LED         = 0x0B,  /* 设置LED状态 */
    CMD_RESET           = 0xFF,  /* 软件复位 */
} CommandType;

/* 设备状态结构体 */
typedef struct {
    bool buzzer_active;
    bool strobe_active;
    bool ultrasonic_active;
    uint16_t buzzer_freq_hz;
    uint32_t buzzer_end_tick;
    uint16_t strobe_flash_freq;
    uint32_t strobe_end_tick;
    uint32_t ultrasonic_end_tick;
    uint32_t uptime_ms;
    uint8_t error_code;
} DeviceState;

/* 传感器数据 */
typedef struct {
    float temperature;      /* 温度 (°C) */
    float humidity;         /* 湿度 (%) */
    uint16_t light_level;   /* 光照 (ADC值 0-4095) */
    float distance_cm;      /* 超声波距离 (cm) */
    bool dht_valid;         /* 温湿度传感器是否有效 */
} SensorData;

/* 全局变量声明 ------------------------------------------*/
extern DeviceState g_device_state;
extern SensorData g_sensor_data;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim2;

/* 函数声明 -----------------------------------------------*/
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART1_UART_Init(void);
void MX_TIM2_Init(void);
void MX_ADC1_Init(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
