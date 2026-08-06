package com.pillbox.app.data

import android.content.Context
import android.content.SharedPreferences
import com.pillbox.app.model.MedicationPlan
import com.pillbox.app.model.MedicationRecord
import org.json.JSONArray
import org.json.JSONObject

/**
 * 本地存储（SharedPreferences）
 */
class LocalStorage(context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences("pillbox_prefs", Context.MODE_PRIVATE)

    // ==================== 监护人号码 ====================

    fun getGuardianPhone(): String = prefs.getString(KEY_GUARDIAN_PHONE, "") ?: ""

    fun setGuardianPhone(phone: String) {
        prefs.edit().putString(KEY_GUARDIAN_PHONE, phone).apply()
    }

    // ==================== 通知开关 ====================

    fun isAlertNotifyEnabled(): Boolean = prefs.getBoolean(KEY_ALERT_NOTIFY, true)
    fun setAlertNotifyEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_ALERT_NOTIFY, enabled).apply()
    }

    fun isAdvanceNotifyEnabled(): Boolean = prefs.getBoolean(KEY_ADVANCE_NOTIFY, true)
    fun setAdvanceNotifyEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_ADVANCE_NOTIFY, enabled).apply()
    }

    fun isVibrateEnabled(): Boolean = prefs.getBoolean(KEY_VIBRATE, true)
    fun setVibrateEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_VIBRATE, enabled).apply()
    }

    fun isSmsEnabled(): Boolean = prefs.getBoolean(KEY_SMS_ENABLED, true)
    fun setSmsEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_SMS_ENABLED, enabled).apply()
    }

    fun isWeightPopupEnabled(): Boolean = prefs.getBoolean(KEY_WEIGHT_POPUP, true)
    fun setWeightPopupEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_WEIGHT_POPUP, enabled).apply()
    }

    // ==================== 用药计划 ====================

    fun savePlans(plans: List<MedicationPlan>) {
        val json = JSONArray()
        plans.forEach { plan ->
            json.put(JSONObject().apply {
                put("slotId", plan.slotId)
                put("hour", plan.hour)
                put("minute", plan.minute)
                put("period", plan.period)
                put("enabled", plan.enabled)
            })
        }
        prefs.edit().putString(KEY_PLANS, json.toString()).apply()
    }

    fun loadPlans(): List<MedicationPlan> {
        val jsonStr = prefs.getString(KEY_PLANS, null) ?: return getDefaultPlans()
        return try {
            val arr = JSONArray(jsonStr)
            (0 until arr.length()).map { i ->
                val obj = arr.getJSONObject(i)
                MedicationPlan(
                    slotId = obj.getInt("slotId"),
                    hour = obj.getInt("hour"),
                    minute = obj.getInt("minute"),
                    period = obj.getInt("period"),
                    enabled = obj.getBoolean("enabled")
                )
            }
        } catch (e: Exception) {
            getDefaultPlans()
        }
    }

    private fun getDefaultPlans(): List<MedicationPlan> = listOf(
        MedicationPlan(slotId = 0, hour = 8, minute = 0, period = 0, enabled = true),
        MedicationPlan(slotId = 1, hour = 12, minute = 30, period = 1, enabled = true),
        MedicationPlan(slotId = 2, hour = 19, minute = 0, period = 2, enabled = true)
    )

    // ==================== 服药记录 ====================

    fun saveRecords(records: List<MedicationRecord>) {
        val json = JSONArray()
        records.forEach { rec ->
            json.put(JSONObject().apply {
                put("timestamp", rec.timestamp)
                put("slotId", rec.slotId)
                put("taken", rec.taken)
                put("medicineName", rec.medicineName)
            })
        }
        prefs.edit().putString(KEY_RECORDS, json.toString()).apply()
    }

    fun loadRecords(): List<MedicationRecord> {
        val jsonStr = prefs.getString(KEY_RECORDS, null) ?: return emptyList()
        return try {
            val arr = JSONArray(jsonStr)
            (0 until arr.length()).map { i ->
                val obj = arr.getJSONObject(i)
                MedicationRecord(
                    timestamp = obj.getLong("timestamp"),
                    slotId = obj.getInt("slotId"),
                    taken = obj.getBoolean("taken"),
                    medicineName = obj.optString("medicineName", "")
                )
            }
        } catch (e: Exception) {
            emptyList()
        }
    }

    fun addRecord(record: MedicationRecord) {
        val records = loadRecords().toMutableList()
        records.add(0, record)
        // 最多保留 200 条
        if (records.size > 200) records.subList(200, records.size).clear()
        saveRecords(records)
    }

    // ==================== 扫码药品 / 药仓 ====================

    fun getMedicineNameByBarcode(barcode: String): String? {
        val jsonStr = prefs.getString(KEY_BARCODE_MEDICINES, null) ?: return null
        return try {
            val obj = JSONObject(jsonStr)
            obj.optString(barcode, "").takeIf { it.isNotBlank() }
        } catch (e: Exception) {
            null
        }
    }

    fun saveBarcodeMedicine(barcode: String, medicineName: String) {
        val obj = try {
            JSONObject(prefs.getString(KEY_BARCODE_MEDICINES, "{}") ?: "{}")
        } catch (e: Exception) {
            JSONObject()
        }
        obj.put(barcode, medicineName)
        prefs.edit().putString(KEY_BARCODE_MEDICINES, obj.toString()).apply()
    }

    fun saveSlotMedicine(slotId: Int, medicineName: String, barcode: String = "") {
        val obj = try {
            JSONObject(prefs.getString(KEY_SLOT_MEDICINES, "{}") ?: "{}")
        } catch (e: Exception) {
            JSONObject()
        }
        obj.put(slotId.toString(), JSONObject().apply {
            put("medicineName", medicineName)
            put("barcode", barcode)
        })
        prefs.edit().putString(KEY_SLOT_MEDICINES, obj.toString()).apply()
    }

    fun clearSlotMedicine(slotId: Int) {
        val obj = try {
            JSONObject(prefs.getString(KEY_SLOT_MEDICINES, "{}") ?: "{}")
        } catch (e: Exception) {
            JSONObject()
        }
        obj.remove(slotId.toString())
        prefs.edit().putString(KEY_SLOT_MEDICINES, obj.toString()).apply()
    }

    fun loadSlotMedicines(): Map<Int, String> {
        val jsonStr = prefs.getString(KEY_SLOT_MEDICINES, null) ?: return emptyMap()
        return try {
            val obj = JSONObject(jsonStr)
            val map = mutableMapOf<Int, String>()
            obj.keys().forEach { key ->
                val slot = key.toIntOrNull() ?: return@forEach
                val name = obj.optJSONObject(key)?.optString("medicineName", "").orEmpty()
                if (name.isNotBlank()) map[slot] = name
            }
            map
        } catch (e: Exception) {
            emptyMap()
        }
    }

    // ==================== 模拟数据 ====================

    fun getDemoSlots(): Map<Int, String> = loadSlotMedicines()

    fun getDemoRecords(): List<MedicationRecord> {
        val now = System.currentTimeMillis()
        return listOf(
            MedicationRecord(now - 3600000, 0, true, "阿司匹林"),
            MedicationRecord(now - 86400000, 1, false, "降压药")
        )
    }

    companion object {
        private const val KEY_GUARDIAN_PHONE = "guardian_phone"
        private const val KEY_ALERT_NOTIFY = "alert_notify"
        private const val KEY_ADVANCE_NOTIFY = "advance_notify"
        private const val KEY_VIBRATE = "vibrate"
        private const val KEY_SMS_ENABLED = "sms_enabled"
        private const val KEY_WEIGHT_POPUP = "weight_popup_enabled"
        private const val KEY_PLANS = "medication_plans"
        private const val KEY_RECORDS = "medication_records"
        private const val KEY_BARCODE_MEDICINES = "barcode_medicines"
        private const val KEY_SLOT_MEDICINES = "slot_medicines"
    }
}
