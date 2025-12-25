# Solar Monitor Configuration

## Overview

ESP32 firmware for monitoring Victron solar equipment via VE.Direct protocol. See [solar-monitor/README.md](../../solar-monitor/README.md) for hardware setup.

## Required Files

### secrets.h Setup
Create `solar-monitor/include/secrets.h`:
```cpp
#ifndef SECRETS_H
#define SECRETS_H

// MQTT Configuration
static const char* MQTT_BROKER = "your.mqtt.broker.com";
static const int MQTT_PORT = 1883;
static const char* MQTT_USER = "";
static const char* MQTT_PASSWORD = "";

// WiFi Configuration (set via WiFiManager portal)
static const char* WIFI_SSID = "";
static const char* WIFI_PASSWORD = "";

#endif
```

## Build & Flash

### Initial Setup
```bash
cd solar-monitor
pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0
```

### OTA Updates
```bash
# Update via IP address
pio run -e esp32dev -t upload --upload-port 192.168.0.XXX
```

## Hardware Connections

### VE.Direct Wiring
Connect Victron equipment to ESP32 via VE.Direct cables:
- SmartShunt → ESP32 RX1/TX1
- MPPT Controller 1 → ESP32 RX2/TX2
- MPPT Controller 2 → ESP32 RX3/TX3

### OLED Display (Optional)
- SDA → GPIO 21
- SCL → GPIO 22
- VCC → 3.3V
- GND → GND

## MQTT Topics

| Topic | Payload | Purpose |
|-------|---------|---------|
| `solar-monitor/battery` | `{voltage, current, soc, ...}` | SmartShunt data |
| `solar-monitor/mppt1` | `{pv_voltage, pv_power, yield, ...}` | MPPT Controller 1 |
| `solar-monitor/mppt2` | `{pv_voltage, pv_power, yield, ...}` | MPPT Controller 2 |
| `solar-monitor/status` | `{uptime, free_heap, ...}` | Device status |

## Configuration

### WiFiManager Portal
1. Device creates AP: "Solar-Monitor-Setup"
2. Connect and configure WiFi credentials
3. Device reboots and connects to network

### Display Configuration
OLED display shows:
- Battery voltage and SOC
- Solar panel voltage
- Current power generation
- Daily energy yield

## Troubleshooting

### No VE.Direct Data
1. Verify VE.Direct cable connections
2. Check serial port configuration
3. Monitor serial output for VE.Direct frames

### Incorrect Readings
1. Verify equipment firmware is up to date
2. Check baud rate (19200 for VE.Direct)
3. Ensure cables not damaged

### MQTT Data Not Publishing
1. Check WiFi connection
2. Verify MQTT broker settings
3. Monitor serial for MQTT publish status

## Monitoring

### Real-time Data
```bash
# Subscribe to all solar monitor topics
mosquitto_sub -h BROKER -t "solar-monitor/#" -v

# Battery status only
mosquitto_sub -h BROKER -t "solar-monitor/battery" -v

# MPPT controllers
mosquitto_sub -h BROKER -t "solar-monitor/mppt+" -v
```
