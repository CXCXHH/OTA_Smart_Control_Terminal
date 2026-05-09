# Project Memory — 工程开发手册

## 2026-05-09 完整开发记录

---

## 一、工程架构

### 1.1 硬件平台
- **MCU**: STM32F103C8T6, 64KB Flash, 20KB SRAM
- **调试器**: CMSIS-DAP (OpenOCD)
- **调试串口**: USART1 → COM24, 115200
- **Modbus串口**: USART2 → RS485 (PA2=TX, PA3=RX, PB2=方向控制), 115200
- **WiFi**: ESP8266 (USART3, PB10/PB11), AT命令
- **OpenOCD**: `D:\openOCD\OpenOCD-20250613-0.12.0\bin\openocd.exe`
- **Keil**: `D:\keil 5\UV4\UV4.exe`

### 1.2 Flash 分区
```
0x08000000 ┌──────────────┐
           │  Bootloader   │  20KB (OTA管理 + 固件跳转)
0x08005000 ├──────────────┤
           │  APP          │  44KB (FreeRTOS + 协议栈 + 传感器)
0x08010000 └──────────────┘
```
- 升级固件先写入 W25Q64 (SPI Flash)
- AT24C02 只存 OTA 标记和长度
- 通过 ThingsBoard OTA 下发

### 1.3 I2C 总线设备 (PB6/PB7, 软件I2C)
| 设备 | 7-bit地址 | 8-bit地址 | 状态 |
|------|----------|----------|------|
| AHT20 | 0x38 | 0x70 | ✅ 正常 |
| INA226 | 0x41 | 0x82 | ✅ 通信正常 |
| OLED | 0x3C | 0x78 | ✅ 显示正常 |
| AT24C02 | 0x50 | 0xA0 | ✅ 正常 |

### 1.4 寄存器映射 (Modbus RTU, 从站地址=1)

**保持寄存器 (FC3/6/16):**
```
HOLD[1] (addr 0) = 输出位域 (bit0=LED1, bit1=LED2, bit2=BEEP, bit3=RELAY)
HOLD[2] (addr 1) = 温度 * 100 (AHT20)
HOLD[3] (addr 2) = 湿度 * 100 (AHT20)
HOLD[4] (addr 3) = 总线电压 * 100 (INA226)
HOLD[5] (addr 4) = 电流 mA (INA226)
HOLD[6] (addr 5) = 功率 mW (INA226)
HOLD[7] (addr 6) = ADC 电压
HOLD[8] (addr 7) = ADC 温度
HOLD[10](addr 9) = 从机地址
```

**线圈 (FC1/5/15):**
```
COIL[1] (addr 0) = LED1
COIL[2] (addr 1) = LED2
COIL[3] (addr 2) = 蜂鸣器
COIL[4] (addr 3) = 继电器
```

### 1.5 GPIO 映射
| 功能 | 引脚 | 说明 |
|------|------|------|
| LED1 | PB9 | 低电平亮 |
| LED2 | PB8 | 低电平亮 |
| 继电器 | PB1 | 高电平吸合 |
| 蜂鸣器 | PC13 | 高电平响 |
| RS485方向 | PB2 | 高=发送, 低=接收 |
| I2C SCL | PB6 | 软件I2C, 推挽输出 |
| I2C SDA | PB7 | 软件I2C, 输出推挽/输入上拉 |

---

## 二、已完成的工作

### 2.1 I2C 软件驱动 ✅
- 软件 I2C 在 PB6/PB7 上稳定工作
- SCL: 推挽输出, SDA: 发送阶段推挽输出, ACK/读阶段切换输入上拉
- 支持 `IIC_WriteBuf`, `IIC_ReadBuf`, `IIC_MemWrite`, `IIC_MemRead`
- `IIC_MemRead` 使用重复起始条件 (repeated START)
- 延时参数: `Delay_us(2)`, 经验证为最佳值

### 2.2 硬件 I2C 不可用 ❌ (待硬件修复)
- **根本原因**: STM32F103 的 AF_OD 模式需要外部上拉电阻 (4.7kΩ→3.3V)
- **现象**: 配置为 AF_OD 后 SCL=SDA=0V, I2C SR2=0x0002 (BUSY标志置位)
- **验证**: 推挽输出可正常拉高 PB6/PB7, 说明MCU引脚功能正常
- **硬件I2C代码**: 已保留在 `bsp_i2c_hw.c`, 包含完整的总线恢复逻辑
- **修复方案**: 在 PB6/PB7 上焊接 4.7kΩ 上拉电阻到 3.3V, 然后切换到硬件 I2C

### 2.3 AHT20 驱动 ✅
- 初始化命令: `{0xBE, 0x08, 0x00}`
- 触发测量: `{0xAC, 0x33, 0x00}` 后等待 >80ms
- 读6字节, 计算温湿度: `T = (raw*625/2^15) - 50 (°C*100)`, `RH = raw*625/2^16 (%*100)`
- 稳定读数: T=25~29°C, RH=47~55%

### 2.4 INA226 驱动 ✅ (通信正常)
- 地址 0x82, MID=0x5449, DID=0x2260 已验证
- **关键修复**: 读寄存器从"重复START"改为"分离写+读" (参考 HAL 版)
  - 旧: `START + addr+W + reg + RESTART + addr+R + data + STOP`
  - 新: `START + addr+W + reg + STOP`, 然后 `START + addr+R + data + STOP`
  - INA226 需要 STOP 来锁存寄存器指针
- **配置**: 0x4527 (16次平均, 1.1ms转换时间, ~35ms/周期)
- **校准**: 5120 (0.1mA LSB, 0.01Ω 采样电阻)
- **总线电压**: 正常读数 4.98V (VBUS已连接)
- **电流/功率**: 正常读数 179-204mA / 640-992mW
- **归零问题**: 片上拉电阻不足导致 I2C 偶发失败, 已添加3次重试
- **CFG/CAL寄存器读回**: 有时显示0000 (可能是 FreeRTOS 任务调度导致的总线时序问题)
- **电压/功率换算代码**:
```c
REG_HOLD_BUF[3] = ina_bus / 8;       // V*100 (raw / 8 = voltage*100)
REG_HOLD_BUF[4] = ina_current / 10;   // mA (raw / 10 = mA, LSB=0.1mA)
REG_HOLD_BUF[5] = (ina_power * 5) / 2; // mW (raw * 2.5 = mW)
```

### 2.5 OLED 显示 ✅
- 字库从 15 字符扩展到完整 95 字符 ASCII (空格~波浪号)
- 字库格式: 列优先, 6字节/字符, bit0=顶行
- 来源: 参考工程 `19-i2c-oled\Core\Inc\codetab.h` 的 `F6x8[][6]`
- 显示行宽: 21字符/行 (128像素 / 6像素/字), 末尾空格填充防残影
- 显示内容: T/°C RH/%, V/I, P/mW, 状态行
- 注意: OLED_ShowStr 的原查找表方式不支持中文字符 (超过127的会越界)

### 2.6 FreeRTOS 任务结构 ✅
| 任务 | 优先级 | 栈 | 功能 |
|------|--------|-----|------|
| ModbusTask | +2 | 256 | eMBPoll + Modbus_Parse (输出轮询) |
| SensorTask | +1 | 256 | AHT20/INA226/ADC 采集, 每500ms |
| MQTTTask | +1 | 384 | WiFi连接 + MQTT 发布订阅 |

### 2.7 MQTT ✅
- ESP8266 AT 命令驱动, 通过 USART3
- 连接 broker.emqx.io:1883
- 发布主题: `STM32V9/UPLoad/{deviceID}`
- 订阅主题: `STM32V9/DownLoad/{deviceID}`
- cJSON 解析下行指令 (待完善)
- 已知问题: USART1 printf 多任务交织 (无互斥锁)

### 2.8 CANOpen ✅
- CanFestival 协议栈, NodeID=1
- 状态: Operational
- CAN1 中断处理 RX0 消息

### 2.9 OTA 基础 ✅
- Bootloader 正确跳转 APP (OTA GO A!)
- OTA_CheckAndRollback() 运行, 显示 "OTA none"
- AT24C02 存储标记和长度
- 待完善: W25Q64 固件搬运 + CRC32 + ThingsBoard OTA 集成

---

## 三、待解决的问题

### 3.1 I2C 上拉电阻 ❌ (硬件)
- **优先级**: 最高
- **现象**: 不触摸上拉电阻时 INA226 读数偶尔归零
- **根因**: PB6/PB7 无外部上拉 → AF_OD 不可用 → 软件 I2C 推挽输出不够稳定
- **解决**: 焊接 4.7kΩ 上拉电阻

### 3.2 上位机输出控制 ❌ (Modbus 协议)
- **优先级**: 高
- **现象**: 点击上位机任意按钮 (LED1/LED2/蜂鸣器/继电器), 所有输出同时触发, 且无法单独关闭
- **分析**: 上位机使用 **FC5 (写单个线圈)** 独立控制每个输出, 不是 FC6
- **参考**: 完整 HAL 参考工程 `MQTT_WIFI+4G+FreeRtos\Middleware\FreeModbus\mbconfig.h` 中线圈启用
- **当前状态**: 线圈已启用, 但似乎上位机仍然走 FC6 写 HOLD[1]=0x000F
- **可能原因**:
  1. 上位机先发 FC17 (Report Slave ID) 探测固件版本, 当前未实现
  2. 上位机通过 MQTT 判断固件能力, 然后选择控制方式
  3. 需要特定的从站ID响应才能进入"线圈控制模式"
- **下一步**: 在 ISR 外添加 Modbus 帧日志 (不要在 ISR 里 printf!), 确认上位机实际发送的功能码

### 3.3 上位机传感器数据显示 ❌
- **现象**: 上位机不显示温湿度/电压/电流数据
- **数据确认**: Modbus 读响应内容正确 (T=2500 → 25°C, RH=4638 → 46.38%)
- **猜测**: 上位机通过 MQTT 读传感器数据, 不是 Modbus 串口
- **验证方法**: 在上位机找 MQTT 连接设置, 连 broker.emqx.io, 订阅 STM32V9/UPLoad/+

### 3.4 INA226 CFG/CAL 寄存器回读异常
- **现象**: INA226_Init() 返回成功, 但紧接着的 CFG/CAL 读返回 0x0000
- **推测**: FreeRTOS 任务调度导致 I2C 时序不稳定, 或 I2C 上拉不足
- **影响**: 不影响实际测量 (电压/电流/功率正常), 但影响诊断
- **解决**: 换硬件 I2C 后应自然解决

### 3.5 U1_printf 多任务交织
- **现象**: SensorTask / MQTTTask / ModbusTask 同时往 USART1 打印, 输出混乱
- **快速修复**: 加互斥量保护 U1_printf
- **长期方案**: 用独立调试串口或 ITM

---

## 四、调试命令速查

### 4.1 编译
```powershell
& "D:\Python 3.8\Python38\python.exe" `
  "C:\Users\m1511\.claude\skills\cx-keil-skill\scripts\cx_build_keil.py" `
  --project "e:\STM32_Project\STM32F103C8T6\OTA_Smart_Control_Terminal\APP1.0\APP1.0.uvprojx" `
  --uv4 "D:\keil 5\UV4\UV4.exe"
```

### 4.2 烧录 (Bootloader + APP)
```powershell
& "D:\openOCD\OpenOCD-20250613-0.12.0\bin\openocd.exe" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c init -c "reset halt" `
  -c "program e:/STM32_Project/STM32F103C8T6/OTA_Smart_Control_Terminal/Bootloader_std/Objects/project.hex verify" `
  -c "program e:/STM32_Project/STM32F103C8T6/OTA_Smart_Control_Terminal/APP1.0/Objects/project.hex verify" `
  -c "mdw 0x08005000 4" -c "reset run" -c shutdown
```

### 4.3 串口监视
```powershell
& "D:\Python 3.8\Python38\python.exe" `
  "C:\Users\m1511\.claude\skills\cx-keil-skill\scripts\cx_serial_monitor.py" `
  --port COM24 --baud 115200 --duration 15 --timestamp
```

### 4.4 软件复位 + 抓启动日志
```powershell
# Terminal 1: 先开串口等待
python cx_serial_monitor.py --port COM24 --baud 115200 --wait-reset --duration 15
# Terminal 2: 再复位
openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c init -c "reset run" -c shutdown
```

### 4.5 仅烧录 APP
```powershell
openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c init -c "reset halt" `
  -c "program e:/STM32_Project/STM32F103C8T6/OTA_Smart_Control_Terminal/APP1.0/Objects/project.hex verify" `
  -c "reset run" -c shutdown
```

---

## 五、关键文件清单

| 文件 | 作用 |
|------|------|
| `APP1.0/User/main.c` | 主入口, 初始化 + 启 FreeRTOS |
| `APP1.0/APP/app_tasks.c` | FreeRTOS 任务 (Modbus/Sensor/MQTT) |
| `APP1.0/APP/modbus_app.c` | Modbus 回调 + 寄存器 + 线圈 + 输出控制 |
| `APP1.0/BSP/Src/bsp_iic.c` | **软件 I2C** 驱动 (当前使用) |
| `APP1.0/BSP/Src/bsp_i2c_hw.c` | **硬件 I2C** 驱动 (待上拉电阻) |
| `APP1.0/APP/Sensor/aht20.c` | AHT20 传感器驱动 |
| `APP1.0/APP/Sensor/ina226.c` | INA226 传感器驱动 (分离写读模式) |
| `APP1.0/APP/oled.c` | OLED 显示 (完整95字ASCII) |
| `APP1.0/BSP/Src/bsp_gpio.c` | GPIO 输出控制 (LED/继电器/蜂鸣器/RS485) |
| `APP1.0/Protocol/FreeModbus/` | FreeModbus V1.6 协议栈 |
| `APP1.0/Protocol/FreeModbus/mbconfig.h` | Modbus 功能码开关 |
| `APP1.0/Protocol/FreeModbus/portserial.c` | RS485 串口驱动 (含 TC 等待) |

---

## 六、下一步开发计划

### 优先级排序:
1. **[硬件]** 焊接 PB6/PB7 上拉电阻 (4.7kΩ → 3.3V), 然后切硬件 I2C
2. **[协议]** 确认上位机实际发送的功能码 (在 eMBPoll 返回后打印帧信息, 不要在 ISR 里 printf)
3. **[协议]** 完善 cJSON MQTT 下行指令解析
4. **[OTA]** W25Q64 固件搬运 + CRC32 + ThingsBoard 集成
5. **[输出]** 确认 GPIO 输出控制逻辑正确 (LED1/LED2/BEEP/RELAY 独立控制)
6. **[串口]** 给 U1_printf 加互斥锁
