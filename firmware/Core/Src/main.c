/**
 * @file    main.c
 * @brief   智能药盒系统 — 主程序
 *
 * STM32F103C8T6 / 72MHz / 20KB SRAM / 64KB Flash
 * 裸机大循环，无 RTOS
 *
 * 主循环流程：
 *   1. 检查 RTC 闹钟 → 到用药时间 → 语音播报 + 转盘转到目标仓
 *   2. 检查取药传感器 → 药片掉落 → 记录
 *   3. 检查超时 → 漏服 → 蓝牙上报 APP
 *   4. 处理蓝牙指令（PLAN / SET / SYNC）
 *   5. 处理屏幕触摸事件
 *   6. 定时更新屏幕时间显示
 *   7. LED 状态指示
 */

#include "main.h"
#include "pinmap.h"
#include "ble.h"
#include "protocol.h"
#include "hmi.h"
#include "rtc_ds3231.h"
#include "motor.h"
#include "sensor.h"
#include "voice.h"
#include "record.h"

/* ==================== 全局变量 ==================== */
volatile SystemState g_sys_state = SYS_IDLE;
volatile uint32_t    g_sys_ticks = 0;

MedicineSlot     g_slots[NUM_SLOTS];
MedicationPlan   g_plans[PERIOD_COUNT];
MedicationRecord g_records[128];
uint16_t         g_record_count  = 0;
uint32_t         g_missed_deadline = 0;
uint8_t          g_missed_slot   = 0;

/* 上次屏幕时间刷新 */
static uint32_t last_hmi_update = 0;
static uint32_t last_led_toggle = 0;
static uint8_t  led_state = 0;

/* ==================== GPIO 初始化 ==================== */

void GPIO_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    /* PC13 — 板载 LED（低电平亮） */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = LED_BUILTIN_PIN;
    HAL_GPIO_Init(LED_BUILTIN_PORT, &gpio);
    HAL_GPIO_WritePin(LED_BUILTIN_PORT, LED_BUILTIN_PIN, GPIO_PIN_SET); /* 灭 */

    /* PB14 — 蓝牙状态 LED */
    gpio.Pin = LED_BLE_PIN;
    HAL_GPIO_Init(LED_BLE_PORT, &gpio);
    HAL_GPIO_WritePin(LED_BLE_PORT, LED_BLE_PIN, GPIO_PIN_RESET); /* 灭 */

    /* PA5 — 测试按键（上拉输入） */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin  = KEY_PIN;
    HAL_GPIO_Init(KEY_PORT, &gpio);
}

/* ==================== 初始化总入口 ==================== */

static void System_Init(void)
{
    HAL_Init();
    SystemClock_Config();

    /* 基础 GPIO */
    GPIO_Init();

    /* 串口初始化 */
    UART_Init();
    BLE_Init();
    HMI_Init();
    Voice_Init();

    /* I²C + RTC */
    if (RTC_Init() != 0) {
        /* RTC 初始化失败 — 闪板载 LED 3 次 */
        for (int i = 0; i < 6; i++) {
            HAL_GPIO_TogglePin(LED_BUILTIN_PORT, LED_BUILTIN_PIN);
            HAL_Delay(200);
        }
    }

    /* 电机 */
    Motor_Init();

    /* 传感器 */
    Sensor_Init();

    /* 记录存储 */
    Record_Init();
    Record_LoadAll();

    /* 初始化全局数据（Demo 数据） */
    memset(g_slots, 0, sizeof(g_slots));
    for (int i = 0; i < NUM_SLOTS; i++) {
        g_slots[i].slot_id = i;
        snprintf(g_slots[i].medicine_name, 32, "药仓%d", i + 1);
        g_slots[i].status = (i < 3) ? SLOT_FILLED : SLOT_EMPTY; /* 前3仓有药 */
    }
    strcpy(g_slots[0].medicine_name, "阿司匹林");
    strcpy(g_slots[1].medicine_name, "降压药");
    strcpy(g_slots[2].medicine_name, "维生素D");

    memset(g_plans, 0, sizeof(g_plans));
    /* 默认计划 */
    g_plans[PERIOD_MORNING].slot_id = 0; g_plans[PERIOD_MORNING].hour = 8;
    g_plans[PERIOD_MORNING].minute = 0;  g_plans[PERIOD_MORNING].period = PERIOD_MORNING;
    g_plans[PERIOD_MORNING].enabled = 1;

    g_plans[PERIOD_NOON].slot_id = 1; g_plans[PERIOD_NOON].hour = 12;
    g_plans[PERIOD_NOON].minute = 30; g_plans[PERIOD_NOON].period = PERIOD_NOON;
    g_plans[PERIOD_NOON].enabled = 1;

    g_plans[PERIOD_EVENING].slot_id = 2; g_plans[PERIOD_EVENING].hour = 18;
    g_plans[PERIOD_EVENING].minute = 0;  g_plans[PERIOD_EVENING].period = PERIOD_EVENING;
    g_plans[PERIOD_EVENING].enabled = 0;

    g_record_count = 0;
    g_sys_state    = SYS_IDLE;
    g_missed_deadline = 0;

    /* 使能全局中断 */
    __enable_irq();
}

/* ==================== 闹钟检查 ==================== */

static void CheckMedicationAlarm(void)
{
    if (RTC_AlarmFired()) {
        RTC_ClearAlarm();

        /* 找到当前时段对应的计划 */
        DateTime dt;
        RTC_GetDateTime(&dt);

        for (int i = 0; i < PERIOD_COUNT; i++) {
            if (g_plans[i].enabled &&
                g_plans[i].hour == dt.hour &&
                g_plans[i].minute == dt.minute) {

                uint8_t slot = g_plans[i].slot_id;
                if (slot >= NUM_SLOTS) continue;
                if (g_slots[slot].status == SLOT_EMPTY) continue;

                /* 语音播报 */
                Voice_RemindSlot(slot);

                /* 转盘转到目标药仓 */
                Motor_GotoSlot(slot);

                /* 启动漏服计时（30分钟） */
                g_missed_deadline = RTC_GetTimestamp() + 1800; /* 30min */
                g_missed_slot     = slot;
                g_sys_state       = SYS_DISPENSING;

                /* HX711 零点校准（取药开始前） */
                HX711_Tare();

                break;
            }
        }

        /* 设置下一次闹钟 */
        /* 找到下一个启用的计划 */
        uint32_t now = RTC_GetTimestamp();
        uint32_t next_alarm = 0xFFFFFFFF;

        for (int i = 0; i < PERIOD_COUNT; i++) {
            if (!g_plans[i].enabled) continue;

            DateTime alarm_dt;
            alarm_dt.hour   = g_plans[i].hour;
            alarm_dt.minute = g_plans[i].minute;
            alarm_dt.second = 0;

            uint32_t alarm_ts = 0; /* 简化计算 */
            (void)alarm_dt;

            if (alarm_ts > now && alarm_ts < next_alarm) {
                next_alarm = alarm_ts;
            }
        }
    }
}

/* ==================== 取药检测 ==================== */

static void CheckDispenseDetection(void)
{
    if (g_sys_state != SYS_DISPENSING) return;

    /* 检查 IRT9606 取药通道传感器 */
    if (ITR9606_DispenseDetected()) {
        /* 再检查 HX711 重量变化确认 */
        HX711_FilterUpdate();
        int32_t weight = HX711_ReadWeight();

        if (weight > 10) {  /* > 0.1g = 有药片 */
            /* 取药成功！ */
            MedicationRecord rec;
            rec.timestamp = RTC_GetTimestamp();
            rec.slot_id   = g_missed_slot;
            rec.taken     = 1;

            Record_Add(&rec);
            BLE_SendRecord(&rec);
            Voice_Play(VOICE_DETECTED);

            /* 复位 */
            g_sys_state       = SYS_IDLE;
            g_missed_deadline = 0;
            Motor_Stop();
        }
    }
}

/* ==================== 漏服检测 ==================== */

static void CheckMissedDose(void)
{
    if (g_sys_state != SYS_DISPENSING) return;
    if (g_missed_deadline == 0) return;

    uint32_t now = RTC_GetTimestamp();
    if (now > g_missed_deadline) {
        /* 超时未取 → 漏服报警 */
        uint8_t slot = g_missed_slot;

        /* 记录漏服 */
        MedicationRecord rec;
        rec.timestamp = now;
        rec.slot_id   = slot;
        rec.taken     = 0;
        Record_Add(&rec);

        /* 找到触发此药仓的计划，获取实际计划时间 */
        uint8_t plan_hour = 0, plan_minute = 0;
        for (int i = 0; i < PERIOD_COUNT; i++) {
            if (g_plans[i].enabled && g_plans[i].slot_id == slot) {
                plan_hour   = g_plans[i].hour;
                plan_minute = g_plans[i].minute;
                break;
            }
        }

        /* 蓝牙上报 APP → 触发通知 + 短信 */
        BLE_SendAlert(slot, plan_hour, plan_minute,
                      g_slots[slot].medicine_name);

        /* 语音警告 */
        Voice_Play(VOICE_MISSED);

        /* 复位 */
        g_sys_state       = SYS_IDLE;
        g_missed_deadline = 0;
        Motor_Stop();
    }
}

/* ==================== 蓝牙数据处理 ==================== */

static void ProcessBleData(void)
{
    char line[128];
    uint16_t len = BLE_ReadLine(line, sizeof(line));
    if (len > 0) {
        Protocol_ParseLine(line);
    }
}

/* ==================== 主函数 ==================== */

int main(void)
{
    /* ---- 初始化 ---- */
    System_Init();

    /* ---- 开机语音 ---- */
    Voice_Play(VOICE_BOOT_OK);

    /* ---- 蓝牙 AT 配置 ---- */
    BLE_Config();

    /* ---- 电机零位校准 ---- */
    Motor_CalibrateHome();

    /* ---- 屏幕初始化 ---- */
    HMI_GotoPage("main");
    HAL_Delay(500);
    HMI_UpdateSlots();
    HMI_UpdatePlans();

    /* ---- 主循环 ---- */
    while (1) {
        uint32_t now = HAL_GetTick();

        /* 1. 闹钟检查 */
        CheckMedicationAlarm();

        /* 2. 取药检测 */
        CheckDispenseDetection();

        /* 3. 漏服检测 */
        CheckMissedDose();

        /* 4. 蓝牙数据处理 */
        ProcessBleData();

        /* 5. 屏幕触摸事件 */
        HMI_ProcessInput();

        /* 6. 定时刷新屏幕（每秒） */
        if (now - last_hmi_update > 1000) {
            last_hmi_update = now;

            DateTime dt;
            RTC_GetDateTime(&dt);
            HMI_UpdateTime(dt.hour, dt.minute, dt.second);

            /* 蓝牙状态 */
            if (BLE_GetState() == BLE_CONNECTED) {
                HMI_SetText("t_ble", "已连接");
                HAL_GPIO_WritePin(LED_BLE_PORT, LED_BLE_PIN, GPIO_PIN_SET);
            } else {
                HMI_SetText("t_ble", "未连接");
                HAL_GPIO_WritePin(LED_BLE_PORT, LED_BLE_PIN, GPIO_PIN_RESET);
            }
        }

        /* 7. LED 心跳 */
        if (now - last_led_toggle > 500) {
            last_led_toggle = now;
            led_state = !led_state;
            HAL_GPIO_WritePin(LED_BUILTIN_PORT, LED_BUILTIN_PIN,
                              led_state ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }

        /* 8. 喂狗（如果启用 IWDG） */
        // HAL_IWDG_Refresh(&hiwdg);
    }
}

/* ==================== UART 初始化 ==================== */

void UART_Init(void)
{
    /* USART1/2/3 的初始化和 GPIO 配置在 BLE_Init / HMI_Init / Voice_Init 中完成 */
    /* 此函数留空或做额外配置 */
}

/* ==================== I²C 初始化 ==================== */

void I2C_Init(void)
{
    /* I²C1 初始化在 RTC_Init() 中完成 */
}

/* ==================== TIM2 PWM 初始化 ==================== */

void TIM2_PWM_Init(void)
{
    /* TIM2 初始化在 Motor_Init() 中完成 */
}

/* ==================== 错误处理 ==================== */

void Error_Handler(void)
{
    __disable_irq();
    /* 快闪 LED 指示错误 */
    while (1) {
        HAL_GPIO_TogglePin(LED_BUILTIN_PORT, LED_BUILTIN_PIN);
        for (volatile uint32_t i = 0; i < 200000; i++) { __NOP(); }
    }
}

/* ==================== SysTick 中断 ==================== */

void SysTick_Handler(void)
{
    HAL_IncTick();
    g_sys_ticks++;
}
