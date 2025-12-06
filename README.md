# ESP Sensor Hub

Multi-device IoT monitoring platform for ESP32/ESP8266/ESP32-S3 with InfluxDB data logging, MQTT integration, and optional OLED displays.

## Table of Contents
- [Overview](#overview)
- [Quick Start](#quick-start)
- [Surveillance Camera](#surveillance-camera)
- [Temperature Sensor Project](#temperature-sensor-project)
- [Solar Monitor Project](#solar-monitor-project)
- [Configuration](#configuration)
- [Architecture](#architecture)
- [API Reference](#api-reference)
- [Troubleshooting](#troubleshooting)

---

## Overview

### Projects

**Temperature Sensor** (Active - 4 Devices Deployed)
- **Hardware**: ESP8266/ESP32 + DS18B20 temperature sensor
- **Features**: Multi-location monitoring, WiFi connectivity, event logging, optional OLED display
- **PCB**: [USB-powered board](docs/pcb_design/usb-powered/) (v1.0 ordered)

**Solar Monitor** (Deployed)
- **Hardware**: ESP32 + Victron SmartShunt + 2x SmartSolar MPPT controllers
- **Features**: Real-time solar monitoring, battery state tracking, VE.Direct protocol, OLED display
- **Status**: Implemented and operational

### System Architecture

```
ESP Devices → Raspberry Pi (192.168.0.167)
              ├── InfluxDB (data storage)
              ├── Grafana (dashboards)
              ├── Home Assistant (automation)
              └── Cloudflare Tunnel (remote access)
```

Backend infrastructure: [raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker)

### Key Features

- **WiFiManager Portal**: Zero hardcoded credentials - configure via captive portal
- **Event Logging**: Device monitoring, boot tracking, error diagnostics
- **Self-Hosted**: Complete local data control with InfluxDB + Grafana
- **OLED Support**: Optional displays for both projects
- **Multi-Board**: Supports ESP8266 and ESP32 platforms

---

## Quick Start

### 1. Setup Credentials

```bash
# Create secrets file for temperature sensor
cp temperature-sensor/include/secrets.h.example temperature-sensor/include/secrets.h
# Edit with your InfluxDB URL, token, org, and bucket
```

### 2. Flash Device

```bash
# Temperature sensor
./scripts/flash_device.sh temp

# Solar monitor
./scripts/flash_device.sh solar
```

### 3. Configure WiFi

1. Device creates AP: "ESP-Setup" or "Temp-{Name}-Setup"
2. Connect to AP and open http://192.168.4.1
3. Enter WiFi credentials and device name
4. Device connects and starts logging data

**Reconfigure**: Double-reset device within 3 seconds to re-enter portal

### 4. Access Dashboard

- **Device Web UI**: http://device-ip
- **Surveillance UI**: See `surveillance/README.md` for `/stream` and presets
- **Grafana**: http://raspberry-pi:3000
- **InfluxDB**: http://raspberry-pi:8086

---

## Temperature Sensor Project

### Hardware Requirements

| Component | Specs | Notes |
|-----------|-------|-------|
| MCU | ESP8266 (NodeMCU) or ESP32 | 4 devices deployed |
| Sensor | DS18B20 | 1-Wire digital temperature |
| Display (optional) | SSD1306 OLED 128x64 | I2C interface |
| Power | USB 5V or custom PCB | PCB design available |

### Wiring

**DS18B20 Connection:**
```
ESP8266/ESP32 GPIO 4 → DS18B20 Data (with 4.7kΩ pullup to 3.3V)
3.3V → DS18B20 VDD
GND → DS18B20 GND
```

**OLED Display (Optional):**
```
ESP8266 (NodeMCU):        ESP32:
D1 (GPIO 5) → SCL         GPIO 22 → SCL
D2 (GPIO 4) → SDA         GPIO 21 → SDA
3.3V → VCC                3.3V → VCC
GND → GND                 GND → GND
```

### Features

- Temperature monitoring in °C and °F
- Web interface with live updates
- InfluxDB data logging every 15 seconds
- WiFi auto-reconnect
- Health check endpoint (`/health`)
- Event logging (boot, WiFi status, errors)
- OLED display shows temp + WiFi status

### Endpoints

| Endpoint | Description |
|----------|-------------|
| `/` | Web dashboard with live temperature |
| `/temperaturec` | Current temperature in Celsius |
| `/temperaturef` | Current temperature in Fahrenheit |
| `/health` | JSON health status with metrics |

### Display Content

```
┌────────────────┐
│  Main Cottage  │
│                │
│    23.5°C      │
│    74.3°F      │
│                │
│  WiFi: ◉       │
│  192.168.1.50  │
└────────────────┘
```

---

## Surveillance Camera

See `surveillance/README.md` for setup, `/stream` endpoint, UI presets (Smooth/Balanced/Detail), and MQTT commands (`restart`, `capture`, status publishing).

---

## Solar Monitor Project

### Hardware Configuration

| Component | Model | Connection |
|-----------|-------|------------|
| MCU | ESP32-WROOM-32 | 3x UART for VE.Direct |
| Battery Monitor | Victron SmartShunt SHU050150050 | UART2 (GPIO 16) |
| Charge Controller 1 | SmartSolar MPPT SCC110050210 | UART1 (GPIO 19) |
| Charge Controller 2 | SmartSolar MPPT SCC110050210 | SoftwareSerial (GPIO 18) |
| Display | SSD1306 OLED 128x64 | I2C (GPIO 21/22) |
| Power | 12V-to-5V converter | Micro USB |

### Wiring

**VE.Direct Connections:**
```
SmartShunt TX (yellow) → ESP32 GPIO 16 (UART2 RX)
MPPT1 TX (yellow)      → ESP32 GPIO 19 (UART1 RX)
MPPT2 TX (yellow)      → ESP32 GPIO 18 (SoftwareSerial RX)

All VE.Direct VCC (red) → ESP32 3.3V
All VE.Direct GND (black) → ESP32 GND
```

**VE.Direct Protocol:** 19200 baud, 8N1, 3.3V TTL (ESP32 compatible, no level shifter needed)

**OLED Display:**
```
GPIO 21 → SDA
GPIO 22 → SCL
3.3V → VCC
GND → GND
```

**Power:**
```
12V Battery → Inline Fuse (5A) → 12V-to-5V Converter → ESP32 VIN
Battery GND → Converter GND → ESP32 GND
```

### Monitored Data

**SmartShunt:**
- Battery voltage, current, power
- State of charge (SOC %)
- Time to go (TTG)
- Consumed Ah, charge cycles
- Historical min/max voltage

**MPPT Controllers:**
- PV voltage and power
- Charge current and state (Off/Bulk/Absorption/Float)
- Daily/total yield statistics
- Error monitoring

### Display Pages

OLED cycles through 5 pages:

```
Page 1: Battery          Page 2: MPPT1           Page 3: MPPT2
┌────────────────┐      ┌────────────────┐      ┌────────────────┐
│   BATTERY      │      │    MPPT 1      │      │    MPPT 2      │
│     85%        │      │    BULK        │      │    FLOAT       │
│  ████████░░    │      │  18.6V  145W   │      │  19.2V  156W   │
│ 12.8V  -2.3A   │      │  Yield: 2.3kWh │      │  Yield: 2.5kWh │
│ Solar:  301W   │      └────────────────┘      └────────────────┘
└────────────────┘

Page 4: Daily Stats      Page 5: System
┌────────────────┐      ┌────────────────┐
│  DAILY STATS   │      │     SYSTEM     │
│  Total: 4.9kWh │      │  Uptime: 2d4h  │
│  MPPT1: 2.3kWh │      │  WiFi: -45dBm  │
│  MPPT2: 2.6kWh │      │  192.168.1.51  │
│  Peak:  525W   │      │  Mem: 45% free │
└────────────────┘      └────────────────┘
```

### API Endpoints

| Endpoint | Description |
|----------|-------------|
| `/` | HTML dashboard |
| `/api/battery` | SmartShunt data (JSON) |
| `/api/solar` | Both MPPTs data (JSON) |
| `/api/system` | Combined system status (JSON) |

**Example `/api/battery` Response:**
```json
{
  "voltage": 13.25,
  "current": -2.34,
  "soc": 85.0,
  "time_remaining": 240,
  "consumed_ah": 15.2,
  "alarm": false,
  "relay": false,
  "valid": true
}
```

**Example `/api/solar` Response:**
```json
{
  "mppt1": {
    "pv_voltage": 18.65,
    "pv_power": 145,
    "charge_current": 4.5,
    "charge_state": "BULK",
    "yield_today": 2.34,
    "valid": true
  },
  "mppt2": {
    "pv_voltage": 19.23,
    "pv_power": 156,
    "charge_current": 5.1,
    "charge_state": "FLOAT",
    "yield_today": 2.56,
    "valid": true
  }
}
```

### VE.Direct Protocol Reference

**Charge States:**
- 0: Off
- 3: Bulk
- 4: Absorption
- 5: Float
- 6: Storage

**Common Error Codes:**
- 0: No error
- 2: Battery voltage too high
- 17: Charger temperature too high
- 33: Input voltage too high (solar panel)

---

## Configuration

### Required: InfluxDB Credentials

Create `include/secrets.h`:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

static const char* INFLUXDB_URL = "http://192.168.0.167:8086";
static const char* INFLUXDB_TOKEN = "your-influxdb-token-here";
static const char* INFLUXDB_ORG = "your-org";
static const char* INFLUXDB_BUCKET = "sensor_data";

#endif
```

### Optional: Enable/Disable OLED

**Temperature Sensor** - Edit `temperature-sensor/include/display.h`:
```cpp
#define OLED_ENABLED 1  // Set to 0 to disable
```

**Solar Monitor** - Edit `solar-monitor/src/display.h`:
```cpp
#define OLED_ENABLED 1  // Set to 0 to disable
```

**Important**: Disable OLED if hardware not connected to prevent boot crashes.

### WiFi Configuration

**No compile-time setup needed!** WiFiManager portal handles all WiFi configuration:

1. First boot → Device creates "ESP-Setup" AP
2. Connect to AP → Open http://192.168.4.1
3. Enter WiFi credentials + device name
4. Device saves config and auto-connects on reboot

**To reconfigure**: Double-reset device within 3 seconds

---

## Architecture

### Project Structure

```
├── src/                    # Temperature sensor firmware
├── surveillance/           # ESP32-S3 camera module (web + MQTT)
│   ├── src/               # Camera streaming + MQTT
│   └── README.md          # Usage, endpoints, presets
├── solar-monitor/          # Solar monitor firmware
│   └── src/               # Solar-specific code
├── include/                # Shared headers (secrets, config)
├── docs/                   # Documentation (reference + hardware)
│   └── reference/         # Platform + configuration guides
├── scripts/                # Flash and deployment scripts
├── platformio.ini          # Build configuration
└── README.md              # This file
```

### Data Flow

```
┌─────────────────────────────────────────────────────────┐
│                    ESP Devices                          │
│  • Read sensors (DS18B20 / VE.Direct)                  │
│  • Serve web interface                                  │
│  • Update OLED display                                  │
│  • WiFiManager configuration portal                     │
└──────────────────────┬──────────────────────────────────┘
                       │
                       │ InfluxDB Line Protocol
                       │ HTTP POST /api/v2/write
                       ↓
    ┌──────────────────────────────────────────────────┐
    │         Raspberry Pi 4 (192.168.0.167)          │
    │                                                  │
    │  ┌──────────────┐         ┌──────────────┐     │
    │  │  InfluxDB    │  ←───   │   Grafana    │     │
    │  │  Port 8086   │         │   Port 3000  │     │
    │  │              │         │              │     │
    │  │ Buckets:     │  Flux   │ Dashboards:  │     │
    │  │ • sensor_data│ Queries │ • Temps      │     │
    │  │ • events     │         │ • Solar      │     │
    │  └──────────────┘         └──────────────┘     │
    └──────────────────────────────────────────────────┘
```

### InfluxDB Data Model

**Measurements:**
- `temperature` - Temperature sensor readings
- `device_events` - Boot, WiFi, configuration events
- `solar_battery` - SmartShunt battery data
- `solar_mppt` - MPPT charge controller data

**Common Tags:**
- `device` - Device name (set via WiFiManager)
- `board` - ESP8266 or ESP32
- `location` - Physical location

**Fields:**
- Temperature: `tempC`, `tempF`
- Solar: `voltage`, `current`, `power`, `soc`, `pv_power`, `yield_today`
- Events: `message`, `severity`

---

## API Reference

### Temperature Sensor Endpoints

**`GET /`** - Web Dashboard
- HTML page with live temperature updates
- Auto-refreshes every 15 seconds

**`GET /temperaturec`** - Temperature in Celsius
- Returns: Plain text (e.g., "23.45")

**`GET /temperaturef`** - Temperature in Fahrenheit
- Returns: Plain text (e.g., "74.21")

**`GET /health`** - Health Status
```json
{
  "status": "ok",
  "device": "Main Cottage",
  "board": "ESP8266",
  "uptime_seconds": 86400,
  "wifi_connected": true,
  "wifi_rssi": -45,
  "temperature_valid": true,
  "current_temp_c": "23.45",
  "current_temp_f": "74.21",
  "metrics": {
    "wifi_reconnects": 0,
    "sensor_read_failures": 1,
    "influx_send_failures": 0,
    "min_temp_c": 18.5,
    "max_temp_c": 25.3
  },
  "last_success": {
    "influx_seconds_ago": 15
  }
}
```

### Solar Monitor Endpoints

**`GET /`** - Web Dashboard
- HTML interface with battery, solar, and system status

**`GET /api/battery`** - Battery Status
```json
{
  "voltage": 13.25,
  "current": -2.34,
  "soc": 85.0,
  "time_remaining": 240,
  "consumed_ah": 15.2,
  "alarm": false,
  "relay": false,
  "last_update": 1234567890,
  "valid": true
}
```

**`GET /api/solar`** - Solar Status
```json
{
  "mppt1": {
    "product_id": "0xA060",
    "serial_number": "HQ2145ABCDE",
    "pv_voltage": 18.65,
    "pv_power": 145,
    "charge_current": 4.5,
    "charge_state": "BULK",
    "error": 0,
    "yield_today": 2.34,
    "yield_yesterday": 3.12,
    "max_power_today": 250,
    "valid": true
  },
  "mppt2": { ... }
}
```

**`GET /api/system`** - Combined System Status
```json
{
  "battery": {
    "voltage": 13.25,
    "current": -2.34,
    "soc": 85.0
  },
  "solar": {
    "total_pv_power": 301,
    "total_charge_current": 9.6,
    "yield_today_total": 4.90
  },
  "system": {
    "uptime": 86400,
    "wifi_rssi": -45,
    "free_heap": 120000
  }
}
```

---

## Troubleshooting

### WiFi Connection Issues

**Symptoms:** Device not connecting to WiFi, frequent disconnections

**Solutions:**
1. Check serial output for WiFi status messages
2. Double-reset to enter WiFiManager portal
3. Verify SSID and password are correct
4. Ensure 2.4GHz WiFi is available (ESP doesn't support 5GHz)
5. Check WiFi signal strength (RSSI in `/health` endpoint)
6. Move device closer to router during setup

### No Data in InfluxDB

**Symptoms:** Device shows WiFi connected but no data appears in Grafana

**Solutions:**
1. Verify `secrets.h` has correct InfluxDB URL, token, org, bucket
2. Check serial output for InfluxDB POST errors
3. Verify InfluxDB is running: `curl http://raspberry-pi:8086/ping`
4. Check InfluxDB token permissions (needs write access to bucket)
5. Test manually: Use `/health` endpoint to confirm device is operational
6. Check device event logs in InfluxDB for error messages

### Sensor Read Failures

**Temperature Sensor:**
- Verify DS18B20 wiring (data, VCC, GND)
- Check 4.7kΩ pullup resistor between data and VCC
- Try different GPIO pin
- Test sensor with multimeter (should show ~3.3V on VCC)

**Solar Monitor:**
- Verify VE.Direct cable connections (TX pin to ESP32 RX)
- Ensure VE.Direct is enabled on Victron devices (via VictronConnect app)
- Check common ground between ESP32 and Victron devices
- Monitor serial output for raw VE.Direct data stream
- Verify baud rate is 19200 for all VE.Direct connections

### OLED Display Not Working

**Symptoms:** OLED blank or shows garbage, or device crashes on boot

**Solutions:**
1. **If OLED not connected**: Set `OLED_ENABLED 0` in display header
2. Verify I2C wiring:
   - ESP8266: D1→SCL, D2→SDA
   - ESP32: GPIO22→SCL, GPIO21→SDA
3. Check OLED I2C address (usually 0x3C)
4. Verify 3.3V power and GND connections
5. Test with I2C scanner sketch
6. Check serial output for I2C initialization errors

### Compilation Errors

**"secrets.h not found":**
```bash
cp temperature-sensor/include/secrets.h.example temperature-sensor/include/secrets.h
# Edit temperature-sensor/include/secrets.h with your credentials
```

**"OLED_ENABLED not defined":**
- Ensure display.h exists in include/ or solar-monitor/src/
- Rebuild project: `platformio run --target clean`

**Library dependencies missing:**
```bash
platformio lib install
```

### WSL2 USB Access (Windows Users)

If using WSL2 for development, USB devices need to be attached from Windows:

```powershell
# Run in Windows PowerShell as Administrator
usbipd list                    # Find device BUSID
usbipd bind --busid 2-11       # One-time setup
usbipd attach --wsl --busid 2-11  # Attach for flashing
```

Then in WSL2:
```bash
ls /dev/ttyUSB*  # Should see device
./scripts/flash_device.sh temp
```

### Device Metrics and Monitoring

Check device health via `/health` endpoint:
- `wifi_reconnects` - Should be 0 (frequent reconnects indicate WiFi issues)
- `sensor_read_failures` - Occasional failures OK, frequent = wiring issue
- `influx_send_failures` - Should be 0 (non-zero = InfluxDB connectivity issue)
- `uptime_seconds` - Track device stability
- `free_heap` - Low memory may cause crashes

**Event Logging:**
All devices log events to InfluxDB `device_events` measurement:
- Boot events (including reset reason)
- WiFi connection/disconnection
- Configuration changes
- Sensor errors
- InfluxDB connectivity issues

Query events in Grafana or InfluxDB for diagnostics.

---

## Building & Deployment

### Flash Single Device

```bash
# Temperature sensor (auto-detects ESP8266/ESP32)
./scripts/flash_device.sh temp

# Solar monitor (ESP32 only)
./scripts/flash_device.sh solar

# Monitor serial output
platformio device monitor -b 115200
```

### Flash Multiple Devices

```bash
# Batch flash temperature sensors
python3 scripts/flash_multiple.py --project temp

# Batch flash solar monitors
python3 scripts/flash_multiple.py --project solar
```

### Validation

```bash
# Validate secrets.h before building
./scripts/validate_secrets.sh
```

### Supported Boards

- **ESP32** (`esp32dev` environment)
- **ESP8266** (`esp8266` / `nodemcuv2` environment)

PlatformIO auto-selects environment based on connected device.

---

## Hardware & PCB

### PCB Design

Custom USB-powered temperature sensor board (v1.0):
- Integrated NodeMCU ESP8266
- DS18B20 sensor with pullup
- USB-C power input
- Compact form factor

**Location**: `docs/pcb_design/usb-powered/`

**Status**: PCB ordered, awaiting delivery

### Bill of Materials

**Temperature Sensor (4x deployed):**
- NodeMCU ESP8266 or ESP32 dev board
- DS18B20 temperature sensor
- 4.7kΩ resistor (pullup)
- Optional: SSD1306 OLED display
- Power: USB cable or custom PCB

**Solar Monitor (1x deployed):**
- ESP32-WROOM-32 dev board
- 3x VE.Direct cables (Victron)
- SSD1306 OLED display
- 12V-to-5V converter (3A, waterproof)
- Inline fuse (5A)
- Enclosure (weatherproof)

---

## Related Projects

- **[raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker)** - Backend infrastructure (InfluxDB, Grafana, Home Assistant)

---

## License & Credits

**Temperature Sensor** - Based on [RandomNerdTutorials](https://randomnerdtutorials.com/) ESP8266/ESP32 projects

**Libraries Used:**
- WiFiManager - tzapu/WiFiManager
- U8g2 - olikraus/U8g2 (OLED display)
- DallasTemperature - milesburton/DallasTemperature
- ArduinoJson - bblanchon/ArduinoJson
- PubSubClient - knolleary/PubSubClient
- ESP_DoubleResetDetector - khoih-prog

**VE.Direct Protocol** - Victron Energy

---

**Last Updated**: December 2, 2025
**Status**: Both projects operational
**Branch**: `feature/oled-display`
**Backend**: Raspberry Pi 4 @ 192.168.0.167
