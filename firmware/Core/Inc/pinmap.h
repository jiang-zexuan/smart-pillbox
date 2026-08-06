/**
 * @file    pinmap.h
 * @brief   STM32F103C8T6 引脚映射表（LQFP-48）
 *
 * ┌────────────┬──────────┬─────────────────────┐
 * │ 外设        │ STM32外设 │ 引脚                 │
 * ├────────────┼──────────┼─────────────────────┤
 * │ TJC屏幕     │ USART1   │ PA9(TX) PA10(RX)   │
 * │ JDY-31 蓝牙 │ USART2   │ PA2(TX) PA3(RX)    │
 * │ DY-SV17F语音│ USART3   │ PB10(TX) PB11(RX)  │
 * │ DS3231 RTC │ I²C1     │ PB6(SCL) PB7(SDA)  │
 * │ TB6560电机  │ TIM2 PWM │ PA0(PUL) PA1(DIR) PA4(EN) │
 * │ HX711 称重  │ GPIO     │ PB0(SCK) PB1(DOUT) │
 * │ ITR9606光电 │ GPIO EXTI│ PB12 PB13          │
 * │ LED         │ GPIO     │ PC13(板载) PB14(蓝牙) │
 * │ 按键        │ GPIO     │ PA5                │
 * │ SWD调试     │ SWD      │ PA13(SWDIO) PA14(SWCLK) │
 * └────────────┴──────────┴─────────────────────┘
 */

#ifndef __PINMAP_H
#define __PINMAP_H

/* ==================== USART1 — TJC 串口屏 ==================== */
#define HMI_USART               USART1
#define HMI_TX_PORT             GPIOA
#define HMI_TX_PIN              GPIO_PIN_9
#define HMI_RX_PORT             GPIOA
#define HMI_RX_PIN              GPIO_PIN_10
#define HMI_BAUDRATE            115200    /* 淘晶驰出厂115200 */

/* ==================== USART2 — JDY-31 蓝牙 ==================== */
#define BLE_USART               USART2
#define BLE_TX_PORT             GPIOA
#define BLE_TX_PIN              GPIO_PIN_2
#define BLE_RX_PORT             GPIOA
#define BLE_RX_PIN              GPIO_PIN_3
#define BLE_BAUDRATE            9600      /* JDY-31 出厂9600 */

/* ==================== USART3 — DY-SV17F 语音 ==================== */
#define VOICE_USART             USART3
#define VOICE_TX_PORT           GPIOB
#define VOICE_TX_PIN            GPIO_PIN_10
#define VOICE_RX_PORT           GPIOB
#define VOICE_RX_PIN            GPIO_PIN_11
#define VOICE_BAUDRATE          9600

/* ==================== I²C1 — DS3231 RTC ==================== */
#define RTC_I2C                 I2C1
#define RTC_SCL_PORT            GPIOB
#define RTC_SCL_PIN             GPIO_PIN_6
#define RTC_SDA_PORT            GPIOB
#define RTC_SDA_PIN             GPIO_PIN_7
#define DS3231_ADDR             0xD0      /* 7-bit 0x68 << 1 */

/* ==================== TB6560 电机（TIM2） ==================== */
#define MOTOR_TIM               TIM2
#define MOTOR_TIM_CHANNEL       TIM_CHANNEL_1
#define MOTOR_PUL_PORT          GPIOA      /* PA0 = TIM2_CH1 */
#define MOTOR_PUL_PIN           GPIO_PIN_0
#define MOTOR_DIR_PORT          GPIOA      /* PA1 = GPIO */
#define MOTOR_DIR_PIN           GPIO_PIN_1
#define MOTOR_EN_PORT           GPIOA      /* PA4 = GPIO */
#define MOTOR_EN_PIN            GPIO_PIN_4
#define MOTOR_PWM_FREQ_HZ       10000      /* 10kHz 脉冲 */
#define MOTOR_MICROSTEPS        16         /* TB6560 16细分 */
#define MOTOR_STEPS_PER_REV     (200 * MOTOR_MICROSTEPS) /* 3200步/圈 */

/* ==================== HX711 称重传感器 ==================== */
#define HX711_SCK_PORT          GPIOB      /* PB0 */
#define HX711_SCK_PIN           GPIO_PIN_0
#define HX711_DOUT_PORT         GPIOB      /* PB1 */
#define HX711_DOUT_PIN          GPIO_PIN_1

/* ==================== ITR9606 槽型光电开关 ==================== */
#define OPTICAL1_PORT           GPIOB      /* PB12 — 取药通道 */
#define OPTICAL1_PIN            GPIO_PIN_12
#define OPTICAL2_PORT           GPIOB      /* PB13 — 转盘零位 */
#define OPTICAL2_PIN            GPIO_PIN_13

/* ==================== LED ==================== */
#define LED_BUILTIN_PORT        GPIOC      /* PC13 — F103板载LED（低电平亮） */
#define LED_BUILTIN_PIN         GPIO_PIN_13
#define LED_BLE_PORT            GPIOB      /* PB14 — 蓝牙状态灯 */
#define LED_BLE_PIN             GPIO_PIN_14

/* ==================== 按键 ==================== */
#define KEY_PORT                GPIOA      /* PA5 */
#define KEY_PIN                 GPIO_PIN_5

#endif /* __PINMAP_H */
