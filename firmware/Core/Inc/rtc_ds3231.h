/**
 * @file    rtc_ds3231.h
 * @brief   DS3231M RTC 时钟模块驱动 — 头文件
 *
 * 接口：I²C1（PB6 SCL / PB7 SDA），地址 0xD0
 * 模块：6 脚（VCC/GND/SCL/SDA/SQW/32K），只接前 4 脚
 * 精度：±2ppm，年误差 < 1 分钟
 */

#ifndef __RTC_DS3231_H
#define __RTC_DS3231_H

#include "main.h"

/* 时间结构体 */
typedef struct {
    uint8_t year;    /* 0–99（2000年偏移） */
    uint8_t month;   /* 1–12 */
    uint8_t date;    /* 1–31 */
    uint8_t day;     /* 1–7（1=周日） */
    uint8_t hour;    /* 0–23 */
    uint8_t minute;  /* 0–59 */
    uint8_t second;  /* 0–59 */
} DateTime;

/* 闹钟结构体 */
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t enabled;
} AlarmTime;

/**
 * @brief 初始化 I²C1 并检测 DS3231
 * @return 0=成功, -1=未检测到设备
 */
int  RTC_Init(void);

/**
 * @brief 读取当前时间
 */
void RTC_GetDateTime(DateTime *dt);

/**
 * @brief 设置当前时间
 */
void RTC_SetDateTime(const DateTime *dt);

/**
 * @brief 获取 Unix 时间戳
 */
uint32_t RTC_GetTimestamp(void);

/**
 * @brief 设置闹钟（仅在 hh:mm 匹配时触发）
 * @param alm 闹钟时间
 */
void RTC_SetAlarm(const AlarmTime *alm);

/**
 * @brief 清除闹钟标志
 */
void RTC_ClearAlarm(void);

/**
 * @brief 检查闹钟是否触发
 * @return true = 已触发
 */
bool RTC_AlarmFired(void);

/**
 * @brief BCD → 二进制
 */
uint8_t RTC_BcdToBin(uint8_t bcd);
uint8_t RTC_BinToBcd(uint8_t bin);

#endif /* __RTC_DS3231_H */
