/**
 * @file    app_time.h
 * @brief   软件时间模块：无 DS3231 时由 APP 通过 TIME:timestamp 同步
 */

#ifndef __APP_TIME_H
#define __APP_TIME_H

#include "main.h"

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} AppDateTime;

void AppTime_Init(void);
void AppTime_SetUnix(uint32_t unix_time);
uint32_t AppTime_Now(void);
void AppTime_GetHMS(uint8_t *hour, uint8_t *minute, uint8_t *second);
void AppTime_GetDateTime(AppDateTime *dt);
bool AppTime_IsSynced(void);

#endif /* __APP_TIME_H */
