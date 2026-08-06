package com.pillbox.app.ui

import android.app.AlertDialog
import android.os.Bundle
import android.text.InputType
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.google.android.material.switchmaterial.SwitchMaterial
import com.pillbox.app.R
import com.pillbox.app.data.LocalStorage
import com.pillbox.app.notification.AlertNotifier
import com.pillbox.app.notification.SmsSender

class SettingsFragment : Fragment() {

    private lateinit var storage: LocalStorage
    private lateinit var alertNotifier: AlertNotifier
    private lateinit var smsSender: SmsSender
    private lateinit var bleSettingTitle: TextView
    private lateinit var bleSettingDesc: TextView
    private lateinit var bleSwitch: SwitchMaterial
    private lateinit var guardianPhoneText: TextView
    private lateinit var switchAlertNotify: SwitchMaterial
    private lateinit var switchAdvanceNotify: SwitchMaterial
    private lateinit var switchVibrate: SwitchMaterial
    private lateinit var switchSms: SwitchMaterial
    private lateinit var switchWeightPopup: SwitchMaterial
    private lateinit var bleDebugLog: TextView
    private lateinit var btnBleTime: Button
    private lateinit var btnBleSync: Button
    private lateinit var btnBleTest: Button
    private lateinit var btnBleBeep: Button
    private lateinit var btnBleTare: Button
    private lateinit var btnMotorTest: Button
    private lateinit var btnMotorCw: Button
    private lateinit var btnMotorCcw: Button
    private lateinit var deviceStatusText: TextView

    private var onBleToggleChanged: ((Boolean) -> Unit)? = null
    private var onBleDebugCommand: ((String) -> Unit)? = null

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_settings, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        storage = LocalStorage(requireContext())
        alertNotifier = AlertNotifier(requireContext())
        smsSender = SmsSender(requireContext())

        bleSettingTitle = view.findViewById(R.id.bleSettingTitle)
        bleSettingDesc = view.findViewById(R.id.bleSettingDesc)
        bleSwitch = view.findViewById(R.id.bleSwitch)
        guardianPhoneText = view.findViewById(R.id.guardianPhoneText)
        switchAlertNotify = view.findViewById(R.id.switchAlertNotify)
        switchAdvanceNotify = view.findViewById(R.id.switchAdvanceNotify)
        switchVibrate = view.findViewById(R.id.switchVibrate)
        switchSms = view.findViewById(R.id.switchSms)
        switchWeightPopup = view.findViewById(R.id.switchWeightPopup)
        bleDebugLog = view.findViewById(R.id.bleDebugLog)
        btnBleTime = view.findViewById(R.id.btnBleTime)
        btnBleSync = view.findViewById(R.id.btnBleSync)
        btnBleTest = view.findViewById(R.id.btnBleTest)
        btnBleBeep = view.findViewById(R.id.btnBleBeep)
        btnBleTare = view.findViewById(R.id.btnBleTare)
        btnMotorTest = view.findViewById(R.id.btnMotorTest)
        btnMotorCw = view.findViewById(R.id.btnMotorCw)
        btnMotorCcw = view.findViewById(R.id.btnMotorCcw)
        deviceStatusText = view.findViewById(R.id.deviceStatusText)

        loadSettings()

        view.findViewById<View>(R.id.bleSettingItem).setOnClickListener {
            val newState = !bleSwitch.isChecked
            bleSwitch.isChecked = newState
            onBleToggleChanged?.invoke(newState)
            updateBleUI(newState)
        }

        view.findViewById<View>(R.id.guardianPhoneItem).setOnClickListener {
            showPhoneInputDialog()
        }

        view.findViewById<View>(R.id.btnSimAlert).setOnClickListener {
            alertNotifier.showDemoAlert()
            Toast.makeText(requireContext(), "已弹出模拟漏服通知", Toast.LENGTH_SHORT).show()
        }

        view.findViewById<View>(R.id.btnSimSms).setOnClickListener {
            smsSender.simulateSms(storage.getGuardianPhone())
        }

        btnBleTime.setOnClickListener { onBleDebugCommand?.invoke("TIME") }
        btnBleSync.setOnClickListener { onBleDebugCommand?.invoke("SYNC") }
        btnBleTest.setOnClickListener { onBleDebugCommand?.invoke("TEST") }
        btnBleBeep.setOnClickListener { onBleDebugCommand?.invoke("BEEP") }
        btnBleTare.setOnClickListener { onBleDebugCommand?.invoke("TARE") }
        btnMotorTest.setOnClickListener { onBleDebugCommand?.invoke("MOTOR_TEST") }
        btnMotorCw.setOnClickListener { onBleDebugCommand?.invoke("MOTOR_CW") }
        btnMotorCcw.setOnClickListener { onBleDebugCommand?.invoke("MOTOR_CCW") }

        switchAlertNotify.setOnCheckedChangeListener { _, isChecked -> storage.setAlertNotifyEnabled(isChecked) }
        switchAdvanceNotify.setOnCheckedChangeListener { _, isChecked -> storage.setAdvanceNotifyEnabled(isChecked) }
        switchVibrate.setOnCheckedChangeListener { _, isChecked -> storage.setVibrateEnabled(isChecked) }
        switchSms.setOnCheckedChangeListener { _, isChecked -> storage.setSmsEnabled(isChecked) }
        switchWeightPopup.setOnCheckedChangeListener { _, isChecked -> storage.setWeightPopupEnabled(isChecked) }
    }

    fun setOnBleToggleChanged(callback: (Boolean) -> Unit) {
        onBleToggleChanged = callback
    }

    fun setOnBleDebugCommand(callback: (String) -> Unit) {
        onBleDebugCommand = callback
    }

    fun appendBleDebugLog(line: String) {
        if (!::bleDebugLog.isInitialized) return
        val old = bleDebugLog.text?.toString().orEmpty()
        bleDebugLog.text = (old.lines() + line).takeLast(12).joinToString("\n")
    }

    fun updateDeviceStatus(text: String) {
        if (::deviceStatusText.isInitialized) deviceStatusText.text = text
    }

    fun updateBleState(connected: Boolean) {
        bleSwitch.isChecked = connected
        updateBleUI(connected)
        if (!connected) {
            updateDeviceStatus("设备状态：未连接\n连接 JDY31 后会显示重量、扫码、蜂鸣器等硬件状态。")
        }
    }

    private fun updateBleUI(connected: Boolean) {
        if (connected) {
            bleSettingTitle.text = "智能药盒"
            bleSettingDesc.text = "已连接 · JDY-31"
        } else {
            bleSettingTitle.text = "智能药盒（未连接）"
            bleSettingDesc.text = "点击连接"
        }
    }

    private fun loadSettings() {
        bleSwitch.isChecked = false
        switchAlertNotify.isChecked = storage.isAlertNotifyEnabled()
        switchAdvanceNotify.isChecked = storage.isAdvanceNotifyEnabled()
        switchVibrate.isChecked = storage.isVibrateEnabled()
        switchSms.isChecked = storage.isSmsEnabled()
        switchWeightPopup.isChecked = storage.isWeightPopupEnabled()
        val phone = storage.getGuardianPhone()
        guardianPhoneText.text = if (phone.isBlank()) {
            "未设置"
        } else if (phone.length == 11) {
            "${phone.substring(0, 3)}****${phone.substring(7)}"
        } else {
            phone
        }
    }

    private fun showPhoneInputDialog() {
        val input = EditText(requireContext()).apply {
            inputType = InputType.TYPE_CLASS_PHONE
            hint = "请输入 11 位手机号"
            setText(storage.getGuardianPhone())
            setPadding(32, 24, 32, 24)
            textSize = 16f
        }

        AlertDialog.Builder(requireContext())
            .setTitle("设置监护人手机号")
            .setMessage("漏服时将向此号码发送短信提醒。")
            .setView(input)
            .setPositiveButton("保存") { _, _ ->
                val phone = input.text.toString().trim()
                if (phone.matches(Regex("^1[3-9]\\d{9}$"))) {
                    storage.setGuardianPhone(phone)
                    guardianPhoneText.text = "${phone.substring(0, 3)}****${phone.substring(7)}"
                    Toast.makeText(requireContext(), "监护人手机号已保存", Toast.LENGTH_SHORT).show()
                } else if (phone.isBlank()) {
                    storage.setGuardianPhone("")
                    guardianPhoneText.text = "未设置"
                } else {
                    Toast.makeText(requireContext(), "号码格式不正确", Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton("取消", null)
            .show()
    }
}
