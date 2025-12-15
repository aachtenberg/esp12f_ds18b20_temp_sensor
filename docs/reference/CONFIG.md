# Configuration Reference

## Required Files

### secrets.h Setup
Create `temperature-sensor/include/secrets.h` (excluded from git):
```cpp
#ifndef SECRETS_H
#define SECRETS_H

// MQTT Configuration
static const char* MQTT_BROKER = "your.mqtt.broker.com";  // e.g., "mosquitto.local" or LAN hostname
static const int MQTT_PORT = 1883;
static const char* MQTT_USER = "";        // Empty if no authentication
static const char* MQTT_PASSWORD = "";    // Empty if no authentication

// WiFi Configuration (also set via WiFiManager portal)
static const char* WIFI_SSID = "";        // Leave empty to use portal
static const char* WIFI_PASSWORD = "";    // Leave empty to use portal

#endif
```

### WiFi Configuration
**WiFi credentials configured via WiFiManager portal - no compile-time setup needed!**

1. Device creates AP "ESP-Setup" (password: "configure") 
2. Connect to AP and open the WiFiManager captive portal (e.g., http://esp-setup.local)
3. Enter WiFi credentials and device name
4. Device saves config and connects automatically

## Deployment Commands

### Flash Single Device
```bash
# Temperature sensor
./scripts/flash_device.sh temp

# Solar monitor  
./scripts/flash_device.sh solar

# Surveillance camera
./scripts/flash_device.sh surveillance
```

### Flash Multiple Devices
```bash
# Temperature sensors
python3 scripts/flash_multiple.py --project temp

# Solar monitors
python3 scripts/flash_multiple.py --project solar
```

### Monitor Device
```bash
# Serial output
platformio device monitor -b 115200

# Web interface (after WiFi connection)
curl http://DEVICE_IP
```

## WSL2 USB Setup (Windows Users)

USB devices require Windows-side attachment using `usbipd`:

```powershell
# Run in Windows PowerShell as Administrator
usbipd list  # Find your device BUSID (e.g., 2-11)
usbipd bind --busid 2-11  # One-time share
usbipd attach --wsl --busid 2-11  # Connect to WSL
```

## Data Queries

### Monitor MQTT Temperature Stream
```bash
mosquitto_sub -h your.mqtt.broker.com -t "esp-sensor-hub/+/temperature" -v
```

### Monitor Device Status (Retained Messages)
```bash
mosquitto_sub -h your.mqtt.broker.com -t "esp-sensor-hub/+/status" -R -v
```

### Monitor Device Events
```bash
mosquitto_sub -h your.mqtt.broker.com -t "esp-sensor-hub/+/events" -v
```

### Example MQTT Payloads
**Temperature message**:
```json
{
  "device": "Small Garage",
  "chip_id": "3C61053ED814",
  "timestamp": 65,
  "celsius": 21.12,
  "fahrenheit": 70.03
}
```

**Status message** (retained, refreshed every 30s):
```json
{
  "device": "Small Garage",
  "chip_id": "3C61053ED814",
  "timestamp": 65,
  "uptime_seconds": 65,
  "wifi_connected": true,
  "wifi_rssi": -63,
  "free_heap": 239812,
  "sensor_healthy": true,
  "wifi_reconnects": 0,
  "sensor_read_failures": 0
}
```

### Query InfluxDB (Optional Bridge)
If Telegraf bridge is running to sync MQTT → InfluxDB v3:
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "temperature")
  |> filter(fn: (r) => r.device == "Small Garage")
```

## Troubleshooting

### Device Won't Connect to MQTT
1. Check serial output for MQTT connection status
2. Verify broker IP and port in secrets.h (e.g., your.mqtt.broker.com:1883)
3. Confirm MQTT broker is running and accessible from network
4. Check device WiFi connection first (`[MQTT] Not connected, skipping publish`)

### Device Won't Connect to WiFi
1. Check serial output for WiFi status
2. Ensure correct SSID/password via WiFiManager portal
3. Device creates "ESP-Setup" AP for reconfiguration
4. Factory reset: hold reset during power-on

### MQTT Payloads Not Appearing
1. Verify device has WiFi connection (`wifi_connected: true` in status)
2. Check MQTT broker is receiving: `mosquitto_sub -h your.mqtt.broker.com -t "esp-sensor-hub/#" -v`
3. Monitor device serial for publish status: `[MQTT] Publishing to ... Publish successful`
4. Check heap memory isn't exhausted (`free_heap` in status message)

### High Memory Usage
1. Monitor `free_heap` value in status payload
2. If < 20KB, device may be unstable or dropping publishes
3. Reduce publish cadence or disable OLED display to free memory
4. Check for memory leaks: restart device and monitor heap over time

### Compilation Errors
1. Ensure `temperature-sensor/include/secrets.h` exists
2. Copy from `temperature-sensor/include/secrets.h.example` if needed
3. Verify MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD are defined
4. Check PlatformIO environment matches board type (esp8266 vs esp32dev)

---

**Key Points**:
- ✅ Only MQTT broker details need compile-time configuration
- ✅ WiFi configured via WiFiManager captive portal (no hardcoded credentials)
- ✅ Device names set via portal, published in MQTT `device` field
- ✅ All data publishing visible in serial logs for debugging