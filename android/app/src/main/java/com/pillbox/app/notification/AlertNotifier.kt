package com.pillbox.app.notification

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.pillbox.app.MainActivity
import com.pillbox.app.R

/**
 * 通知栏提醒
 */
class AlertNotifier(private val context: Context) {

    private val notificationManager = NotificationManagerCompat.from(context)
    private var notificationId = 1000

    init {
        createNotificationChannel()
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "漏服提醒",
            NotificationManager.IMPORTANCE_HIGH
        ).apply {
            description = "智能药盒漏服提醒通知"
            enableVibration(true)
        }
        notificationManager.createNotificationChannel(channel)
    }

    /**
     * 发送漏服通知
     */
    fun showMissedDoseAlert(slotId: Int, medicineName: String, hour: Int, minute: Int) {
        val timeStr = String.format("%02d:%02d", hour, minute)
        val title = "⚠️ 漏服提醒"
        val content = "今天 $timeStr 的 $medicineName（${slotId + 1}号仓）未按时服用，请关注。"

        val intent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            context, 0, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val notification = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_dialog_alert)
            .setContentTitle(title)
            .setContentText(content)
            .setStyle(NotificationCompat.BigTextStyle().bigText(content))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setContentIntent(pendingIntent)
            .setAutoCancel(true)
            .build()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS)
                == PackageManager.PERMISSION_GRANTED
            ) {
                notificationManager.notify(notificationId++, notification)
            }
        } else {
            notificationManager.notify(notificationId++, notification)
        }
    }

    /**
     * 发送模拟通知（测试用）
     */
    fun showDemoAlert() {
        showMissedDoseAlert(0, "阿司匹林", 8, 0)
    }

    companion object {
        const val CHANNEL_ID = "pillbox_missed_dose"
    }
}
