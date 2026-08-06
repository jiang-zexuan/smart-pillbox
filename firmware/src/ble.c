/**
 * @file    ble.c
 * @brief   JDY-31 BLE 蓝牙模块驱动 — 实现
 *
 * JDY-31 连接 STM32 USART2（PA2 TX / PA3 RX），3.3V 直连
 *
 * 模块 6 脚定义：
 *   ① VCC  (3.3V)
 *   ② GND
 *   ③ TXD  → STM32 PA3 (RX)
 *   ④ RXD  ← STM32 PA2 (TX)
 *   ⑤ STATE → 未用（可接 GPIO 读取连接状态）
 *   ⑥ EN    → 未用（拉高进入 AT 模式，串口 +++ 也可）
 *
 * 关键 AT 指令：
 *   AT+NAME<name>      设置广播名称
 *   AT+BAUD<rate>      设置波特率
 *   AT+RESET           软复位
 *   AT+EXIT            退出 AT 模式
 *   AT+PASSWD<123456>  设置配对密码
 */

#include "main.h"
#include "pinmap.h"
#include "ble.h"
#include "feature_config.h"
#include "debug_uart.h"
#include <stdarg.h>

/* ==================== 接收环形缓冲区 ==================== */
static uint8_t  ble_rx_buf[BLE_RX_BUF_SIZE];
static volatile uint16_t ble_rx_head = 0;
static volatile uint16_t ble_rx_tail = 0;

/* 连接状态 */
static volatile BleConnState ble_state = BLE_DISCONNECTED;
static volatile uint32_t ble_last_rx_tick = 0;  /* 最后收到数据的时间 */

/* AT 响应缓冲区 */
static char ble_at_buf[BLE_AT_BUF_SIZE];
static volatile uint8_t ble_at_ready = 0;

/* USART2 句柄 */
static UART_HandleTypeDef ble_uart;

/* ==================== 初始化 ==================== */

void BLE_Init(void)
{
    /* ---- GPIO 时钟 ---- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* ---- TX: PA2, RX: PA3 ---- */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin   = BLE_TX_PIN;
    HAL_GPIO_Init(BLE_TX_PORT, &gpio);

    gpio.Mode  = GPIO_MODE_AF_INPUT;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Pin   = BLE_RX_PIN;
    HAL_GPIO_Init(BLE_RX_PORT, &gpio);

    /* ---- USART2 配置 ---- */
    ble_uart.Instance        = BLE_USART;
    ble_uart.Init.BaudRate   = BLE_BAUDRATE;
    ble_uart.Init.WordLength = UART_WORDLENGTH_8B;
    ble_uart.Init.StopBits   = UART_STOPBITS_1;
    ble_uart.Init.Parity     = UART_PARITY_NONE;
    ble_uart.Init.Mode       = UART_MODE_TX_RX;
    ble_uart.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    ble_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&ble_uart);

    /* ---- 启用接收中断（单字节）---- */
    __HAL_UART_ENABLE_IT(&ble_uart, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* ==================== AT 指令 ==================== */

int BLE_SendAT(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    /* 发送 AT 指令 */
    char at_line[64];
    int len = snprintf(at_line, sizeof(at_line), "%s\r\n", cmd);
    HAL_UART_Transmit(&ble_uart, (uint8_t *)at_line, len, 100);

    if (expect == NULL) return 0;  /* 不等响应 */

    /* 等待响应 */
    ble_at_ready = 0;
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout_ms) {
        if (ble_at_ready) {
            ble_at_ready = 0;
            if (strstr(ble_at_buf, expect) != NULL)
                return 0;   /* 匹配成功 */
            if (strstr(ble_at_buf, "ERR") != NULL)
                return -2;  /* 模块返回错误 */
        }
    }
    return -1;  /* 超时 */
}

int BLE_Config(void)
{
    /* JDY-31 在未连接蓝牙时自动处于 AT 模式，无需 +++ 或 EN 引脚操作
     * 注意：如果模块已连接蓝牙（STATE 脚高），AT 指令将被透传，不会生效
     * 因此 AT 配置必须在模块未连接时执行（即上电后、手机连接前）
     */

    /* 等待模块稳定 */
    HAL_Delay(500);

    /* 1. 测试 AT 通信
     * JDY-31 没有可靠的裸 AT 应答，资料建议用 AT+VERSION 检查串口通信。
     */
    if (BLE_SendAT("AT+VERSION", "+VERSION", 700) != 0) {
        HAL_Delay(500);
        if (BLE_SendAT("AT+VERSION", "+VERSION", 700) != 0)
            return -1;
    }

    /* 2. 设置广播名称（默认 JDY-31-SPP；参数和指令之间没有等号） */
    BLE_SendAT("AT+NAMESmartPillbox", "OK", 700);

    /* 3. 确认当前波特率 */
    BLE_SendAT("AT+BAUD", "+BAUD", 300);

    /* 注意：不需要发送 AT+EXIT — 当手机连接后模块自动切换为透传模式 */
    return 0;
}

/* ==================== 数据发送 ==================== */

void BLE_Send(const char *data)
{
    HAL_UART_Transmit(&ble_uart, (uint8_t *)data, strlen(data), 200);
    /* JDY-31 透传模式自动在 APP 端以 notify 方式发送 */
}

void BLE_SendRecord(const MedicationRecord *rec)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "REC:%lu,%u,%u\r\n",
             (unsigned long)rec->timestamp, rec->slot_id, rec->taken);
    BLE_Send(buf);
}

void BLE_SendAlert(uint8_t slot, uint8_t hour, uint8_t minute,
                   const char *med_name)
{
    char buf[80];
    snprintf(buf, sizeof(buf), "ALERT:%u,%02u,%02u,%s\r\n",
             slot, hour, minute, med_name);
    BLE_Send(buf);
}

void BLE_SendStatus(void)
{
    char buf[256];
    int pos = 0;

    /* 药仓状态 */
    pos += snprintf(buf + pos, sizeof(buf) - pos, "STATUS:");
    for (int i = 0; i < NUM_SLOTS; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "%u,%s,%u",
                        g_slots[i].slot_id,
                        g_slots[i].medicine_name,
                        g_slots[i].status);
        if (i < NUM_SLOTS - 1) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, ";");
        }
    }
    /* 计划 */
    pos += snprintf(buf + pos, sizeof(buf) - pos, "|");
    for (int i = 0; i < PERIOD_COUNT; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "%u,%02u,%02u,%u,%u",
                        g_plans[i].slot_id, g_plans[i].hour, g_plans[i].minute,
                        g_plans[i].period, g_plans[i].enabled);
        if (i < PERIOD_COUNT - 1) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, ";");
        }
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "\r\n");
    BLE_Send(buf);
}

/* ==================== 数据接收 ==================== */

uint16_t BLE_Available(void)
{
    if (ble_rx_head >= ble_rx_tail)
        return ble_rx_head - ble_rx_tail;
    else
        return BLE_RX_BUF_SIZE - (ble_rx_tail - ble_rx_head);
}

int BLE_ReadByte(uint8_t *ch)
{
    if (BLE_Available() == 0) return -1;
    *ch = ble_rx_buf[ble_rx_tail];
    ble_rx_tail = (ble_rx_tail + 1) % BLE_RX_BUF_SIZE;
    return 0;
}

uint16_t BLE_ReadLine(char *buf, uint16_t maxlen)
{
    uint16_t count = 0;
    uint8_t ch;

    /* 查找 \n */
    uint16_t avail = BLE_Available();
    if (avail == 0) return 0;

    uint16_t idx = ble_rx_tail;
    bool has_newline = false;
    for (uint16_t i = 0; i < avail; i++) {
        if (ble_rx_buf[idx] == '\n') {
            has_newline = true;
            break;
        }
        idx = (idx + 1) % BLE_RX_BUF_SIZE;
    }
    if (!has_newline) return 0;  /* 还没收到完整行 */

    /* 读出直到 \n */
    while (count < maxlen - 1) {
        if (BLE_ReadByte(&ch) != 0) break;
        if (ch == '\r') continue;
        if (ch == '\n') break;
        buf[count++] = (char)ch;
    }
    buf[count] = '\0';

    /* 丢弃此行的剩余字节（如果有） */
    while (BLE_Available() > 0) {
        /* peek */
        if (ble_rx_buf[ble_rx_tail] == '\n') {
            BLE_ReadByte(&ch);  /* 丢弃 \n */
            break;
        }
        BLE_ReadByte(&ch);
    }

    return count;
}

BleConnState BLE_GetState(void)
{
    /* 如果 5 秒内没收到任何数据，认为断连 */
    if (HAL_GetTick() - ble_last_rx_tick > 5000)
        ble_state = BLE_DISCONNECTED;
    return ble_state;
}

/* ==================== 中断处理 ==================== */

void BLE_IRQHandler(uint16_t sr)
{
    if (sr & UART_FLAG_RXNE) {
        uint8_t ch = (uint8_t)(ble_uart.Instance->DR);
#if FEATURE_DEBUG_UART && 0
        if (ch >= 32 && ch <= 126) {
            DebugUart_Printf("[BLE_RX] '%c' 0x%02X\r\n", ch, ch);
        } else {
            DebugUart_Printf("[BLE_RX] 0x%02X\r\n", ch);
        }
#endif
        ble_rx_buf[ble_rx_head] = ch;
        ble_rx_head = (ble_rx_head + 1) % BLE_RX_BUF_SIZE;
        ble_last_rx_tick = HAL_GetTick();
        ble_state = BLE_CONNECTED;

        /* 收集 AT 响应 */
        static uint16_t at_idx = 0;
        if (at_idx < BLE_AT_BUF_SIZE - 1) {
            ble_at_buf[at_idx++] = (char)ch;
            ble_at_buf[at_idx] = '\0';
            if (strstr(ble_at_buf, "OK") != NULL || strstr(ble_at_buf, "ERR") != NULL) {
                at_idx = 0;
                ble_at_ready = 1;
            }
            if (ch == '\n') {
                ble_at_buf[at_idx] = '\0';
                at_idx = 0;
                ble_at_ready = 1;
            }
        } else {
            at_idx = 0;  /* 溢出重置 */
        }
    }

    if (sr & (UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE)) {
        /* 清除溢出/帧错误/噪声标志 */
        __HAL_UART_CLEAR_FLAG(&ble_uart,
            UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE);
        /* 读 DR 清除 ORE */
        (void)ble_uart.Instance->DR;
    }
}
