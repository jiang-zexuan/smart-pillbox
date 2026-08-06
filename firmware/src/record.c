/**
 * @file    record.c
 * @brief   服药记录存储 — Flash 模拟 EEPROM 实现
 *
 * 使用 STM32F103C8T6 最后一页 1KB Flash
 * 每条记录 8 字节（4 字节对齐）：
 *   [31:0]  timestamp, [39:32] slot_id, [47:40] taken, [55:48] reserved, [63:56] checksum
 * 校验和 = (timestamp + slot_id + taken + reserved) & 0xFF
 * 循环写入：写到头后回到起始地址覆盖旧数据
 */

#include "main.h"
#include "record.h"

typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uint8_t  slot_id;
    uint8_t  taken;
    uint8_t  reserved;
    uint8_t  checksum;
} RecordEntry;

/* 当前写入偏移 */
static uint16_t rec_write_offset = 0;
static uint16_t rec_total_count  = 0;

/* ==================== 初始化 ==================== */

void Record_Init(void)
{
    /* 扫描找到最后一条有效记录的位置 */
    rec_total_count = 0;
    rec_write_offset = 0;

    for (uint16_t i = 0; i < RECORD_MAX_COUNT; i++) {
        uint32_t addr = RECORD_FLASH_ADDR + i * RECORD_ENTRY_SIZE;
        uint32_t ts  = *(volatile uint32_t *)addr;

        if (ts == 0xFFFFFFFF || ts == 0x00000000) {
            /* 空条目 → 从这里开始写 */
            rec_write_offset = i;
            break;
        }
        rec_total_count++;
        rec_write_offset = i + 1;
    }

    /* 如果页已满，从头开始（循环覆盖） */
    if (rec_write_offset >= RECORD_MAX_COUNT) {
        rec_write_offset = 0;
    }
}

/* ==================== 添加记录 ==================== */

int Record_Add(const MedicationRecord *rec)
{
    if (rec_write_offset >= RECORD_MAX_COUNT) {
        return -1;  /* 不应该到这里 */
    }

    RecordEntry entry;
    entry.timestamp = rec->timestamp;
    entry.slot_id   = rec->slot_id;
    entry.taken     = rec->taken;
    entry.reserved  = 0;
    entry.checksum  = (uint8_t)(
        (rec->timestamp & 0xFF) +
        rec->slot_id + rec->taken) & 0xFF;

    uint32_t addr = RECORD_FLASH_ADDR + rec_write_offset * RECORD_ENTRY_SIZE;

    /* 解锁 Flash */
    HAL_FLASH_Unlock();

    /* 擦除目标（如果是页首） */
    if (rec_write_offset == 0) {
        FLASH_EraseInitTypeDef erase = {0};
        erase.TypeErase   = FLASH_TYPEERASE_PAGES;
        erase.PageAddress = RECORD_FLASH_ADDR;
        erase.NbPages     = 1;
        uint32_t page_error;
        HAL_FLASHEx_Erase(&erase, &page_error);
    }

    /* 按字（32-bit）写入 */
    uint32_t *data = (uint32_t *)&entry;
    for (int i = 0; i < RECORD_ENTRY_SIZE / 4; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          addr + i * 4, data[i]);
    }

    HAL_FLASH_Lock();

    /* 更新索引 */
    rec_write_offset++;
    if (rec_write_offset >= RECORD_MAX_COUNT) {
        rec_write_offset = 0;  /* 循环 */
    }
    if (rec_total_count < RECORD_MAX_COUNT) {
        rec_total_count++;
    }

    return 0;
}

/* ==================== 读取记录 ==================== */

void Record_LoadAll(void)
{
    g_record_count = 0;

    for (uint16_t i = 0; i < rec_total_count; i++) {
        uint16_t idx = (rec_write_offset >= rec_total_count)
            ? i
            : ((rec_write_offset + i) % RECORD_MAX_COUNT);

        uint32_t addr = RECORD_FLASH_ADDR + idx * RECORD_ENTRY_SIZE;
        RecordEntry entry;
        memcpy(&entry, (void *)addr, sizeof(RecordEntry));

        /* 校验 */
        uint8_t calc_checksum = (uint8_t)(
            (entry.timestamp & 0xFF) +
            entry.slot_id + entry.taken) & 0xFF;

        if (entry.timestamp != 0xFFFFFFFF &&
            entry.timestamp != 0x00000000 &&
            entry.checksum == calc_checksum) {

            g_records[g_record_count].timestamp = entry.timestamp;
            g_records[g_record_count].slot_id   = entry.slot_id;
            g_records[g_record_count].taken     = entry.taken;
            g_record_count++;

            if (g_record_count >= 128) break;
        }
    }
}

/* ==================== 统计查询 ==================== */

void Record_GetStats(uint32_t day_start, uint16_t *taken_count,
                     uint16_t *missed_count)
{
    *taken_count  = 0;
    *missed_count = 0;

    uint32_t day_end = day_start + 86400;

    for (uint16_t i = 0; i < g_record_count; i++) {
        if (g_records[i].timestamp >= day_start &&
            g_records[i].timestamp < day_end) {
            if (g_records[i].taken) {
                (*taken_count)++;
            } else {
                (*missed_count)++;
            }
        }
    }
}

uint16_t Record_GetByDate(uint32_t day_start)
{
    uint32_t day_end = day_start + 86400;
    uint16_t count = 0;

    for (uint16_t i = 0; i < g_record_count; i++) {
        if (g_records[i].timestamp >= day_start &&
            g_records[i].timestamp < day_end) {
            g_records[count] = g_records[i];
            count++;
            if (count >= 128) break;
        }
    }
    return count;
}

/* ==================== 擦除 ==================== */

void Record_EraseAll(void)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = RECORD_FLASH_ADDR;
    erase.NbPages     = 1;
    uint32_t page_error;
    HAL_FLASHEx_Erase(&erase, &page_error);

    HAL_FLASH_Lock();

    rec_write_offset = 0;
    rec_total_count  = 0;
}

uint16_t Record_GetCount(void)
{
    return rec_total_count;
}
