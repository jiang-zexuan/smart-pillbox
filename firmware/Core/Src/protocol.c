/**
 * @file    protocol.c
 * @brief   蓝牙通信协议解析 — 实现
 */

#include "main.h"
#include "pinmap.h"
#include "ble.h"
#include "protocol.h"

/* ==================== 主解析入口 ==================== */

void Protocol_ParseLine(const char *line)
{
    if (line == NULL || line[0] == '\0') return;

    /* 无冒号的单字命令（心跳等）先处理 */
    if (strcmp(line, "PING") == 0) {
        BLE_Send("PONG\r\n");
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
    g_slots[slot].status = SLOT_FILLED;

    /* 确认回复 */
    char ack[64];
    snprintf(ack, sizeof(ack), "SET_ACK:%d,%s\r\n",
             slot, g_slots[slot].medicine_name);
    BLE_Send(ack);
}
