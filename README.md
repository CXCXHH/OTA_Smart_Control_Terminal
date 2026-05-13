# OTA Smart Control Terminal

STM32F103C8T6 smart control terminal with FreeRTOS, Modbus RTU, CANopen, ThingsBoard MQTT telemetry/control, and a W25Q64-backed OTA boot flow.

## Features

- FreeRTOS application with four runtime tasks:
  - `SensorTask`: AHT20, INA226, ADC/system voltage, CPU temperature sampling
  - `ModbusTask`: Modbus RTU slave
  - `CANopenTask`: CANopen slave based on CanFestival
  - `MQTTTask`: ML307 Cat.1 / ESP8266 AT MQTT access for ThingsBoard
- Shared register model for Modbus, CANopen, and MQTT.
- Mutex-protected shared data access between protocol and sensor tasks.
- ThingsBoard telemetry:
  - ambient temperature
  - humidity
  - device voltage
  - system current
  - device power
  - system/ADC voltage
  - CPU temperature
- Remote control through protocol shared output bits:
  - LED1
  - LED2
  - buzzer
  - relay
- Cloud OTA:
  - application downloads firmware chunks through MQTT
  - firmware is written to W25Q64 block 0
  - metadata is stored in AT24C02
  - bootloader copies firmware from W25Q64 to internal flash A region
- Bootloader firmware slot selection:
  - block 0: cloud OTA automatic upgrade slot
  - block 1: fallback/original application slot, selected by ESC/PA8
  - block 2: second application slot, selected by OK/PB12
- Bootloader OLED status display with a compact SSD1306 driver.

## Repository Layout

```text
APP1.0/           Main application: FreeRTOS + sensors + Modbus + CANopen + MQTT + OTA download
APP1.1/           Minimal OTA verification firmware, LED2 blink test
Bootloader_std/   Bootloader: startup selection, W25Q64 to A-region update, OLED prompt
```

## Hardware

- MCU: STM32F103C8T6
- External flash: W25Q64 over SPI1
- EEPROM: AT24C02 over software I2C
- Display: SSD1306 OLED over I2C
- Network module: ML307 Cat.1 as primary MQTT module
- Optional WiFi path: ESP8266 support remains in code, selected by `MQTT_WIFI_4G_ENABLE`
- Sensors:
  - AHT20 temperature/humidity
  - INA226 voltage/current/power
  - internal ADC for system voltage and CPU temperature
- User keys in bootloader:
  - ESC: PA8, selects W25Q64 block 1
  - OK: PB12, selects W25Q64 block 2

## Flash Layout

Internal STM32 flash:

```text
0x08000000 - 0x08004FFF   Bootloader / B region, 20 KB
0x08005000 - 0x0800FFFF   Application / A region, 44 KB
```

External W25Q64 slots:

```text
block 0   Cloud OTA automatic upgrade slot
block 1   Fallback/original APP1.0 slot
block 2   Secondary test/application slot
block 3+  Reserved
```

## Build

The projects are Keil MDK projects.

Application:

```powershell
& 'D:\keil 5\UV4\UV4.exe' -r '.\APP1.0\APP1.0.uvprojx' -j0 -t 'APP1.0' -o '.\APP1.0\.vscode\uv4.log'
```

Bootloader:

```powershell
& 'D:\keil 5\UV4\UV4.exe' -r '.\Bootloader_std\project.uvprojx' -j0 -t 'Bootloader_Std' -o '.\Bootloader_std\Bootloader_Std_build.log'
```

## Flashing

Example with OpenOCD and CMSIS-DAP:

```powershell
& 'D:\openOCD\OpenOCD-20250613-0.12.0\bin\openocd.exe' -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c "program Bootloader_std/Objects/project.hex verify reset exit"
& 'D:\openOCD\OpenOCD-20250613-0.12.0\bin\openocd.exe' -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c "program APP1.0/Objects/project.hex verify reset exit"
```

## Bootloader Behavior

At reset, the bootloader displays:

```text
BOOT
ESC B1
OK B2
```

Startup choices:

- Press `w` on UART1 within the startup window to enter the bootloader command line.
- Press ESC/PA8 to copy W25Q64 block 1 into the A region.
- Press OK/PB12 to copy W25Q64 block 2 into the A region.
- If no key is pressed:
  - OTA flag set: copy W25Q64 block 0 into the A region.
  - OTA flag clear: jump directly to the A-region application.

The bootloader refuses to update A region when the selected block length is zero or larger than the A-region capacity.

## ThingsBoard / MQTT Notes

The MQTT side is implemented with AT commands and subscribes to:

```text
v1/devices/me/attributes
v1/devices/me/rpc/request/+
v2/fw/response/+/chunk/#
```

OTA uses 256-byte firmware chunks for stable ML307/cloud behavior. The APP writes the finished firmware to W25Q64 block 0 and sets metadata in AT24C02 before resetting into the bootloader.

## Validation Status

Tested status at cleanup:

- APP1.0 build passes.
- Bootloader build passes and remains under the 20 KB B-region limit.
- 44 KB APP1.0 OTA through ThingsBoard completes in about one minute.
- Bootloader validates length/CRC and upgrades A region successfully.
- Modbus, CANopen, MQTT, sensor update, and output control have been tested together.
- Bootloader ESC/OK key selection and OLED status display have been tested.

## Notes Before Publishing

- Device credentials and server-side ThingsBoard configuration are intentionally not included.
- Generated files under `Objects/`, `Listings/`, `.vscode/`, and local logs are ignored.
- `project_Mem.md` is a local handoff note and is not part of the open-source repository.
