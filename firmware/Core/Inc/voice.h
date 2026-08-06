/**
 * @file    voice.h
 * @brief   DY-SV17F 语音播报模块驱动 — 头文件
 *
 * 接口：USART3（PB10 TX / PB11 RX），5V 电平
 * DY-SV17F 8 脚定义：
 *   ① VCC(5V) ② GND ③ TX ④ RX ⑤ IO1 ⑥ IO2 ⑦ SPK+ ⑧ SPK-
 *
 * 串口播放指令（16进制）：
 *   AA 07 02 00 01 00 00 55 — 播放第 1 段语音
 *   AA 07 02 00 02 00 00 55 — 播放第 2 段
 *   通用格式：AA 07 02 00 <file_H> <file_L> 00 <checksum>
 *   checksum = (07+02+00+file_H+file_L+00) & 0xFF
 *
 * 预录语音文件（存模块 Flash，USB 拷贝）：
 *   001.mp3 = "请服用1号仓药品"
 *   002.mp3 = "请服用2号仓药品"
 *   003.mp3 = "请服用3号仓药品"
 *   004.mp3 = "已检测到取药，请按时服用"
 *   005.mp3 = "用药时间已过，请尽快服药"
 *   006.mp3 = "开机自检完成，系统正常"
 *   007.mp3 = "系统时间已校准"
 */

#ifndef __VOICE_H
#define __VOICE_H

#include "main.h"

/* 语音文件编号 */
#define VOICE_TAKE_MED1     1    /* 1号仓 */
#define VOICE_TAKE_MED2     2    /* 2号仓 */
#define VOICE_TAKE_MED3     3    /* 3号仓 */
#define VOICE_DETECTED      4    /* 已取药 */
#define VOICE_MISSED        5    /* 漏服警告 */
#define VOICE_BOOT_OK       6    /* 开机正常 */
#define VOICE_TIME_SET      7    /* 时间校准 */

/**
 * @brief 初始化 USART3 连接语音模块
 */
void Voice_Init(void);

/**
 * @brief 播放指定编号的语音
 * @param file_id  语音文件编号 1–255
 */
void Voice_Play(uint8_t file_id);

/**
 * @brief 停止播放
 */
void Voice_Stop(void);

/**
 * @brief 播放指定药仓的用药提醒
 */
void Voice_RemindSlot(uint8_t slot);

#endif /* __VOICE_H */
