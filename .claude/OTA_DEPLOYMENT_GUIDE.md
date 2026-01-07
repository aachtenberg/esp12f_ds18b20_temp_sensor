# OTA Deployment Guide

## Quick Reference

### OTA Password
- Defined in `temperature-sensor/include/secrets.h`
- Set via environment variable when uploading (do not commit)

## Deployment Process

### 1. Bump Version
Update firmware version in `temperature-sensor/platformio.ini`:
```bash
# Current version format: MAJOR.MINOR.PATCH
sed -i 's/FIRMWARE_VERSION_PATCH=X/FIRMWARE_VERSION_PATCH=Y/g' temperature-sensor/platformio.ini
```

### 2. Build Firmware
```bash
cd temperature-sensor
pio run -e esp32dev-battery    # For battery-powered ESP32 (Sauna, Spa)
pio run -e esp32-small-garage  # For mains ESP32 with OLED (Small Garage)
pio run -e esp8266             # For ESP8266 devices (Pump House, Main Cottage, Mobile Temp Sensor)
```

### 3. Flash via OTA
```bash
# For battery-powered ESP32 (Sauna, Spa)
export PLATFORMIO_UPLOAD_FLAGS="--auth=<OTA_PASSWORD>"
pio run -e esp32dev-battery -t upload --upload-port <IP_ADDRESS>

# For Small Garage (ESP32 with OLED)
export PLATFORMIO_UPLOAD_FLAGS="--auth=<OTA_PASSWORD>"
pio run -e esp32-small-garage -t upload --upload-port <IP_ADDRESS>

# For ESP8266 devices (Pump House, Main Cottage, Mobile Temp Sensor)
export PLATFORMIO_UPLOAD_FLAGS="--auth=<OTA_PASSWORD>"
pio run -e esp8266 -t upload --upload-port <IP_ADDRESS>
```

### 4. Verify Deployment
Check device health endpoint:
```bash
curl http://<IP_ADDRESS>/health
```

## Device-Specific Notes

### ESP32 Battery Devices (Sauna, Spa) - `esp32dev-battery`
- Battery-powered with deep sleep capability
- OLED disabled for power saving
- HTTP server disabled (battery mode)
- MQTT buffer size: 2048 bytes
- CPU: 80MHz, WiFi power save: WIFI_PS_NONE
- Battery monitoring enabled (GPIO 34 ADC)

### ESP32 Small Garage - `esp32-small-garage`
- Mains-powered with OLED display
- HTTP server enabled (API endpoints only)
- MQTT buffer size: 2048 bytes
- CPU: 80MHz, WiFi power save: WIFI_PS_MIN_MODEM
- Deep sleep disabled

### ESP8266 Devices (Pump House, Main Cottage, Mobile Temp Sensor) - `esp8266`
- Mains-powered
- OLED disabled
- HTTP server enabled (API endpoints only)
- MQTT buffer size: 512 bytes
- CPU: 80MHz, WiFi power save: WIFI_LIGHT_SLEEP
- Deep sleep disabled

## MQTT Commands

### Configure Deep Sleep
```bash
mosquitto_pub -h 192.168.0.167 -t "esp-sensor-hub/<DeviceName>/command" -m "deepsleep <seconds>"
```

### Disable Deep Sleep (for OTA)
```bash
mosquitto_pub -h 192.168.0.167 -t "esp-sensor-hub/<DeviceName>/command" -m "deepsleep 0"
```

### Restart Device
```bash
mosquitto_pub -h 192.168.0.167 -t "esp-sensor-hub/<DeviceName>/command" -m "restart"
```

### Trigger WiFi Config Portal
```bash
mosquitto_pub -h 192.168.0.167 -t "esp-sensor-hub/<DeviceName>/command" -m "config"
# or
mosquitto_pub -h 192.168.0.167 -t "esp-sensor-hub/<DeviceName>/command" -m "portal"
```

### Request Status
```bash
mosquitto_pub -h 192.168.0.167 -t "esp-sensor-hub/<DeviceName>/command" -m "status"
```

## Recent Changes (v1.0.9)

### Double-Reset Detection (DRD) - FIXED
- **Working**: Press reset button twice within 10 seconds to trigger WiFi config portal
- **Storage**: ESP32 uses SPIFFS, ESP8266 uses LittleFS for DRD flag persistence
- **Critical Fix**: DRD check moved to early in `setup()` before WiFi/MQTT initialization
  - Prevents timeout expiry before user can double-tap reset
  - Debug output enabled to verify flag operations
- **Config Defines**: Storage backend must be defined BEFORE including ESP_DoubleResetDetector.h

### Removed Features
- **Tracing**: Removed all trace_id, traceparent, and seq_num fields
  - Saves ~107 bytes per MQTT message
  - Reduces heap fragmentation
  - Removed files: `trace.h`, `trace.cpp`

### Power Management
- CPU runs at full speed (80MHz, no frequency reduction)
- Power savings handled by deep sleep instead
- Prevents timing issues and slow OTA transfers

### Deep Sleep Behavior
- **Manual reset detection**: Pressing reset button disables deep sleep
- **Timer wake**: Normal deep sleep wake continues with deep sleep enabled
- Allows for OTA updates and debugging after manual reset

### Library Dependencies
- All dependencies migrated from git URLs to PlatformIO registry versions
- Fixes git cache corruption issues
- Standardized across all build environments

## Troubleshooting

### OTA Authentication Fails
1. Verify OTA password matches `secrets.h`
2. Ensure environment variable is set: `export PLATFORMIO_UPLOAD_FLAGS="--auth=<OTA_PASSWORD>"`
3. Check device is not in deep sleep (wake it or disable deep sleep via MQTT)

### Device Not Responding
1. Check if device is in deep sleep
2. Send MQTT command to disable deep sleep
3. Wait for next wake cycle or manually reset device

### Build Failures
1. Clean build artifacts: `pio run --target clean`
2. Rebuild: `pio run -e <environment>`
3. Check for missing dependencies

### Double-Reset Not Working
1. Verify DRD storage defines are set BEFORE `#include <ESP_DoubleResetDetector.h>`
2. Check serial debug output for flag operations (0xD0D01234 = SET, 0xD0D04321 = CLEAR)
3. Ensure `drd->detectDoubleReset()` is called early in `setup()` before WiFi initialization
4. Press reset twice within 10 seconds (DRD_TIMEOUT)

### ESP8266 Compilation Errors
1. Check for duplicate variable declarations (common with `resetReason`)
2. Ensure `#ifdef ESP8266` blocks don't conflict with ESP32 code paths
3. Verify LittleFS is available for ESP8266 core 2.7.1+

### Brownout Detector Triggered
**Symptom**: Device repeatedly resets with "Brownout detector was triggered" message in serial output

**Root Causes**:
1. **Hardware defect**: Weak voltage regulator or insufficient decoupling capacitors on ESP32 module
2. **Power supply issue**: Weak USB port, bad cable, or dying battery
3. **High current draw**: WiFi initialization draws 250-400mA burst current

**Diagnosis**:
1. Test with different ESP32 module - if new module works, original has hardware defect
2. Add 470µF capacitor across VCC/GND near ESP32 power pins
3. Try different USB cable/port or use external 3.3V power supply
4. Check voltage regulator (AMS1117 or similar) for proper operation

**Serial Flash Workaround**:
If device is in brownout reset loop and OTA is impossible, use serial flash environment:
```bash
pio run -e esp32dev-battery-display-serial -t upload --upload-port /dev/ttyUSB0
```

The `esp32dev-battery-display-serial` environment uses `upload_protocol = esptool` for USB serial flashing instead of OTA.

## Memory Optimization

### After Tracing Removal (v1.0.9)
- **ESP32**: RAM usage ~15.7% (51KB used)
- **Flash**: 78.7% (1MB used)
- Saved per message: ~107 bytes
- Reduced String operations and heap fragmentation

## Version History
- **v1.0.13** (2025-12-30): Serial flash environment for brownout troubleshooting
  - Added `esp32dev-battery-display-serial` environment with `OLED_ENABLED=0` for serial USB flashing
  - Fixed Spa device brownout reset loop by replacing faulty ESP32 module
  - Documented brownout detector troubleshooting steps
- **v1.0.9** (2025-12-24): Fixed double-reset detection, removed tracing, library dependency cleanup
  - DRD now works: check moved early in setup() before WiFi/MQTT init
  - DRD storage configured: ESP32=SPIFFS, ESP8266=LittleFS
  - Fixed ESP8266 duplicate resetReason variable compilation error
  - Migrated all library dependencies from git URLs to registry versions
  - All 6 devices deployed and verified
- **v1.0.8**: OTA gated by deep sleep, graceful MQTT disconnect
- **v1.0.7**: WiFi power save fixes
- **v1.0.6**: Deep sleep optimizations
