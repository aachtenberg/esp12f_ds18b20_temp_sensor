# BME280 Environmental Sensor

Environmental monitoring sensor measuring temperature, humidity, and atmospheric pressure.

## Features

- **BME280 Sensor**: Measures temperature, humidity, pressure, and calculates altitude
- **Double Reset Detector**: Press reset twice within 10 seconds to enter WiFi configuration portal
- **WiFi Configuration**: WiFiManager portal for easy SSID/password configuration (AP at 192.168.4.1)
- **MQTT Integration**: Publishes readings to MQTT broker with full device metrics
- **Pressure Baseline Tracking**: Barometer-style weather tracking with pressure change trends
- **OTA Updates**: Over-the-air firmware updates via ArduinoOTA
- **Battery Support**: Optional battery voltage monitoring (ESP32)
- **Deep Sleep**: Battery-optimized deep sleep mode with configurable interval (MQTT-controlled)
- **WiFi Power Save Disabled**: Ensures stable MQTT connectivity

## Hardware

### Required
- ESP32 development board (esp32dev)
- BME280 I2C environmental sensor module
- USB cable for power and programming

### Optional
- Battery (3.7V lithium-ion)
- SSD1306 OLED display (I2C)
- Voltage divider for battery monitoring (10K + 10K resistors)

### Pinout (ESP32 and ESP32-S3)

**ESP32-S3 WROOM (GPIO 8/9)**
```
BME280 I2C:
  SDA → GPIO 8 (I2C Data)
  SCL → GPIO 9 (I2C Clock)
  VCC → 3.3V
  GND → GND
```

**ESP32 Standard (GPIO 21/22)**
```
BME280 I2C:
  SDA → GPIO 21 (I2C Data)
  SCL → GPIO 22 (I2C Clock)
  VCC → 3.3V
  GND → GND
```

**Battery Monitor (Optional, both boards):**
```
  Battery+ → 10K → GPIO 34 → 10K → GND
```

**For complete battery setup with TP4056 charger:**
See [docs/hardware/BATTERY_SETUP_GUIDE.md](../docs/hardware/BATTERY_SETUP_GUIDE.md) for:
- TP4056 lithium battery charger wiring
- Complete connection diagram (ESP32 + TP4056 + battery)
- Safety warnings and component specifications
- Solar panel integration (optional)

## Quick Start

### 1. Setup

```bash
cd bme280-sensor

# Copy secrets template
cp include/secrets.h.example include/secrets.h

# Edit secrets.h with your MQTT broker details
nano include/secrets.h
```

### 2. First Upload (USB)

**ESP32-S3 WROOM**
```bash
# Initial upload via USB serial for ESP32-S3
pio run -e esp32s3-serial -t upload
```

**ESP32 Standard**
```bash
# Initial upload via USB serial for ESP32 (battery mode, no display)
pio run -e esp32dev-battery-display-serial -t upload
```

### 3. WiFi Configuration

**Method 1: Double Reset Detector (Recommended)**
- Press the reset button twice within 10 seconds
- Device enters configuration portal automatically

**Method 2: First Boot**
- Device will create WiFi access point on first boot

**Configuration Portal:**
- SSID: `BME280-{DeviceName}-Setup`
- Open browser: `http://192.168.4.1`
- Select your WiFi network and enter password
- Optionally customize device name
- Portal timeout: 5 minutes

### 4. Subsequent Uploads (OTA)

**ESP32-S3 WROOM**
```bash
export PLATFORMIO_UPLOAD_FLAGS="--auth=YOUR_OTA_PASSWORD"
pio run -e esp32s3 -t upload --upload-port 192.168.X.X
```

**ESP32 Standard**
```bash
export PLATFORMIO_UPLOAD_FLAGS="--auth=YOUR_OTA_PASSWORD"
pio run -e esp32dev-battery -t upload --upload-port 192.168.X.X
```

## MQTT Topics

### Publish

- `esp-sensor-hub/{device-name}/readings` - Environmental data (temperature, humidity, pressure, altitude)
- `esp-sensor-hub/{device-name}/status` - Device health and metrics
- `esp-sensor-hub/{device-name}/events` - Device events (startup, errors, etc.)

### Subscribe

- `esp-sensor-hub/{device-name}/command` - Device control commands

### Example Payload (readings)

```json
{
  "device": "Kitchen-Sensor",
  "chip_id": "A0B1C2D3E4F5",
  "firmware_version": "1.1.7-build20260104",
  "schema_version": 1,
  "timestamp": 1234567890,
  "uptime_seconds": 3600,
  "temperature_c": 22.5,
  "humidity_rh": 45.2,
  "pressure_pa": 101325,
  "pressure_hpa": 1013.25,
  "altitude_m": -45.2,
  "pressure_change_pa": -150.0,
  "pressure_change_hpa": -1.5,
  "pressure_trend": "falling",
  "baseline_hpa": 980.0,
  "battery_voltage": 3.85,
  "battery_percent": 85
}
```

## MQTT Commands

All commands sent to `esp-sensor-hub/{device-name}/command` topic:

### Pressure Baseline (Barometer Calibration)

```bash
# Set current pressure as baseline (weather tracking)
mosquitto_pub -h BROKER -t "esp-sensor-hub/Kitchen-Sensor/command" -m "calibrate"
# OR
mosquitto_pub -h BROKER -t "esp-sensor-hub/Kitchen-Sensor/command" -m "set_baseline"

# Set specific baseline value (in hPa)
mosquitto_pub -h BROKER -t "esp-sensor-hub/Kitchen-Sensor/command" -m "baseline 980.0"

# Clear baseline (disable tracking)
mosquitto_pub -h BROKER -t "esp-sensor-hub/Kitchen-Sensor/command" -m "clear_baseline"
```

### Deep Sleep (Battery Mode)

```bash
# Enable 60-second deep sleep interval
mosquitto_pub -h BROKER -t "esp-sensor-hub/Kitchen-Sensor/command" -m "deepsleep 60"

# Disable deep sleep (continuous operation)
mosquitto_pub -h BROKER -t "esp-sensor-hub/Kitchen-Sensor/command" -m "deepsleep 0"
```

### Device Control

```bash
# Request status update
mosquitto_pub -h BROKER -t "esp-sensor-hub/Kitchen-Sensor/command" -m "status"

# Restart device
mosquitto_pub -h BROKER -t "esp-sensor-hub/Kitchen-Sensor/command" -m "restart"
```

## Platforms

### esp32s3
- ESP32-S3 WROOM with minimal power consumption
- GPIO 8 (SDA), GPIO 9 (SCL) for BME280 I2C
- CPU at 80 MHz, WiFi power save enabled
- Suitable for 3.7V battery operation
- No OLED display

### esp32s3-serial
- First-time USB programming for ESP32-S3
- Higher upload speed (460800 baud)
- Use before switching to OTA

### esp32dev-battery
- Standard ESP32 with minimal power consumption
- GPIO 21 (SDA), GPIO 22 (SCL) for BME280 I2C
- CPU at 80 MHz, WiFi power save enabled
- Suitable for 3.7V battery operation
- No OLED display

### esp32dev-battery-display
- Standard ESP32 with OLED support
- Same power optimizations as battery mode
- OLED display optional at runtime
- Higher memory requirements

### esp32dev-battery-display-serial
- First-time USB programming for ESP32
- Higher upload speed (460800 baud)
- Use before switching to OTA

### esp32dev
- Development mode, wired power
- No power optimizations
- Full speed (240 MHz available)

## Troubleshooting

### Sensor Not Detected

```bash
# Check I2C connectivity
pio device monitor

# Look for: "[SENSOR] BME280 initialized successfully"
# If failed, check SDA (GPIO 21) and SCL (GPIO 22) connections
```

### MQTT Not Connected

1. Verify broker address in `secrets.h`
2. Check WiFi connection: `mosquitto_pub -h BROKER -t "test" -m "hello"`
3. Monitor device: `pio device monitor`

### Missing Readings in MQTT

1. Check device is connected: `mosquitto_sub -h BROKER -t "esp-sensor-hub/+/status" -v`
2. Monitor WiFi signal: status topic shows RSSI
3. Check free heap: may need to reduce features if critical low

## Development

### Update Firmware Version

```bash
cd bme280-sensor
./update_version.sh --patch

# Then build and upload
pio run -e esp32dev-battery -t upload --upload-port 192.168.X.X
```

### Monitor Logs

```bash
pio device monitor -p /dev/ttyUSB0 -b 115200
```

### View All Sensor Data

```bash
mosquitto_sub -h BROKER -t "esp-sensor-hub/+/#" -v
```

## Libraries

- **Adafruit BME280**: Sensor driver
- **PubSubClient**: MQTT client
- **ArduinoJson**: JSON serialization
- **WiFiManager**: WiFi configuration portal
- **ArduinoOTA**: Over-the-air updates

## License

Same as esp-sensor-hub repository
