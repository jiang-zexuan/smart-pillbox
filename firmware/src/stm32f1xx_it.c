/**
 * @file    stm32f1xx_it.c
 * @brief   中断服务函数
 *
 * 中断向量表在 startup_stm32f103xb.s 中定义
 */

#include "main.h"
#include "pinmap.h"
#include "ble.h"
#include "hmi.h"
#include "sensor.h"
#include "qr100.h"

/* 外部函数声明 */
extern void BLE_IRQHandler(uint16_t sr);
extern void HMI_IRQHandler(uint16_t sr);
extern void Sensor_IRQHandler(uint16_t pin);
extern void QR100_IRQHandler(uint16_t sr);

/* ==================== USART2 中断（蓝牙） ==================== */

void USART2_IRQHandler(void)
{
    uint16_t sr = USART2->SR;
    BLE_IRQHandler(sr);
}

/* ==================== USART1 中断（TJC 屏幕） ==================== */

void USART1_IRQHandler(void)
{
    uint16_t sr = USART1->SR;
    HMI_IRQHandler(sr);
}

/* ==================== USART3 中断（QR100 扫码模块） ==================== */

void USART3_IRQHandler(void)
{
    uint16_t sr = USART3->SR;
    QR100_IRQHandler(sr);
}

/* ==================== EXTI 中断（ITR9606 取药通道） ==================== */

void EXTI15_10_IRQHandler(void)
{
    /* PB12 (EXTI12) — 取药通道传感器 */
    if (__HAL_GPIO_EXTI_GET_IT(OPTICAL1_PIN) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(OPTICAL1_PIN);
        Sensor_IRQHandler(OPTICAL1_PIN);
    }
}

/* ==================== HardFault / NMI / 默认处理 ==================== */

void NMI_Handler(void)           { while (1) {} }
void HardFault_Handler(void)     { while (1) {} }
void MemManage_Handler(void)     { while (1) {} }
void BusFault_Handler(void)      { while (1) {} }
void UsageFault_Handler(void)    { while (1) {} }
void SVC_Handler(void)           {}
void DebugMon_Handler(void)      {}
void PendSV_Handler(void)        {}
