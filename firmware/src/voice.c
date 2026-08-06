/**
 * @file    voice.c
 * @brief   DY-SV17F 语音播报模块驱动 — 实现
 */

#include "main.h"
#include "pinmap.h"
#include "voice.h"

static UART_HandleTypeDef voice_uart;

/* ==================== 初始化 ==================== */

void Voice_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    /* PB10 TX, PB11 RX */
    GPIO_InitTypeDef gpio = {0};
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin  = VOICE_TX_PIN;
    HAL_GPIO_Init(VOICE_TX_PORT, &gpio);

    gpio.Mode = GPIO_MODE_AF_INPUT;
    gpio.Pin  = VOICE_RX_PIN;
    HAL_GPIO_Init(VOICE_RX_PORT, &gpio);

    voice_uart.Instance        = VOICE_USART;
    voice_uart.Init.BaudRate   = VOICE_BAUDRATE;
    voice_uart.Init.WordLength = UART_WORDLENGTH_8B;
    voice_uart.Init.StopBits   = UART_STOPBITS_1;
    voice_uart.Init.Parity     = UART_PARITY_NONE;
    voice_uart.Init.Mode       = UART_MODE_TX;      /* 只发不收 */
    voice_uart.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    voice_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&voice_uart);
}

/* ==================== 播放控制 ==================== */

void Voice_Play(uint8_t file_id)
{
    /* DY-SV17F 串口指令格式：
       AA 07 02 00 <file_H> <file_L> 00 <checksum>
       文件编号范围 1–65535，这里只用低字节 */

    uint8_t cmd[8];
    cmd[0] = 0xAA;          /* 帧头 */
    cmd[1] = 0x07;          /* 长度（不含帧头和校验）= 7 */
    cmd[2] = 0x02;          /* 命令：播放 */
    cmd[3] = 0x00;          /* 文件编号高字节 */
    cmd[4] = 0x00;          /* 文件编号低字节 — 暂未用（≤255 时只用低字节） */
    cmd[5] = file_id;       /* 文件编号（0-255 → 实际播放 file_id.mp3） */
    cmd[6] = 0x00;          /* 保留 */
    /* 校验：命令+数据各字节累加和的低 8 位 */
    cmd[7] = (cmd[1] + cmd[2] + cmd[3] + cmd[4] + cmd[5] + cmd[6]) & 0xFF;

    HAL_UART_Transmit(&voice_uart, cmd, 8, 100);
}

void Voice_Stop(void)
{
    /* 停止播放指令 */
    uint8_t stop_cmd[5] = {0xAA, 0x02, 0x00, 0x00, 0x02};
    HAL_UART_Transmit(&voice_uart, stop_cmd, 5, 50);
}

void Voice_RemindSlot(uint8_t slot)
{
    /* 药仓 1-3 对应语音 001-003.mp3
       药仓 4-6 也用 001-003（循环复用，改语音文件内容即可） */
    uint8_t file_id = (slot % 3) + 1;
    Voice_Play(file_id);
}
