/**
 * @file    oled.h
 * @brief   SSD1306 128x64 OLED I²C 驱动 — 头文件
 *
 * I²C 地址：通常 0x3C（SA0=GND）或 0x3D（SA0=VCC）
 * 共用 I²C1 总线（PB6 SCL / PB7 SDA），与 DS3231 RTC 同一条
 */

#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/* OLED 尺寸 */
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_PAGES   (OLED_HEIGHT / 8)  /* 8 页，每页 8 像素高 */

/* 常用 I²C 地址 */
#define SSD1306_ADDR_0  0x78  /* 0x3C << 1 */
#define SSD1306_ADDR_1  0x7A  /* 0x3D << 1 */

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化 OLED
 *        扫描 I²C 总线，自动检测地址（0x3C / 0x3D）
 * @return 检测到的地址（如 0x3C），0 表示未找到设备
 */
uint8_t OLED_Init(void);

/**
 * @brief 清屏（全部填充 0x00）
 */
void OLED_Clear(void);

/**
 * @brief 全屏填充
 * @param pattern  填充字节
 */
void OLED_Fill(uint8_t pattern);

/**
 * @brief 设置光标位置
 * @param page  页（0–7），每页 8 像素
 * @param col   列（0–127）
 */
void OLED_SetCursor(uint8_t page, uint8_t col);

/**
 * @brief 写一个字符（5×8 点阵，当前光标位置）
 * @param ch  ASCII 字符（0x20–0x7F）
 */
void OLED_PutChar(char ch);

/**
 * @brief 写字符串（自动换行）
 * @param s  字符串指针
 */
void OLED_PutString(const char *s);

/**
 * @brief 格式化输出（类 printf，最多 128 字节）
 */
void OLED_Printf(const char *fmt, ...);

/**
 * @brief 设置指定像素
 * @param x  列（0–127）
 * @param y  行（0–63）
 * @param on 1=点亮, 0=熄灭
 */
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t on);

/**
 * @brief 画水平线
 */
void OLED_DrawHLine(uint8_t x, uint8_t y, uint8_t w);

/**
 * @brief 画填充矩形
 */
void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/**
 * @brief 显示调试信息页（替代串口打印）
 *        第0行=标题, 第1-7行=日志
 * @param line  行号（0–7）
 * @param text  文本内容
 */
void OLED_DebugLine(uint8_t line, const char *text);

#endif /* __OLED_H */
