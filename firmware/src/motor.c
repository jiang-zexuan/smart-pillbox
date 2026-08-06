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
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = MOTOR_PUL_PIN;
    HAL_GPIO_Init(MOTOR_PUL_PORT, &gpio);

    gpio.Pin = MOTOR_DIR_PIN;
    HAL_GPIO_Init(MOTOR_DIR_PORT, &gpio);

    gpio.Pin = MOTOR_EN_PIN;
    HAL_GPIO_Init(MOTOR_EN_PORT, &gpio);

    HAL_GPIO_WritePin(MOTOR_PUL_PORT, MOTOR_PUL_PIN, GPIO_PIN_SET);   /* open-drain idle: Hi-Z/off */
    HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN, GPIO_PIN_SET);   /* open-drain idle: Hi-Z/off */
    HAL_GPIO_WritePin(MOTOR_EN_PORT,  MOTOR_EN_PIN,  GPIO_PIN_SET);   /* EN not used */
}

/* ==================== 基本控制 ==================== */

void Motor_Enable(void)
{
    /* EN is not connected in the current working wiring. Keep it idle-high. */
    HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_SET);
}

void Motor_Disable(void)
{
    /* Keep EN idle-high; do not toggle the driver while testing slot moves. */
    HAL_GPIO_WritePin(MOTOR_EN_PORT, MOTOR_EN_PIN, GPIO_PIN_SET);
}

void Motor_SetDir(MotorDir dir)
{
    HAL_GPIO_WritePin(MOTOR_DIR_PORT, MOTOR_DIR_PIN,
                      (dir == DIR_CW) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void Motor_Stop(void)
{
    HAL_GPIO_WritePin(MOTOR_PUL_PORT, MOTOR_PUL_PIN, GPIO_PIN_SET);
}

/* ==================== 步进控制 ==================== */

static void Motor_DelayUs(uint32_t us)
{
    while (us--) {
        for (volatile uint32_t i = 0; i < 8; i++) { __NOP(); }
    }
}

void Motor_Step(uint32_t steps, MotorDir dir)
{
    if (steps == 0) return;

    Motor_Enable();
    HAL_Delay(10);
    Motor_SetDir(dir);
    HAL_Delay(5);

    for (uint32_t i = 0; i < steps; i++) {
        HAL_GPIO_WritePin(MOTOR_PUL_PORT, MOTOR_PUL_PIN, GPIO_PIN_RESET); /* opto on */
        Motor_DelayUs(2000);
        HAL_GPIO_WritePin(MOTOR_PUL_PORT, MOTOR_PUL_PIN, GPIO_PIN_SET);   /* opto off */
        Motor_DelayUs(2000);
    }

    int32_t delta = (dir == DIR_CW) ? (int32_t)steps : -(int32_t)steps;
    motor_pos += delta;
}

void Motor_GotoSlot(uint8_t slot)
{
    if (slot >= NUM_SLOTS) return;

    /* Relative 6-slot positioning: current power-on position is slot 0.
     * Full-step mode: 200 steps/rev, target slots at 0/33/66/100/133/166.
     * Later, optical homing can set motor_pos=0 before using this. */
    int32_t target = ((int32_t)slot * (int32_t)MOTOR_STEPS_PER_REV) / (int32_t)NUM_SLOTS;
    int32_t pos = motor_pos % (int32_t)MOTOR_STEPS_PER_REV;
    if (pos < 0) pos += (int32_t)MOTOR_STEPS_PER_REV;

    int32_t diff = target - pos;
    if (diff > (int32_t)MOTOR_STEPS_PER_REV / 2) {
        diff -= (int32_t)MOTOR_STEPS_PER_REV;
    } else if (diff < -((int32_t)MOTOR_STEPS_PER_REV / 2)) {
        diff += (int32_t)MOTOR_STEPS_PER_REV;
    }

    MotorDir dir = (diff >= 0) ? DIR_CW : DIR_CCW;
    uint32_t steps = (uint32_t)(diff >= 0 ? diff : -diff);
    Motor_Step(steps, dir);
    Motor_Stop();
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
