# Configuration Reference

## Required Files

### secrets.h Setup
Create `include/secrets.h` (excluded from git):
```cpp
#ifndef SECRETS_H
#define SECRETS_H

// InfluxDB Configuration (REQUIRED)
static const char* INFLUXDB_URL = "http://192.168.1.100:8086";
static const char* INFLUXDB_TOKEN = "your-token-here";
static const char* INFLUXDB_ORG = "your-org";
static const char* INFLUXDB_BUCKET = "sensor_data";

// Optional: Compile-time device name (can be set via WiFiManager portal)
static const char* DEVICE_NAME = ""; // Leave empty to use portal configuration

#endif
```

### WiFi Configuration
**WiFi credentials configured via WiFiManager portal - no compile-time setup needed!**

1. Device creates AP "ESP-Setup" (password: "configure") 
2. Connect to AP and navigate to http://192.168.4.1
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

### Recent Temperature Data
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "temperature")
  |> filter(fn: (r) => r.device_name == "your-device")
```

### Device Events (Troubleshooting)
```flux
from(bucket: "sensor_data") 
  |> range(start: -7d)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r.device_name == "your-device")
```

### Solar Performance
```flux
from(bucket: "sensor_data")
  |> range(start: -24h) 
  |> filter(fn: (r) => r._measurement == "solar")
  |> filter(fn: (r) => r.device_name == "your-device")
```

## Troubleshooting

### Device Won't Connect to WiFi
1. Check serial output for WiFi status
2. Ensure correct SSID/password via portal
3. Device creates "ESP-Setup" AP for reconfiguration
4. Factory reset: hold reset during power-on

### No Data in InfluxDB  
1. Verify InfluxDB token and URL in secrets.h
2. Check device serial output for HTTP errors
3. Verify InfluxDB container is running: `docker ps`
4. Test InfluxDB connectivity: `curl http://your-pi:8086/ping`

### Compilation Errors
1. Ensure `include/secrets.h` exists
2. Copy from `include/secrets.h.example` if needed
3. Verify all required credentials are set
4. Check PlatformIO environment matches project type

---

**Key Points**:
- ✅ Only InfluxDB credentials need compile-time configuration
- ✅ WiFi configured via captive portal (no hardcoded credentials)  
- ✅ Device names can be set via portal or secrets.h
- ✅ Single secrets.h file supports both temp and solar projects