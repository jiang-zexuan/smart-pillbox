/**
 * @file    protocol.h
 * @brief   钃濈墮閫氫俊鍗忚瑙ｆ瀽 鈥?澶存枃浠? *
 * 鍗忚鏍煎紡锛堥€楀彿鍒嗛殧锛孿r\n 缁撳熬锛夛細
 *   APP 鈫?MCU:
 *     PLAN:slot,hh,mm,period,en    璁剧疆鐢ㄨ嵂璁″垝
 *     SET:slot,medname             璁剧疆鑽粨鑽搧
 *     TIME:unix_timestamp          鍚屾杞欢鏃堕棿锛堟棤 DS3231 鏃跺繀闇€锛? *     TEST:slot                    鎵嬪姩瑙﹀彂鏌愪粨鎻愰啋锛屼究浜庢紨绀? *     BEEP                         娴嬭瘯铚傞福鍣? *     TARE                         HX711 鍘荤毊
 *     SYNC:ALL                     璇锋眰鍏ㄩ噺鍚屾
 *   MCU 鈫?APP:
 *     STATUS:slots|plans           鍏ㄩ噺鐘舵€? *     ALERT:slot,hh,mm,name       婕忔湇鎶ヨ
 *     REC:timestamp,slot,taken    鏈嶈嵂璁板綍
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "main.h"

/* 鍛戒护绫诲瀷 */
typedef enum {
    CMD_NONE = 0,
    CMD_PLAN,      /* PLAN:slot,hh,mm,period,en */
    CMD_SET,       /* SET:slot,medname */
    CMD_SYNC,      /* SYNC:ALL */
    CMD_TIME,      /* TIME:timestamp */
    CMD_TEST,      /* TEST:slot */
    CMD_BEEP,      /* BEEP */
    CMD_TARE       /* TARE */
} CommandType;

/**
 * @brief 瑙ｆ瀽鏀跺埌鐨勮摑鐗欐暟鎹
 *        鍦ㄥぇ寰幆涓皟鐢?BLE_ReadLine() 鑾峰彇涓€琛?鈫?璋冩鍑芥暟
 * @param line  涓€琛屾枃鏈紙涓嶅惈 \r\n锛? */
void Protocol_ParseLine(const char *line);

/**
 * @brief 瑙ｆ瀽涓€鏉?PLAN 鍛戒护
 * @param payload "slot,hh,mm,period,en"
 */
void Protocol_HandlePlan(const char *payload);

/**
 * @brief 瑙ｆ瀽涓€鏉?SET 鍛戒护
 * @param payload "slot,medname"
 */
void Protocol_HandleSet(const char *payload);
void Protocol_HandleTime(const char *payload);
void Protocol_HandleTest(const char *payload);
void Protocol_HandleMotor(const char *payload);
void Protocol_HandleBeep(void);
void Protocol_HandleTare(void);

#endif /* __PROTOCOL_H */

