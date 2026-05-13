# OTA Smart Control Terminal Project Memory

> 本文件用于新窗口快速接手项目。只作为本地工作清单使用，不提交 git。

## 当前项目状态

工程路径：

`E:\STM32_Project\STM32F103C8T6\OTA_Smart_Control_Terminal`

当前主工程：

`APP1.0\APP1.0.uvprojx`

Keil 路径：

`D:\keil 5\UV4\UV4.exe`

串口：

`COM14`, `115200`

最近关键提交：

- `待提交 refactor: 统一三协议输出刷新入口`
- `fe38942 fix: 修复CANopen中断上下文控制回写并同步任务记忆`
- `c31d4ec fix: 固化共享寄存器索引映射`
- `f5865bc 增加任务间共享寄存器互斥锁`
- `d1a6e33 fix app mqtt rpc control`
- `bf8ab94 feat: ML307 ThingsBoard 接入（遥测上报 + 栈修复）`
- `d7a3a98 feat: CANopen OD 0x2000 映射到 REG_HOLD_BUF，三协议共享寄存器`

已完成能力：

- APP1.0 可连接 `xthings.cloud` / ThingsBoard MQTT。
- 遥测数据可上报：温度、湿度、设备电压、电流、功率、系统电压、CPU 温度。
- 云端 RPC 可控制 `led1`、`beep`、`relay`。
- MQTT 接收轮询已优化到 `50ms`，控制响应正常。
- FreeRTOS 任务之间已增加共享寄存器互斥锁。
- `REG_HOLD_BUF[]` 仍作为 Modbus / MQTT / CANopen 的共享寄存器区。
- LED2 暂不做业务控制，保留给 APP1.2 验证闪烁。
- 4G/WiFi 网络切换策略后续由按键外部中断和硬件切换开关控制。

当前验证状态：

- Keil 编译通过：`0 Error(s), 0 Warning(s)`。
- 用户实测：协议之间运行顺滑，互斥锁改动无明显问题。
- 2026-05-13 真云端 OTA 小固件验证已打通：`APP1.0` 已能正常连接 `xthings.cloud`，下载 `APP1.1` 小固件到 W25Q64，复位后 Bootloader 自动搬运到 A 区并启动。
  - 串口现象：
    - `Sensors OK`
    - `MQTT Server OK`
    - `OTA meta ver=APP1.1 size=1288 crc=dd9297d6`
    - `OTA ready block2 len=1288 crc=DD9297D6`
    - `OTA reboot for bootloader select`
    - `OTA YES!`
    - `长度1288字节`
    - `校验成功`
    - `升级完成`
    - `OTA GO A!`
  - 结论：
    - APP 侧 OTA 元数据解析、chunk 下载、W25Q64 写入、EEPROM 标记写入已形成闭环
    - Bootloader 自动检测 OTA_FLAG、从 W25Q64 block 0 搬运到 A 区、写后校验、清标志并跳转 A 区已验证
    - 小固件启动后 LED2 闪烁，证明 APP1.1 已运行
    - 大固件 44KB 仍需继续做长链路稳定性和速度优化；当前保持 ThingsBoard/APP1_1 兼容的 256B chunk
- 任务 1 已完成：共享寄存器索引宏已固化，`sensor_app.c`、`mqtt.c`、`modbus_app.c` 关键下标已替换为 `REG_IDX_*`。
- 任务 2 已完成：CANopen `0x2000` 已改为任务上下文同步快照读取，控制/从机地址写入改为 ISR 安全的延后应用，用户实测 CAN 控制恢复正常。
- 任务 3 已完成：MQTT、Modbus、CANopen 已统一走共享寄存器输出刷新入口，保持 Modbus coils 与 holding 输出组合逻辑一致。

## 当前代码结构重点

共享寄存器：

- 定义位置：`APP1.0/APP/modbus_app.c`
- 头文件：`APP1.0/APP/modbus_app.h`
- `REG_HOLD_BUF[0]`：输出控制位。
- `REG_HOLD_BUF[1]`：环境温度 `*100`。
- `REG_HOLD_BUF[2]`：湿度 `*100`。
- `REG_HOLD_BUF[3]`：设备电压 `*100`。
- `REG_HOLD_BUF[4]`：系统/设备电流 `mA`。
- `REG_HOLD_BUF[5]`：设备功率 `mW`。
- `REG_HOLD_BUF[6]`：ADC/系统电压 `*100`。
- `REG_HOLD_BUF[7]`：CPU 温度 `*100`。
- `REG_HOLD_BUF[9]`：Modbus 从机地址。

输出控制位：

- `LED1_CMD`
- `LED2_CMD`
- `BEEP_CMD`
- `RELAY_CMD`

互斥锁：

- 初始化：`REG_Lock_Init()`，在 `App_Tasks_Init()` 创建任务前调用。
- 加锁：`REG_Lock()`
- 解锁：`REG_Unlock()`
- 保护范围：`REG_HOLD_BUF[]` 和 `REG_COILS_BUF[]` 的读写。
- 注意：不要把 MQTT 发包、串口等待、OLED 显示、GPIO 控制长时间放在锁内。正确做法是加锁复制快照，解锁后再执行耗时操作。

FreeRTOS 当前任务：

- `ModbusTask`：10ms，处理 `eMBPoll()` 和输出刷新。
- `SensorTask`：500ms，采集传感器并写入共享寄存器。
- `CANopenTask`：100ms，处理 CANopen。
- `MQTTTask`：50ms 轮询下行队列，5s 上报遥测。

## 后续任务清单

### 1. 固化共享寄存器映射

状态：

- 已完成（commit: `c31d4ec fix: 固化共享寄存器索引映射`）。

目标：

让 Modbus / MQTT / CANopen 对同一套数据含义保持一致，避免后续新增协议时误用数组下标。

具体实现：

- 在 `APP1.0/APP/modbus_app.h` 中为 `REG_HOLD_BUF[]` 增加索引宏。
- 示例：

```c
#define REG_IDX_OUTPUT      0
#define REG_IDX_TEMP        1
#define REG_IDX_HUMI        2
#define REG_IDX_DEV_VOLT    3
#define REG_IDX_DEV_CURR    4
#define REG_IDX_DEV_POWER   5
#define REG_IDX_SYS_VOLT    6
#define REG_IDX_CPU_TEMP    7
#define REG_IDX_SLAVE_ADDR  9
```

- 将 `sensor_app.c`、`mqtt.c`、`modbus_app.c` 中的关键 `[0]..[7]` 替换为宏。
- 不改变寄存器地址，不改变云端字段名。

验证：

- Keil 编译 `0 Error(s), 0 Warning(s)`。
- xthings 遥测字段仍正常刷新。
- RPC 控制 `led1`、`beep`、`relay` 正常。

### 2. 补齐 CANopen 共享寄存器读写

状态：

- 已完成（本轮修复待提交）。

目标：

让 CANopen 和 Modbus/MQTT 一样，通过 `REG_HOLD_BUF[]` 查看传感器状态和控制输出。

具体实现：

- 检查 `APP1.0/APP/canopen_app.c` 和对象字典映射。
- 保持已有 `0x2000` 映射到 `REG_HOLD_BUF` 的思路。
- CANopen 写 `REG_HOLD_BUF[0]` 时必须使用 `REG_Lock()` / `REG_Unlock()`。
- CANopen 读传感器寄存器时也使用互斥锁复制快照。
- CANopen 写入输出位后调用输出刷新逻辑，避免只改寄存器不驱动物理设备。

验证：

- Keil 编译通过。
- 使用 CANopen SDO 读 `0x2000` 子索引，能看到温湿度、电压、电流、功率等寄存器。
- 使用 CANopen 写输出位后，`led1`、`beep`、`relay` 状态变化。

### 3. 整理输出控制入口

状态：

- 已完成（本轮提交待生成）。

目标：

让 MQTT、Modbus、CANopen 三个协议控制输出时行为一致。

具体实现：

- 当前底层输出函数在 `APP1.0/BSP/Src/bsp_gpio.c`。
- 当前 Modbus 输出刷新在 `Modbus_Parse()`。
- 后续可增加一个很薄的应用层函数，例如：

```c
void App_Output_ApplySnapshot(uint16_t hold0, uint8_t coil0, uint8_t coil1, uint8_t coil2, uint8_t coil3);
```

- 也可以暂时继续复用 `Output_Control(hold0)`，但要注意 Modbus coils 与 holding bits 的 OR 关系。
- LED2 继续保留底层和 Modbus coils 能力，但不要加入 MQTT RPC 面板业务控制。

验证：

- MQTT RPC 控制不受影响。
- Modbus coils 控制仍优先参与输出刷新。
- 后续 CANopen 写输出位后能统一刷新物理输出。

### 4. 完善 OTA APP 侧下载到 W25Q64

目标：

APP 侧能通过 MQTT/HTTP/云端 OTA 流程接收二进制文件，并写入 W25Q64，写入完成后设置升级标志。

当前进展（2026-05-13）：

- 已完成：
  - `APP1.0` 体积压缩到可用于 A 区升级验证的范围
  - APP 可正常启动并连接 `xthings.cloud`
  - OTA attributes 元数据已能解析出 `fw_version / fw_size / fw_checksum`
- `APP1.1` 小固件 OTA 已完成端到端验证：
  - APP 下载 1288B 固件到 W25Q64 block 0
  - APP 写 EEPROM OTA_FLAG、固件长度、版本和 CRC 元数据
  - Bootloader 复位后读取 OTA_FLAG，搬运 W25Q64 到 A 区
  - Bootloader 写后校验成功并跳转 A 区
  - APP1.1 运行后 LED2 闪烁
- 仍需优化：
  - 44KB 大固件长链路下载偶发超时
  - 512B/1KB chunk 在当前 xthings/ML307 链路上首包不稳定，暂回退到 APP1_1 兼容的 256B chunk
  - 后续可减少串口日志、增加断点续传或失败后从当前 chunk 重试来提升大固件体验
- 下一步排查重点：
  - 保持 `v2/fw/request/<id>/chunk/<n>` payload 为 `256`，与参考工程 `APP1_1` 一致
  - 大固件若再卡住，优先记录最后成功 chunk 和重试 chunk，判断是云端/模组偶发丢包还是解析层边界问题

具体实现：

- 检查 `APP1.0/APP/ota.c`、`APP1.0/APP/ota.h`。
- 确认 W25Q64 驱动接口：擦除、页写、读回校验。
- 设计 OTA 元数据：
  - 固件大小
  - 固件 CRC
  - 目标版本
  - 写入完成标志
  - 待升级标志
- 下载流程：
  1. 收到 OTA 开始命令。
  2. 擦除 W25Q64 指定区域。
  3. 分包写入二进制。
  4. 每包写后可选读回校验。
  5. 全部写完后校验 CRC。
  6. 写升级标志。
  7. 重启进入 BootLoader。

验证：

- 小文件写入 W25Q64 后读回一致。
- 模拟 OTA 包大小和 CRC 正确。
- 掉电或中断时不会误置升级完成标志。

### 5. 完善 B 区 BootLoader

目标：

B 区启动后判断是否需要 OTA。需要升级时，从 W25Q64 读取 APP 二进制，通过串口终端写入 A 区；不需要升级时直接跳转 A 区。

具体实现：

- BootLoader 启动后读取 W25Q64 升级标志。
- 没有升级标志：
  - 检查 A 区栈顶和复位向量是否合法。
  - 跳转 A 区。
- 有升级标志：
  - 读取固件大小和 CRC。
  - 从 W25Q64 分块读取。
  - 擦除 A 区 APP Flash。
  - 写入 A 区。
  - 写后 CRC 校验。
  - 清除升级标志。
  - 跳转 A 区。

风险点：

- BootLoader 和 APP 的 Flash 分区必须固定。
- APP 链接地址必须和 A 区起始地址一致。
- 中断向量表跳转要正确设置 `MSP` 和 `SCB->VTOR`。

验证：

- 无升级标志时能稳定跳转 APP1.0。
- 写入 APP1.2 后，LED2 闪烁作为升级成功标志。
- 升级失败时不能清除旧 APP 或不能跳转非法地址。

### 6. 制作 APP1.2 验证固件

目标：

提供一个明显可区分的 OTA 升级目标，用 LED2 闪烁验证升级成功。

具体实现：

- 复制或新建 APP1.2 工程。
- 保持 A 区链接地址与 APP1.0 一致。
- APP1.2 启动后 LED2 周期闪烁。
- 其他业务功能可以先最小化，不要求完整 MQTT/Modbus。

验证：

- 单独烧录 APP1.2 到 A 区，LED2 能闪烁。
- 通过 BootLoader OTA 写入 APP1.2 后，LED2 能闪烁。

### 7. 4G/WiFi 网络管理策略

目标：

后续通过按键外部中断和硬件切换开关控制 4G/WiFi 主备网络。

具体实现：

- 增加网络模式状态：
  - `NET_MODE_4G`
  - `NET_MODE_WIFI`
  - `NET_MODE_AUTO`
- 外部中断只置位事件标志，不做 AT 指令和打印。
- 网络任务读取事件标志后执行切换。
- 切换流程：
  1. 停止当前 MQTT 连接。
  2. 切换硬件电源或使能脚。
  3. 初始化目标模块。
  4. 重新连接 MQTT。
  5. 重新订阅 RPC topic。

验证：

- 按键触发后不会卡死。
- 切换后遥测恢复上报。
- RPC 控制恢复可用。

### 8. 协议一致性测试

目标：

确认 MQTT、Modbus、CANopen 三个协议读到的数据一致，写控制位不会互相覆盖。

具体测试项：

- SensorTask 刷新时，MQTT 上报和 Modbus 读保持同一量级。
- MQTT 打开 `led1` 后，Modbus 读 holding register 0 能看到对应 bit。
- Modbus 写 coils 后，物理输出变化。
- CANopen 写输出位后，MQTT 状态查询能返回正确状态。
- 高频 MQTT RPC 和 Modbus 轮询同时进行时，无 HardFault，无明显卡顿。

验证工具：

- xthings.cloud / ThingsBoard dashboard。
- Modbus 调试工具。
- USB-CAN 或 CANopen SDO 工具。
- 串口 COM6 日志。

## 常用命令

Keil 编译：

```powershell
& 'D:\keil 5\UV4\UV4.exe' -b 'E:\STM32_Project\STM32F103C8T6\OTA_Smart_Control_Terminal\APP1.0\APP1.0.uvprojx' -j0 -t 'APP1.0' -o 'E:\STM32_Project\STM32F103C8T6\OTA_Smart_Control_Terminal\APP1.0\.vscode\uv4.log'
```

Keil 烧录：

```powershell
& 'D:\keil 5\UV4\UV4.exe' -f 'E:\STM32_Project\STM32F103C8T6\OTA_Smart_Control_Terminal\APP1.0\APP1.0.uvprojx' -j0 -t 'APP1.0' -o 'E:\STM32_Project\STM32F103C8T6\OTA_Smart_Control_Terminal\APP1.0\.vscode\uv4_flash.log'
```

查看编译结果：

```powershell
Get-Content -LiteralPath 'APP1.0\.vscode\uv4.log' -Tail 80
```

查看 git 状态：

```powershell
git status --short
```

## 新窗口接手建议

优先顺序：

1. 先读本文件。
2. 再读 `APP1.0/APP/app_tasks.c`，理解四个 FreeRTOS 任务。
3. 再读 `APP1.0/APP/modbus_app.c/.h`，理解共享寄存器和互斥锁。
4. 再读 `APP1.0/APP/mqtt.c`，理解遥测和 RPC 控制。
5. 再读 `APP1.0/APP/Sensor/sensor_app.c`，理解传感器数据来源。
6. 下一步优先做“完善 OTA APP 侧下载到 W25Q64”。

当前不建议立刻做的大改：

- 不要把 `REG_HOLD_BUF[]` 直接替换成全局结构体。
- 不要重写 MQTT 连接流程。
- 不要现在做复杂自动网络切换。
- 不要把 LED2 加入 MQTT 控制面板。
- 不要在互斥锁内执行长时间 AT 指令、串口等待、OLED 刷屏。
