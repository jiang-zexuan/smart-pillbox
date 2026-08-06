package com.pillbox.app.notification

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.telephony.SmsManager
import android.widget.Toast
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

/**
 * 短信发送
 */
class SmsSender(private val context: Context) {

    /**
     * 发送漏服短信给监护人
     */
    fun sendMissedDoseSms(
        phoneNumber: String,
        slotId: Int,
        medicineName: String,
        hour: Int,
        minute: Int
    ): Boolean {
        if (phoneNumber.isBlank()) {
            Toast.makeText(context, "未设置监护人号码，无法发送短信", Toast.LENGTH_SHORT).show()
            return false
        }

        if (!isPhoneValid(phoneNumber)) {
            Toast.makeText(context, "监护人号码格式不正确: $phoneNumber", Toast.LENGTH_SHORT).show()
            return false
        }

        val timeStr = String.format("%02d:%02d", hour, minute)
        val message = "【智能药盒】提醒：用户今天 $timeStr 的 $medicineName（${slotId + 1}号仓）未按时服用，请关注。"

        return try {
            val smsManager = SmsManager.getDefault()
            val parts = smsManager.divideMessage(message)
            smsManager.sendMultipartTextMessage(phoneNumber, null, parts, null, null)
            Toast.makeText(context, "已发送短信提醒到 $phoneNumber", Toast.LENGTH_SHORT).show()
            true
        } catch (e: SecurityException) {
            Toast.makeText(context, "无短信发送权限，请在设置中授权", Toast.LENGTH_LONG).show()
            false
        } catch (e: Exception) {
            Toast.makeText(context, "短信发送失败: ${e.message}", Toast.LENGTH_SHORT).show()
            false
        }
    }

    /**
     * 模拟发短信（测试用，只弹 Toast 不真发）
     */
    fun simulateSms(phoneNumber: String) {
        val displayPhone = phoneNumber.ifBlank { "未设置号码" }
        val message = "【智能药盒】提醒：用户今天 08:00 的阿司匹林（1号仓）未按时服用，请关注。"
        Toast.makeText(
            context,
            "[模拟短信] 收件人: $displayPhone\n内容: $message",
            Toast.LENGTH_LONG
        ).show()
    }

    /**
     * 检查是否有短信权限
     */
    fun hasSmsPermission(): Boolean =
        ContextCompat.checkSelfPermission(context, Manifest.permission.SEND_SMS) ==
            PackageManager.PERMISSION_GRANTED

    /**
     * 请求短信权限
     */
    fun requestSmsPermission(activity: Activity, requestCode: Int) {
        ActivityCompat.requestPermissions(
            activity,
            arrayOf(Manifest.permission.SEND_SMS),
            requestCode
        )
    }

    private fun isPhoneValid(phone: String): Boolean {
        return phone.matches(Regex("^1[3-9]\\d{9}$"))
    }

    companion object {
        const val REQUEST_SMS_PERMISSION = 2001
    }
}
