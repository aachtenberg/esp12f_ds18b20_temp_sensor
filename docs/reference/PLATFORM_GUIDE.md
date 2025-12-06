# ESP Multi-Device IoT Monitoring Platform

## Overview

Self-hosted IoT platform supporting temperature sensors (ESP8266/ESP32), solar monitoring (ESP32), and surveillance cameras (ESP32-S3) with WiFiManager portal configuration, InfluxDB time-series storage, MQTT integration, and Grafana visualization.

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
2. Connect and navigate to http://192.168.4.1
3. Enter WiFi credentials and device name
4. Device connects and starts sending data

### 3. Monitor Data
- **Grafana Dashboards**: http://your-pi:3000
- **InfluxDB Interface**: http://your-pi:8086
- **Device Web UI**: http://device-ip

## Architecture

```
┌────────────────────────┐    ┌────────────────────────┐
│   Temperature Sensor   │    │      Solar Monitor      │
│   ESP8266/ESP32        │    │        ESP32            │
│  - DS18B20 sensor     │    │  - SmartShunt (UART2)  │
│  - WiFiManager portal │    │  - MPPT1 (UART1)      │
│  - Event logging      │    │  - MPPT2 (SoftSerial)  │
└────────────┬───────────┘    └────────────┬───────────┘
            │                              │
            │ InfluxDB Line Protocol           │
            │ POST /api/v2/write               │
            └─────────────┬────────────────────┘
                         │
                         ↓
    ┌────────────────────────────────────────────────┐
    │              Raspberry Pi 4                   │
    │         Docker Infrastructure                 │
    │                                              │
    │  ┌────────────────┐   ┌────────────────┐  │
    │  │   InfluxDB 2.x     │   │   Grafana 12.x   │  │
    │  │  Port: 8086       │   │  Port: 3000     │  │
    │  │  - sensor_data    │   │  - Dashboards   │  │
    │  │  - device_events  │   │  - Alerts       │  │
    │  │  - Time-series DB │   │  - Event logs   │  │
    │  └────────────┬───────┘   └────────────────┘  │
    │           │            Flux queries            │
    └───────────┴───────────────────────────────────┘
```

## Configuration

### Secrets Setup
Create `include/secrets.h` (excluded from git):
```cpp
static const char* INFLUXDB_URL = "http://192.168.1.100:8086";
static const char* INFLUXDB_TOKEN = "your-influxdb-token";  
static const char* INFLUXDB_ORG = "your-org";
static const char* INFLUXDB_BUCKET = "sensor_data";
```

### Data Model
**InfluxDB Measurements**:
- `sensor_data`: Temperature readings, solar metrics, battery data
- `device_events`: Boot, WiFi connect/disconnect, error events

**Common Tags**: device_name, project_type (temp/solar), location  
**Common Fields**: temperature_c, voltage, current, power, rssi, heap_free

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
# Connect to "ESP-Setup" AP → http://192.168.4.1
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
- Captive portal setup at 192.168.4.1
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