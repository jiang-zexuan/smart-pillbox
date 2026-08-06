/**
 * @file    hmi.c
 * @brief   TJC8048X270_011R 淘晶驰串口屏驱动 — 实现
 *
 * 淘晶驰 X2 系列使用自定义文本协议（带帧尾 0xFF 0xFF 0xFF）
 * 屏幕出厂波特率 115200
 */

#include "main.h"
#include "pinmap.h"
#include "hmi.h"
#include <stdarg.h>

static UART_HandleTypeDef hmi_uart;

/* 接收缓冲区 */
#define HMI_RX_BUF_SIZE 256
static uint8_t  hmi_rx_buf[HMI_RX_BUF_SIZE];
static volatile uint16_t hmi_rx_head = 0;
static volatile uint16_t hmi_rx_tail = 0;

/* ==================== 初始化 ==================== */

void HMI_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    /* PA9 TX, PA10 RX */
    GPIO_InitTypeDef gpio = {0};
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin  = HMI_TX_PIN;
    HAL_GPIO_Init(HMI_TX_PORT, &gpio);

    gpio.Mode = GPIO_MODE_AF_INPUT;
    gpio.Pin  = HMI_RX_PIN;
    HAL_GPIO_Init(HMI_RX_PORT, &gpio);

    /* USART1 */
    hmi_uart.Instance        = HMI_USART;
    hmi_uart.Init.BaudRate   = HMI_BAUDRATE;
    hmi_uart.Init.WordLength = UART_WORDLENGTH_8B;
    hmi_uart.Init.StopBits   = UART_STOPBITS_1;
    hmi_uart.Init.Parity     = UART_PARITY_NONE;
    hmi_uart.Init.Mode       = UART_MODE_TX_RX;
    hmi_uart.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    hmi_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&hmi_uart);

    /* 启用接收中断 */
    __HAL_UART_ENABLE_IT(&hmi_uart, UART_IT_RXNE);

    /* 使能 NVIC 中断通道（优先级低于 BLE，避免抢占）*/
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ==================== 指令发送 ==================== */

/* TJC 帧尾：3 字节 0xFF */
static const uint8_t HMI_FRAME_END[3] = {0xFF, 0xFF, 0xFF};

void HMI_SendCmd(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        HAL_UART_Transmit(&hmi_uart, (uint8_t *)buf, len, 100);
        /* 追加帧尾 */
        HAL_UART_Transmit(&hmi_uart, (uint8_t *)HMI_FRAME_END, 3, 100);
    }
}

void HMI_GotoPage(const char *page_name)
{
    HMI_SendCmd("page %s", page_name);
}

void HMI_SetText(const char *widget_name, const char *text)
{
    HMI_SendCmd("%s.txt=\"%s\"", widget_name, text);
}

void HMI_SetValue(const char *widget_name, int32_t value)
{
    HMI_SendCmd("%s.val=%ld", widget_name, (long)value);
}

/* ==================== 业务逻辑 ==================== */

void HMI_UpdateSlots(void)
{
    for (int i = 0; i < NUM_SLOTS; i++) {
        /* 控件命名约定：slot_name0, slot_name1, ... slot_name5
           slot_status0, ... slot_status5 */
        char wname[16];
        snprintf(wname, sizeof(wname), "slot_name%d", i);
        HMI_SetText(wname, g_slots[i].medicine_name);

        snprintf(wname, sizeof(wname), "slot_status%d", i);
        if (g_slots[i].status == SLOT_EMPTY) {
            HMI_SetText(wname, "空");
        } else {
            HMI_SetText(wname, "有药");
        }
    }
}

void HMI_UpdateTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    HMI_SendCmd("t_time.txt=\"%02u:%02u:%02u\"", hour, minute, second);
}

void HMI_UpdatePlans(void)
{
    static const char *period_names[] = {"早晨", "中午", "晚间"};
    for (int i = 0; i < PERIOD_COUNT; i++) {
        char wname[16];
        snprintf(wname, sizeof(wname), "plan_time%d", i);
        if (g_plans[i].enabled) {
            HMI_SendCmd("%s.txt=\"%s %02u:%02u\"",
                        wname, period_names[i],
                        g_plans[i].hour, g_plans[i].minute);
        } else {
            HMI_SendCmd("%s.txt=\"%s 未设置\"", wname, period_names[i]);
        }
    }
}

/* ==================== 触摸事件处理 ==================== */

void HMI_ProcessInput(void)
{
    /* 检查接收缓冲 */
    uint16_t avail;
    if (hmi_rx_head >= hmi_rx_tail)
        avail = hmi_rx_head - hmi_rx_tail;
    else
        avail = HMI_RX_BUF_SIZE - (hmi_rx_tail - hmi_rx_head);

    if (avail < 4) return;  /* 至少需要 4 字节 */

    /* TJC 触摸事件格式（新协议）：
       0x65 + 页面ID + 控件ID + 事件类型(+ 值) + 0xFF 0xFF 0xFF */
    uint16_t idx = hmi_rx_tail;

    /* 查找帧头 0x65 */
    if (hmi_rx_buf[idx] == 0x65 && avail >= 4) {
        /* 这里可以做简单的触摸事件处理 */
        /* 实际使用中根据控件ID来判断哪个按钮被按下 */
        /* 简化：直接把数据读到 log */
        uint8_t page_id = hmi_rx_buf[(idx + 1) % HMI_RX_BUF_SIZE];
        uint8_t ctrl_id = hmi_rx_buf[(idx + 2) % HMI_RX_BUF_SIZE];
        uint8_t evt_type = hmi_rx_buf[(idx + 3) % HMI_RX_BUF_SIZE];

        /* 消费这些字节 */
        hmi_rx_tail = (hmi_rx_tail + 4) % HMI_RX_BUF_SIZE;

        /* 处理特定控件（示例） */
        /* 按钮 btn_sync: 同步计划到药盒
           btn_calibrate: 校准零位
           具体 page_id/ctrl_id 取决于淘晶驰 Editor 中的分配 */
        (void)page_id;
        (void)ctrl_id;
        (void)evt_type;
    } else {
        /* 跳过非帧头字节 */
        hmi_rx_tail = (hmi_rx_tail + 1) % HMI_RX_BUF_SIZE;
    }
}

/* ==================== USART1 中断回调（在 stm32f1xx_it.c 中调用） ==================== */

void HMI_IRQHandler(uint16_t sr)
{
    if (sr & UART_FLAG_RXNE) {
        uint8_t ch = (uint8_t)(hmi_uart.Instance->DR);
        hmi_rx_buf[hmi_rx_head] = ch;
        hmi_rx_head = (hmi_rx_head + 1) % HMI_RX_BUF_SIZE;
    }

    /* 清除溢出错误 */
    if (sr & (UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE)) {
        __HAL_UART_CLEAR_FLAG(&hmi_uart,
            UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE);
        (void)hmi_uart.Instance->DR;
    }
}
