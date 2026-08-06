/**
 * @file    protocol.c
 * @brief   蓝牙通信协议解析 — 实现
 */

#include "main.h"
#include "pinmap.h"
#include "ble.h"
#include "protocol.h"
#include "app_time.h"
#include "buzzer.h"
#include "sensor.h"
#include "feature_config.h"

#if FEATURE_MOTOR_RESERVED
#include "motor.h"
#endif

/* ==================== 主解析入口 ==================== */

void Protocol_ParseLine(const char *line)
{
    if (line == NULL || line[0] == '\0') return;

    if (strcmp(line, "BEEP") == 0) {
        Protocol_HandleBeep();
        return;
    }
    if (strcmp(line, "BEEP10") == 0) {
        Buzzer_Remind10s();
        BLE_Send("BEEP_ACK:10S\r\n");
        return;
    }
    if (strcmp(line, "TARE") == 0) {
        Protocol_HandleTare();
        return;
    }

    /* 提取命令关键字（冒号前） */
    char cmd_str[16] = {0};
    const char *colon = strchr(line, ':');
    if (colon == NULL) return;

    uint16_t cmd_len = (uint16_t)(colon - line);
    if (cmd_len >= sizeof(cmd_str)) cmd_len = sizeof(cmd_str) - 1;
    strncpy(cmd_str, line, cmd_len);

    const char *payload = colon + 1;  /* 冒号后面的数据 */

    /* 分派命令 */
    if (strcmp(cmd_str, "PLAN") == 0) {
        Protocol_HandlePlan(payload);
    } else if (strcmp(cmd_str, "SET") == 0) {
        Protocol_HandleSet(payload);
    } else if (strcmp(cmd_str, "SYNC") == 0) {
        /* SYNC:ALL → 回复全量状态 */
        BLE_SendStatus();
    } else if (strcmp(cmd_str, "TIME") == 0) {
        Protocol_HandleTime(payload);
    } else if (strcmp(cmd_str, "TEST") == 0) {
        Protocol_HandleTest(payload);
    } else if (strcmp(cmd_str, "MOTOR") == 0) {
        Protocol_HandleMotor(payload);
    }
}

/* ==================== PLAN 处理 ==================== */

void Protocol_HandlePlan(const char *payload)
{
    /* 格式：slot,hh,mm,period,en */
    int slot = 0, hh = 0, mm = 0, period = 0, en = 0;

    if (sscanf(payload, "%d,%d,%d,%d,%d",
               &slot, &hh, &mm, &period, &en) < 5) {
        return;  /* 格式错误 */
    }

    /* 范围校验 */
    if (slot < 0 || slot >= NUM_SLOTS) return;
    if (hh < 0 || hh > 23) return;
    if (mm < 0 || mm > 59) return;
    if (period < 0 || period >= PERIOD_COUNT) return;

    /* 保存计划 */
    g_plans[period].slot_id = (uint8_t)slot;
    g_plans[period].hour    = (uint8_t)hh;
    g_plans[period].minute  = (uint8_t)mm;
    g_plans[period].period  = (uint8_t)period;
    g_plans[period].enabled = (uint8_t)en;

    /* 确认回复 */
    char ack[64];
    snprintf(ack, sizeof(ack), "PLAN_ACK:%d,%02d,%02d,%d,%d\r\n",
             slot, hh, mm, period, en);
    BLE_Send(ack);
}

/* ==================== SET 处理 ==================== */

void Protocol_HandleSet(const char *payload)
{
    /* 格式：slot,medname */
    int slot = 0;
    const char *comma = strchr(payload, ',');
    if (comma == NULL) return;

    /* 解析仓位号 */
    char slot_str[4] = {0};
    uint16_t name_offset = (uint16_t)(comma - payload);
    if (name_offset >= sizeof(slot_str)) return;
    strncpy(slot_str, payload, name_offset);
    slot = atoi(slot_str);

    if (slot < 0 || slot >= NUM_SLOTS) return;

    /* 保存药品名 */
    const char *name = comma + 1;
    strncpy(g_slots[slot].medicine_name, name,
            sizeof(g_slots[slot].medicine_name) - 1);
    g_slots[slot].medicine_name[sizeof(g_slots[slot].medicine_name) - 1] = '\0';
    g_slots[slot].status = (g_slots[slot].medicine_name[0] == '\0') ? SLOT_EMPTY : SLOT_FILLED;

    /* 确认回复 */
    char ack[64];
    snprintf(ack, sizeof(ack), "SET_ACK:%d,%s\r\n",
             slot, g_slots[slot].medicine_name);
    BLE_Send(ack);

#if FEATURE_MOTOR_RESERVED
    /*
     * App QR flow means: after scanning and selecting a slot, the selected
     * slot should rotate to the loading position.  Do it on SET as well as
     * MOTOR:SLOT so the action is reliable even if two Bluetooth lines are
     * sent back-to-back and the second line is delayed/lost.
     */
    Motor_GotoSlot((uint8_t)slot);
    {
        char motor_ack[48];
        snprintf(motor_ack, sizeof(motor_ack), "MOTOR_ACK:SET_SLOT,%d\r\n", slot);
        BLE_Send(motor_ack);
    }
#endif
}

/* ==================== TIME 处理 ==================== */

void Protocol_HandleTime(const char *payload)
{
    uint32_t ts = (uint32_t)strtoul(payload, NULL, 10);
    if (ts == 0) return;

    AppTime_SetUnix(ts);

    char ack[48];
    snprintf(ack, sizeof(ack), "TIME_ACK:%lu\r\n", (unsigned long)ts);
    BLE_Send(ack);
}

/* ==================== TEST 处理 ==================== */

void Protocol_HandleTest(const char *payload)
{
    int slot = atoi(payload);
    if (slot < 0 || slot >= NUM_SLOTS) return;

    char msg[96];
    snprintf(msg, sizeof(msg), "ALERT:%d,%02u,%02u,%s\r\n",
             slot, g_plans[0].hour, g_plans[0].minute, g_slots[slot].medicine_name);
    BLE_Send(msg);

    Buzzer_Remind();
}


void Protocol_HandleMotor(const char *payload)
{
#if FEATURE_MOTOR_RESERVED
    if (payload == NULL) return;

    if (strcmp(payload, "TEST") == 0) {
        BLE_Send("MOTOR_ACK:TEST,START\r\n");

        /* No-home smoke test: 300 microsteps = about 1/4 turn at 16 microsteps. */
        Motor_Step(300, DIR_CW);
        HAL_Delay(250);
        Motor_Step(300, DIR_CCW);
        Motor_Stop();
        Motor_Disable();

        BLE_Send("MOTOR_ACK:TEST,DONE\r\n");
        return;
    }

    if (strncmp(payload, "SLOT,", 5) == 0) {
        int slot = atoi(payload + 5);
        if (slot < 0 || slot >= NUM_SLOTS) {
            BLE_Send("MOTOR_ACK:ERR,SLOT\r\n");
            return;
        }
        Motor_GotoSlot((uint8_t)slot);
        char ack[48];
        snprintf(ack, sizeof(ack), "MOTOR_ACK:SLOT,%d\r\n", slot);
        BLE_Send(ack);
        return;
    }

    if (strncmp(payload, "CW,", 3) == 0 || strncmp(payload, "CCW,", 4) == 0) {
        MotorDir dir = (payload[0] == 'C' && payload[1] == 'W') ? DIR_CW : DIR_CCW;
        const char *steps_str = strchr(payload, ',');
        uint32_t steps = (steps_str != NULL) ? (uint32_t)strtoul(steps_str + 1, NULL, 10) : 0;
        if (steps == 0 || steps > 6400) {
            BLE_Send("MOTOR_ACK:ERR,STEPS\r\n");
            return;
        }

        Motor_Step(steps, dir);
        Motor_Stop();
        Motor_Disable();

        char ack[48];
        snprintf(ack, sizeof(ack), "MOTOR_ACK:STEP,%lu\r\n", (unsigned long)steps);
        BLE_Send(ack);
        return;
    }

    BLE_Send("MOTOR_ACK:ERR,CMD\r\n");
#else
    (void)payload;
    BLE_Send("MOTOR_ACK:ERR,DISABLED\r\n");
#endif
}

void Protocol_HandleBeep(void)
{
    Buzzer_Remind();
    BLE_Send("BEEP_ACK:OK\r\n");
}

void Protocol_HandleTare(void)
{
    HX711_Tare();
    BLE_Send("TARE_ACK:OK\r\n");
}
