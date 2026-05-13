# OTA 智能控制终端

基于 `STM32F103C8T6` 的智能控制终端，集成 `FreeRTOS`、`Modbus RTU`、`CANopen`、`ThingsBoard MQTT` 遥测/控制，以及基于 `W25Q64` 的 OTA 启动升级流程。

## 功能特性

- 基于 `FreeRTOS` 的应用程序，包含四个主要运行任务：
  - `SensorTask`：采集 `AHT20`、`INA226`、ADC 系统电压、CPU 温度
  - `ModbusTask`：`Modbus RTU` 从站
  - `CANopenTask`：基于 `CanFestival` 的 `CANopen` 从站
  - `MQTTTask`：通过 `ML307 Cat.1` / `ESP8266 AT` 接入 `ThingsBoard MQTT`
- `Modbus`、`CANopen`、`MQTT` 共用同一套寄存器模型。
- 协议任务与传感器任务之间通过互斥锁保护共享数据访问。
- `ThingsBoard` 遥测上报内容包括：
  - 环境温度
  - 环境湿度
  - 设备电压
  - 系统电流
  - 设备功率
  - 系统/ADC 电压
  - CPU 温度
- 通过协议共享输出位远程控制：
  - `LED1`
  - `LED2`
  - 蜂鸣器
  - 继电器
- 云端 OTA 流程：
  - 应用程序通过 `MQTT` 下载固件分片
  - 固件写入 `W25Q64` 的 `block 0`
  - 元数据存入 `AT24C02`
  - Bootloader 将固件从 `W25Q64` 复制到内部 Flash A 区
- Bootloader 固件槽选择：
  - `block 0`：云端 OTA 自动升级槽
  - `block 1`：原始/回退应用槽，通过 `ESC/PA8` 选择
  - `block 2`：第二应用槽，通过 `OK/PB12` 选择
- Bootloader 带简化版 `SSD1306 OLED` 状态显示。

## 仓库结构

```text
APP1.0/           主应用：FreeRTOS + 传感器 + Modbus + CANopen + MQTT + OTA 下载
APP1.1/           最小 OTA 验证固件，LED2 闪烁测试
Bootloader_std/   Bootloader：启动选择、W25Q64 到 A 区更新、OLED 提示
```

## 硬件组成

- MCU：`STM32F103C8T6`
- 外部 Flash：`W25Q64`，使用 `SPI1`
- EEPROM：`AT24C02`，使用软件 `I2C`
- 显示屏：`SSD1306 OLED`，使用 `I2C`
- 主通信模块：`ML307 Cat.1`
- 可选 WiFi 通道：代码中保留 `ESP8266` 支持，通过 `MQTT_WIFI_4G_ENABLE` 选择
- 传感器：
  - `AHT20` 温湿度
  - `INA226` 电压/电流/功率
  - MCU 内部 ADC 采样系统电压和 CPU 温度
- Bootloader 启动按键：
  - `ESC`：`PA8`，选择 `W25Q64 block 1`
  - `OK`：`PB12`，选择 `W25Q64 block 2`

## 原理图引脚映射

仓库中的代码与原理图网络名对应关系如下。这一节可以直接作为复刻硬件或移植固件时的最小接线参考。

| 功能 | 原理图网络名 | STM32 引脚 | 说明 |
| --- | --- | --- | --- |
| 调试串口 | `TXD1` / `RXD1` | `PA9` / `PA10` | `USART1`，用于 APP 日志和 Bootloader 命令窗口 |
| Modbus RTU | `TXD2` / `RXD2` | `PA2` / `PA3` | `USART2`，连接 `RS485` 收发器 |
| RS485 方向控制 | `RS485WR2` | `PB2` | 高电平发送，低电平接收 |
| 4G / WiFi AT 串口 | `TXD3` / `RXD3` | `PB10` / `PB11` | `USART3`，连接 `ML307` 或 `ESP8266` |
| CAN 总线 | `CANTX` / `CANRX` | `PA12` / `PA11` | `CAN1`，当前固件配置为 `500 kbps` |
| OLED / EEPROM / AHT20 | `SCL1` / `SDA1` | `PB6` / `PB7` | 共用软件 `I2C` 总线 |
| W25Q64 SPI | `FLASH_SCK` / `FLASH_MISO` / `FLASH_MOSI` / `FLASH_CS` | `PA5` / `PA6` / `PA7` / `PA4` | APP 和 Bootloader 共用 OTA 存储 |
| INA226 总线 | `SCL1` / `SDA1` | `PB6` / `PB7` | 与 EEPROM / OLED 共用同一 `I2C` 总线 |
| 电压采样 | `ADC_VR` | `PA1` | `ADC1_IN1`，用于板级电压采样 |
| Boot 按键 ESC | `SW_LEFT` | `PA8` | Bootloader 选择 `W25Q64 block 1` |
| Boot 按键 OK | `SW_OK` | `PB12` | Bootloader 选择 `W25Q64 block 2` |
| `LED1` | `LED1` | `PB9` | 低电平点亮 |
| `LED2` | `LED2` | `PB8` | 低电平点亮 |
| 蜂鸣器 | `BEEP` | `PC13` | 高电平有效 |
| 继电器 | `RELAY` | `PB1` | 高电平有效 |

原理图中其它按键通过 `H2` 扩展接口引出，当前开源固件启动流程暂未使用。

## 接口说明

- `J4` USB 转串口：
  - `TXD1` / `RXD1` 通过 `CH340G` 接到 PC 调试串口
  - 推荐用于 Bootloader 交互和 APP 日志采集
- `J3` RS485：
  - 差分 `A/B` 由 `SP3485` 收发器输出
  - 固件通过 `PB2` 控制收发方向
- `J2` CAN：
  - 差分 `CANH/CANL` 由 `SIT1040` 收发器输出
- `P1` 4G Cat.1：
  - `ML307` 的 AT 串口使用 `PB10/PB11`
- `J1` ESP-12F：
  - 可选 `ESP8266` AT 通道，同样复用 `PB10/PB11`

## 板级功能模块

原理图按多个小功能块组织。对于第一次接手项目的人，下表可以快速说明每个模块的器件、作用以及在固件中的用途。

| 原理图模块 | 主要器件 | 作用 | 固件用途 |
| --- | --- | --- | --- |
| STM32 主控 | `STM32F103C8T6` | 核心控制器 | 运行 Bootloader 和 `FreeRTOS` 主应用 |
| USB 转串口 | `CH340G` | PC 下载/日志串口 | 承载 `USART1` 调试输出和 Bootloader CLI |
| 4G Cat.1 模块 | `ML307` | 主 `MQTT/ThingsBoard` 通信链路 | 由 `USART3` 的 AT 驱动使用 |
| ESP8266 WiFi 模块 | `ESP-12F / ESP8266MOD` | 可选 WiFi 通信链路 | 与 `USART3` 共用 AT 通道 |
| CAN 收发器 | `SIT1040QT` | MCU CAN 转差分 CAN 总线 | 供 `CAN1` / `CANopenTask` 使用 |
| RS485 收发器 | `SP3485EN` | MCU UART 转差分 RS485 总线 | 供 `USART2` / `Modbus RTU` 从站使用 |
| SPI Flash | `W25Q64JVSSIQ` | 外部 OTA 固件存储 | APP 写 OTA 包，Bootloader 负责搬运到内部 Flash |
| EEPROM | `24C02` | 存放 OTA 元数据和少量持久化信息 | 挂在软件 `I2C` 总线上 |
| 温湿度传感器 | `AHT20` | 采集环境温度和湿度 | 由 `SensorTask` 通过 `I2C` 读取 |
| 电流/功率监测 | `INA226AIDGSR` | 采集总线电压、电流、功率 | 由 `SensorTask` 通过 `I2C` 读取 |
| OLED 显示 | `0.96" I2C OLED` | 本地状态显示 | 主要用于 Bootloader 启动/升级提示 |
| 继电器驱动 | `S8050 + relay` | 控制外部负载通断 | 通过共享输出寄存器位控制 |
| 蜂鸣器驱动 | `S8050 + buzzer` | 提供声音提示 | 通过共享输出寄存器位控制 |
| 指示灯 | `LED1 / LED2` | 提供状态指示 | 通过共享输出寄存器位控制 |
| 启动按键区 | 多按键网络 | 本地启动选择和人机交互 | 当前 Bootloader 主要使用 `SW_LEFT` 和 `SW_OK` |
| ADC 分压 / 电位器 | 分压电阻 + `VR1` | 板级电压 / 模拟量采样 | 通过 `PA1` 的 `ADC_VR` 采样 |
| 电源模块 | `AMS1117-3.3` | 从 5V 生成 `VCC3V3` | 为 MCU 和外设供电 |
| 时钟模块 | `8 MHz` HSE + `32.768 kHz` LSE | 主系统时钟和 RTC 时钟 | 提供 STM32 时钟源 |
| JTAG 调试接口 | 调试排针 | SWD/JTAG 下载与调试 | 用于 `Keil` / `OpenOCD` / `CMSIS-DAP` 工作流 |

## 模块备注

- APP 和 Bootloader 共用同一条 `W25Q64 + AT24C02` OTA 存储链路。
- `PB6/PB7` 被 OLED、`AHT20`、`INA226`、`EEPROM` 共用，因此复刻硬件时要注意上拉和总线负载。
- `PB10/PB11` 在板级上被 `ML307` 和 `ESP8266` 复用，但当前开源工程默认以 `ML307` 作为主 MQTT 通道。
- Bootloader 的本地交互刻意保持简单：`OLED + PA8 + PB12 + UART1`。

## Boot 按键说明

- `PA8` 对应原理图左键，在 Bootloader 中作为 `ESC` 使用。
- `PB12` 对应原理图中间确认键，在 Bootloader 中作为 `OK` 使用。
- 两个按键均为低电平有效，Bootloader 内部开启上拉输入进行采样。

## Flash 布局

STM32 内部 Flash：

```text
0x08000000 - 0x08004FFF   Bootloader / B 区，20 KB
0x08005000 - 0x0800FFFF   Application / A 区，44 KB
```

外部 `W25Q64` 槽位：

```text
block 0   云端 OTA 自动升级槽
block 1   APP1.0 原始/回退槽
block 2   第二测试/应用槽
block 3+  预留
```

## 编译

工程为 `Keil MDK` 工程。

应用程序：

```powershell
& 'D:\keil 5\UV4\UV4.exe' -r '.\APP1.0\APP1.0.uvprojx' -j0 -t 'APP1.0' -o '.\APP1.0\.vscode\uv4.log'
```

Bootloader：

```powershell
& 'D:\keil 5\UV4\UV4.exe' -r '.\Bootloader_std\bootloader_std.uvprojx' -j0 -t 'Bootloader_Std' -o '.\Bootloader_std\Bootloader_Std_build.log'
```

## 烧录

下面是使用 `OpenOCD + CMSIS-DAP` 的示例：

```powershell
& 'D:\openOCD\OpenOCD-20250613-0.12.0\bin\openocd.exe' -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c "program Bootloader_std/Objects/project.hex verify reset exit"
& 'D:\openOCD\OpenOCD-20250613-0.12.0\bin\openocd.exe' -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c "program APP1.0/Objects/project.hex verify reset exit"
```

## Bootloader 行为

上电复位后，Bootloader OLED 显示：

```text
BOOT
ESC B1
OK B2
```

启动选择逻辑：

- 在启动窗口期通过 `UART1` 发送字符 `w`，进入 Bootloader 命令行。
- 按下 `ESC/PA8`，将 `W25Q64 block 1` 复制到 A 区。
- 按下 `OK/PB12`，将 `W25Q64 block 2` 复制到 A 区。
- 如果没有按键：
  - 若 OTA 标志已置位：将 `W25Q64 block 0` 复制到 A 区。
  - 若 OTA 标志未置位：直接跳转到 A 区应用程序。

当所选块长度为 0，或超出 A 区容量时，Bootloader 会拒绝执行更新。

## ThingsBoard / MQTT 说明

MQTT 侧通过 AT 指令实现，并订阅以下主题：

```text
v1/devices/me/attributes
v1/devices/me/rpc/request/+
v2/fw/response/+/chunk/#
```

OTA 采用 `256` 字节固件分片，以兼顾 `ML307` 与云端链路的稳定性。APP 在下载完成后会把固件写入 `W25Q64 block 0`，并把元数据写入 `AT24C02`，随后复位进入 Bootloader。

## 验证状态

当前整理后的实测状态：

- `APP1.0` 编译通过。
- Bootloader 编译通过，并且仍然小于 `20 KB` B 区限制。
- `44 KB` 的 `APP1.0` 固件可通过 `ThingsBoard OTA` 在约一分钟内完成升级。
- Bootloader 能正确校验长度/CRC 并完成 A 区升级。
- `Modbus`、`CANopen`、`MQTT`、传感器采集和输出控制已联合测试通过。
- Bootloader 的 `ESC/OK` 按键选择和 OLED 状态显示已测试通过。

## 开源前说明

- 设备凭证和服务端 `ThingsBoard` 配置不会包含在仓库中。
- `Objects/`、`Listings/`、`.vscode/` 以及本地日志等生成文件均已忽略。
- `project_Mem.md` 是本地交接笔记，不属于开源仓库内容。
