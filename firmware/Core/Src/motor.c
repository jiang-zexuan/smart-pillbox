/**
 * @file    motor.c
 * @brief   TB6560 + NEMA17 步进电机驱动 — 实现
 *
 * TIM2 配置为 PWM 输出，频率 10kHz
 * 定时器计数周期 = 72MHz / 10kHz = 7200
 * 每个 PWM 脉冲 = 电机一步（含 TB6560 细分）
 */

#include "main.h"
#include "pinmap.h"
#include "motor.h"

static TIM_HandleTypeDef motor_tim;

/* 当前位置（绝对步数，以校准零位为原点） */
static volatile int32_t motor_pos = 0;

/* 零位是否已校准 */
static volatile bool motor_homed = false;

/* 每仓步数（16细分后） */
#define STEPS_PER_SLOT  (MOTOR_STEPS_PER_REV / NUM_SLOTS)
/* 校准慢转速度：500Hz */
#define HOME_STEP_FREQ  500

/* ==================== 初始化 ==================== */

void Motor_Init(void)
{
    /* GPIO 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* ---- PA0 = TIM2_CH1 (PUL PWM) ---- */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin   = MOTOR_PUL_PIN;
    HAL_GPIO_Init(MOTOR_PUL_PORT, &gpio);

    /* ---- PA1 = DIR, PA4 = EN ---- */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin   = MOTOR_DIR_PIN;
    HAL_GPIO_Init(MOTOR_DIR_PORT, &gpio);

    gpio.Pin   = MOTOR_EN_PIN;
    HAL_GPIO_Init(MOTOR_EN_PORT, &gpio);

    /* 初始状态：禁能、方向 CW */
    HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_EN_PORT,  MOTOR_EN_PIN,  GPIO_PIN_SET); /* HIGH=禁能 */

    /* ---- TIM2 PWM ---- */
    motor_tim.Instance               = MOTOR_TIM;
    motor_tim.Init.Prescaler         = 0;            /* 不分频 */
    motor_tim.Init.CounterMode       = TIM_COUNTERMODE_UP;
    motor_tim.Init.Period            = 7200 - 1;     /* 72M / 7200 = 10kHz */
    motor_tim.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    motor_tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&motor_tim);

    /* PWM 输出通道：占空比 50% */
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 3600;  /* 50% duty */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&motor_tim, &oc, MOTOR_TIM_CHANNEL);

    /* 初始不输出 PWM */
    HAL_TIM_PWM_Stop(&motor_tim, MOTOR_TIM_CHANNEL);
}

/* ==================== 基本控制 ==================== */

void Motor_Enable(void)
{
    HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_RESET); /* LOW=使能 */
}

void Motor_Disable(void)
{
    HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_SET); /* HIGH=禁能 */
}

void Motor_SetDir(MotorDir dir)
{
    HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN,
                      (dir == DIR_CW) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void Motor_Stop(void)
{
    HAL_TIM_PWM_Stop(&motor_tim, MOTOR_TIM_CHANNEL);
}

/* ==================== 步进控制 ==================== */

void Motor_Step(uint32_t steps, MotorDir dir)
{
    if (steps == 0) return;

    Motor_Enable();
    Motor_SetDir(dir);

    /* 重新配置定时器周期以控制速度（默认 10kHz = 最大速度） */
    __HAL_TIM_SET_AUTORELOAD(&motor_tim, 7200 - 1);
    __HAL_TIM_SET_COMPARE(&motor_tim, MOTOR_TIM_CHANNEL, 3600);

    /* 启动 PWM 输出 */
    HAL_TIM_PWM_Start(&motor_tim, MOTOR_TIM_CHANNEL);

    /* 用定时器更新中断计数步数 */
    /* 简单实现：用滴答定时器延时估算（每步 100us @ 10kHz） */
    uint32_t delay_us = steps * 100;  /* 100us/步 @ 10kHz */
    if (delay_us < 1000) delay_us = 1000;

    /* 分块延时避免看门狗 */
    while (delay_us > 0) {
        uint32_t chunk = (delay_us > 50000) ? 50000 : delay_us;
        HAL_Delay(chunk / 1000);
        if (chunk < 1000) {
            /* 微秒级：用空循环 */
            for (volatile uint32_t i = 0; i < chunk * 10; i++) { __NOP(); }
        }
        delay_us -= chunk;
    }

    HAL_TIM_PWM_Stop(&motor_tim, MOTOR_TIM_CHANNEL);

    /* 更新位置 */
    int32_t delta = (dir == DIR_CW) ? (int32_t)steps : -(int32_t)steps;
    motor_pos += delta;
}

void Motor_GotoSlot(uint8_t slot)
{
    if (slot >= NUM_SLOTS) return;
    if (!motor_homed) {
        Motor_CalibrateHome();
        if (!motor_homed) return;
    }

    /* 计算目标位置（每个仓 60° = STEPS_PER_SLOT 步） */
    int32_t target = (int32_t)slot * STEPS_PER_SLOT;
    int32_t diff = target - motor_pos;

    /* 最短路径 */
    if (diff > MOTOR_STEPS_PER_REV / 2) {
        diff -= MOTOR_STEPS_PER_REV;
    } else if (diff < -MOTOR_STEPS_PER_REV / 2) {
        diff += MOTOR_STEPS_PER_REV;
    }

    MotorDir dir = (diff >= 0) ? DIR_CW : DIR_CCW;
    Motor_Step((uint32_t)(diff >= 0 ? diff : -diff), dir);
}

/* ==================== 零位校准 ==================== */

int Motor_CalibrateHome(void)
{
    Motor_Enable();

    /* 先慢转一整圈找零位传感器（OPTICAL2 = PB13） */
    /* 降低 PWM 频率到 500Hz 以便检测 */
    __HAL_TIM_SET_AUTORELOAD(&motor_tim, (72000000 / HOME_STEP_FREQ) - 1);
    __HAL_TIM_SET_COMPARE(&motor_tim, MOTOR_TIM_CHANNEL,
                          (72000000 / HOME_STEP_FREQ) / 2);

    HAL_TIM_PWM_Start(&motor_tim, MOTOR_TIM_CHANNEL);

    uint32_t max_steps = MOTOR_STEPS_PER_REV + 200;  /* 一圈多一点点 */
    bool found = false;

    for (uint32_t i = 0; i < max_steps; i++) {
        /* 检测零位传感器：LOW = 被遮挡（到达零位标记） */
        if (HAL_GPIO_ReadPin(OPTICAL2_PORT, OPTICAL2_PIN) == GPIO_PIN_RESET) {
            /* 去抖：再走 5 步确认 */
            HAL_Delay(2);
            if (HAL_GPIO_ReadPin(OPTICAL2_PORT, OPTICAL2_PIN) == GPIO_PIN_RESET) {
                found = true;
                break;
            }
        }
        HAL_Delay(1);  /* ~1ms/步 @ 500Hz */
    }

    HAL_TIM_PWM_Stop(&motor_tim, MOTOR_TIM_CHANNEL);

    if (!found) {
        /* 恢复默认速度 */
        __HAL_TIM_SET_AUTORELOAD(&motor_tim, 7200 - 1);
        Motor_Disable();
        return -1;
    }

    /* 零位基准 */
    motor_pos  = 0;
    motor_homed = true;

    /* 恢复默认速度 */
    __HAL_TIM_SET_AUTORELOAD(&motor_tim, 7200 - 1);
    __HAL_TIM_SET_COMPARE(&motor_tim, MOTOR_TIM_CHANNEL, 3600);
    Motor_Disable();

    return 0;
}

int32_t Motor_GetPosition(void)
{
    return motor_pos;
}
