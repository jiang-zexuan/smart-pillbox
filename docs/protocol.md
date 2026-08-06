# 串口协议（已核实命令）

传输为 Bluetooth SPP/RFCOMM 字节流，Android 每条命令追加 `\\r\\n`，STM32 按行解析。下表只列出在 Android 与 STM32 源码中均能核实的业务命令。

| 命令 | 格式 | 固件行为 / 回执 |
| --- | --- | --- |
| `TIME` | `TIME:<unix_timestamp>` | 设置软件时钟；回 `TIME_ACK:<timestamp>`。 |
| `PLAN` | `PLAN:<slot>,<hh>,<mm>,<period>,<enabled>` | 校验 6 个仓位、时间和时段，保存计划；回 `PLAN_ACK:...`。 |
| `SET` | `SET:<slot>,<medicine_name>` | 写入药品名称并更新仓位状态；回 `SET_ACK:...`，启用电机时还回 `MOTOR_ACK:SET_SLOT,...`。 |
| `MOTOR` | `MOTOR:TEST`、`MOTOR:SLOT,<slot>`、`MOTOR:CW,<steps>`、`MOTOR:CCW,<steps>` | 在 `FEATURE_MOTOR_RESERVED=1` 时执行步进测试、转到仓位或按方向步进；回 `MOTOR_ACK:...`。源码将步数限制为 1..6400。 |
| `SYNC` | `SYNC:ALL` | 请求全量状态；固件调用 `BLE_SendStatus()` 回传状态/计划/仓位。 |

其它源码中出现的 `PING`、`TEST`、`BEEP`、`TARE`、`STAT`、`QR`、`REMIND`、`ALERT`、`REC` 属于心跳、调试或设备内部事件，本页不把它们列为已承诺的跨模块业务 API。

## 示例

```text
TIME:1780000000\r\n
PLAN:0,8,30,0,1\r\n
SET:0,Vitamin-C\r\n
MOTOR:SLOT,0\r\n
SYNC:ALL\r\n
```

协议没有版本字段、认证或加密；调用方应处理无回执、格式错误和设备断连。
