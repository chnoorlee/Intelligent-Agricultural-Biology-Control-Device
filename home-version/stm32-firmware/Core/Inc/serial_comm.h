/**
 * @file    serial_comm.h
 * @brief   串口通信模块 — 上位机命令解析与响应
 *
 * 协议格式:
 *   [0xAA] [CMD(1B)] [LEN(1B)] [DATA(LEN)] [CHK(1B)] [0xBB]
 *   校验和 = CMD ^ LEN ^ DATA[0] ^ ... ^ DATA[LEN-1]
 */

#ifndef __SERIAL_COMM_H
#define __SERIAL_COMM_H

#include "main.h"

/* 响应码 */
#define RESP_OK             0x00  /* 命令执行成功 */
#define RESP_ERR_CMD        0x01  /* 未知命令 */
#define RESP_ERR_CHK        0x02  /* 校验和错误 */
#define RESP_ERR_BUSY       0x03  /* 设备忙 */
#define RESP_ERR_PARAM      0x04  /* 参数错误 */
#define RESP_ERR_LENGTH     0x05  /* 数据长度错误 */

/**
 * @brief  初始化串口通信
 * @note   配置USART1中断接收, 波特率115200
 */
void SerialComm_Init(void);

/**
 * @brief  处理接收到的字节 (在UART中断/主循环中调用)
 * @param  byte 接收到的字节
 * @note   实现状态机解析协议帧
 */
void SerialComm_ProcessByte(uint8_t byte);

/**
 * @brief  发送响应帧到上位机
 * @param  resp_code 响应码
 * @param  data      附加数据
 * @param  data_len  数据长度
 */
void SerialComm_SendResponse(uint8_t resp_code,
                             const uint8_t *data, uint8_t data_len);

/**
 * @brief  发送传感器数据到上位机
 */
void SerialComm_SendSensorData(void);

/**
 * @brief  串口通信状态更新 (主循环)
 */
void SerialComm_Update(void);

/**
 * @brief  获取接收到的最后一个有效命令
 * @return 命令类型
 */
CommandType SerialComm_GetLastCommand(void);

#endif /* __SERIAL_COMM_H */
