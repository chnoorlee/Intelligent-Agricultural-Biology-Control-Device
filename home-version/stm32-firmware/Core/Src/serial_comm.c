/**
 * @file    serial_comm.c
 * @brief   串口通信协议实现 — 帧解析、命令执行、响应发送
 *
 * 协议格式:
 *   请求帧: [0xAA] [CMD(1B)] [LEN(1B)] [DATA[LEN]] [CHK(1B)] [0xBB]
 *   响应帧: [0xAA] [0x00(RESP)] [LEN(1B)] [RESP_CODE(1B)|DATA] [CHK(1B)] [0xBB]
 *
 * 校验和: CMD ^ LEN ^ DATA全XOR
 *
 * 状态机:
 *   IDLE → (收到0xAA) → GOT_HEADER
 *   GOT_HEADER → 读取CMD → GOT_CMD
 *   GOT_CMD → 读取LEN → GOT_LEN
 *   GOT_LEN → 读取DATA[LEN] → GOT_DATA
 *   GOT_DATA → 读取CHK, 校验 → 读取0xBB
 *   成功 → 执行命令 → 回到IDLE
 *   失败 → 丢弃帧 → 回到IDLE
 */

#include "serial_comm.h"
#include "buzzer.h"
#include "strobe.h"
#include "sensor.h"
#include <string.h>

/* 解析状态机 */
typedef enum {
    PARSE_IDLE = 0,
    PARSE_GOT_HEADER,
    PARSE_GOT_CMD,
    PARSE_GOT_LEN,
    PARSE_GOT_DATA,
    PARSE_WAIT_FOOTER,
} ParseState;

static ParseState g_parse_state = PARSE_IDLE;

/* 接收缓冲区 */
static uint8_t g_rx_cmd;              /* 当前命令类型 */
static uint8_t g_rx_len;              /* 当前数据长度 */
static uint8_t g_rx_data[CMD_BUF_SIZE]; /* 数据缓冲区 */
static uint8_t g_rx_data_idx;          /* 已接收数据字节数 */
static uint8_t g_rx_checksum;          /* 计算中的校验和 */

/* 最后一次有效命令 */
static CommandType g_last_command = CMD_STOP_ALL;

/* 响应缓冲区 */
static uint8_t g_tx_buf[CMD_BUF_SIZE + 6];  /* 最大响应帧大小 */


/* ================================================================
 * 初始化
 * ================================================================ */

void SerialComm_Init(void)
{
    g_parse_state = PARSE_IDLE;
    g_rx_cmd = 0;
    g_rx_len = 0;
    g_rx_data_idx = 0;
    g_rx_checksum = 0;
    memset(g_rx_data, 0, sizeof(g_rx_data));
    memset(g_tx_buf, 0, sizeof(g_tx_buf));

    /* UART硬件初始化在main.c中完成 */
}


/* ================================================================
 * 帧解析
 * ================================================================ */

void SerialComm_ProcessByte(uint8_t byte)
{
    switch (g_parse_state)
    {
    case PARSE_IDLE:
        if (byte == FRAME_HEADER) {
            g_parse_state = PARSE_GOT_HEADER;
            g_rx_checksum = 0;  /* 重置校验和 */
        }
        break;

    case PARSE_GOT_HEADER:
        g_rx_cmd = byte;
        g_rx_checksum = byte;   /* 开始计算校验和 */
        g_parse_state = PARSE_GOT_CMD;
        break;

    case PARSE_GOT_CMD:
        g_rx_len = byte;
        g_rx_checksum ^= byte;
        if (g_rx_len == 0) {
            /* 无数据，直接进入校验 */
            g_parse_state = PARSE_GOT_DATA;
            g_rx_data_idx = 0;
        } else if (g_rx_len > CMD_BUF_SIZE) {
            /* 长度超限，丢弃 */
            g_parse_state = PARSE_IDLE;
        } else {
            g_rx_data_idx = 0;
            g_parse_state = PARSE_GOT_LEN;
        }
        break;

    case PARSE_GOT_LEN:
        g_rx_data[g_rx_data_idx++] = byte;
        g_rx_checksum ^= byte;
        if (g_rx_data_idx >= g_rx_len) {
            g_parse_state = PARSE_GOT_DATA;
        }
        break;

    case PARSE_GOT_DATA:
        /* 校验和验证 */
        if (byte == g_rx_checksum) {
            g_parse_state = PARSE_WAIT_FOOTER;
        } else {
            /* 校验失败 */
            SerialComm_SendResponse(RESP_ERR_CHK, NULL, 0);
            g_parse_state = PARSE_IDLE;
        }
        break;

    case PARSE_WAIT_FOOTER:
        if (byte == FRAME_FOOTER) {
            /* 帧完整，执行命令 */
            SerialComm_ExecuteCommand(g_rx_cmd, g_rx_data, g_rx_len);
        } else {
            /* 帧尾错误 */
            SerialComm_SendResponse(RESP_ERR_CMD, NULL, 0);
        }
        g_parse_state = PARSE_IDLE;
        break;

    default:
        g_parse_state = PARSE_IDLE;
        break;
    }
}


/* ================================================================
 * 命令执行
 * ================================================================ */

static void SerialComm_ExecuteCommand(uint8_t cmd,
                                       const uint8_t *data,
                                       uint8_t data_len)
{
    uint8_t resp_data[4];
    uint8_t resp_len = 0;

    switch ((CommandType)cmd)
    {
    /* ---- 蜂鸣器控制 ---- */
    case CMD_BUZZER_ON:
        if (data_len >= 4) {
            /* 参数: freq_hz(2B big-endian) + duration_ms(2B big-endian) */
            uint16_t freq = ((uint16_t)data[0] << 8) | data[1];
            uint32_t duration = ((uint32_t)data[2] << 8) | data[3];
            Buzzer_On(freq, duration);
            g_last_command = CMD_BUZZER_ON;
            SerialComm_SendResponse(RESP_OK, NULL, 0);
        } else {
            SerialComm_SendResponse(RESP_ERR_PARAM, NULL, 0);
        }
        break;

    case CMD_BUZZER_OFF:
        Buzzer_Off();
        g_last_command = CMD_BUZZER_OFF;
        SerialComm_SendResponse(RESP_OK, NULL, 0);
        break;

    /* ---- 爆闪灯控制 ---- */
    case CMD_STROBE_ON:
        if (data_len >= 3) {
            /* 参数: flash_freq(1B) + duration_ms(2B big-endian) */
            uint8_t freq = data[0];
            uint32_t duration = ((uint32_t)data[1] << 8) | data[2];
            Strobe_On(freq, duration);
            g_last_command = CMD_STROBE_ON;
            SerialComm_SendResponse(RESP_OK, NULL, 0);
        } else {
            SerialComm_SendResponse(RESP_ERR_PARAM, NULL, 0);
        }
        break;

    case CMD_STROBE_OFF:
        Strobe_Off();
        g_last_command = CMD_STROBE_OFF;
        SerialComm_SendResponse(RESP_OK, NULL, 0);
        break;

    /* ---- 紧急驱离 ---- */
    case CMD_FULL_DETER:
        /* 全功率驱离: 4KHz蜂鸣 + 10Hz爆闪, 持续10秒 */
        Buzzer_On(4000, 10000);
        Strobe_On(10, 10000);
        g_last_command = CMD_FULL_DETER;
        SerialComm_SendResponse(RESP_OK, NULL, 0);
        break;

    case CMD_STOP_ALL:
        Buzzer_Off();
        Strobe_Off();
        g_last_command = CMD_STOP_ALL;
        SerialComm_SendResponse(RESP_OK, NULL, 0);
        break;

    /* ---- 心跳 ---- */
    case CMD_PING:
        /* 回应 0xAA 0xBB (简化回应) */
        resp_data[0] = FRAME_HEADER;
        resp_data[1] = FRAME_FOOTER;
        HAL_UART_Transmit(&huart1, resp_data, 2, 100);
        break;

    /* ---- 超声波控制 ---- */
    case CMD_ULTRASONIC_ON:
        g_device_state.ultrasonic_active = true;
        g_device_state.ultrasonic_end_tick = 0xFFFFFFFF;
        g_last_command = CMD_ULTRASONIC_ON;
        SerialComm_SendResponse(RESP_OK, NULL, 0);
        break;

    case CMD_ULTRASONIC_OFF:
        g_device_state.ultrasonic_active = false;
        g_last_command = CMD_ULTRASONIC_OFF;
        SerialComm_SendResponse(RESP_OK, NULL, 0);
        break;

    /* ---- 查询传感器 ---- */
    case CMD_GET_SENSORS:
        SerialComm_SendSensorData();
        break;

    /* ---- 设置LED ---- */
    case CMD_SET_LED:
        if (data_len >= 1) {
            if (data[0]) {
                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET); /* ON */
            } else {
                HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);   /* OFF */
            }
            SerialComm_SendResponse(RESP_OK, NULL, 0);
        } else {
            SerialComm_SendResponse(RESP_ERR_PARAM, NULL, 0);
        }
        break;

    /* ---- 软件复位 ---- */
    case CMD_RESET:
        SerialComm_SendResponse(RESP_OK, NULL, 0);
        HAL_Delay(100);
        NVIC_SystemReset();
        break;

    default:
        SerialComm_SendResponse(RESP_ERR_CMD, NULL, 0);
        break;
    }
}


/* ================================================================
 * 响应发送
 * ================================================================ */

void SerialComm_SendResponse(uint8_t resp_code,
                              const uint8_t *data, uint8_t data_len)
{
    uint8_t frame_len = 0;

    /* 构建响应帧 */
    g_tx_buf[frame_len++] = FRAME_HEADER;
    g_tx_buf[frame_len++] = 0x00;        /* 响应类型 */
    g_tx_buf[frame_len++] = data_len + 1; /* 长度: 响应码+数据 */
    g_tx_buf[frame_len++] = resp_code;    /* 响应码 */

    if (data != NULL && data_len > 0) {
        memcpy(&g_tx_buf[frame_len], data, data_len);
        frame_len += data_len;
    }

    /* 校验和: CMD_TYPE ^ LEN ^ RESP_CODE ^ DATA */
    uint8_t chk = 0x00;
    chk ^= g_tx_buf[2];  /* LEN */
    chk ^= resp_code;
    for (uint8_t i = 0; i < data_len; i++) {
        chk ^= (data != NULL) ? data[i] : 0;
    }
    g_tx_buf[frame_len++] = chk;
    g_tx_buf[frame_len++] = FRAME_FOOTER;

    /* 发送 */
    HAL_UART_Transmit(&huart1, g_tx_buf, frame_len, 100);
}


/* ================================================================
 * 传感器数据上报
 * ================================================================ */

void SerialComm_SendSensorData(void)
{
    /*
     * 传感器数据包格式:
     * [温度_H] [温度_L] [湿度] [光照_H] [光照_L] [距离_H] [距离_L]
     * 温度: int16, 实际值 = value/10  (如 255 -> 25.5°C)
     * 湿度: uint8, 0-100%
     * 光照: uint16, ADC原始值 0-4095
     * 距离: uint16, 实际值 = value/10  (如 1234 -> 123.4cm)
     */
    uint8_t data[7];

    int16_t temp = (int16_t)(g_sensor_data.temperature * 10.0f);
    data[0] = (temp >> 8) & 0xFF;
    data[1] = temp & 0xFF;
    data[2] = (uint8_t)g_sensor_data.humidity;

    uint16_t light = g_sensor_data.light_level;
    data[3] = (light >> 8) & 0xFF;
    data[4] = light & 0xFF;

    uint16_t dist = (uint16_t)(g_sensor_data.distance_cm * 10.0f);
    data[5] = (dist >> 8) & 0xFF;
    data[6] = dist & 0xFF;

    SerialComm_SendResponse(RESP_OK, data, sizeof(data));
}


/* ================================================================
 * 状态更新 & 工具函数
 * ================================================================ */

void SerialComm_Update(void)
{
    /* 在主循环中处理超时等 */
    /* 如果帧解析卡在中间状态超过100ms, 自动重置 */
    static uint32_t last_parse_activity = 0;
    static ParseState last_state = PARSE_IDLE;

    if (g_parse_state != last_state) {
        last_parse_activity = g_systick_ms;
        last_state = g_parse_state;
    }

    if (g_parse_state != PARSE_IDLE &&
        g_systick_ms - last_parse_activity > 100)
    {
        g_parse_state = PARSE_IDLE;
    }
}

CommandType SerialComm_GetLastCommand(void)
{
    return g_last_command;
}
