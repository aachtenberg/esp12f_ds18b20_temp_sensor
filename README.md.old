# ESP Sensor Hub

Multi-device IoT monitoring platform for ESP32/ESP8266/ESP32-S3 with MQTT data publishing, optional InfluxDB/Grafana integration, and WiFiManager configuration portal.

## Table of Contents
- [Overview](#overview)
- [Quick Start](#quick-start)
- [Surveillance Camera](#surveillance-camera)
- [Temperature Sensor Project](#temperature-sensor-project)
- [Solar Monitor Project](#solar-monitor-project)
- [Configuration](#configuration)
- [Secrets Hygiene](#secrets-hygiene)
- [Architecture](#architecture)
- [API Reference](#api-reference)
- [Troubleshooting](#troubleshooting)

---

## Overview

### Projects

**Temperature Sensor** (Active - 3+ Devices Deployed)
- **Hardware**: ESP8266/ESP32 + DS18B20 temperature sensor
- **Transport**: MQTT JSON publishing to central broker
- **Features**: WiFiManager portal config, event logging, optional OLED display
- **Status**: ✅ Production - Multiple locations monitoring

**Surveillance Camera** (Active)
- **Hardware**: ESP32-S3 + OV2640 camera + optional SD card
- **Transport**: MQTT JSON publishing, web stream endpoint
- **Features**: WiFiManager config, camera presets, SD card recording
- **Status**: ✅ Production - Indoor monitoring

**Solar Monitor** (Deployed)
- **Hardware**: ESP32 + Victron SmartShunt + 2x SmartSolar MPPT controllers
- **Transport**: MQTT JSON publishing
- **Features**: Real-time solar monitoring, battery state tracking, VE.Direct protocol
- **Status**: ✅ Production - Solar system monitoring

### System Architecture

```
Devices (Temperature/Solar/Camera)
    ↓
MQTT Broker (Mosquitto) on your network (default port 1883)
    ↓
┌───────────────────────────────────────┐
│      Raspberry Pi Infrastructure       │
├───────────────────────────────────────┤
│  Optional: Telegraf (MQTT → InfluxDB)  │
│  Optional: InfluxDB 3.x (time-series)  │
│  Optional: Grafana (visualization)     │
└───────────────────────────────────────┘
```

**Data Flow**: Devices → MQTT Broker → (Optional) InfluxDB → (Optional) Grafana

### Key Features

- **MQTT Transport**: Standardized JSON payloads, hierarchical topics, retained status messages
- **WiFiManager Portal**: Zero hardcoded credentials - configure via captive portal
- **Event Logging**: Device monitoring, boot tracking, error diagnostics  
- **Flexible Backends**: Works standalone with MQTT, or integrate InfluxDB/Grafana
- **Optional Displays**: OLED support for temperature and solar projects
- **Multi-Platform**: Supports ESP8266, ESP32, and ESP32-S3

---

## Quick Start

### 1. Setup MQTT Broker Configuration
Create `temperature-sensor/include/secrets.h`:
```bash
cp temperature-sensor/include/secrets.h.example temperature-sensor/include/secrets.h
# Edit with your MQTT broker host: MQTT_BROKER = "your.mqtt.broker.com"
```

### 2. Flash Device
```bash
# Temperature sensor (ESP8266/ESP32)
cd temperature-sensor && pio run -e esp8266 -t upload --upload-port /dev/ttyUSB0

# Or ESP32:
pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0
```

### 3. Configure via WiFiManager Portal
1. Device creates AP "ESP-Setup" (no password)
2. Connect to AP and open the WiFiManager captive portal page (shown after connecting)
3. Select WiFi network and enter credentials + device name
4. Device connects and starts publishing MQTT data

### 4. Monitor Data
```bash
# View temperature stream
mosquitto_sub -h your.mqtt.broker.com -t "esp-sensor-hub/+/temperature" -v

# View retained status
mosquitto_sub -h your.mqtt.broker.com -t "esp-sensor-hub/+/status" -R -v
```

---

## Secrets Hygiene

- Secrets files are gitignored: `**/secrets.h`, `.env*`, `*.key`, `*.pem`, `*.crt`.
- Never commit real credentials; use the `*.example` templates and keep your local copies untracked.
- **Automated scanning**: GitHub Actions runs [gitleaks](https://github.com/gitleaks/gitleaks) on every push and PR to detect secrets before they reach the public repo (see [`.github/workflows/secrets-check.yml`](.github/workflows/secrets-check.yml)).
- Optional pre-push guard: install the secret scanner hook so pushes fail if secret-like files or private keys slip in:
  ```bash
  cd $(git rev-parse --show-toplevel)
  ln -sf scripts/pre-push-secrets.sh .git/hooks/pre-push
  chmod +x scripts/pre-push-secrets.sh
  ```
  The local hook checks for tracked `secrets.h`, `.env`, key files, and PEM blobs (private keys).

### Hardware Requirements

| Component | Specs | Notes |
|-----------|-------|-------|
| MCU | ESP8266 (NodeMCU) or ESP32 | 4 devices deployed |
| Sensor | DS18B20 | 1-Wire digital temperature |
| Display (optional) | SSD1306 OLED 128x64 | I2C interface |
| Power | USB 5V or custom PCB | PCB design available |

│  MPPT2: 2.6kWh │      │  device-ip     │
### Wiring

**DS18B20 Connection:**
```
ESP8266/ESP32 GPIO 4 → DS18B20 Data (with 4.7kΩ pullup to 3.3V)
3.3V → DS18B20 VDD
GND → DS18B20 GND
```

**OLED Display (Optional):**
```
static const char* INFLUXDB_URL = "http://your-pi:8086";
D1 (GPIO 5) → SCL         GPIO 22 → SCL
D2 (GPIO 4) → SDA         GPIO 21 → SDA
3.3V → VCC                3.3V → VCC
GND → GND                 GND → GND
```

### Features

- Temperature monitoring in °C and °F via MQTT
- MQTT JSON publishing every 30 seconds
- Retained status messages with WiFi/heap info
- Event logging (boot, WiFi, sensor errors)
- Optional OLED display for local status
- WiFi auto-reconnect with failure tracking
- Serial logging for debugging

### MQTT Topics

| Topic | Payload | Retained |
|-------|---------|----------|
| `esp-sensor-hub/{device}/temperature` | `{device, chip_id, timestamp, celsius, fahrenheit}` | No |
| `esp-sensor-hub/{device}/status` | `{device, uptime, wifi_connected, rssi, free_heap, ...}` | **Yes** |
| `esp-sensor-hub/{device}/events` | `{event, severity, message, ...}` | No |

### OLED Display Content (When Enabled)

```
┌────────────────┐
│  Small Garage  │
│                │
│    21.4°C      │
│    70.5°F      │
│                │
│  WiFi: -63dBm  │
│  Heap: 240KB   │
└────────────────┘
```

---

## Surveillance Camera Project

The ESP32-S3 camera firmware publishes motion detection and status events to MQTT. See [surveillance/README.md](surveillance/README.md) for setup, stream endpoints, and capture presets.

---

## Solar Monitor Project

The ESP32 solar monitor publishes real-time solar metrics (voltage, current, power, battery state) from Victron equipment via MQTT. See [solar-monitor/README.md](solar-monitor/README.md) for hardware configuration and wiring instructions.

---

## Documentation

- **[PLATFORM_GUIDE.md](docs/reference/PLATFORM_GUIDE.md)** - Architecture overview, features, and platform design
- **[CONFIG.md](docs/reference/CONFIG.md)** - Configuration, setup, and troubleshooting
- **[OLED_DISPLAY_GUIDE.md](docs/hardware/OLED_DISPLAY_GUIDE.md)** - Optional display integration

---

## Key Technologies

| Component | Library | Version | Purpose |
|-----------|---------|---------|---------|
| WiFi Config | WiFiManager | 2.0.17 | Captive portal for credentials |
| MQTT | PubSubClient | 2.8.0 | MQTT client for JSON publishing |
| JSON | ArduinoJson | 7.4.2 | Payload serialization |
| Temperature | DallasTemperature | 4.0.5 | DS18B20 sensor reading |
| OneWire | OneWire | 2.3.8 | 1-Wire protocol |
| Display | U8g2 | 2.36.15 | OLED driver (optional) |
| Reset | ESP_DoubleResetDetector | 1.3.2 | Factory reset on double-reset (ESP8266) |
| Reset (S3) | Preferences (NVS) | Built-in | Triple-reset detection for ESP32-S3 |

---

## Project Status

| Project | Status | Devices | Data Transport |
|---------|--------|---------|-----------------|
| Temperature Sensor | ✅ Production | 3+ deployed | MQTT JSON |
| Surveillance Camera | ✅ Production | 1 deployed | MQTT JSON + Web stream |
| Solar Monitor | ✅ Production | 1 deployed | MQTT JSON |
| Backend | ✅ Running | Raspberry Pi 4 | InfluxDB v3 (optional bridge) |

---

**Last Updated**: December 15, 2025  
**Current Branch**: main  
**Architecture**: MQTT-based data streaming with optional InfluxDB v3 integration
