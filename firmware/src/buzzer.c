/**
 * @file    buzzer.c
 * @brief   有源蜂鸣器驱动实现
 */

#include "buzzer.h"
#include "pinmap.h"

void Buzzer_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = BUZZER_PIN;
    HAL_GPIO_Init(BUZZER_PORT, &gpio);

    Buzzer_Off();
}

void Buzzer_On(void)
{
    /* 低电平触发蜂鸣器：拉低 = 响 */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void Buzzer_Off(void)
{
    /* 低电平触发蜂鸣器：拉高 = 关闭 */
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

void Buzzer_Beep(uint16_t on_ms)
{
    Buzzer_On();
    HAL_Delay(on_ms);
    Buzzer_Off();
}

void Buzzer_Remind(void)
{
    for (int i = 0; i < 3; i++) {
        Buzzer_Beep(160);
        HAL_Delay(120);
    }
}

void Buzzer_Remind10s(void)
{
    Buzzer_On();
    HAL_Delay(10000);
    Buzzer_Off();
}

void Buzzer_Alarm(void)
{
    for (int i = 0; i < 6; i++) {
        Buzzer_Beep(90);
        HAL_Delay(80);
    }
}

void Buzzer_BootOk(void)
{
    Buzzer_Beep(80);
    HAL_Delay(80);
    Buzzer_Beep(80);
}
