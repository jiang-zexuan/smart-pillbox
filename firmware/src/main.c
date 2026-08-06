/**
 * @file    main.c
 * @brief   ?????? ? ???????????????
 *
 * ???????JDY-31?HX711?QR100?OLED?TJC????????
 * ???DS3231????????????? feature_config.h ???
 */

#include "main.h"
#include "feature_config.h"
#include "pinmap.h"
#include "ble.h"
#include "protocol.h"
#include "hmi.h"
#include "sensor.h"
#include "record.h"
#include "oled.h"
#include "app_time.h"
#include "buzzer.h"
#include "qr100.h"
#include "debug_uart.h"

#if FEATURE_DS3231_RTC
#include "rtc_ds3231.h"
#endif

#if FEATURE_MOTOR_RESERVED
#include "motor.h"
#endif

#if FEATURE_SERIAL_VOICE
#include "voice.h"
#endif

/* ==================== ???? ==================== */
volatile SystemState g_sys_state = SYS_IDLE;
volatile uint32_t    g_sys_ticks = 0;

MedicineSlot     g_slots[NUM_SLOTS];
MedicationPlan   g_plans[PERIOD_COUNT];
MedicationRecord g_records[128];
uint16_t         g_record_count  = 0;
uint32_t         g_missed_deadline = 0;
uint8_t          g_missed_slot   = 0;

static uint32_t last_hmi_update = 0;
static uint32_t last_led_toggle = 0;
static uint32_t last_ble_debug_tx = 0;
static uint32_t last_hx711_update = 0;
static uint8_t  led_state = 0;
static int32_t  dispense_start_weight = 0;
static uint16_t last_alarm_key = 0xFFFF;

/* ==================== ????? ==================== */

static uint32_t System_Now(void)
{
#if FEATURE_DS3231_RTC
    return RTC_GetTimestamp();
#else
    return AppTime_Now();
#endif
}

static void System_GetHMS(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
#if FEATURE_DS3231_RTC
    DateTime dt;
    RTC_GetDateTime(&dt);
    if (hour) *hour = dt.hour;
    if (minute) *minute = dt.minute;
    if (second) *second = dt.second;
#else
    AppTime_GetHMS(hour, minute, second);
#endif
}

static void System_GetDateTime(AppDateTime *adt)
{
    if (adt == NULL) return;
#if FEATURE_DS3231_RTC
    DateTime dt;
    RTC_GetDateTime(&dt);
    adt->year = 2000U + dt.year;
    adt->month = dt.month;
    adt->day = dt.date;
    adt->hour = dt.hour;
    adt->minute = dt.minute;
    adt->second = dt.second;
#else
    AppTime_GetDateTime(adt);
#endif
}

static bool System_TimeReady(void)
{
#if FEATURE_DS3231_RTC
    return true;
#else
    return AppTime_IsSynced();
#endif
}

/* ==================== GPIO ??? ==================== */

void GPIO_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = LED_BUILTIN_PIN;
    HAL_GPIO_Init(LED_BUILTIN_PORT, &gpio);
    HAL_GPIO_WritePin(LED_BUILTIN_PORT, LED_BUILTIN_PIN, GPIO_PIN_SET);

    gpio.Pin = LED_BLE_PIN;
    HAL_GPIO_Init(LED_BLE_PORT, &gpio);
    HAL_GPIO_WritePin(LED_BLE_PORT, LED_BLE_PIN, GPIO_PIN_RESET);
}

/* ==================== ?????? ==================== */

static void InitDemoData(void)
{
    memset(g_slots, 0, sizeof(g_slots));
    for (int i = 0; i < NUM_SLOTS; i++) {
        g_slots[i].slot_id = i;
        snprintf(g_slots[i].medicine_name, sizeof(g_slots[i].medicine_name), "??%d", i + 1);
        g_slots[i].status = SLOT_EMPTY;
    }
    strcpy(g_slots[0].medicine_name, "????");
    strcpy(g_slots[1].medicine_name, "???");
    strcpy(g_slots[2].medicine_name, "???D");
    g_slots[0].status = SLOT_FILLED;
    g_slots[1].status = SLOT_FILLED;
    g_slots[2].status = SLOT_FILLED;

    memset(g_plans, 0, sizeof(g_plans));
    g_plans[PERIOD_MORNING] = (MedicationPlan){0, 8, 0, PERIOD_MORNING, 1};
    g_plans[PERIOD_NOON]    = (MedicationPlan){1, 12, 30, PERIOD_NOON, 1};
    g_plans[PERIOD_EVENING] = (MedicationPlan){2, 18, 0, PERIOD_EVENING, 0};
    g_plans[PERIOD_EXTRA1]  = (MedicationPlan){3, 8, 30, PERIOD_EXTRA1, 0};
    g_plans[PERIOD_EXTRA2]  = (MedicationPlan){4, 13, 0, PERIOD_EXTRA2, 0};
    g_plans[PERIOD_EXTRA3]  = (MedicationPlan){5, 19, 30, PERIOD_EXTRA3, 0};

    g_record_count = 0;
    g_sys_state = SYS_IDLE;
    g_missed_deadline = 0;
    g_missed_slot = 0;
}

static void System_Init(void)
{
    HAL_Init();
    SystemClock_Config();

    GPIO_Init();
    AppTime_Init();

#if FEATURE_BUZZER
    Buzzer_Init();
#endif

#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
    DebugUart_Init();
    DebugUart_Print("\r\n[BOOT] SmartPillbox current-hardware firmware\r\n");
#endif

#if FEATURE_BLE_JDY31
    BLE_Init();
#endif

#if FEATURE_TJC_HMI
    HMI_Init();
#endif

#if FEATURE_QR100
    QR100_Init();
#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
    DebugUart_Print("[INIT] QR100 USART3 ready\r\n");
#endif
#endif

#if FEATURE_SERIAL_VOICE
    Voice_Init();
#endif

#if FEATURE_DS3231_RTC
    (void)RTC_Init();
#endif

#if FEATURE_MOTOR_RESERVED
    Motor_Init();
#endif

#if FEATURE_HX711
    Sensor_Init();
#endif

    Record_Init();
    Record_LoadAll();
    InitDemoData();

#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
    DebugUart_Print("[INIT] Core modules ready\r\n");
#endif

    __enable_irq();
}

/* ==================== ??/??/?? ==================== */

static void StartDispense(uint8_t slot)
{
    if (slot >= NUM_SLOTS) return;

    g_missed_slot = slot;
    g_missed_deadline = System_Now() + MISSED_TIMEOUT_SECONDS;
    g_sys_state = SYS_DISPENSING;

#if FEATURE_HX711
    HX711_FilterUpdate();
    dispense_start_weight = HX711_ReadWeight();
#endif

#if FEATURE_BUZZER
    Buzzer_Remind10s();
#endif

#if FEATURE_SERIAL_VOICE
    Voice_RemindSlot(slot);
#endif

#if FEATURE_MOTOR_RESERVED
    Motor_GotoSlot(slot);
#endif

    {
        char msg[96];
        snprintf(msg, sizeof(msg), "REMIND:%u,%s\r\n", slot, g_slots[slot].medicine_name);
        BLE_Send(msg);
    }
}

static void CheckMedicationAlarm(void)
{
    if (!System_TimeReady() || g_sys_state != SYS_IDLE) return;

    uint8_t hour = 0, minute = 0, second = 0;
    System_GetHMS(&hour, &minute, &second);

    uint16_t alarm_key = ((uint16_t)hour << 8) | minute;
    (void)second;
    if (alarm_key == last_alarm_key) return;

    for (int i = 0; i < PERIOD_COUNT; i++) {
        if (g_plans[i].enabled && g_plans[i].hour == hour && g_plans[i].minute == minute) {
            last_alarm_key = alarm_key;
            StartDispense(g_plans[i].slot_id);
            break;
        }
    }
}

static void CheckDispenseDetection(void)
{
    if (g_sys_state != SYS_DISPENSING) return;

#if FEATURE_HX711
    HX711_FilterUpdate();
    int32_t current_weight = HX711_ReadWeight();
    if ((dispense_start_weight - current_weight) > TAKE_WEIGHT_DROP_THRESHOLD) {
        MedicationRecord rec;
        rec.timestamp = System_Now();
        rec.slot_id = g_missed_slot;
        rec.taken = 1;

        Record_Add(&rec);
    BLE_SendRecord(&rec);

#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
        DebugUart_Printf("[TAKEN] slot=%u ts=%lu\r\n",
                         rec.slot_id, (unsigned long)rec.timestamp);
#endif

#if FEATURE_BUZZER
        Buzzer_Beep(120);
#endif

        g_sys_state = SYS_IDLE;
        g_missed_deadline = 0;
#if FEATURE_MOTOR_RESERVED
        Motor_Stop();
#endif
    }
#endif
}

static void CheckMissedDose(void)
{
    if (g_sys_state != SYS_DISPENSING || g_missed_deadline == 0) return;

    uint32_t now = System_Now();
    if (now > g_missed_deadline) {
        uint8_t slot = g_missed_slot;

        MedicationRecord rec;
        rec.timestamp = now;
        rec.slot_id = slot;
        rec.taken = 0;
        Record_Add(&rec);

        uint8_t plan_hour = 0, plan_minute = 0;
        for (int i = 0; i < PERIOD_COUNT; i++) {
            if (g_plans[i].enabled && g_plans[i].slot_id == slot) {
                plan_hour = g_plans[i].hour;
                plan_minute = g_plans[i].minute;
                break;
            }
        }

        BLE_SendAlert(slot, plan_hour, plan_minute, g_slots[slot].medicine_name);

#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
        DebugUart_Printf("[MISSED] slot=%u ts=%lu\r\n",
                         slot, (unsigned long)now);
#endif

#if FEATURE_BUZZER
        Buzzer_Alarm();
#endif

#if FEATURE_SERIAL_VOICE
        Voice_Play(VOICE_MISSED);
#endif

        g_sys_state = SYS_IDLE;
        g_missed_deadline = 0;
#if FEATURE_MOTOR_RESERVED
        Motor_Stop();
#endif
    }
}

/* ==================== ???? ==================== */

static void ProcessBleData(void)
{
    char line[128];
    uint16_t len = BLE_ReadLine(line, sizeof(line));
    if (len > 0) {
#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
        DebugUart_Printf("[BLE_LINE] %s\r\n", line);
#endif
        Protocol_ParseLine(line);
    }
}

static void ProcessQrData(void)
{
#if FEATURE_QR100
    char code[96];
    uint16_t len = QR100_ReadFrame(code, sizeof(code));
    if (len > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "QR:%s\r\n", code);
        BLE_Send(msg);

#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
        DebugUart_Printf("[QR] %s\r\n", code);
#endif

        OLED_SetCursor(0, 0);
        OLED_PutString("QR scanned       ");
        OLED_SetCursor(1, 0);
        OLED_PutString("                    ");
        OLED_SetCursor(1, 0);
        OLED_PutString(code);

#if FEATURE_BUZZER
        Buzzer_Beep(80);
#endif
    }
#endif
}

/* ==================== ??? ==================== */

int main(void)
{
    System_Init();

#if FEATURE_OLED
    uint8_t o_addr = OLED_Init();
    if (o_addr) {
        OLED_Clear();
        OLED_SetCursor(0, 0);
        OLED_PutString("SmartPillbox v1.0");
        OLED_SetCursor(1, 0);
        OLED_Printf("OLED addr: 0x%02X", o_addr);
        OLED_SetCursor(2, 0);
        OLED_PutString("Wait APP TIME...");
    }
#endif

#if FEATURE_BUZZER
    Buzzer_BootOk();
#endif

#if FEATURE_BLE_JDY31
#if FEATURE_BLE_AT_CONFIG
    int ble_ret = BLE_Config();
    OLED_SetCursor(3, 0);
    OLED_Printf("BLE cfg: %s", (ble_ret == 0) ? "OK" : "FAIL");
#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
    DebugUart_Printf("[INIT] BLE cfg=%s\r\n", (ble_ret == 0) ? "OK" : "FAIL");
#endif
#else
    OLED_SetCursor(3, 0);
    OLED_PutString("BLE UART ready   ");
#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
    DebugUart_Print("[INIT] BLE UART ready, AT config skipped\r\n");
#endif
#endif
#endif

#if FEATURE_MOTOR_RESERVED
    OLED_SetCursor(4, 0);
    OLED_PutString("Motor: test ready ");
#else
    OLED_SetCursor(4, 0);
    OLED_PutString("Motor: reserved   ");
#endif

#if FEATURE_TJC_HMI
    HMI_GotoPage("main");
    HAL_Delay(300);
    HMI_UpdateSlots();
    HMI_UpdatePlans();
#endif

    while (1) {
        uint32_t now = HAL_GetTick();

        CheckMedicationAlarm();
        CheckDispenseDetection();
        CheckMissedDose();
        ProcessBleData();
        ProcessQrData();

#if FEATURE_TJC_HMI
        HMI_ProcessInput();
#endif

#if FEATURE_HX711
        if (now - last_hx711_update > 50) {
            last_hx711_update = now;
            HX711_FilterUpdate();
        }
#endif

        if (now - last_hmi_update > 1000) {
            last_hmi_update = now;

            uint8_t hour = 0, minute = 0, second = 0;
            System_GetHMS(&hour, &minute, &second);
            AppDateTime dt;
            System_GetDateTime(&dt);
            int32_t weight_cg = HX711_ReadWeight();

#if FEATURE_TJC_HMI
            HMI_UpdateTime(hour, minute, second);
            if (BLE_GetState() == BLE_CONNECTED) {
                HMI_SetText("t_ble", "???");
                HAL_GPIO_WritePin(LED_BLE_PORT, LED_BLE_PIN, GPIO_PIN_SET);
            } else {
                HMI_SetText("t_ble", "???");
                HAL_GPIO_WritePin(LED_BLE_PORT, LED_BLE_PIN, GPIO_PIN_RESET);
            }
#endif

#if FEATURE_OLED
            OLED_SetCursor(0, 0);
            OLED_PutString("Smart Pillbox    ");
            OLED_SetCursor(1, 0);
            if (System_TimeReady()) {
                OLED_Printf("%02u-%02u %02u:%02u:%02u ",
                            dt.month, dt.day, dt.hour, dt.minute, dt.second);
            } else {
                OLED_PutString("-- -- --:--:--  ");
            }
            OLED_SetCursor(2, 0);
            OLED_Printf("BLE:%s Sync:%c",
                        (BLE_GetState() == BLE_CONNECTED) ? "OK " : "NO ",
                        System_TimeReady() ? 'Y' : 'N');
            OLED_SetCursor(3, 0);
            OLED_Printf("State:%u Recs:%u  ", (unsigned)g_sys_state, g_record_count);
            OLED_SetCursor(4, 0);
            OLED_Printf("W:%ld.%02ldg       ",
                        (long)(weight_cg / 100),
                        (long)(weight_cg % 100));
            OLED_SetCursor(5, 0);
            OLED_Printf("Slots:%c%c%c%c%c%c",
                        (g_slots[0].status == SLOT_FILLED) ? 'F' : 'E',
                        (g_slots[1].status == SLOT_FILLED) ? 'F' : 'E',
                        (g_slots[2].status == SLOT_FILLED) ? 'F' : 'E',
                        (g_slots[3].status == SLOT_FILLED) ? 'F' : 'E',
                        (g_slots[4].status == SLOT_FILLED) ? 'F' : 'E',
                        (g_slots[5].status == SLOT_FILLED) ? 'F' : 'E');
            OLED_SetCursor(6, 0);
            OLED_PutString("QR: USART3 Ready ");
            OLED_SetCursor(7, 0);
            OLED_PutString("APP can debug    ");
#endif

#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
            DebugUart_Printf("[STAT] %02u-%02u %02u:%02u:%02u sync=%u ble=%u state=%u weight=%ld.%02ldg recs=%u\r\n",
                             dt.month, dt.day, hour, minute, second,
                             (unsigned)System_TimeReady(),
                             (unsigned)BLE_GetState(),
                             (unsigned)g_sys_state,
                             (long)(weight_cg / 100),
                             (long)(weight_cg % 100),
                             g_record_count);
#endif

            {
                char stat_msg[128];
                snprintf(stat_msg, sizeof(stat_msg),
                         "STAT:%lu,%u,%u,%u,%ld,%u\r\n",
                         (unsigned long)System_Now(),
                         (unsigned)System_TimeReady(),
                         (unsigned)BLE_GetState(),
                         (unsigned)g_sys_state,
                         (long)weight_cg,
                         g_record_count);
                BLE_Send(stat_msg);
            }
        }

        if (now - last_led_toggle > 500) {
            last_led_toggle = now;
            led_state = !led_state;
            HAL_GPIO_WritePin(LED_BUILTIN_PORT, LED_BUILTIN_PIN,
                              led_state ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }

        if (now - last_ble_debug_tx > 5000) {
            last_ble_debug_tx = now;
            char dbg_msg[96];
            snprintf(dbg_msg, sizeof(dbg_msg),
                     "DBG:t=%lu,sync=%u,weight=%ld\r\n",
                     (unsigned long)System_Now(),
                     (unsigned)System_TimeReady(),
                     (long)HX711_ReadWeight());
            BLE_Send(dbg_msg);
#if FEATURE_DEBUG_UART && !FEATURE_TJC_HMI
            DebugUart_Print("[BLE_TX] DBG heartbeat\r\n");
#endif
        }
    }
}

void UART_Init(void) {}
void I2C_Init(void) {}
void TIM2_PWM_Init(void) {}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(LED_BUILTIN_PORT, LED_BUILTIN_PIN);
        for (volatile uint32_t i = 0; i < 200000; i++) { __NOP(); }
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
    g_sys_ticks++;
}
