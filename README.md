# ESP Sensor Hub

Multi-device IoT monitoring platform for ESP32/ESP8266/ESP32-S3 with MQTT data publishing, optional InfluxDB/Grafana integration, and WiFiManager configuration portal.

## Quick Navigation

**📖 Documentation**
- **[PLATFORM_GUIDE.md](docs/reference/PLATFORM_GUIDE.md)** - Architecture, features, and platform overview
- **[CONFIG.md](docs/reference/CONFIG.md)** - Configuration, deployment, and troubleshooting
- **[DEVICE_INVENTORY.md](temperature-sensor/docs/DEVICE_INVENTORY.md)** - Device tracking and versions

**🚀 Projects**
- **[temperature-sensor/](temperature-sensor/)** - DS18B20 temperature monitoring (8 devices deployed)
- **[bme280-sensor/](bme280-sensor/)** - BME280 environmental sensor (temperature, humidity, pressure)
- **[surveillance/](surveillance/)** - ESP32-S3 camera monitoring (3 devices deployed)
- **[solar-monitor/](solar-monitor/)** - Victron solar system monitoring (1 device deployed)

**🛠️ Hardware**
- **[Battery Setup Guide](docs/hardware/BATTERY_SETUP_GUIDE.md)** - TP4056 charger + battery wiring
- **[OLED Display Guide](docs/hardware/OLED_DISPLAY_GUIDE.md)** - Optional display integration
- **[PCB Design](docs/pcb_design/)** - USB-powered temperature sensor board

---

## Projects Overview

### Temperature Sensor (Active - 8 Devices)
- **Hardware**: ESP8266/ESP32 + DS18B20 waterproof sensor
- **Features**: MQTT, deep sleep, WiFiManager, optional OLED, OTA updates
- **Status**: ✅ Production - 8 locations (v1.0.8-v1.1.0)
- **Docs**: [temperature-sensor/README.md](temperature-sensor/README.md)

### BME280 Sensor (Development)
- **Hardware**: ESP32/ESP32-S3 + BME280 I2C sensor
- **Features**: Temperature, humidity, pressure, altitude calculation, MQTT
- **Status**: 🔧 Testing - Ready for deployment
- **Docs**: [bme280-sensor/README.md](bme280-sensor/README.md)

### Surveillance Camera (Active - 3 Devices)
- **Hardware**: ESP32-S3 + OV2640 camera + optional SD card
- **Features**: MQTT events, web stream, camera presets, motion detection
- **Status**: ✅ Production - Indoor/outdoor monitoring
- **Docs**: [surveillance/README.md](surveillance/README.md)

### Solar Monitor (Active - 1 Device)
- **Hardware**: ESP32 + Victron SmartShunt + 2× MPPT controllers
- **Features**: Real-time monitoring, battery SOC, VE.Direct protocol, MQTT
- **Status**: ✅ Production - Solar system tracking
- **Docs**: [solar-monitor/README.md](solar-monitor/README.md)

---

## Quick Start

### 1. Clone and Setup Secrets
```bash
git clone https://github.com/aachtenberg/esp-sensor-hub.git
cd esp-sensor-hub

# Setup secrets for your project (temperature-sensor example)
cp temperature-sensor/include/secrets.h.example temperature-sensor/include/secrets.h
nano temperature-sensor/include/secrets.h  # Edit MQTT_SERVER, OTA_PASSWORD
```

### 2. Build and Flash Device
```bash
cd temperature-sensor

# Update firmware version before building
./update_version.sh --patch

# Initial USB flash (required once)
pio run -e esp8266-serial -t upload --upload-port /dev/ttyUSB0

# Monitor serial output
pio device monitor
```

### 3. Configure WiFi
1. Device creates access point on first boot
2. Connect to AP (SSID shown in serial output)
3. Open browser to `http://192.168.4.1`
4. Enter WiFi credentials and device name
5. Device connects and starts publishing to MQTT

### 4. Subsequent OTA Updates
```bash
export PLATFORMIO_UPLOAD_FLAGS="--auth=YOUR_OTA_PASSWORD"
pio run -e esp8266 -t upload --upload-port 192.168.0.X
```

**Detailed instructions**: See project-specific README files and [CONFIG.md](docs/reference/CONFIG.md)

---

## System Architecture

```
Devices (Temperature/BME280/Solar/Camera)
    ↓
MQTT Broker (Mosquitto) - Port 1883
    ↓
┌───────────────────────────────────────┐
│      Optional Infrastructure           │
├───────────────────────────────────────┤
│  • Telegraf (MQTT → InfluxDB bridge)   │
│  • InfluxDB 3.x (time-series storage)  │
│  • Grafana (visualization/alerts)      │
└───────────────────────────────────────┘
```

**Key Features**:
- **MQTT-First**: All devices publish JSON to MQTT broker
- **WiFiManager**: Zero hardcoded credentials - captive portal configuration
- **Event Logging**: Boot tracking, errors, configuration changes
- **Multi-Platform**: ESP8266, ESP32, ESP32-S3 support
- **Optional Backends**: Works standalone or with InfluxDB/Grafana

---

## Hardware Requirements

### Temperature Sensor
| Component | Specs | Notes |
|-----------|-------|-------|
| MCU | ESP8266 (NodeMCU) or ESP32 | 8 devices deployed |
| Sensor | DS18B20 | 1-Wire digital temperature |
| Display (optional) | SSD1306 OLED 128x64 | I2C interface |
| Power | USB 5V or custom PCB | PCB design available |

### BME280 Sensor
| Component | Specs | Notes |
|-----------|-------|-------|
| MCU | ESP32 or ESP32-S3 | I2C communication |
| Sensor | BME280 | Temperature, humidity, pressure |
| Display (optional) | SSD1306 OLED 128x64 | I2C interface |
| Power | USB 5V or battery | Battery monitoring supported |

**Battery Setup**: See [Battery Setup Guide](docs/hardware/BATTERY_SETUP_GUIDE.md) for TP4056 charger wiring

---

## Secrets Hygiene

- Secrets files gitignored: `**/secrets.h`, `.env*`, `*.key`, `*.pem`
- Never commit real credentials - use `*.example` templates
- **Automated scanning**: GitHub Actions runs gitleaks on every push
- **Pre-push hook**: Optional local secret scanner
  ```bash
  ln -sf scripts/pre-push-secrets.sh .git/hooks/pre-push
  chmod +x scripts/pre-push-secrets.sh
  ```

---

## MQTT Topics

All devices use hierarchical topic structure:

**Temperature Sensor**: `esp-sensor-hub/{device-name}/`
- `/temperature` - Temperature readings (°C, °F)
- `/status` - Device status (retained)
- `/events` - Boot, error, configuration events
- `/command` - Remote commands (deepsleep, status, restart)

**BME280 Sensor**: `esp-sensor-hub/{device-name}/`
- `/readings` - Temperature, humidity, pressure, altitude
- `/status` - Device status (retained)
- `/events` - Device events
- `/command` - Remote commands

**Solar Monitor**: `esp-solar-hub/{device-name}/`
- `/status` - Solar metrics (voltage, current, power, SOC)
- `/events` - Device events

**Surveillance Camera**: `esp-camera/{device-name}/`
- `/status` - Camera status and metrics
- `/events` - Motion detection, configuration changes

---

## Documentation

- **[PLATFORM_GUIDE.md](docs/reference/PLATFORM_GUIDE.md)** - Complete architecture and features
- **[CONFIG.md](docs/reference/CONFIG.md)** - Setup, deployment, troubleshooting
- **[DEVICE_INVENTORY.md](temperature-sensor/docs/DEVICE_INVENTORY.md)** - Deployed devices and versions
- **Project READMEs**: See individual project directories for detailed documentation

---

## Project Status

| Project | Status | Devices | Version | Last Update |
|---------|--------|---------|---------|-------------|
| Temperature Sensor | ✅ Production | 8 | v1.0.8-v1.1.0 | Dec 24, 2025 |
| BME280 Sensor | 🔧 Testing | 0 | v1.0.0 | Jan 3, 2026 |
| Surveillance | ✅ Production | 3 | Active | Dec 2025 |
| Solar Monitor | ✅ Production | 1 | Active | Dec 2025 |

---

**Repository**: [github.com/aachtenberg/esp-sensor-hub](https://github.com/aachtenberg/esp-sensor-hub)  
**License**: See LICENSE file  
**Last Updated**: January 3, 2026
