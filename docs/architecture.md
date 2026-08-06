# 系统架构

智能药盒系统由 Android 应用、经典蓝牙串口链路和 STM32 固件组成。应用通过 Bluetooth SPP 的 RFCOMM socket 与 JDY-31 类串口蓝牙模块通信；模块的 UART 接到 STM32。固件再驱动屏幕、人机界面、二维码扫描器、蜂鸣器、HX711 称重和步进电机。

```text
Android UI / 本地存储
        |
        | Bluetooth SPP (RFCOMM, UUID 00001101-0000-1000-8000-00805F9B34FB)
        v
JDY-31 经典蓝牙串口模块
        |
        | UART, CRLF 分行文本
        v
STM32F103C8T6 firmware
        +--> TJC8048X270 HMI / OLED
        +--> QR100 scanner
        +--> HX711 load-cell sensor
        +--> buzzer and stepper motor
```

## 数据流

应用负责扫描/选择设备、建立 RFCOMM socket、按 CRLF 发送命令并解析回执。固件在主循环中逐行读取 UART，交给 `src/protocol.c` 分派；状态、服药记录和提醒通过同一链路回传。默认示例数据在固件启动时初始化，时间由应用通过 `TIME` 同步（当前未启用 DS3231 RTC）。

## 构建边界

- Android：`android/` 是 Gradle Android application，目标 SDK 34，最低 SDK 24。
- Firmware：`firmware/platformio.ini` 使用 `bluepill_f103c8`、STM32Cube framework；`Core/` 与 `Drivers/` 是 HAL/CubeMX 支持树，`include/src` 是 PlatformIO 当前编译入口。
- Mechanical：`mechanical/src/` 保存 CadQuery/Python 参数化模型源，`mechanical/models/` 只保存发布用代表性 STEP/STL 输出。
