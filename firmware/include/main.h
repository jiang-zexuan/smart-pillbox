/**
 * @file    main.h
 * @brief   智能药盒系统 — 主头文件
 * @author  组员C（蓝牙+APP）
 * @date    2026-06-29
 *
 * 硬件平台：STM32F103C8T6（LQFP-48，72MHz，20KB SRAM，64KB Flash）
 * 开发环境：Keil MDK-ARM / STM32CubeIDE + STM32Cube F1 HAL
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 标准库 ==================== */
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ==================== 系统参数 ==================== */
#define SYSTEM_CLOCK_HZ         72000000UL   /* 系统主频 72MHz */
#define APB1_CLOCK_HZ           36000000UL   /* APB1 36MHz */
#define APB2_CLOCK_HZ           72000000UL   /* APB2 72MHz */

/* ==================== 药仓参数 ==================== */
#define NUM_SLOTS               6            /* 药仓数量 */
#define SLOT_ANGLE_DEG          60           /* 每仓间隔角度 */
#define STEPS_PER_REV           200          /* NEMA17 整步 200步/圈 */
#define STEPS_PER_SLOT          (STEPS_PER_REV / NUM_SLOTS) /* 每仓约33步 */

/* ==================== 用药时段枚举 ==================== */
typedef enum {
    PERIOD_MORNING = 0,
    PERIOD_NOON    = 1,
    PERIOD_EVENING = 2,
    PERIOD_EXTRA1  = 3,
    PERIOD_EXTRA2  = 4,
    PERIOD_EXTRA3  = 5,
    PERIOD_COUNT   = 6
} MedicationPeriod;

/* ==================== 药仓状态 ==================== */
typedef enum {
    SLOT_EMPTY    = 0,    /* 空仓 */
    SLOT_FILLED   = 1,    /* 有药 */
    SLOT_DISPENSING = 2   /* 正在出药 */
} SlotStatus;

/* ==================== 药仓信息 ==================== */
typedef struct {
    uint8_t  slot_id;              /* 仓位号 0–5 */
    char     medicine_name[32];    /* 药品名称（UTF-8） */
    uint8_t  status;               /* SlotStatus */
} MedicineSlot;

/* ==================== 用药计划 ==================== */
typedef struct {
    uint8_t  slot_id;              /* 仓位号 */
    uint8_t  hour;                 /* 提醒时刻（0–23） */
    uint8_t  minute;               /* 提醒分钟（0–59） */
    uint8_t  period;               /* MedicationPeriod */
    uint8_t  enabled;              /* 1=启用 0=禁用 */
} MedicationPlan;

/* ==================== 服药记录 ==================== */
typedef struct {
    uint32_t timestamp;            /* Unix 时间戳 */
    uint8_t  slot_id;              /* 仓位号 */
    uint8_t  taken;                /* 1=已服 0=漏服 */
} MedicationRecord;

/* ==================== 系统运行状态 ==================== */
typedef enum {
    SYS_IDLE      = 0,
    SYS_DISPENSING = 1,           /* 正在出药流程 */
    SYS_ALERTING  = 2              /* 正在报警 */
} SystemState;

/* ==================== 全局变量声明 ==================== */
extern volatile SystemState g_sys_state;
extern volatile uint32_t    g_sys_ticks;       /* 1ms 系统滴答 */
extern MedicineSlot         g_slots[NUM_SLOTS];
extern MedicationPlan       g_plans[PERIOD_COUNT];
extern MedicationRecord     g_records[128];     /* 最近128条记录 */
extern uint16_t             g_record_count;
extern uint32_t             g_missed_deadline;  /* 漏服截止时间戳(0=无) */
extern uint8_t              g_missed_slot;      /* 漏服药仓号 */

/* ==================== 函数声明 ==================== */
void SystemClock_Config(void);
void GPIO_Init(void);
void UART_Init(void);
void I2C_Init(void);
void TIM2_PWM_Init(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
