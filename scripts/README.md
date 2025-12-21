# Build & Flash Scripts

Helper scripts for building and flashing all three project types.

## Overview

This repository contains three independent projects:
- **Temperature Sensor**: ESP8266/ESP32 + DS18B20
- **Solar Monitor**: ESP32 + Victron SmartShunt + MPPT controllers
- **Surveillance Camera**: ESP32-CAM / ESP32-S3

## Build Systems

### PlatformIO (Temperature Sensor & Solar Monitor)

Both temperature sensor and solar monitor use **PlatformIO** build system:

```bash
cd temperature-sensor
pio run -e esp8266 --target upload --upload-port /dev/ttyUSB0

# Or ESP32:
pio run -e esp32dev --target upload --upload-port /dev/ttyUSB0
```

For solar monitor (ESP32 only):
```bash
cd solar-monitor
pio run --target upload --upload-port /dev/ttyUSB0
pio device monitor --baud 115200
```

### Arduino CLI (Surveillance Camera)

Surveillance camera uses **Arduino CLI** with custom build system:

```bash
cd surveillance-arduino
./COMPILE.sh              # ESP32-CAM (AI-Thinker)
./COMPILE_ESP32S3.sh      # ESP32-S3 (Freenove)
```

Manual upload:
```bash
# ESP32-CAM
./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance

# ESP32-S3
./bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3:PSRAM=enabled,PartitionScheme=huge_app,FlashMode=qio ESP32CAM_Surveillance

# Monitor
./bin/arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## Device Configuration

All devices use **WiFiManager captive portal** for configuration:

1. Flash device using appropriate build system
2. Device creates AP (e.g., `ESP-Setup`, `Solar-Monitor-Setup`, `ESP32-CAM-Setup`)
3. Connect to AP and open captive portal (usually auto-opens)
4. Configure WiFi credentials and device name
5. Settings persist in flash/NVS across reboots

## Board Support

| Board | Project | Port | Build System | FQBN |
|-------|---------|------|--------------|------|
| ESP8266 (NodeMCU) | Temperature Sensor | `/dev/ttyUSB*` | PlatformIO | `esp8266` |
| ESP32 (WROOM) | Temperature Sensor / Solar Monitor | `/dev/ttyUSB*` | PlatformIO | `esp32dev` |
| ESP32-CAM (AI-Thinker) | Surveillance | `/dev/ttyUSB*` | Arduino CLI | `esp32:esp32:esp32cam` |
| ESP32-S3 (Freenove) | Surveillance | `/dev/ttyACM*` | Arduino CLI | `esp32:esp32:esp32s3` |

## USB Device Paths

Common serial ports by board:
- **ESP8266/ESP32**: `/dev/ttyUSB0`, `/dev/ttyUSB1` (multiple devices)
- **ESP32-CAM**: `/dev/ttyUSB*` (USB bridge)
- **ESP32-S3**: `/dev/ttyACM0` (native USB)

Check connected devices:
```bash
ls -la /dev/tty*
# or
platformio device list
```

## Quick Reference

**Temperature Sensor (all boards)**:
```bash
cd temperature-sensor
pio run -e esp8266 --target upload --upload-port /dev/ttyUSB0
```

**Solar Monitor (ESP32 only)**:
```bash
cd solar-monitor
pio run --target upload --upload-port /dev/ttyUSB0
```

**Surveillance - ESP32-CAM**:
```bash
cd surveillance-arduino
./COMPILE.sh
./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance
```

**Surveillance - ESP32-S3**:
```bash
cd surveillance-arduino
./COMPILE_ESP32S3.sh
./bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3:PSRAM=enabled,PartitionScheme=huge_app,FlashMode=qio ESP32CAM_Surveillance
```

## WSL2 USB Support

For Windows users with WSL2, use `usbipd` to attach USB devices:

```powershell
# Windows PowerShell (as Administrator)
usbipd list          # Find device BUSID
usbipd bind --busid 2-11
usbipd attach --wsl --busid 2-11
```

Then use the device normally in WSL2 terminal.

## Monitoring

View serial output:
```bash
# PlatformIO
pio device monitor --baud 115200

# Arduino CLI
./bin/arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## Configuration Files

Each project requires a `secrets.h` file with credentials:

**Temperature Sensor**:
```
temperature-sensor/include/secrets.h
```

**Solar Monitor**:
```
solar-monitor/include/secrets.h
```

**Surveillance Camera**:
```
surveillance-arduino/ESP32CAM_Surveillance/secrets.h
```

See `*.example` files for templates. Never commit real `secrets.h` files.

## Project-Specific Documentation

- **Temperature Sensor**: See [README.md](../README.md) for platform overview and [PLATFORM_GUIDE.md](../docs/reference/PLATFORM_GUIDE.md) for details
- **Solar Monitor**: See [solar-monitor/README.md](../solar-monitor/README.md)
- **Surveillance Camera**: See [surveillance/README.md](../surveillance/README.md)
- ✅ Valid InfluxDB URL and token
- ✅ WiFi networks configured