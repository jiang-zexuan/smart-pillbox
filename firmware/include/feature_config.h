/**
 * @file    feature_config.h
 * @brief   硬件功能配置开关 — 编译期裁剪
 *
 * 通过宏定义切换各功能模块的启用状态，适配不同硬件配置。
 * 当前默认配置：JDY-31 蓝牙、HX711 称重、QR100 扫码、OLED、TJC 串口屏、
 * 蜂鸣器、步进电机驱动。
 * 可选扩展模块：DS3231 RTC、光电传感器、串口语音模块。
 */

#ifndef __FEATURE_CONFIG_H
#define __FEATURE_CONFIG_H

/* 已启用功能模块 */
#define FEATURE_BLE_JDY31          1
#define FEATURE_HX711              1
#define FEATURE_OLED               1
#define FEATURE_TJC_HMI            1
#define FEATURE_QR100              1
#define FEATURE_BUZZER             1
#define FEATURE_DEBUG_UART         0
#define FEATURE_BLE_AT_CONFIG      0

/* 可选扩展模块 — 接入对应硬件后改为 1 即可编译启用 */
#define FEATURE_MOTOR_RESERVED     1
#define FEATURE_DS3231_RTC         0
#define FEATURE_OPTICAL_SENSORS    0
#define FEATURE_SERIAL_VOICE       0

/* HX711 取药判定阈值，单位 0.01g，500 = 5g */
#define TAKE_WEIGHT_DROP_THRESHOLD 500

/* 漏服超时判定时间，单位秒 */
#define MISSED_TIMEOUT_SECONDS     1800UL

#endif /* __FEATURE_CONFIG_H */
