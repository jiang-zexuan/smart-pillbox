package com.pillbox.app.model

/**
 * 服药记录
 */
data class MedicationRecord(
    val timestamp: Long,   // Unix 时间戳（毫秒）
    val slotId: Int,       // 仓位号
    val taken: Boolean,    // true=已服 false=漏服
    val medicineName: String = ""  // 药品名称
)
