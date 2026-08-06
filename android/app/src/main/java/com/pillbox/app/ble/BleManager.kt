package com.pillbox.app.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.provider.Settings
import android.util.Log
import androidx.core.content.ContextCompat
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID

/**
 * 蓝牙管理器：扫描、连接、收发数据
 *
 * JDY-31 Bluetooth 3.0 SPP 透传模块（非 BLE！）
 * SPP UUID: 00001101-0000-1000-8000-00805F9B34FB
 *
 * 通信方式：经典蓝牙 RFCOMM Socket，流式读写（\r\n 分隔行）
 */
@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {

    companion object {
        // 标准 Bluetooth SPP UUID（非 BLE GATT UUID）
        val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

        private const val TAG = "BleManager"
        private const val MAX_RETRY = 5
        private const val SCAN_TIMEOUT_MS = 15000L
    }

    private val bluetoothAdapter: BluetoothAdapter? =
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager?)?.adapter

    // RFCOMM Socket
    private var btSocket: BluetoothSocket? = null
    private var inputStream: InputStream? = null
    private var outputStream: OutputStream? = null
    private var retryCount = 0
    private var reconnectJob: Job? = null
    private var scanTimeoutJob: Job? = null
    private var readLoopJob: Job? = null
    private var heartbeatJob: Job? = null
    private var connectionGeneration = 0

    // 经典蓝牙扫描广播接收器
    private var discoveryReceiver: BroadcastReceiver? = null

    // 连接状态
    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState

    // 收到数据的回调
    private var onDataReceived: ((String) -> Unit)? = null
    // 连接状态变化回调
    private var onConnectionStateChanged: ((ConnectionState) -> Unit)? = null
    // 调试日志回调
    private var onDebugLog: ((String) -> Unit)? = null

    enum class ConnectionState {
        SCANNING, CONNECTING, CONNECTED, DISCONNECTED
    }

    /**
     * 检查蓝牙是否可用
     */
    fun isBleSupported(): Boolean = bluetoothAdapter != null

    fun isBleEnabled(): Boolean = bluetoothAdapter?.isEnabled == true

    fun hasPermissions(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val scanOk = ContextCompat.checkSelfPermission(
                context, Manifest.permission.BLUETOOTH_SCAN
            ) == PackageManager.PERMISSION_GRANTED
            val connectOk = ContextCompat.checkSelfPermission(
                context, Manifest.permission.BLUETOOTH_CONNECT
            ) == PackageManager.PERMISSION_GRANTED
            scanOk && connectOk
        } else {
            ContextCompat.checkSelfPermission(
                context, Manifest.permission.ACCESS_FINE_LOCATION
            ) == PackageManager.PERMISSION_GRANTED
        }
    }

    fun setOnDataReceived(callback: (String) -> Unit) {
        onDataReceived = callback
    }

    fun setOnConnectionStateChanged(callback: (ConnectionState) -> Unit) {
        onConnectionStateChanged = callback
    }

    fun setOnDebugLog(callback: (String) -> Unit) {
        onDebugLog = callback
    }

    /**
     * 检查位置服务是否开启（国产手机经典蓝牙扫描也需要定位）
     */
    fun isLocationEnabled(): Boolean {
        val lm = context.getSystemService(Context.LOCATION_SERVICE) as? LocationManager ?: return true
        return lm.isProviderEnabled(LocationManager.GPS_PROVIDER) ||
               lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
    }

    /**
     * 打开位置服务设置页
     */
    fun openLocationSettings() {
        val intent = Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        context.startActivity(intent)
    }

    /**
     * 开始经典蓝牙扫描（查找 JDY-31 SPP 设备）
     */
    fun startScan() {
        if (!isBleSupported()) {
            log("设备不支持蓝牙")
            return
        }
        if (!isBleEnabled()) {
            log("系统蓝牙未开启，请在通知栏打开蓝牙")
            setConnectionState(ConnectionState.DISCONNECTED)
            return
        }
        if (!hasPermissions()) {
            log("缺少蓝牙权限")
            return
        }
        if (!isLocationEnabled()) {
            log("请开启位置服务(GPS)，否则无法扫描蓝牙设备")
            openLocationSettings()
            setConnectionState(ConnectionState.DISCONNECTED)
            return
        }

        log("开始经典蓝牙扫描...")
        setConnectionState(ConnectionState.SCANNING)
        retryCount = 0
        connectionGeneration++
        reconnectJob?.cancel()
        reconnectJob = null

        // 优先连接已配对的 JDY-31。很多情况下系统蓝牙已配对，
        // startDiscovery 不一定再次发现它，导致 APP 看似在连但没有 RFCOMM Socket。
        val bondedTarget = bluetoothAdapter?.bondedDevices?.firstOrNull { device ->
            val n = device.name ?: ""
            n.contains("JDY", ignoreCase = true) ||
                n.contains("SmartPillbox", ignoreCase = true) ||
                n.contains("Pillbox", ignoreCase = true) ||
                n.contains("SPP", ignoreCase = true)
        }
        if (bondedTarget != null) {
            log("发现已配对目标：${bondedTarget.name ?: bondedTarget.address}，直接连接")
            stopScan()
            connectToDevice(bondedTarget)
            return
        }

        // 先取消之前的扫描
        try { bluetoothAdapter?.cancelDiscovery() } catch (e: Exception) {}

        // 注册广播接收器监听经典蓝牙发现结果
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(ctx: Context, intent: Intent) {
                when (intent.action) {
                    BluetoothDevice.ACTION_FOUND -> {
                        val device: BluetoothDevice? =
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                                intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                            } else {
                                @Suppress("DEPRECATION")
                                intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                            }
                        val name: String? = intent.getStringExtra(BluetoothDevice.EXTRA_NAME)
                        val rssi: Short = intent.getShortExtra(BluetoothDevice.EXTRA_RSSI, Short.MIN_VALUE)

                        if (device == null) return
                        val addr = device.address
                        val deviceName = name ?: "(未知)"

                        log("发现: $deviceName [$addr] RSSI=$rssi")

                        // 匹配 JDY-31 系列设备
                        if (deviceName.contains("JDY", ignoreCase = true) ||
                            deviceName.contains("SmartPillbox", ignoreCase = true) ||
                            deviceName.contains("Pillbox", ignoreCase = true) ||
                            deviceName.contains("SPP", ignoreCase = true)) {
                            log(">>> 匹配目标！停止扫描: $deviceName [$addr]")
                            stopScan()
                            connectToDevice(device)
                        }
                    }

                    BluetoothAdapter.ACTION_DISCOVERY_FINISHED -> {
                        log("经典蓝牙扫描结束")
                        if (_connectionState.value == ConnectionState.SCANNING) {
                            log("扫描结束，未找到目标设备")
                            setConnectionState(ConnectionState.DISCONNECTED)
                        }
                    }

                    BluetoothAdapter.ACTION_DISCOVERY_STARTED -> {
                        log("蓝牙发现已开始")
                    }
                }
            }
        }

        discoveryReceiver = receiver
        val filter = IntentFilter().apply {
            addAction(BluetoothDevice.ACTION_FOUND)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED)
            addAction(BluetoothAdapter.ACTION_DISCOVERY_STARTED)
        }
        context.registerReceiver(receiver, filter)

        // 开始经典蓝牙发现
        bluetoothAdapter?.startDiscovery()

        // 超时自动停止
        scanTimeoutJob = CoroutineScope(Dispatchers.IO).launch {
            delay(SCAN_TIMEOUT_MS)
            stopScan()
            if (_connectionState.value == ConnectionState.SCANNING) {
                log("扫描超时(15s)，未找到 JDY-31 设备")
                setConnectionState(ConnectionState.DISCONNECTED)
            }
        }
    }

    fun stopScan() {
        scanTimeoutJob?.cancel()
        scanTimeoutJob = null
        try { bluetoothAdapter?.cancelDiscovery() } catch (e: Exception) {}
        discoveryReceiver?.let {
            try { context.unregisterReceiver(it) } catch (e: Exception) {}
        }
        discoveryReceiver = null
    }

    /**
     * 手动连接到指定 MAC 地址（可选）
     */
    fun connectByAddress(address: String) {
        val device = bluetoothAdapter?.getRemoteDevice(address) ?: return
        connectToDevice(device)
    }

    /**
     * 断开连接
     */
    fun disconnect() {
        connectionGeneration++
        reconnectJob?.cancel()
        scanTimeoutJob?.cancel()
        retryCount = MAX_RETRY  // 阻止重连
        stopScan()
        closeSocket()
        setConnectionState(ConnectionState.DISCONNECTED)
    }

    /**
     * 发送数据到 STM32（自动追加 \r\n）
     */
    fun send(data: String) {
        try {
            val out = outputStream
            if (out == null) {
                log("TX失败：未建立 RFCOMM 输出流，丢弃 <$data>")
                return
            }
            log("TX: $data")
            val bytes = (data + "\r\n").toByteArray(Charsets.UTF_8)
            out.write(bytes)
            out.flush()
            log("TX完成: $data")
        } catch (e: Exception) {
            log("发送失败: ${e.message}")
        }
    }

    /**
     * 发送 PLAN 指令
     */
    fun sendPlan(plan: com.pillbox.app.model.MedicationPlan) {
        val cmd = "PLAN:${plan.slotId},${plan.hour},${plan.minute},${plan.period},${if (plan.enabled) 1 else 0}"
        send(cmd)
    }

    /**
     * 请求同步
     */
    fun requestSync() {
        send("SYNC:ALL")
    }

    /**
     * 同步手机当前 Unix 时间戳到 STM32。
     * STM32 当前未接 DS3231 时，依赖该时间做用药计划判断。
     */
    fun sendCurrentTime() {
        val unixSeconds = System.currentTimeMillis() / 1000L
        send("TIME:$unixSeconds")
    }

    // ==================== 内部实现 ====================

    private fun log(msg: String) {
        Log.d(TAG, msg)
        onDebugLog?.invoke(msg)
    }

    private fun setConnectionState(state: ConnectionState) {
        _connectionState.value = state
        onConnectionStateChanged?.invoke(state)
    }

    /**
     * 通过 RFCOMM Socket 连接经典蓝牙设备
     */
    private fun connectToDevice(device: BluetoothDevice) {
        val generation = connectionGeneration
        val deviceName = device.name ?: device.address
        val bondText = when (device.bondState) {
            BluetoothDevice.BOND_BONDED -> "已配对"
            BluetoothDevice.BOND_BONDING -> "配对中"
            BluetoothDevice.BOND_NONE -> "未配对"
            else -> "未知配对状态"
        }
        log("正在连接: $deviceName [$bondText] ${device.address}")
        device.uuids?.joinToString { it.uuid.toString() }?.let {
            log("设备UUID: $it")
        } ?: log("设备UUID: 空，尝试 fetchUuidsWithSdp")
        setConnectionState(ConnectionState.CONNECTING)
        if (retryCount == 0) {
            log("开始新连接会话")
        }

        CoroutineScope(Dispatchers.IO).launch {
            try {
                if (generation != connectionGeneration) return@launch
                closeSocket()
                try { bluetoothAdapter?.cancelDiscovery() } catch (e: Exception) {}
                try { device.fetchUuidsWithSdp() } catch (e: Exception) {}
                delay(1200)
                if (generation != connectionGeneration) return@launch

                val socket = createConnectedSocket(device)
                if (generation != connectionGeneration) {
                    try { socket.close() } catch (_: Exception) {}
                    return@launch
                }
                btSocket = socket

                inputStream = socket.inputStream
                outputStream = socket.outputStream

                log("RFCOMM 已连接: $deviceName")
                retryCount = 0
                withContext(Dispatchers.Main) {
                    setConnectionState(ConnectionState.CONNECTED)
                }

                // 启动心跳（每10秒发一次 PING，维持 STM32 的连接状态判断）
                startHeartbeat()

                // 启动读取循环
                startReadLoop(generation)

            } catch (e: Exception) {
                log("连接失败: ${e.message}")
                withContext(Dispatchers.Main) {
                    if (generation == connectionGeneration) {
                        closeSocket()
                        setConnectionState(ConnectionState.DISCONNECTED)
                    }
                }

                // 自动重连
                if (generation == connectionGeneration && retryCount < MAX_RETRY) {
                    retryCount++
                    log("自动重连 $retryCount/$MAX_RETRY...")
                    reconnectJob = CoroutineScope(Dispatchers.IO).launch {
                        delay(3000)
                        if (generation == connectionGeneration) {
                            connectToDevice(device)
                        }
                    }
                }
            }
        }
    }

    private fun createConnectedSocket(device: BluetoothDevice): BluetoothSocket {
        val uuidAttempts = mutableListOf<Pair<String, () -> BluetoothSocket>>()
        device.uuids?.forEachIndexed { index, parcelUuid ->
            val uuid = parcelUuid.uuid
            uuidAttempts.add("device-uuid-$index-$uuid" to {
                device.createRfcommSocketToServiceRecord(uuid)
            })
            uuidAttempts.add("device-insecure-uuid-$index-$uuid" to {
                device.createInsecureRfcommSocketToServiceRecord(uuid)
            })
        }

        val attempts = uuidAttempts + listOf(
            "secure-spp" to {
                device.createRfcommSocketToServiceRecord(SPP_UUID)
            },
            "insecure-spp" to {
                device.createInsecureRfcommSocketToServiceRecord(SPP_UUID)
            },
            "channel-1-reflection" to {
                val method = device.javaClass.getMethod("createRfcommSocket", Int::class.javaPrimitiveType)
                method.invoke(device, 1) as BluetoothSocket
            }
        )

        var lastError: Exception? = null
        for ((name, factory) in attempts) {
            var socket: BluetoothSocket? = null
            try {
                log("尝试连接方式：$name")
                socket = factory()
                socket.connect()
                log("连接方式成功：$name")
                return socket
            } catch (e: Exception) {
                lastError = e
                log("连接方式失败：$name -> ${e.message}")
                try { socket?.close() } catch (_: Exception) {}
                Thread.sleep(300)
            }
        }
        throw lastError ?: IllegalStateException("所有 RFCOMM 连接方式均失败")
    }

    /**
     * 心跳：每 10 秒发一次 PING，让 STM32 维持 BLE_CONNECTED 状态
     */
    private fun startHeartbeat() {
        heartbeatJob?.cancel()
        heartbeatJob = CoroutineScope(Dispatchers.IO).launch {
            while (isActive) {
                delay(10_000L)
                try {
                    val bytes = "PING\r\n".toByteArray(Charsets.UTF_8)
                    outputStream?.write(bytes)
                    outputStream?.flush()
                } catch (e: Exception) {
                    log("心跳发送失败: ${e.message}")
                    break
                }
            }
        }
    }

    /**
     * 读取循环：在 IO 线程不断从 inputStream 读取，按 \r\n 分行
     */
    private fun startReadLoop(generation: Int) {
        readLoopJob?.cancel()
        readLoopJob = CoroutineScope(Dispatchers.IO).launch {
            val buffer = ByteArray(1024)
            val lineBuffer = StringBuilder()

            try {
                val input = inputStream ?: return@launch
                while (isActive) {
                    val count = input.read(buffer)
                    if (count == -1) break  // 流结束 = 对端断开

                    val text = String(buffer, 0, count, Charsets.UTF_8)
                    for (ch in text) {
                        if (ch == '\n') {
                            val line = lineBuffer.toString().trimEnd('\r')
                            lineBuffer.clear()
                            if (line.isNotEmpty()) {
                                log("RX: $line")
                                onDataReceived?.invoke(line)
                            }
                        } else if (ch != '\r') {
                            lineBuffer.append(ch)
                        }
                    }
                }
            } catch (e: Exception) {
                log("读取异常: ${e.message}")
            }

            // 读取循环退出 = 连接断开
            log("蓝牙连接已断开")
            withContext(Dispatchers.Main) {
                if (generation == connectionGeneration) {
                    closeSocket()
                    setConnectionState(ConnectionState.DISCONNECTED)
                } else {
                    log("忽略旧连接读循环退出")
                }
            }
        }
    }

    private fun closeSocket() {
        heartbeatJob?.cancel()
        heartbeatJob = null
        readLoopJob?.cancel()
        readLoopJob = null
        try { inputStream?.close() } catch (e: Exception) {}
        try { outputStream?.close() } catch (e: Exception) {}
        try { btSocket?.close() } catch (e: Exception) {}
        inputStream = null
        outputStream = null
        btSocket = null
    }
}
