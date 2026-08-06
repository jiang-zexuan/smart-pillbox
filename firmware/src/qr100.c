/**
 * @file    qr100.c
 * @brief   QR100 条码/二维码扫描模块串口驱动实现
 */

#include "qr100.h"
#include "pinmap.h"
#include "feature_config.h"
#include "debug_uart.h"

static UART_HandleTypeDef qr_uart;
static uint8_t qr_rx_buf[QR100_RX_BUF_SIZE];
static volatile uint16_t qr_rx_head = 0;
static volatile uint16_t qr_rx_tail = 0;
static volatile uint32_t qr_last_rx_tick = 0;

static void QR100_PushByte(uint8_t ch)
{
    uint16_t next = (qr_rx_head + 1) % QR100_RX_BUF_SIZE;
    if (next != qr_rx_tail) {
        qr_rx_buf[qr_rx_head] = ch;
        qr_rx_head = next;
    }
    qr_last_rx_tick = HAL_GetTick();

#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
    if (ch >= 32 && ch <= 126) {
        DebugUart_Printf("[QR_RX] '%c' 0x%02X\r\n", ch, ch);
    } else {
        DebugUart_Printf("[QR_RX] 0x%02X\r\n", ch);
    }
#endif
}

void QR100_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    /* PB10 TX，可选；PB11 RX 必接 QR100 TX */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin  = QR100_TX_PIN;
    HAL_GPIO_Init(QR100_TX_PORT, &gpio);

    gpio.Mode = GPIO_MODE_AF_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin  = QR100_RX_PIN;
    HAL_GPIO_Init(QR100_RX_PORT, &gpio);

    qr_uart.Instance = QR100_USART;
    qr_uart.Init.BaudRate = QR100_BAUDRATE;
    qr_uart.Init.WordLength = UART_WORDLENGTH_8B;
    qr_uart.Init.StopBits = UART_STOPBITS_1;
    qr_uart.Init.Parity = UART_PARITY_NONE;
    qr_uart.Init.Mode = UART_MODE_TX_RX;
    qr_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    qr_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&qr_uart);

    __HAL_UART_ENABLE_IT(&qr_uart, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

void QR100_Poll(void)
{
    while (__HAL_UART_GET_FLAG(&qr_uart, UART_FLAG_RXNE) != RESET) {
        uint8_t ch = (uint8_t)(qr_uart.Instance->DR);
        QR100_PushByte(ch);
    }

    if (__HAL_UART_GET_FLAG(&qr_uart, UART_FLAG_ORE) != RESET ||
        __HAL_UART_GET_FLAG(&qr_uart, UART_FLAG_FE) != RESET ||
        __HAL_UART_GET_FLAG(&qr_uart, UART_FLAG_NE) != RESET) {
        __HAL_UART_CLEAR_FLAG(&qr_uart, UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE);
        (void)qr_uart.Instance->DR;
    }
}

uint16_t QR100_ReadLine(char *buf, uint16_t maxlen)
{
    if (buf == NULL || maxlen == 0) return 0;

    uint16_t avail = (qr_rx_head >= qr_rx_tail)
        ? (qr_rx_head - qr_rx_tail)
        : (QR100_RX_BUF_SIZE - (qr_rx_tail - qr_rx_head));

    if (avail == 0) return 0;

    uint16_t idx = qr_rx_tail;
    bool found_newline = false;
    for (uint16_t i = 0; i < avail; i++) {
        if (qr_rx_buf[idx] == '\n' || qr_rx_buf[idx] == '\r') {
            found_newline = true;
            break;
        }
        idx = (idx + 1) % QR100_RX_BUF_SIZE;
    }
    if (!found_newline) return 0;

    uint16_t out = 0;
    while (qr_rx_tail != qr_rx_head && out < maxlen - 1) {
        uint8_t ch = qr_rx_buf[qr_rx_tail];
        qr_rx_tail = (qr_rx_tail + 1) % QR100_RX_BUF_SIZE;
        if (ch == '\r' || ch == '\n') break;
        buf[out++] = (char)ch;
    }
    buf[out] = '\0';

    while (qr_rx_tail != qr_rx_head) {
        uint8_t ch = qr_rx_buf[qr_rx_tail];
        if (ch != '\r' && ch != '\n') break;
        qr_rx_tail = (qr_rx_tail + 1) % QR100_RX_BUF_SIZE;
    }

    return out;
}

uint16_t QR100_ReadFrame(char *buf, uint16_t maxlen)
{
    if (buf == NULL || maxlen == 0) return 0;

    uint16_t avail = (qr_rx_head >= qr_rx_tail)
        ? (qr_rx_head - qr_rx_tail)
        : (QR100_RX_BUF_SIZE - (qr_rx_tail - qr_rx_head));

    if (avail == 0) return 0;

    /* 优先按 \r/\n 分帧 */
    uint16_t line_len = QR100_ReadLine(buf, maxlen);
    if (line_len > 0) return line_len;

    /* 有些 QR100 配置不会追加回车换行：收完一段数据后静默 80ms 就当作一帧 */
    if ((HAL_GetTick() - qr_last_rx_tick) < 80) return 0;

    uint16_t out = 0;
    while (qr_rx_tail != qr_rx_head && out < maxlen - 1) {
        uint8_t ch = qr_rx_buf[qr_rx_tail];
        qr_rx_tail = (qr_rx_tail + 1) % QR100_RX_BUF_SIZE;
        if (ch == '\r' || ch == '\n') break;
        buf[out++] = (char)ch;
    }
    buf[out] = '\0';
    return out;
}

void QR100_IRQHandler(uint16_t sr)
{
    if (sr & UART_FLAG_RXNE) {
        uint8_t ch = (uint8_t)(qr_uart.Instance->DR);
        QR100_PushByte(ch);
    }

    if (sr & (UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE)) {
        __HAL_UART_CLEAR_FLAG(&qr_uart, UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE);
        (void)qr_uart.Instance->DR;
    }
}
