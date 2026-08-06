/**
 * @file    buzzer.h
 * @brief   有源蜂鸣器驱动
 */

#ifndef __BUZZER_H
#define __BUZZER_H

#include "main.h"

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Beep(uint16_t on_ms);
void Buzzer_Remind(void);
void Buzzer_Remind10s(void);
void Buzzer_Alarm(void);
void Buzzer_BootOk(void);

#endif /* __BUZZER_H */
