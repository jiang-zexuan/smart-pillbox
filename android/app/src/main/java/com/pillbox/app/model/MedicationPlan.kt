package com.pillbox.app.model

/**
 * 用药计划
 */
data class MedicationPlan(
    val slotId: Int,       // 仓位号
    val hour: Int,         // 提醒时间-时
    val minute: Int,       // 提醒时间-分
    val period: Int,       // 0=早 1=中 2=晚
    val enabled: Boolean = true  // 是否启用
)
