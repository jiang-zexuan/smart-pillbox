/**
 * @file    sensor.h
 * @brief   HX711 称重传感器 + ITR9606 光电开关 驱动 — 头文件
 *
 * HX711：
 *   - GPIO 模拟时序（PB0 SCK / PB1 DOUT）
 *   - 24-bit Σ-Δ ADC，增益 128
 *   - 输出速率：10Hz 或 80Hz
 *
 * ITR9606 ×2：
 *   - PB12：取药通道检测（EXTI 中断）
 *   - PB13：转盘零位检测
 *   - DO = HIGH（无遮挡）→ LOW（遮挡）
 */

#ifndef __SENSOR_H
#define __SENSOR_H

#include "main.h"

/**
 * @brief 初始化 HX711 GPIO + ITR9606 GPIO/EXTI
 */
void Sensor_Init(void);

/**
 * @brief 读取 HX711 原始值（24-bit）
 * @return 原始 ADC 值（有符号 32-bit）
 */
int32_t HX711_Read(void);

/**
 * @brief 读取 HX711 重量（经过零点校准和比例换算）
 * @return 重量，单位 0.01g（100 = 1g）
 */
int32_t HX711_ReadWeight(void);

/**
 * @brief 称重传感器零点校准
 *        确保取药平台空载时调用
 */
void HX711_Tare(void);

/**
 * @brief 取药通道是否有物体通过
 * @return true = 被遮挡（有物体穿过）
 */
bool ITR9606_DispenseDetected(void);

/**
 * @brief 零位传感器是否被遮挡
 * @return true = 在零位
 */
bool ITR9606_HomeDetected(void);

/**
 * @brief 添加重量到滑动平均滤波
 */
void HX711_FilterUpdate(void);

#endif /* __SENSOR_H */
