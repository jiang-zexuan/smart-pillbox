/**
 * @file    protocol.h
 * @brief   蓝牙通信协议解析 — 头文件
 *
 * 协议格式（逗号分隔，\r\n 结尾）：
 *   APP → MCU:
 *     PLAN:slot,hh,mm,period,en    设置用药计划
 *     SET:slot,medname             设置药仓药品
 *     SYNC:ALL                     请求全量同步
 *   MCU → APP:
 *     STATUS:slots|plans           全量状态
 *     ALERT:slot,hh,mm,name       漏服报警
 *     REC:timestamp,slot,taken    服药记录
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "main.h"

/* 命令类型 */
typedef enum {
    CMD_NONE = 0,
    CMD_PLAN,      /* PLAN:slot,hh,mm,period,en */
    CMD_SET,       /* SET:slot,medname */
    CMD_SYNC       /* SYNC:ALL */
} CommandType;

/**
 * @brief 解析收到的蓝牙数据行
 *        在大循环中调用 BLE_ReadLine() 获取一行 → 调此函数
 * @param line  一行文本（不含 \r\n）
 */
void Protocol_ParseLine(const char *line);

/**
 * @brief 解析一条 PLAN 命令
 * @param payload "slot,hh,mm,period,en"
 */
void Protocol_HandlePlan(const char *payload);

/**
 * @brief 解析一条 SET 命令
 * @param payload "slot,medname"
 */
void Protocol_HandleSet(const char *payload);

#endif /* __PROTOCOL_H */
