/**
 * @file    debug_uart.h
 * @brief   USART1 调试输出，串口屏禁用时给 USB-TTL 使用
 */

#ifndef __DEBUG_UART_H
#define __DEBUG_UART_H

#include "main.h"

void DebugUart_Init(void);
void DebugUart_Print(const char *s);
void DebugUart_Printf(const char *fmt, ...);

#endif /* __DEBUG_UART_H */
