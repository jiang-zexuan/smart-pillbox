/**
 * @file    rtc_ds3231.c
 * @brief   DS3231M RTC — I²C 驱动实现
 *
 * DS3231 寄存器地址：
 *   0x00  秒（BCD，bit7=OSF振荡器停止标志）
 *   0x01  分（BCD）
 *   0x02  时（BCD，bit6=12/24模式）
 *   0x03  星期
 *   0x04  日（BCD）
 *   0x05  月（BCD，bit7=世纪）
 *   0x06  年（BCD）
 *   0x07  A1秒
 *   0x08  A1分
 *   0x09  A1时
 *   0x0E  控制寄存器
 *   0x0F  状态寄存器（bit0=A1F闹钟1标志）
 */

#include "main.h"
#include "pinmap.h"
#include "rtc_ds3231.h"

static I2C_HandleTypeDef rtc_i2c;

/* ==================== BCD 转换 ==================== */

uint8_t RTC_BcdToBin(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

uint8_t RTC_BinToBcd(uint8_t bin) {
    return ((bin / 10) << 4) | (bin % 10);
}

/* ==================== 底层读写 ==================== */

static HAL_StatusTypeDef RTC_WriteReg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&rtc_i2c, DS3231_ADDR,
                             reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, 100);
}

static HAL_StatusTypeDef RTC_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return HAL_I2C_Mem_Read(&rtc_i2c, DS3231_ADDR,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, 100);
}

/* ==================== 初始化 ==================== */

int RTC_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* ---- I²C1 Remap：SCL/SDA → PB8/PB9 ---- */
    __HAL_AFIO_REMAP_I2C1_ENABLE();

    /* PB8 SCL, PB9 SDA — 开漏，外接 4.7kΩ 上拉 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_AF_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin   = RTC_SCL_PIN;
    HAL_GPIO_Init(RTC_SCL_PORT, &gpio);

    gpio.Pin   = RTC_SDA_PIN;
    HAL_GPIO_Init(RTC_SDA_PORT, &gpio);

    /* I²C1：标准模式 100kHz */
    rtc_i2c.Instance             = RTC_I2C;
    rtc_i2c.Init.ClockSpeed      = 100000;
    rtc_i2c.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    rtc_i2c.Init.OwnAddress1     = 0;
    rtc_i2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    rtc_i2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    rtc_i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    rtc_i2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&rtc_i2c);

    /* 检测设备 */
    uint8_t dummy;
    if (HAL_I2C_Master_Receive(&rtc_i2c, DS3231_ADDR, &dummy, 1, 50)
        != HAL_OK) {
        return -1;  /* 未检测到 DS3231 */
    }

    /* 清除 OSF 标志 */
    uint8_t sec;
    HAL_I2C_Mem_Read(&rtc_i2c, DS3231_ADDR, 0x00,
                     I2C_MEMADD_SIZE_8BIT, &sec, 1, 50);
    if (sec & 0x80) {
        sec &= 0x7F;  /* 清除 OSF */
        HAL_I2C_Mem_Write(&rtc_i2c, DS3231_ADDR, 0x00,
                          I2C_MEMADD_SIZE_8BIT, &sec, 1, 50);
    }

    return 0;
}

/* ==================== 时间读写 ==================== */

void RTC_GetDateTime(DateTime *dt)
{
    uint8_t buf[7];
    if (RTC_ReadRegs(0x00, buf, 7) != HAL_OK) {
        memset(dt, 0, sizeof(*dt));
        return;
    }

    dt->second = RTC_BcdToBin(buf[0] & 0x7F);
    dt->minute = RTC_BcdToBin(buf[1] & 0x7F);
    dt->hour   = RTC_BcdToBin(buf[2] & 0x3F);  /* 24小时模式 */
    dt->day    = buf[3] & 0x07;
    dt->date   = RTC_BcdToBin(buf[4] & 0x3F);
    dt->month  = RTC_BcdToBin(buf[5] & 0x1F);
    dt->year   = RTC_BcdToBin(buf[6]);
}

void RTC_SetDateTime(const DateTime *dt)
{
    uint8_t buf[7];
    buf[0] = RTC_BinToBcd(dt->second);
    buf[1] = RTC_BinToBcd(dt->minute);
    buf[2] = RTC_BinToBcd(dt->hour);      /* 24小时模式 */
    buf[3] = dt->day;
    buf[4] = RTC_BinToBcd(dt->date);
    buf[5] = RTC_BinToBcd(dt->month);
    buf[6] = RTC_BinToBcd(dt->year);

    HAL_I2C_Mem_Write(&rtc_i2c, DS3231_ADDR, 0x00,
                      I2C_MEMADD_SIZE_8BIT, buf, 7, 100);
}

/* ==================== Unix 时间戳 ==================== */

uint32_t RTC_GetTimestamp(void)
{
    DateTime dt;
    RTC_GetDateTime(&dt);

    /* 简化的 Unix 时间戳计算（2000-2099） */
    /* 月份天数累计表 */
    static const uint16_t month_days[] =
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    uint16_t full_year = 2000 + dt.year;
    uint32_t days = (full_year - 2000) * 365;
    /* 闰年补偿 */
    days += (full_year - 2000 + 3) / 4;
    days += month_days[dt.month - 1];
    /* 当前月超2月且是闰年 */
    if (dt.month > 2 && (full_year % 4 == 0 && full_year % 100 != 0)) {
        days += 1;
    }
    days += dt.date - 1;
    /* 加上 2000-01-01 00:00:00 到 Unix 纪元的秒数 */
    days += 10957;  /* 2000-01-01 = day 10957 */

    return days * 86400UL + dt.hour * 3600UL +
           dt.minute * 60 + dt.second;
}

/* ==================== 闹钟 ==================== */

void RTC_SetAlarm(const AlarmTime *alm)
{
    /* 闹钟 1：匹配时+分 */
    uint8_t sec  = 0x80;  /* bit7=1 忽略秒 */
    uint8_t min  = RTC_BinToBcd(alm->minute);
    uint8_t hour = RTC_BinToBcd(alm->hour);

    /* 清除闹钟使能位 */
    uint8_t ctrl = 0;
    HAL_I2C_Mem_Read(&rtc_i2c, DS3231_ADDR, 0x0E,
                     I2C_MEMADD_SIZE_8BIT, &ctrl, 1, 50);
    ctrl &= ~0x05;  /* 关 A1IE 和 INTCN=0（方波模式） */
    HAL_I2C_Mem_Write(&rtc_i2c, DS3231_ADDR, 0x0E,
                      I2C_MEMADD_SIZE_8BIT, &ctrl, 1, 50);

    /* 写闹钟寄存器 */
    uint8_t alm_regs[3] = {sec, min, hour};
    HAL_I2C_Mem_Write(&rtc_i2c, DS3231_ADDR, 0x07,
                      I2C_MEMADD_SIZE_8BIT, alm_regs, 3, 50);

    /* 使能闹钟 1 中断 */
    ctrl |= 0x01;  /* A1IE = 1 */
    HAL_I2C_Mem_Write(&rtc_i2c, DS3231_ADDR, 0x0E,
                      I2C_MEMADD_SIZE_8BIT, &ctrl, 1, 50);
}

bool RTC_AlarmFired(void)
{
    uint8_t status;
    if (HAL_I2C_Mem_Read(&rtc_i2c, DS3231_ADDR, 0x0F,
                         I2C_MEMADD_SIZE_8BIT, &status, 1, 50) != HAL_OK)
        return false;
    return (status & 0x01) != 0;  /* A1F */
}

void RTC_ClearAlarm(void)
{
    uint8_t status;
    HAL_I2C_Mem_Read(&rtc_i2c, DS3231_ADDR, 0x0F,
                     I2C_MEMADD_SIZE_8BIT, &status, 1, 50);
    status &= ~0x01;  /* 清除 A1F */
    HAL_I2C_Mem_Write(&rtc_i2c, DS3231_ADDR, 0x0F,
                      I2C_MEMADD_SIZE_8BIT, &status, 1, 50);
}
