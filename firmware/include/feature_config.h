/**
 * @file    feature_config.h
 * @brief   褰撳墠纭欢閰嶇疆寮€鍏? *
 * 鐜伴樁娈靛凡鍒拌揣纭欢锛? *   JDY-31銆丠X711銆丵R100銆丱LED銆乀JC8048X270-011R銆佽渹楦ｅ櫒銆丼T-Link銆乁SB-TTL
 *
 * 鍚庣画棰勭暀锛? *   鐢垫満/椹卞姩銆丏S3231 RTC銆佸厜鐢典紶鎰熷櫒銆佷覆鍙ｈ闊虫ā鍧? */

#ifndef __FEATURE_CONFIG_H
#define __FEATURE_CONFIG_H

/* 宸插埌璐х‖浠?*/
#define FEATURE_BLE_JDY31          1
#define FEATURE_HX711              1
#define FEATURE_OLED               1
#define FEATURE_TJC_HMI            1
#define FEATURE_QR100              1
#define FEATURE_BUZZER             1
#define FEATURE_DEBUG_UART         0
#define FEATURE_BLE_AT_CONFIG      0

/* 棰勭暀纭欢锛氬埌璐у悗鏀规垚 1锛屽啀鎺ョ嚎璋冭瘯 */
#define FEATURE_MOTOR_RESERVED     1
#define FEATURE_DS3231_RTC         0
#define FEATURE_OPTICAL_SENSORS    0
#define FEATURE_SERIAL_VOICE       0

/* HX711 鍙栬嵂鍒ゅ畾闃堝€硷紝鍗曚綅 0.01g锛?00 = 5g */
#define TAKE_WEIGHT_DROP_THRESHOLD 500

/* 婕忔湇绛夊緟鏃堕棿锛屽崟浣嶇 */
#define MISSED_TIMEOUT_SECONDS     1800UL

#endif /* __FEATURE_CONFIG_H */
