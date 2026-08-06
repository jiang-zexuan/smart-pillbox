/**
 * @file    record.h
 * @brief   服药记录存储 — 头文件
 *
 * 使用 STM32F103C8T6 内部 Flash 模拟 EEPROM：
 *   - F103C8T6 最后一页（0x0800FC00–0x0800FFFF）共 1KB 用于存储记录
 *   - 每条记录 7 字节（timestamp 4B + slot_id 1B + taken 1B + checksum 1B）
 *   - 可存储约 140 条记录
 *   - 循环写入（满后覆盖最旧记录）
 */

#ifndef __RECORD_H
#define __RECORD_H

#include "main.h"

/* 内部 Flash 存储参数 */
#define RECORD_FLASH_ADDR       0x0800FC00   /* 最后一页起始 */
#define RECORD_FLASH_PAGE_SIZE  1024         /* F103 最后一页大小 */
#define RECORD_ENTRY_SIZE       8            /* 每条记录 8 字节（4B对齐） */
#define RECORD_MAX_COUNT        (RECORD_FLASH_PAGE_SIZE / RECORD_ENTRY_SIZE) /* 128条 */
#define RECORD_MAGIC            0xDEADBEEF   /* 数据有效标记 */

/**
 * @brief 初始化记录存储（检查 Flash 页面是否有效）
 *        如果是首次使用（全 FF），执行格式化
 */
void Record_Init(void);

/**
 * @brief 添加一条服药记录
 * @param rec 记录指针
 * @return 0=成功, -1=存储已满
 */
int  Record_Add(const MedicationRecord *rec);

/**
 * @brief 从 Flash 中读取所有记录到全局数组
 */
void Record_LoadAll(void);

/**
 * @brief 按日期获取记录数量统计
 * @param timestamp  当天 00:00:00 的 Unix 时间戳
 * @param taken_count 输出已服数量
 * @param missed_count 输出漏服数量
 */
void Record_GetStats(uint32_t timestamp, uint16_t *taken_count,
                     uint16_t *missed_count);

/**
 * @brief 获取指定日期的记录列表（存入 g_records）
 * @param timestamp 当天 00:00:00
 * @return 该天的记录条数
 */
uint16_t Record_GetByDate(uint32_t timestamp);

/**
 * @brief 擦除所有记录（格式化）
 */
void Record_EraseAll(void);

/**
 * @brief 获取存储的记录总数
 */
uint16_t Record_GetCount(void);

#endif /* __RECORD_H */
