package com.pillbox.app

import android.app.Application
import android.app.NotificationManager
import com.pillbox.app.data.LocalStorage
import com.pillbox.app.notification.AlertNotifier

class PillboxApplication : Application() {

    lateinit var storage: LocalStorage
        private set

    override fun onCreate() {
        super.onCreate()
        storage = LocalStorage(this)

        // 创建通知渠道
        AlertNotifier(this)
    }
}
