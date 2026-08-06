/**
 * @file    motor.h
 * @brief   TB6560 + NEMA17 步进电机驱动 — 头文件
 *
 * TB6560 控制信号（共阳接法）：
 *   PUL- → PA0 (TIM2_CH1 PWM)
 *   DIR- → PA1 (GPIO)
 *   EN-  → PA4 (GPIO, LOW=使能)
 *   PUL+ / DIR+ / EN+ → 3.3V 经 1kΩ 限流（光耦 LED 阳极）
 *
 * TB6560 DIP 设置：
 *   电流：按电机额定 1.7A 设置（SW1-SW3）
 *   细分：16 细分（SW5=OFF SW6=ON SW7=ON SW8=ON）
 */

#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"

/* 电机转动方向 */
typedef enum {
    DIR_CW  = 0,   /* 顺时针（转盘从正面看） */
    DIR_CCW = 1    /* 逆时针 */
} MotorDir;

/**
 * @brief 初始化 TIM2 PWM + GPIO 控制脚
 */
void Motor_Init(void);

/**
 * @brief 使能电机（通电锁定）
 */
void Motor_Enable(void);

/**
 * @brief 禁能电机（断电自由）
 */
void Motor_Disable(void);

/**
 * @brief 设置转动方向
 */
void Motor_SetDir(MotorDir dir);

/**
 * @brief 转动指定步数（阻塞）
 * @param steps  步数（16细分后 3200步/圈）
 * @param dir    方向
 */
void Motor_Step(uint32_t steps, MotorDir dir);

/**
 * @brief 转动到指定药仓
 * @param slot  目标仓位 0–5
 */
void Motor_GotoSlot(uint8_t slot);

/**
 * @brief 执行零位校准
 *        慢转一圈找到 ITR9606(零位) 遮挡点 → 设为零位基准
 * @return 0=成功, -1=未找到零位
 */
int  Motor_CalibrateHome(void);

/**
 * @brief 获取当前绝对步数（以零位为 0）
 */
int32_t Motor_GetPosition(void);

/**
 * @brief 停止电机
 */
void Motor_Stop(void);

#endif /* __MOTOR_H */
