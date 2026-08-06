package com.pillbox.app

import android.Manifest
import android.app.AlertDialog
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.InputType
import android.view.Gravity
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.pillbox.app.ble.BleManager
import com.pillbox.app.data.LocalStorage
import com.pillbox.app.model.MedicationPlan
import com.pillbox.app.notification.AlertNotifier
import com.pillbox.app.notification.SmsSender
import com.pillbox.app.ui.*
import com.pillbox.app.weight.WeightChangeDetector
import com.pillbox.app.weight.WeightChangeEvent
import java.util.Calendar

class MainActivity : AppCompatActivity() {

    private lateinit var viewPager: ViewPager2
    private lateinit var bottomNav: BottomNavigationView
    private lateinit var storage: LocalStorage
    private lateinit var bleManager: BleManager
    private lateinit var alertNotifier: AlertNotifier
    private lateinit var smsSender: SmsSender

    private lateinit var homeFragment: HomeFragment
    private lateinit var planFragment: PlanFragment
    private lateinit var historyFragment: HistoryFragment
    private lateinit var settingsFragment: SettingsFragment

    // 是否等待权限授予后启动 BLE 扫描
    private var pendingBleStart = false
    private var lastQrCode: String = "暂无"
    private val weightChangeDetector = WeightChangeDetector()
    private var lastWeightFeedback: String = "暂无"
    private val mainHandler = Handler(Looper.getMainLooper())
    private var activePlans: List<MedicationPlan> = emptyList()
    private var lastAppAlarmKey: String = ""
    private val appPlanAlarmRunnable = object : Runnable {
        override fun run() {
            checkAppPlanAlarm()
            mainHandler.postDelayed(this, 1000)
        }
    }

    // 权限请求
    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val allGranted = permissions.values.all { it }
        if (allGranted) {
            Toast.makeText(this, "权限已授予", Toast.LENGTH_SHORT).show()
            // 如果是 BLE 开关触发的权限请求，授予后自动开始扫描
            if (pendingBleStart) {
                pendingBleStart = false
                bleManager.startScan()
            }
        } else {
            Toast.makeText(this, "部分权限被拒绝，可能影响功能", Toast.LENGTH_LONG).show()
            pendingBleStart = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        storage = LocalStorage(this)
        alertNotifier = AlertNotifier(this)
        smsSender = SmsSender(this)
        bleManager = BleManager(this)

        homeFragment = HomeFragment()
        planFragment = PlanFragment()
        historyFragment = HistoryFragment()
        settingsFragment = SettingsFragment()

        viewPager = findViewById(R.id.viewPager)
        bottomNav = findViewById(R.id.bottomNav)

        val fragments = listOf<Fragment>(homeFragment, planFragment, historyFragment, settingsFragment)
        viewPager.adapter = FragmentAdapter(this, fragments)
        viewPager.isUserInputEnabled = true

        bottomNav.setOnItemSelectedListener { item ->
            when (item.itemId) {
                R.id.nav_home -> viewPager.currentItem = 0
                R.id.nav_plan -> viewPager.currentItem = 1
                R.id.nav_history -> viewPager.currentItem = 2
                R.id.nav_settings -> viewPager.currentItem = 3
            }
            true
        }

        viewPager.registerOnPageChangeCallback(object : ViewPager2.OnPageChangeCallback() {
            override fun onPageSelected(position: Int) {
                bottomNav.menu.getItem(position).isChecked = true
            }
        })

        // 设置页的回调
        settingsFragment.setOnBleToggleChanged { enable ->
            if (enable) {
                requestPermissionsAndConnect()
            } else {
                bleManager.disconnect()
                homeFragment.updateBleState(false)
                settingsFragment.updateBleState(false)
            }
        }

        settingsFragment.setOnBleDebugCommand { cmd ->
            val sentCommand = when (cmd) {
                "TIME" -> { bleManager.sendCurrentTime(); "TIME:<now>" }
                "SYNC" -> { bleManager.requestSync(); "SYNC:ALL" }
                "TEST" -> { bleManager.send("TEST:0"); "TEST:0" }
                "BEEP" -> { bleManager.send("BEEP"); "BEEP" }
                "TARE" -> { bleManager.send("TARE"); "TARE" }
                "MOTOR_TEST" -> { bleManager.send("MOTOR:TEST"); "MOTOR:TEST" }
                "MOTOR_CW" -> { bleManager.send("MOTOR:CW,300"); "MOTOR:CW,300" }
                "MOTOR_CCW" -> { bleManager.send("MOTOR:CCW,300"); "MOTOR:CCW,300" }
                else -> ""
            }
            if (sentCommand.isNotBlank()) {
                settingsFragment.appendBleDebugLog("APP TX $sentCommand")
            }
        }

        homeFragment.setOnSlotSelected { slotId ->
            bleManager.send("MOTOR:SLOT,$slotId")
            settingsFragment.appendBleDebugLog("???${slotId + 1}???")
        }

        homeFragment.setOnSlotChanged { slotId, medicineName ->
            if (medicineName.isNullOrBlank()) {
                bleManager.send("SET:$slotId,")
                settingsFragment.appendBleDebugLog("手动清空：${slotId + 1}号药仓")
            } else {
                bleManager.send("SET:$slotId,$medicineName")
                settingsFragment.appendBleDebugLog("手动编辑：${slotId + 1}号药仓 <- $medicineName")
            }
        }

        // 计划页同步按钮
        planFragment.setOnSyncClicked {
            val plans = planFragment.getPlans()
            storage.savePlans(plans)
            activePlans = plans
            lastAppAlarmKey = ""
            bleManager.sendCurrentTime()
            settingsFragment.appendBleDebugLog("APP TX TIME:<now>")
            plans.forEachIndexed { index, plan ->
                mainHandler.postDelayed({
                    bleManager.sendPlan(plan)
                    settingsFragment.appendBleDebugLog(
                        "APP TX PLAN:${plan.slotId},${plan.hour},${plan.minute},${plan.period},${if (plan.enabled) 1 else 0}"
                    )
                }, 250L * (index + 1))
            }
            Toast.makeText(this, "?????? ${plans.size} ??????App????????10?", Toast.LENGTH_LONG).show()
        }

        // BLE ????
        bleManager.setOnDataReceived { data ->
            runOnUiThread {
                settingsFragment.appendBleDebugLog("RX $data")
                handleBleData(data)
            }
        }

        // BLE 连接状态监听 — 自动更新首页和设置页 UI
        bleManager.setOnConnectionStateChanged { state ->
            runOnUiThread {
                val connected = state == BleManager.ConnectionState.CONNECTED
                homeFragment.updateBleState(connected)
                settingsFragment.updateBleState(connected)
                if (connected) {
                    settingsFragment.appendBleDebugLog("连接成功，1秒后自动 TIME/SYNC")
                    Handler(Looper.getMainLooper()).postDelayed({
                        bleManager.sendCurrentTime()
                        bleManager.requestSync()
                    }, 1000)
                }
                // 状态变化时弹出提示
                val stateText = when (state) {
                    BleManager.ConnectionState.SCANNING -> "扫描中..."
                    BleManager.ConnectionState.CONNECTING -> "正在连接..."
                    BleManager.ConnectionState.CONNECTED -> "已连接！"
                    BleManager.ConnectionState.DISCONNECTED -> "未连接"
                }
                Toast.makeText(this, "BLE: $stateText", Toast.LENGTH_SHORT).show()
            }
        }

        // BLE 调试日志 — 输出到 logcat（避免 Toast 泛滥）
        bleManager.setOnDebugLog { msg ->
            android.util.Log.d("MainActivity", msg)
            runOnUiThread {
                settingsFragment.appendBleDebugLog(msg)
            }
        }

        // 请求必要权限
        activePlans = storage.loadPlans()
        mainHandler.post(appPlanAlarmRunnable)

        requestPermissionsIfNeeded()
    }

    override fun onDestroy() {
        mainHandler.removeCallbacks(appPlanAlarmRunnable)
        super.onDestroy()
    }

    private fun checkAppPlanAlarm() {
        if (!::bleManager.isInitialized || activePlans.isEmpty()) return
        val now = Calendar.getInstance()
        val hour = now.get(Calendar.HOUR_OF_DAY)
        val minute = now.get(Calendar.MINUTE)
        val matched = activePlans.firstOrNull { it.enabled && it.hour == hour && it.minute == minute } ?: return
        val key = "$hour:$minute:${matched.period}"
        if (key == lastAppAlarmKey) return
        lastAppAlarmKey = key
        bleManager.send("BEEP10")
        settingsFragment.appendBleDebugLog("APP ALARM ${matched.hour}:${String.format("%02d", matched.minute)} slot=${matched.slotId + 1} -> BEEP10")
        Toast.makeText(this, "?????${matched.slotId + 1}????????10?", Toast.LENGTH_LONG).show()
    }

    private fun requestPermissionsIfNeeded() {
        val permissions = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN)
                != PackageManager.PERMISSION_GRANTED
            ) permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED
            ) permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED
            ) permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED
            ) permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.SEND_SMS)
            != PackageManager.PERMISSION_GRANTED
        ) permissions.add(Manifest.permission.SEND_SMS)

        if (permissions.isNotEmpty()) {
            requestPermissionLauncher.launch(permissions.toTypedArray())
        }
    }

    private fun requestPermissionsAndConnect() {
        val permissions = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN)
                != PackageManager.PERMISSION_GRANTED
            ) permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED
            ) permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED
            ) permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (permissions.isNotEmpty()) {
            pendingBleStart = true
            requestPermissionLauncher.launch(permissions.toTypedArray())
        } else {
            bleManager.startScan()
        }
    }

    private fun handleBleData(data: String) {
        // 解析 STM32 发来的协议数据
        // 格式: ALERT:slot,hh,mm  /  REC:timestamp,slot,taken  /  STATUS:...
        try {
            val parts = data.split(":", limit = 2)
            if (parts.size < 2) return

            when (parts[0]) {
                "ALERT" -> {
                    // ALERT:slot,hh,mm → 漏服提醒
                    val args = parts[1].split(",")
                    if (args.size >= 3) {
                        val slotId = args[0].toIntOrNull() ?: 0
                        val hour = args[1].toIntOrNull() ?: 0
                        val minute = args[2].toIntOrNull() ?: 0
                        val medName = storage.getDemoSlots()[slotId] ?: "未知药品"

                        // 弹通知
                        if (storage.isAlertNotifyEnabled()) {
                            alertNotifier.showMissedDoseAlert(slotId, medName, hour, minute)
                        }

                        // 发短信
                        if (storage.isSmsEnabled()) {
                            val phone = storage.getGuardianPhone()
                            if (phone.isNotBlank()) {
                                smsSender.sendMissedDoseSms(phone, slotId, medName, hour, minute)
                            }
                        }
                    }
                }
                "REMIND" -> {
                    val args = parts[1].split(",")
                    val slotId = args.getOrNull(0)?.toIntOrNull() ?: 0
                    val medName = args.getOrNull(1) ?: "药品"
                    Toast.makeText(this, "请服用：$medName（${slotId + 1}号仓）", Toast.LENGTH_LONG).show()
                }
                "REC" -> {
                    // REC:timestamp,slot,taken → 服药记录
                    val args = parts[1].split(",")
                    if (args.size >= 3) {
                        val timestamp = args[0].toLongOrNull() ?: System.currentTimeMillis()
                        val slotId = args[1].toIntOrNull() ?: 0
                        val taken = args[2] == "1"
                        val medName = storage.getDemoSlots()[slotId] ?: ""
                        storage.addRecord(
                            com.pillbox.app.model.MedicationRecord(
                                timestamp = timestamp * 1000,  // 秒转毫秒
                                slotId = slotId,
                                taken = taken,
                                medicineName = medName
                            )
                        )
                    }
                }
                "STATUS" -> {
                    // STATUS:0=阿司匹林&1=降压药&...  → 刷新首页药仓
                    // 简化处理：不做实时解析，仅日志
                }
                "STAT" -> {
                    // STAT:unix,timeReady,bleState,sysState,weightCg,recordCount
                    val args = parts[1].split(",")
                    if (args.size >= 6) {
                        val ts = args[0]
                        val timeReady = args[1] == "1"
                        val bleState = args[2]
                        val sysState = args[3]
                        val weightCg = args[4].toLongOrNull() ?: 0L
                        val weightText = String.format("%.2fg", weightCg / 100.0)
                        handleWeightChangeFeedback(weightCg, weightText)
                        val recordCount = args[5]
                        val sysText = when (sysState) {
                            "0" -> "空闲"
                            "1" -> "提醒/等待取药"
                            "2" -> "漏服处理"
                            else -> sysState
                        }
                        settingsFragment.updateDeviceStatus(
                            "设备状态：在线\n" +
                                "时间戳：$ts\n" +
                                "时间同步：${if (timeReady) "已同步" else "未同步"}\n" +
                                "蓝牙状态值：$bleState\n" +
                                "系统状态：$sysText\n" +
                                "压力/HX711：$weightText\n" +
                                "称重反馈：$lastWeightFeedback\n" +
                                "服药记录数：$recordCount\n" +
                                "最近扫码：$lastQrCode"
                        )
                    }
                }
                "TIME_ACK" -> {
                    Toast.makeText(this, "药盒时间已同步：${parts[1]}", Toast.LENGTH_SHORT).show()
                }
                "BEEP_ACK" -> {
                    Toast.makeText(this, "蜂鸣器测试成功", Toast.LENGTH_SHORT).show()
                }
                "TARE_ACK" -> {
                    weightChangeDetector.reset()
                    lastWeightFeedback = "已去皮，等待下一次称重变化"
                    Toast.makeText(this, "压力传感器已去皮", Toast.LENGTH_SHORT).show()
                }
                "MOTOR_ACK" -> {
                    val feedback = parts.drop(1).joinToString(":")
                    settingsFragment.appendBleDebugLog("MCU MOTOR_ACK $feedback")
                    Toast.makeText(this, "?????$feedback", Toast.LENGTH_SHORT).show()
                }
                "QR" -> {
                    val code = parts.getOrNull(1).orEmpty()
                    if (code.isNotBlank()) {
                        lastQrCode = code
                        settingsFragment.appendBleDebugLog("扫码结果：$code")
                        handleQrMedicine(code)
                        Toast.makeText(this, "扫码结果：$code", Toast.LENGTH_SHORT).show()
                    }
                }
                "DBG" -> {
                    // 调试心跳只显示在 BLE 日志里，不弹窗。
                }
            }
        } catch (e: Exception) {
            // 协议解析失败，忽略
        }
    }

    private fun handleWeightChangeFeedback(weightCg: Long, weightText: String) {
        val event = weightChangeDetector.onWeightCg(weightCg) ?: return
        when (event) {
            WeightChangeEvent.Placed -> {
                lastWeightFeedback = "已放入药品（当前 $weightText）"
                if (storage.isWeightPopupEnabled()) {
                    showWeightFeedbackDialog("已放入药品", "检测到重量增加超过 5g。\n当前重量：$weightText")
                }
            }
            WeightChangeEvent.Taken -> {
                lastWeightFeedback = "已取出药品，请按时服药（当前 $weightText）"
                if (storage.isWeightPopupEnabled()) {
                    showWeightFeedbackDialog("已取出药品", "检测到重量减少超过 5g。\n请按时服药。\n当前重量：$weightText")
                }
            }
        }
        settingsFragment.appendBleDebugLog("称重反馈：$lastWeightFeedback")
    }

    private fun showWeightFeedbackDialog(title: String, message: String) {
        if (isFinishing || isDestroyed) return
        AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton("知道了", null)
            .show()
    }

    private fun handleQrMedicine(code: String) {
        settingsFragment.updateDeviceStatus(
            "扫码原始结果：\n$code\n\n长度：${code.length} 个字符\n" +
                "说明：普通商品条码通常是一串数字；二维码可能是一串文字、网址或编号。"
        )
        val knownName = storage.getMedicineNameByBarcode(code)
        if (knownName.isNullOrBlank()) {
            showFirstScanNameDialog(code)
        } else {
            showSlotChooseDialog(code, knownName)
        }
    }

    private fun showFirstScanNameDialog(code: String) {
        val input = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_TEXT
            hint = "例如：阿司匹林"
            setPadding(32, 24, 32, 24)
            textSize = 16f
        }

        AlertDialog.Builder(this)
            .setTitle("首次扫描到新药品")
            .setMessage("二维码/条码：$code\n请输入这个药品的名称。以后再次扫描会自动识别。")
            .setView(input)
            .setPositiveButton("保存并选药仓") { _, _ ->
                val name = input.text.toString().trim()
                if (name.isBlank()) {
                    Toast.makeText(this, "药品名不能为空", Toast.LENGTH_SHORT).show()
                    return@setPositiveButton
                }
                storage.saveBarcodeMedicine(code, name)
                settingsFragment.appendBleDebugLog("已绑定：$code -> $name")
                showSlotChooseDialog(code, name)
            }
            .setNegativeButton("取消", null)
            .show()
    }

    private fun showSlotChooseDialog(code: String, medicineName: String) {
        val container = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(40, 18, 40, 8)
        }

        container.addView(TextView(this).apply {
            text = "扫码原始结果：\n$code\n\n请选择放入哪个药仓："
            textSize = 14f
            setPadding(0, 0, 0, 16)
        })

        val dialog = AlertDialog.Builder(this)
            .setTitle("识别到：$medicineName")
            .setView(container)
            .setNegativeButton("取消", null)
            .create()

        fun addSlotButton(slotId: Int) {
            container.addView(Button(this).apply {
                text = "${slotId + 1}号药仓"
                textSize = 18f
                gravity = Gravity.CENTER
                setOnClickListener {
                    saveMedicineToSlot(code, medicineName, slotId)
                    dialog.dismiss()
                }
            })
        }

        for (slotId in 0..5) {
            addSlotButton(slotId)
        }
        dialog.show()
    }

    private fun saveMedicineToSlot(code: String, medicineName: String, slotId: Int) {
        storage.saveSlotMedicine(slotId, medicineName, code)
        bleManager.send("SET:$slotId,$medicineName")
        settingsFragment.appendBleDebugLog("APP TX SET:$slotId,$medicineName")
        Handler(Looper.getMainLooper()).postDelayed({
            bleManager.send("MOTOR:SLOT,$slotId")
            settingsFragment.appendBleDebugLog("APP TX MOTOR:SLOT,$slotId")
        }, 300)
        homeFragment.refreshSlots()
        settingsFragment.appendBleDebugLog("入仓：${slotId + 1}号 <- $medicineName")
        settingsFragment.updateDeviceStatus(
            "扫码识别：$medicineName\n" +
                "扫码原始结果：$code\n" +
                "原始长度：${code.length} 个字符\n" +
                "已选择：${slotId + 1}号药仓\n" +
                "正在转到：${slotId + 1}号药仓\n" +
                "已同步到药盒：SET:$slotId,$medicineName / MOTOR:SLOT,$slotId"
        )
        Toast.makeText(this, "$medicineName 已选择 ${slotId + 1}号药仓，电机正在转动", Toast.LENGTH_SHORT).show()
    }

}
