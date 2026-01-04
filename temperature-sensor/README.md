# DS18B20 Temperature Sensor

WiFi-enabled temperature monitoring with DS18B20 sensors for environmental tracking.

## Features

- **DS18B20 Sensor**: Waterproof 1-Wire digital temperature sensor (-55°C to +125°C)
- **WiFi Configuration**: WiFiManager portal with double-reset detection
- **MQTT Integration**: Real-time temperature publishing to MQTT broker
- **OTA Updates**: Over-the-air firmware updates via ArduinoOTA
- **Battery Support**: Optional battery voltage monitoring (ESP32)
- **Deep Sleep**: Battery-optimized deep sleep mode with remote configuration
- **Optional Display**: SSD1306 OLED display (ESP32 only)
- **Multi-Platform**: ESP8266, ESP32, and ESP32-S3 support

## Hardware

### Required
- ESP8266 (NodeMCU) or ESP32 development board
- DS18B20 waterproof temperature sensor with cable
- 4.7K ohm resistor (pull-up for 1-Wire bus)
- USB cable for power and initial programming

### Optional
- SSD1306 OLED display (128x64, I2C) - ESP32 only
- Battery (3.7V lithium-ion) with TP4056 charger
- Voltage divider for battery monitoring (10K + 10K resistors)

### Pinout

**DS18B20 Connection (All Boards)**
```
DS18B20:
  Red (VCC)    → 3.3V
  Yellow (DATA) → GPIO 4 + 4.7K pullup resistor to 3.3V
  Black (GND)   → GND
```

**OLED Display (ESP32 only, optional)**
```
SSD1306 OLED:
  SDA → GPIO 21
  SCL → GPIO 22
  VCC → 3.3V
  GND → GND
```

**Battery Monitor (ESP32 only, optional)**
```
Voltage Divider:
  Battery+ → 10K → GPIO 34 → 10K → GND
```

**For complete battery setup with TP4056 charger:**
See [docs/hardware/BATTERY_SETUP_GUIDE.md](../docs/hardware/BATTERY_SETUP_GUIDE.md)

## Quick Start

### 1. Setup

```bash
cd temperature-sensor

# Copy secrets template
cp include/secrets.h.example include/secrets.h

# Edit with your MQTT broker details
nano include/secrets.h
```

### 2. First Upload (USB)

**ESP8266 (most common for this project)**
```bash
# Upload via USB for ESP8266 devices
pio run -e esp8266-serial -t upload

# Monitor serial output
pio device monitor
```

**ESP32 with Display**
```bash
# Upload via USB for ESP32 with OLED
pio run -e esp32dev-serial -t upload
```

**ESP32 Battery-Powered (No Display)**
```bash
# Upload via USB for battery-powered ESP32
pio run -e esp32dev-battery-serial -t upload
```

### 3. WiFi Configuration

On first boot, device creates access point:
- **SSID**: `ESP-TempSensor` (or custom device name)
- **Portal**: Open browser to `http://192.168.4.1`
- **Configure**: Select WiFi network, enter password, set device name

**Double-Reset to Reconfigure**: Press reset button twice within 10 seconds to open portal again.

### 4. Subsequent Uploads (OTA)

```bash
# Set OTA password from secrets.h
export PLATFORMIO_UPLOAD_FLAGS="--auth=YOUR_OTA_PASSWORD"

# Upload via network (replace IP address)
pio run -e esp8266 -t upload --upload-port 192.168.0.X   # ESP8266
pio run -e esp32dev -t upload --upload-port 192.168.0.X  # ESP32
```

**For WSL2/Windows firewall issues**, see [docs/reference/CONFIG.md](../docs/reference/CONFIG.md#ota-upload-issues-wsl2windows)

## Configuration

### Device Name
Set via WiFiManager portal on first boot or after double-reset. Used for:
- MQTT topic prefix: `esp-sensor-hub/{device-name}/`
- OTA hostname
- Display name

### Deep Sleep (Battery Mode)

**Enable via MQTT**:
```bash
# Enable deep sleep (30 seconds)
mosquitto_pub -h BROKER -t "esp-sensor-hub/Spa/command" -m "deepsleep 30"

# Disable deep sleep
mosquitto_pub -h BROKER -t "esp-sensor-hub/Spa/command" -m "deepsleep 0"
```

**Configuration persists** across reboots (stored in SPIFFS/LittleFS).

**ESP8266 Deep Sleep**: Requires GPIO16 → RST hardware modification (disabled by default via `DISABLE_DEEP_SLEEP` flag)

### Firmware Versioning

**Always bump version before deploying**:
```bash
cd temperature-sensor

# Bump patch version (bug fixes)
./update_version.sh --patch

# Bump minor version (features)
./update_version.sh --minor

# Build and upload
pio run -e esp32dev -t upload --upload-port 192.168.0.X
```

Version appears in all MQTT messages as `firmware_version: "1.0.13-build20251230"`

## MQTT Topics

All topics use prefix: `esp-sensor-hub/{device-name}/`

### Published Topics

**Temperature Readings** (`/temperature`)
```json
{
  "device": "Pump House",
  "chip_id": "D8F15B0E72A5",
  "firmware_version": "1.0.13-build20251230",
  "schema_version": 1,
  "timestamp": 12345,
  "uptime_seconds": 600,
  "current_temp_c": 23.5,
  "battery_voltage": 3.9,
  "battery_percent": 75
}
```

**Status Messages** (`/status`)
```json
{
  "device": "Pump House",
  "chip_id": "D8F15B0E72A5",
  "firmware_version": "1.0.13-build20251230",
  "uptime_seconds": 600,
  "free_heap": 35000,
  "rssi": -65,
  "mqtt_connected": true,
  "mqtt_publish_failures": 0,
  "sensor_read_failures": 0,
  "wifi_reconnects": 0,
  "deep_sleep_seconds": 0
}
```

**Events** (`/events`)
```json
{
  "event": "mqtt_connected",
  "severity": "info",
  "message": "Connected to MQTT broker",
  "timestamp": 12345
}
```

### Command Topic (`/command`)

Send commands to device:
```bash
# Request status
mosquitto_pub -h BROKER -t "esp-sensor-hub/Pump-House/command" -m "status"

# Restart device
mosquitto_pub -h BROKER -t "esp-sensor-hub/Pump-House/command" -m "restart"

# Configure deep sleep
mosquitto_pub -h BROKER -t "esp-sensor-hub/Pump-House/command" -m "deepsleep 30"
```

## Platform-Specific Notes

### ESP8266
- **Most common**: 6 of 8 deployed devices
- **Memory constrained**: No OLED display support
- **Deep sleep disabled**: Requires GPIO16 → RST hardware mod (see CONFIG.md)
- **MQTT buffer**: 512 bytes (sufficient for temperature readings)
- **Power**: ~80mA active, deep sleep not recommended without hardware mod

### ESP32
- **Display support**: OLED enabled for some devices (e.g., Small Garage)
- **Battery optimized**: Deep sleep with RTC timer (no hardware mods needed)
- **MQTT buffer**: 2048 bytes (supports complex payloads)
- **Power**: ~80mA active, ~10µA deep sleep
- **Average (30s cycle)**: ~3mA with deep sleep enabled

### ESP32-S3
- **Supported**: Compatible with ESP32 configuration
- **Reset detection**: Uses NVS (Preferences) instead of RTC memory
- **MQTT buffer**: 2048 bytes

## Deployed Devices

**8 Active Devices** - See [docs/DEVICE_INVENTORY.md](docs/DEVICE_INVENTORY.md) for complete list:

- **Pump House** (ESP8266 @ 192.168.0.122) - v1.0.8
- **Main Cottage** (ESP8266 @ 192.168.0.139) - v1.0.8
- **Small Garage** (ESP32 @ 192.168.0.176) - v1.0.8, OLED display
- **Spa** (ESP32 @ 192.168.0.196) - v1.0.8, battery-powered
- **Sauna** (ESP32 @ 192.168.0.135) - v1.0.8, battery-powered
- **Mobile Temp Sensor** (ESP8266) - v1.0.8
- **Big Garage** (ESP32) - Active
- **Temp Sensor** (Unknown) - Active

## Troubleshooting

### No Temperature Readings

1. **Check sensor wiring**: Verify DS18B20 connections (red=3.3V, yellow=GPIO4, black=GND)
2. **Check pull-up resistor**: 4.7K between DATA (GPIO4) and 3.3V
3. **Serial monitor**: Look for `[SENSOR] Found X DS18B20 devices`
4. **Test sensor**: Run `pio device monitor` and watch for temperature values

### WiFi Connection Issues

1. **Double-reset**: Press reset button twice within 10 seconds to open portal
2. **Check SSID**: Look for access point named after device
3. **Portal timeout**: 5 minutes (300 seconds), then uses saved config
4. **Signal strength**: Move closer to router, check RSSI in status messages

### MQTT Not Connecting

1. **Check broker**: Verify MQTT broker IP and port in `secrets.h`
2. **Check credentials**: Ensure `MQTT_USER` and `MQTT_PASSWORD` are correct
3. **Monitor broker**: `mosquitto_sub -h BROKER -t "esp-sensor-hub/#" -v`
4. **Serial output**: Look for `[MQTT] Connected!` or error codes

### OTA Upload Fails

1. **Check network**: Ping device IP address
2. **Verify password**: OTA_PASSWORD in secrets.h matches upload flags
3. **Deep sleep**: Device must have `deepsleep 0` for OTA (or use serial)
4. **WSL2 firewall**: See CONFIG.md for Windows Firewall workaround

### Deep Sleep Issues

**Device won't wake**:
- ESP32: Missing WiFi/MQTT disconnect before sleep (fixed in v1.0.8)
- ESP8266: Requires GPIO16 → RST hardware modification

**Can't configure remotely**:
- Missing MQTT command processing window (fixed in v1.0.8)
- Device needs 5-second `mqttClient.loop()` delay before sleeping

## Documentation

- **[Device Inventory](docs/DEVICE_INVENTORY.md)** - Complete device list with IPs and versions
- **[Platform Guide](../docs/reference/PLATFORM_GUIDE.md)** - Architecture and features
- **[Configuration Reference](../docs/reference/CONFIG.md)** - Setup and troubleshooting
- **[Battery Setup Guide](../docs/hardware/BATTERY_SETUP_GUIDE.md)** - TP4056 + battery wiring
- **[Version History](README_VERSION.md)** - Firmware version tracking details

## Development

### Building

```bash
# List all environments
pio run --list-targets

# Build specific environment
pio run -e esp8266
pio run -e esp32dev
pio run -e esp32dev-battery
```

### Testing

```bash
# Monitor serial output
pio device monitor

# Monitor MQTT messages
mosquitto_sub -h BROKER -t "esp-sensor-hub/#" -v

# Check device health
curl http://192.168.0.X/health
```

### Version Management

```bash
# Update version before deploying
./update_version.sh --patch   # 1.0.5 → 1.0.6
./update_version.sh --minor   # 1.0.5 → 1.1.0
./update_version.sh --major   # 1.0.5 → 2.0.0
```

## License

See repository LICENSE file.
