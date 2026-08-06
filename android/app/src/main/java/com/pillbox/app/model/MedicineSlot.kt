package com.pillbox.app.model

/**
 * 药仓信息
 */
data class MedicineSlot(
    val slotId: Int,           // 仓位号 0-5
    val medicineName: String,  // 药品名称
    val barcode: String = "",  // 条码
    val hasMedicine: Boolean = false,  // 是否有药
    val planHour: Int = -1,    // 计划时间-时（-1=未设置）
    val planMinute: Int = -1,  // 计划时间-分
    val period: Int = -1       // 时段 0=早 1=中 2=晚
)
