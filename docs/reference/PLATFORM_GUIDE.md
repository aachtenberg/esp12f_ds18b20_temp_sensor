# ESP Multi-Device IoT Monitoring Platform

## Overview

Self-hosted IoT platform supporting temperature sensors (ESP8266/ESP32), solar monitoring (ESP32), and surveillance cameras (ESP32-S3). Devices publish data via MQTT to a central broker, with optional downstream integration to InfluxDB v3 and Grafana visualization. All devices configured via WiFiManager portal (zero hardcoded credentials).

## Quick Start

### 1. Flash Device
```bash
# Temperature sensor (ESP8266/ESP32)
./scripts/flash_device.sh temp

# Solar monitor (ESP32 only)  
./scripts/flash_device.sh solar

# Surveillance camera (ESP32-S3)
./scripts/flash_device.sh surveillance
```

### 2. Configure via WiFiManager
1. Device creates AP "ESP-Setup" (password: "configure")
2. Connect and open the WiFiManager captive portal (e.g., http://esp-setup.local)
3. Enter WiFi credentials and device name
4. Device connects and starts sending data

### 3. Monitor Data
- **Grafana Dashboards**: http://your-pi:3000
- **InfluxDB Interface**: http://your-pi:8086
- **Device Web UI**: http://device-ip

## Architecture

```
┌────────────────────────┐    ┌────────────────────────┐    ┌────────────────────────┐
│   Temperature Sensor   │    │      Solar Monitor      │    │  Surveillance Camera   │
│   ESP8266/ESP32        │    │        ESP32            │    │      ESP32-S3          │
│  - DS18B20 sensor      │    │  - SmartShunt (UART2)  │    │  - OV2640 camera       │
│  - MQTT publisher      │    │  - MPPT1 (UART1)       │    │  - MQTT publisher      │
│  - WiFiManager portal  │    │  - MPPT2 (SoftSerial)  │    │  - WiFi/SD card        │
└────────────┬───────────┘    └────────────┬───────────┘    └────────────┬───────────┘
            │                              │                              │
            │ MQTT JSON                    │ MQTT JSON                    │ MQTT JSON
            │ esp-sensor-hub/*/temperature │ esp-solar-hub/*/status       │ esp-camera/*/status
            └─────────────┬────────────────┼──────────────────────────────┘
                         │                │
                         ↓                ↓
    ┌──────────────────────────────────────────────────────────────┐
    │              MQTT Broker (Mosquitto)                         │
    │              your.mqtt.broker.com:1883                      │
    │         Topic subscriptions: esp-*/#                         │
    └──────────┬──────────────────────────┬──────────┬─────────────┘
              │                          │          │
              ↓                          ↓          ↓
    ┌──────────────────────┐   ┌──────────────────────┐   ┌──────────────────────┐
    │   Raspberry Pi 4     │   │  Telegraf Bridge     │   │   Grafana 12.x       │
    │  Docker Infrastructure                        │   │   Port: 3000         │
    │                      │   │ (MQTT → InfluxDB)   │   │   - Dashboards       │
    │  ┌────────────────┐  │   └──────────────┬──────┘   │   - Alerts           │
    │  │  InfluxDB 3.x  │  │                 │          │   - Event logs       │
    │  │  Port: 8086    │  │                 ↓          └──────────────────────┘
    │  │  - sensor_data │  │       ┌──────────────────┐
    │  │  - device_events│  │       │   InfluxDB 3.x   │
    │  │  - Time-series  │  │       │   (optional)     │
    │  └────────────────┘  │       └──────────────────┘
    └──────────────────────┘
```

**Data Flow**:
1. Devices connect to WiFi and MQTT broker
2. Publish hierarchical JSON payloads every 30 seconds
3. MQTT broker persists retained status messages
4. Optional Telegraf bridge maps MQTT → InfluxDB v3
5. Grafana queries InfluxDB or MQTT directly for dashboards

## Configuration

### MQTT Broker Setup
Devices publish to a central MQTT broker:
```
MQTT_BROKER: your.mqtt.broker.com (e.g., mosquitto.local or LAN hostname)
MQTT_PORT: 1883
MQTT_USER: (optional, empty by default)
MQTT_PASSWORD: (optional, empty by default)
```

### Temperature Sensor Topic Structure
```
esp-sensor-hub/{device_name}/temperature
└─ JSON payload: {"device": "...", "chip_id": "...", "timestamp": ..., "celsius": ..., "fahrenheit": ...}

esp-sensor-hub/{device_name}/status (retained)
└─ JSON payload: {"device": "...", "uptime_seconds": ..., "wifi_connected": true, "wifi_rssi": ..., "free_heap": ...}

esp-sensor-hub/{device_name}/events
└─ JSON payload: {"event": "boot|wifi_disconnect|sensor_error", "severity": "info|warning|error", ...}
```

### Secrets Setup
Create `temperature-sensor/include/secrets.h` (excluded from git):
```cpp
static const char* MQTT_BROKER = "your.mqtt.broker.com";  // e.g., "mosquitto.local" or LAN hostname
static const int MQTT_PORT = 1883;
static const char* MQTT_USER = "";        // Leave empty if no auth
static const char* MQTT_PASSWORD = "";    // Leave empty if no auth
```

## Project Support

### Temperature Sensors
| Hardware | Sensors | Data Points |
|----------|---------|-------------|
| ESP8266/ESP32 | DS18B20 | temperature_c, wifi_rssi, heap_free |

### Solar Monitor  
| Hardware | Sensors | Data Points |
|----------|---------|-------------|
| ESP32 | SmartShunt, MPPT | voltage, current, power, battery_soc |

## Deployment Scripts

### Single Device
```bash
# Flash with project type
./scripts/flash_device.sh temp
./scripts/flash_device.sh solar

# Monitor serial output
platformio device monitor -b 115200
```

### Multiple Devices
```bash
# Bulk deployment
python3 scripts/flash_multiple.py --project temp
python3 scripts/flash_multiple.py --project solar
```

## Troubleshooting

### Device Issues
```bash
# Serial monitoring
platformio device monitor -b 115200

# WiFiManager portal access
# Connect to "ESP-Setup" AP → WiFiManager captive portal (e.g., esp-setup.local)
```

### Data Verification
```bash
# Recent device events
curl -H "Authorization: Token YOUR_TOKEN" \
  "http://your-pi:8086/api/v2/query?org=YOUR_ORG" \
  --data-urlencode 'query=from(bucket:"sensor_data")|>range(start:-1h)|>filter(fn:(r)=>r._measurement=="device_events")'
```

### Infrastructure Status
```bash
# Docker stack health
sudo docker ps
sudo docker logs influxdb
sudo docker logs grafana
```

## Benefits

✅ **Self-Hosted**: Zero recurring costs, complete data ownership  
✅ **Portal Configuration**: No hardcoded WiFi credentials  
✅ **Event Logging**: Comprehensive device lifecycle tracking  
✅ **Multi-Device**: Single infrastructure supports all device types  
✅ **Grafana Integration**: Professional dashboards and alerting  
✅ **Auto-Discovery**: Devices self-register via InfluxDB tags

## Key Features

### WiFiManager Portal Configuration
- Eliminates hardcoded WiFi credentials
- Captive portal available via WiFiManager (device-hostname.local)
- Device name configuration via web interface
- Automatic WiFi reconnection and recovery

### Comprehensive Event Logging
- Device boot and connectivity events
- Error tracking and debugging information  
- Configuration changes and system status
- Stored in InfluxDB `device_events` measurement

### Multi-Project Architecture
- Single repository supports multiple device types
- Automatic environment detection (ESP8266/ESP32)
- Project-specific build configurations
- Shared infrastructure and deployment tools

### Self-Hosted Infrastructure
- InfluxDB 2.x time-series database
- Grafana 12.x visualization and alerting
- Docker containerized deployment
- Complete local data control

---

**Generated**: November 24, 2025  
**Architecture**: InfluxDB + Grafana + WiFiManager Portal  
**Projects**: Temperature Sensors + Solar Monitor