/**
 * @file    debug_uart.c
 * @brief   USART1 调试输出实现
 */

#include "debug_uart.h"
#include "pinmap.h"
#include <stdarg.h>

static UART_HandleTypeDef dbg_uart;

void DebugUart_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = HMI_TX_PIN;      /* PA9 / USART1_TX */
    HAL_GPIO_Init(HMI_TX_PORT, &gpio);

    gpio.Mode = GPIO_MODE_AF_INPUT;
    gpio.Pin = HMI_RX_PIN;      /* PA10 / USART1_RX */
    HAL_GPIO_Init(HMI_RX_PORT, &gpio);

    dbg_uart.Instance = USART1;
    dbg_uart.Init.BaudRate = 115200;
    dbg_uart.Init.WordLength = UART_WORDLENGTH_8B;
    dbg_uart.Init.StopBits = UART_STOPBITS_1;
    dbg_uart.Init.Parity = UART_PARITY_NONE;
    dbg_uart.Init.Mode = UART_MODE_TX_RX;
    dbg_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    dbg_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&dbg_uart);
}

void DebugUart_Print(const char *s)
{
    if (s == NULL) return;
    HAL_UART_Transmit(&dbg_uart, (uint8_t *)s, strlen(s), 200);
}

void DebugUart_Printf(const char *fmt, ...)
{
    char buf[160];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if (len > (int)sizeof(buf)) len = sizeof(buf);
        HAL_UART_Transmit(&dbg_uart, (uint8_t *)buf, (uint16_t)len, 300);
    }
}
