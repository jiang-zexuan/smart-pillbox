/**
 * @file    app_time.c
 * @brief   软件时间模块实现
 */

#include "app_time.h"

static uint32_t s_base_unix = 0;
static uint32_t s_base_tick = 0;
static bool s_synced = false;

#define APP_TIME_BEIJING_OFFSET_SECONDS   (8UL * 3600UL)

static bool AppTime_IsLeapYear(uint16_t year)
{
    return ((year % 4U == 0U) && (year % 100U != 0U)) || (year % 400U == 0U);
}

void AppTime_Init(void)
{
    s_base_unix = 0;
    s_base_tick = HAL_GetTick();
    s_synced = false;
}

void AppTime_SetUnix(uint32_t unix_time)
{
    s_base_unix = unix_time;
    s_base_tick = HAL_GetTick();
    s_synced = (unix_time != 0);
}

uint32_t AppTime_Now(void)
{
    return s_base_unix + ((HAL_GetTick() - s_base_tick) / 1000UL);
}

void AppTime_GetHMS(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    AppDateTime dt;
    AppTime_GetDateTime(&dt);
    if (hour)   *hour   = dt.hour;
    if (minute) *minute = dt.minute;
    if (second) *second = dt.second;
}

void AppTime_GetDateTime(AppDateTime *dt)
{
    static const uint8_t days_in_month_common[12] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (dt == NULL) return;

    uint64_t local_seconds = (uint64_t)AppTime_Now() + APP_TIME_BEIJING_OFFSET_SECONDS;
    uint32_t days = (uint32_t)(local_seconds / 86400ULL);
    uint32_t sec_of_day = (uint32_t)(local_seconds % 86400ULL);

    uint16_t year = 1970;
    while (1) {
        uint16_t year_days = AppTime_IsLeapYear(year) ? 366U : 365U;
        if (days < year_days) break;
        days -= year_days;
        year++;
    }

    uint8_t month = 1;
    for (uint8_t i = 0; i < 12; i++) {
        uint8_t mdays = days_in_month_common[i];
        if (i == 1 && AppTime_IsLeapYear(year)) mdays = 29;
        if (days < mdays) break;
        days -= mdays;
        month++;
    }

    dt->year = year;
    dt->month = month;
    dt->day = (uint8_t)(days + 1U);
    dt->hour = (uint8_t)(sec_of_day / 3600UL);
    dt->minute = (uint8_t)((sec_of_day % 3600UL) / 60UL);
    dt->second = (uint8_t)(sec_of_day % 60UL);
}

bool AppTime_IsSynced(void)
{
    return s_synced;
}
