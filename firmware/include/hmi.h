/**
 * @file    hmi.h
 * @brief   TJC8048X270_011R 淘晶驰串口屏驱动 — 头文件
 *
 * 接口：USART1（PA9 TX / PA10 RX），经 BSS138 电平转换（5V ↔ 3.3V）
 * 波特率：115200bps / 8N1（淘晶驰出厂默认）
 *
 * 屏 4Pin 端子（从上到下）：GND / RX / TX / VCC(5V)
 * 注意：TJC RX ← STM32 TX, TJC TX → STM32 RX
 *
 * 淘晶驰串口指令格式：
 *   指令字符串 + 0xFF 0xFF 0xFF（3 字节帧尾）
 *   例如：page main\xFF\xFF\xFF
 *   文本设置：t0.txt="hello"\xFF\xFF\xFF
 *
 * 触摸事件（屏幕→MCU）：
 *   0x65 + 页面ID(1B) + 控件ID(1B) + 事件类型(1B) + 值(可选)
 *   或旧协议：0xFF 0xFF 0xFF + 按键名 + 值
 */

#ifndef __HMI_H
#define __HMI_H

#include "main.h"

/* 触摸事件类型 */
typedef enum {
    HMI_EVT_PRESS   = 0x01,
    HMI_EVT_RELEASE = 0x00
} HmiEventType;

/**
 * @brief 初始化 USART1 连接 TJC 屏幕
 */
void HMI_Init(void);

/**
 * @brief 发送字符串指令（自动加帧尾 \xFF\xFF\xFF）
 */
void HMI_SendCmd(const char *fmt, ...);

/**
 * @brief 切换页面
 * @param page_name  页面名（如 "main", "plan", "history", "settings"）
 */
void HMI_GotoPage(const char *page_name);

/**
 * @brief 设置文本框内容
 * @param widget_name  控件名（如 "t0", "t_time", "t_ble"）
 * @param text         文本内容
 */
void HMI_SetText(const char *widget_name, const char *text);

/**
 * @brief 设置数值
 */
void HMI_SetValue(const char *widget_name, int32_t value);

/**
 * @brief 更新药仓显示（6个仓）
 */
void HMI_UpdateSlots(void);

/**
 * @brief 更新时间显示
 * @param hour   时
 * @param minute 分
 * @param second 秒
 */
void HMI_UpdateTime(uint8_t hour, uint8_t minute, uint8_t second);

/**
 * @brief 更新用药计划显示
 */
void HMI_UpdatePlans(void);

/**
 * @brief 处理屏幕发来的触摸事件
 *        在主循环中调用
 */
void HMI_ProcessInput(void);

#endif /* __HMI_H */
