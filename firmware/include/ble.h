/**
 * @file    ble.h
 * @brief   JDY-31 BLE 蓝牙模块驱动 — 头文件
 *
 * JDY-31 参数：
 *   - Bluetooth 3.0 SPP (Classic)，非 BLE！
 *   - UART 透传，RFCOMM 通信
 *   - 出厂 9600bps / 8N1
 *   - SPP UUID: 00001101-0000-1000-8000-00805F9B34FB（标准 SPP）
 *   - 出厂名称: JDY-31-SPP
 *   - 出厂配对码: 1234
 *   - AT 指令进入：未连接蓝牙时自动处于 AT 模式（无需 +++ 或 EN 脚）
 *   - 透传模式：手机连接后自动切换，断连后自动回到 AT 模式
 *   - iOS 不支持（iOS 不开放 SPP 协议）
 */

#ifndef __BLE_H
#define __BLE_H

#include "main.h"

/* BLE 连接状态 */
typedef enum {
    BLE_DISCONNECTED = 0,
    BLE_CONNECTED    = 1
} BleConnState;

/* 最大 AT 响应长度 */
#define BLE_AT_BUF_SIZE     128

/* 接收环形缓冲区 */
#define BLE_RX_BUF_SIZE     512

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化蓝牙模块
 *        配置 USART2 9600bps/8N1、使能接收中断
 */
void BLE_Init(void);

/**
 * @brief 发送 AT 指令（阻塞等待响应）
 * @param cmd    AT 指令字符串（不含 \r\n，自动补齐）
 * @param expect 期望的响应关键字（如 "OK"），NULL 则不等
 * @param timeout_ms 超时毫秒
 * @return 0=成功, -1=超时, -2=响应不匹配
 */
int  BLE_SendAT(const char *cmd, const char *expect, uint32_t timeout_ms);

/**
 * @brief 配置 JDY-31（设置名称、透传模式）
 * @return 0=成功
 */
int  BLE_Config(void);

/**
 * @brief 通过蓝牙发送数据到手机 APP
 * @param data  数据字符串（自动追加 \r\n）
 */
void BLE_Send(const char *data);

/**
 * @brief 发送服药记录到 APP
 */
void BLE_SendRecord(const MedicationRecord *rec);

/**
 * @brief 发送漏服报警到 APP
 */
void BLE_SendAlert(uint8_t slot, uint8_t hour, uint8_t minute,
                   const char *med_name);

/**
 * @brief 发送全量状态同步到 APP
 */
void BLE_SendStatus(void);

/**
 * @brief 检查是否有新数据
 * @return 接收缓冲区中可读字节数
 */
uint16_t BLE_Available(void);

/**
 * @brief 读取一个字节（非阻塞）
 * @param ch 输出指针
 * @return 0=有数据, -1=无数据
 */
int  BLE_ReadByte(uint8_t *ch);

/**
 * @brief 读取一行（以 \n 结尾），存入 buf，最多 maxlen-1 字节
 * @return 实际读取字节数，0 表示尚无完整行
 */
uint16_t BLE_ReadLine(char *buf, uint16_t maxlen);

/**
 * @brief 获取蓝牙连接状态
 *  JDY-31 STATE 脚：HIGH=已连接, LOW=未连接
 *  此函数也可通过上层数据超时判断
 */
BleConnState BLE_GetState(void);

/**
 * @brief USART2 中断回调（在 stm32f1xx_it.c 中调用）
 */
void BLE_IRQHandler(uint16_t sr);

#endif /* __BLE_H */
