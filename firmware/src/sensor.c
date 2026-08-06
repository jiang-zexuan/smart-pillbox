/**
 * @file    sensor.c
 * @brief   HX711 称重传感器 + ITR9606 光电开关 驱动 — 实现
 *
 * HX711 时序：
 *   1. 等待 DOUT 变低（数据就绪）
 *   2. 发 25 个 SCK 脉冲，每脉冲读 1 bit（24 data + 1 增益选择）
 *   3. 增益 128 → 第 25 脉冲 HIGH
 */

#include "main.h"
#include "pinmap.h"
#include "sensor.h"
#include "feature_config.h"

/* HX711 零点偏移 */
static int32_t hx711_offset = 0;

/* 标定系数 — 需实际标定后修改 */
/* 标定方法：放已知重量砝码（如 10g 硬币），读差值，算出每克的 ADC 值 */
#define HX711_SCALE  (450.0f)  /* ADC count per gram; later calibrate with known weight */

/* Tare jitter deadband, unit 0.01g; 25 = 0.25g */
#define HX711_ZERO_DEADBAND_CG  25

/* 滑动平均滤波窗口 */
#define FILTER_WINDOW 4
static int32_t filter_buf[FILTER_WINDOW] = {0};
static uint8_t filter_idx = 0;
static int64_t filter_sum = 0;
static uint8_t filter_count = 0;

/* ITR9606 状态 */
static volatile bool optical1_triggered = false;  /* 取药通道 */

/* ==================== 初始化 ==================== */

void Sensor_Init(void)
{
    /* ---- GPIO 时钟 ---- */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    /* HX711：PB0 推挽输出(SCK), PB1 浮空输入(DOUT) */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = HX711_SCK_PIN;
    HAL_GPIO_Init(HX711_SCK_PORT, &gpio);

    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Pin   = HX711_DOUT_PIN;
    HAL_GPIO_Init(HX711_DOUT_PORT, &gpio);

#if FEATURE_OPTICAL_SENSORS
    /* ITR9606：PB12, PB13 输入 */
    gpio.Mode  = GPIO_MODE_IT_FALLING;  /* 下降沿中断（遮挡→有物体）*/
    gpio.Pull  = GPIO_PULLUP;
    gpio.Pin   = OPTICAL1_PIN;        /* 取药通道 */
    HAL_GPIO_Init(OPTICAL1_PORT, &gpio);

    gpio.Mode  = GPIO_MODE_INPUT;     /* 零位用轮询 */
    gpio.Pull  = GPIO_PULLUP;
    gpio.Pin   = OPTICAL2_PIN;        /* 转盘零位 */
    HAL_GPIO_Init(OPTICAL2_PORT, &gpio);

    /* 配置 EXTI 中断优先级 */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
#endif

    /* 初始 SCK 低 */
    HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET);
}

/* ==================== HX711 读取 ==================== */

int32_t HX711_Read(void)
{
    int32_t val = 0;
    uint32_t timeout = 100000;

    /* 等待 DOUT 变低 */
    while (HAL_GPIO_ReadPin(HX711_DOUT_PORT, HX711_DOUT_PIN) != GPIO_PIN_RESET) {
        if (--timeout == 0) return 0;  /* 超时 */
    }

    /* 读 24 位数据 */
    for (int i = 0; i < 24; i++) {
        HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_SET);
        /* 最小 SCK 高电平脉冲宽度：0.2us */
        for (volatile int d = 0; d < 10; d++) { __NOP(); }
        val <<= 1;
        if (HAL_GPIO_ReadPin(HX711_DOUT_PORT, HX711_DOUT_PIN) == GPIO_PIN_SET) {
            val |= 1;
        }
        HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET);
        for (volatile int d = 0; d < 10; d++) { __NOP(); }
    }

    /* 第 25 个脉冲 — 设置下次增益为 128 */
    HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_SET);
    for (volatile int d = 0; d < 10; d++) { __NOP(); }
    HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET);

    /* 符号扩展（24-bit → 32-bit） */
    if (val & 0x800000) {
        val |= 0xFF000000;
    }

    return val;
}

int32_t HX711_ReadWeight(void)
{
    int32_t raw_avg = 0;

    if (filter_count > 0) {
        raw_avg = (int32_t)(filter_sum / filter_count);
    } else {
        raw_avg = HX711_Read();
    }

    int32_t raw = raw_avg - hx711_offset;

    /* Load cell direction may be positive or negative; display grams as positive. */
    if (raw < 0) raw = -raw;

    /* Unit: 0.01g. Example: 1234 = 12.34g */
    int32_t weight_cg = (int32_t)((float)raw / HX711_SCALE * 100.0f);

    /* Suppress small +/- jitter around tare point. */
    if (weight_cg < HX711_ZERO_DEADBAND_CG) {
        weight_cg = 0;
    }

    return weight_cg;
}
void HX711_Tare(void)
{
    /* 多次采样求平均值作为零点 */
    const int tare_samples = 10;
    int64_t sum = 0;
    int valid = 0;

    for (int i = 0; i < tare_samples; i++) {
        int32_t v = HX711_Read();
        if (v != 0) {
            sum += v;
            valid++;
        }
        HAL_Delay(50);  /* 等待下一次转换 */
    }

    if (valid > 0) {
        hx711_offset = (int32_t)(sum / valid);
        filter_idx = 0;
        filter_sum = 0;
        filter_count = 0;
        memset(filter_buf, 0, sizeof(filter_buf));
    }
}

void HX711_FilterUpdate(void)
{
    int32_t raw = HX711_Read();
    if (raw == 0) return;

    /* 滑动平均 */
    filter_sum -= filter_buf[filter_idx];
    filter_buf[filter_idx] = raw;
    filter_sum += raw;
    filter_idx = (filter_idx + 1) % FILTER_WINDOW;
    if (filter_count < FILTER_WINDOW) filter_count++;
}

/* ==================== ITR9606 光电传感器 ==================== */

bool ITR9606_DispenseDetected(void)
{
#if FEATURE_OPTICAL_SENSORS
    if (optical1_triggered) {
        optical1_triggered = false;
        /* 去抖：再读一次确认 */
        HAL_Delay(5);
        if (HAL_GPIO_ReadPin(OPTICAL1_PORT, OPTICAL1_PIN) == GPIO_PIN_RESET) {
            return true;
        }
    }
#endif
    return false;
}

bool ITR9606_HomeDetected(void)
{
#if FEATURE_OPTICAL_SENSORS
    /* 零位传感器：LOW = 转盘标记遮挡 */
    return (HAL_GPIO_ReadPin(OPTICAL2_PORT, OPTICAL2_PIN) == GPIO_PIN_RESET);
#else
    return false;
#endif
}

/* ==================== EXTI 中断回调 ==================== */

/**
 * @brief EXTI9_5 或 EXTI15_10 中断处理
 *        在 stm32f1xx_it.c 的 EXTI15_10_IRQHandler() 中调用
 */
void Sensor_IRQHandler(uint16_t pin)
{
#if FEATURE_OPTICAL_SENSORS
    if (pin == OPTICAL1_PIN) {
        optical1_triggered = true;
    }
#else
    (void)pin;
#endif
    /* 清除 EXTI 标志在 HAL_GPIO_EXTI_IRQHandler 中完成 */
}
