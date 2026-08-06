/**
 * @file    qr100.h
 * @brief   QR100 条码/二维码扫描模块串口驱动
 */

#ifndef __QR100_H
#define __QR100_H

#include "main.h"

#define QR100_RX_BUF_SIZE 256

void QR100_Init(void);
void QR100_Poll(void);
uint16_t QR100_ReadLine(char *buf, uint16_t maxlen);
uint16_t QR100_ReadFrame(char *buf, uint16_t maxlen);
void QR100_IRQHandler(uint16_t sr);

#endif /* __QR100_H */
