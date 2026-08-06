/**
 * @file    oled.c
 * @brief   SSD1306 128x64 OLED I²C 驱动 — 实现
 *
 * 基于 SSD1306 数据手册的初始化序列和命令集
 * I²C1 总线（100kHz 标准模式），与 DS3231 RTC 共用
 *
 * GDDRAM 布局（128×64）：
 *   Page 0: rows 0–7,   cols 0–127
 *   Page 1: rows 8–15,  cols 0–127
 *   ...
 *   Page 7: rows 56–63, cols 0–127
 *
 * 每个字节 = 一列中 8 个垂直像素，LSB 在上
 */

#include "main.h"
#include "pinmap.h"
#include "oled.h"
#include <stdarg.h>

/* ==================== I²C 句柄 ==================== */
static I2C_HandleTypeDef oled_i2c;
static uint8_t oled_addr = 0;  /* 7-bit 地址 (0x3C or 0x3D) */
static uint8_t oled_cur_page = 0;
static uint8_t oled_cur_col  = 0;

/* ==================== 5×8 字体 (ASCII 0x20–0x7F) ==================== */
static const uint8_t font5x8[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   空格 */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x00,0x08,0x14,0x22,0x41}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x41,0x22,0x14,0x08,0x00}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x01,0x01}, /* F */
    {0x3E,0x41,0x41,0x51,0x32}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x41,0x41,0x7F,0x00,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x08,0x14,0x54,0x54,0x3C}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x00,0x7F,0x10,0x28,0x44}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* ~ (→) */
    {0x08,0x1C,0x2A,0x08,0x08}, /* ← */
};

/* ==================== 底层 I²C 写入 ==================== */

/**
 * @brief 写命令字节到 SSD1306
 */
static void oled_write_cmd(uint8_t cmd)
{
    HAL_I2C_Mem_Write(&oled_i2c, oled_addr, 0x00,
                      I2C_MEMADD_SIZE_8BIT, &cmd, 1, 50);
}

/**
 * @brief 写多个命令字节
 */
static void oled_write_cmds(const uint8_t *cmds, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        oled_write_cmd(cmds[i]);
    }
}

/**
 * @brief 写数据字节到 GDDRAM
 */
static void oled_write_data(uint8_t data)
{
    HAL_I2C_Mem_Write(&oled_i2c, oled_addr, 0x40,
                      I2C_MEMADD_SIZE_8BIT, &data, 1, 50);
}

/**
 * @brief 写多个数据字节
 */
static void oled_write_datas(const uint8_t *data, uint16_t len)
{
    HAL_I2C_Mem_Write(&oled_i2c, oled_addr, 0x40,
                      I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, 200);
}

/* ==================== 初始化 ==================== */

uint8_t OLED_Init(void)
{
    /* 确保 GPIO 时钟使能 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* ---- I²C1 Remap：SCL/SDA → PB8/PB9（默认是 PB6/PB7）---- */
    __HAL_AFIO_REMAP_I2C1_ENABLE();

    /* 配置 I²C1 GPIO（PB8 SCL, PB9 SDA），可能已由 RTC_Init 配置 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_AF_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = RTC_SCL_PIN;
    HAL_GPIO_Init(RTC_SCL_PORT, &gpio);

    gpio.Pin = RTC_SDA_PIN;
    HAL_GPIO_Init(RTC_SDA_PORT, &gpio);

    /* I²C1 配置 */
    oled_i2c.Instance             = I2C1;
    oled_i2c.Init.ClockSpeed      = 100000;
    oled_i2c.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    oled_i2c.Init.OwnAddress1     = 0;
    oled_i2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    oled_i2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    oled_i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    oled_i2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&oled_i2c);

    /* 扫描 OLED 地址 */
    uint8_t dummy;
    uint8_t addrs[] = {0x78, 0x7A};  /* 0x3C<<1, 0x3D<<1 */
    oled_addr = 0;

    for (int i = 0; i < 2; i++) {
        if (HAL_I2C_Master_Receive(&oled_i2c, addrs[i], &dummy, 1, 20) == HAL_OK) {
            oled_addr = addrs[i];
            break;
        }
    }
    if (oled_addr == 0) {
        return 0;  /* 未找到 OLED */
    }

    /* ---- SSD1306 初始化序列 ---- */
    HAL_Delay(10);  /* 等待 VDD 稳定 */

    static const uint8_t init_cmds[] = {
        0xAE,       /* 关闭显示 */
        0xD5, 0x80, /* 设置 OSC 频率 */
        0xA8, 0x3F, /* 设置 MUX 比例 = 63（64行） */
        0xD3, 0x00, /* 设置显示偏移 = 0 */
        0x40,       /* 设置起始行 = 0 */
        0x8D, 0x14, /* 启用电荷泵 */
        0x20, 0x00, /* 水平寻址模式 */
        0xA1,       /* 段重映射（左右翻转） */
        0xC8,       /* COM 扫描方向（上下翻转） */
        0xDA, 0x12, /* COM 硬件配置 */
        0x81, 0xCF, /* 对比度 */
        0xD9, 0xF1, /* 预充电周期 */
        0xDB, 0x40, /* VCOMH 取消选择 */
        0xA4,       /* 全屏恢复（非全亮） */
        0xA6,       /* 正常显示（非反色） */
        0x2E,       /* 停止滚动 */
        0xAF,       /* 开启显示 */
    };
    oled_write_cmds(init_cmds, sizeof(init_cmds));
    HAL_Delay(50);

    OLED_Clear();
    return oled_addr >> 1;  /* 返回 7-bit 地址(0x3C/0x3D) */
}

/* ==================== 清屏 & 填充 ==================== */

void OLED_Clear(void)
{
    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        oled_write_cmd(0xB0 | page);  /* 设置页地址 */
        oled_write_cmd(0x00);         /* 列低 4 位 */
        oled_write_cmd(0x10);         /* 列高 4 位 */
        for (uint16_t col = 0; col < OLED_WIDTH; col++) {
            oled_write_data(0x00);
        }
    }
    oled_cur_page = 0;
    oled_cur_col  = 0;
}

void OLED_Fill(uint8_t pattern)
{
    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        oled_write_cmd(0xB0 | page);
        oled_write_cmd(0x00);
        oled_write_cmd(0x10);
        for (uint16_t col = 0; col < OLED_WIDTH; col++) {
            oled_write_data(pattern);
        }
    }
    oled_cur_page = 0;
    oled_cur_col  = 0;
}

/* ==================== 光标定位 ==================== */

void OLED_SetCursor(uint8_t page, uint8_t col)
{
    if (page >= OLED_PAGES) page = OLED_PAGES - 1;
    if (col  >= OLED_WIDTH) col  = OLED_WIDTH - 1;
    oled_cur_page = page;
    oled_cur_col  = col;
    oled_write_cmd(0xB0 | page);
    oled_write_cmd(col & 0x0F);
    oled_write_cmd(0x10 | (col >> 4));
}

/* ==================== 像素操作 ==================== */

void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t on)
{
    /* 需要读-改-写操作，简化为使用内部缓冲区。
       由于 RAM 紧张（F103C8 只有 20KB），不使用帧缓冲，
       改为逐页写入方式。
       这里提供一个简化实现：此函数不做实际硬件操作，
       用户应使用 OLED_PutChar / OLED_PutString 等高级接口。 */
    (void)x; (void)y; (void)on;
}

/* ==================== 字符 & 字符串 ==================== */

void OLED_PutChar(char ch)
{
    if (ch < 0x20 || ch > 0x7F) ch = '?'; /* 不可打印字符 */
    uint8_t idx = (uint8_t)(ch - 0x20);

    /* 检查是否需要换行 */
    if (oled_cur_col + 6 > OLED_WIDTH) {
        /* 换到下一页 */
        OLED_SetCursor(oled_cur_page + 1, 0);
    }

    /* 写 5 列字体数据 + 1 列间距 */
    for (uint8_t i = 0; i < 5; i++) {
        oled_write_data(font5x8[idx][i]);
    }
    oled_write_data(0x00);  /* 列间距 */

    oled_cur_col += 6;
}

void OLED_PutString(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            /* 换行到下一页开头 */
            OLED_SetCursor(oled_cur_page + 1, 0);
            s++;
            continue;
        }
        if (*s == '\r') {
            /* 回车到当前行开头 */
            OLED_SetCursor(oled_cur_page, 0);
            s++;
            continue;
        }
        OLED_PutChar(*s++);
    }
}

void OLED_Printf(const char *fmt, ...)
{
    char buf[128];  /* 128 字节足够显示 21 列 */
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OLED_PutString(buf);
}

/* ==================== 简单图形 ==================== */

void OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t w)
{
    if (y >= OLED_HEIGHT) return;
    uint8_t page = y / 8;
    uint8_t bit  = y % 8;

    OLED_SetCursor(page, x);
    for (uint8_t i = 0; i < w && (x + i) < OLED_WIDTH; i++) {
        oled_write_data(1 << bit);
    }
}

void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    for (uint8_t row = y; row < y + h && row < OLED_HEIGHT; row++) {
        OLED_DrawHLine(x, row, w);
    }
}

/* ==================== 调试信息 ==================== */

void OLED_DebugLine(uint8_t line, const char *text)
{
    if (line >= 8) return;
    OLED_SetCursor(line, 0);
    /* 每行最多 21 个字符（128 / 6），先清空该行 */
    for (uint8_t i = 0; i < 21; i++) {
        oled_write_data(0x00);
        oled_write_data(0x00);
        oled_write_data(0x00);
        oled_write_data(0x00);
        oled_write_data(0x00);
        oled_write_data(0x00);
    }
    OLED_SetCursor(line, 0);
    OLED_PutString(text);
}
