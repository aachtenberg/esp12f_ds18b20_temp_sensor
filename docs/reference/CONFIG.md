# Configuration Reference

Multi-project ESP IoT platform configuration guide. This document covers common configuration shared across all projects, with links to project-specific details.

## Projects

### [Temperature Sensor](../temperature-sensor/CONFIG.md)
ESP8266/ESP32 + DS18B20 temperature monitoring with deep sleep mode and OLED display support.

### [Surveillance Camera](../surveillance/CONFIG.md)
ESP32-S3 camera with motion detection, web streaming, and SD card recording.

### [Solar Monitor](../solar-monitor/CONFIG.md)
ESP32 + Victron equipment monitoring via VE.Direct protocol with OLED display.

---

## Common Configuration

All projects share these configuration requirements:

### secrets.h Setup

Each project requires `PROJECT/include/secrets.h` (excluded from git):

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// MQTT Configuration (required for all projects)
static const char* MQTT_BROKER = "your.mqtt.broker.com";  // Hostname or IP
static const int MQTT_PORT = 1883;
static const char* MQTT_USER = "";        // Empty if no authentication
static const char* MQTT_PASSWORD = "";    // Empty if no authentication

// WiFi Configuration (also set via WiFiManager portal)
static const char* WIFI_SSID = "";        // Leave empty to use portal
static const char* WIFI_PASSWORD = "";    // Leave empty to use portal

// OTA Password (for over-the-air updates)
static const char* OTA_PASSWORD = "your-ota-password";

#endif
```

### WiFi Configuration via WiFiManager

**All projects use WiFiManager for zero-hardcoded credentials**:

1. **Initial Setup**: Device creates AP with name based on project (e.g., "Temp-DEVICE-Setup", "ESP32CAM-Setup", "Solar-Monitor-Setup")
2. **Connect to AP**: No password required (or check project-specific docs)
3. **Configure**: Open captive portal (usually http://192.168.4.1)
4. **Enter Credentials**: Select WiFi network, enter password, set device name
5. **Auto-Connect**: Device saves config and connects automatically on future boots

**Factory Reset**: Double-tap reset button (or triple-tap for ESP32-S3) to re-enter configuration portal

### MQTT Broker Setup

**All projects publish to MQTT broker** with hierarchical topic structure:

- Temperature sensors: `esp-sensor-hub/{device}/*`
- Surveillance: `surveillance/{device}/*`
- Solar monitor: `solar-monitor/*`

**Broker Requirements**:
- Mosquitto 2.0+ recommended
- Port 1883 (MQTT) or 8883 (MQTT-TLS)
- Anonymous or username/password auth
- QoS 0 or 1 support

**Testing Broker**:
```bash
# Verify broker is accessible
mosquitto_sub -h your.mqtt.broker.com -t "#" -v

# Test publish
mosquitto_pub -h your.mqtt.broker.com -t "test/topic" -m "hello"
```

---

## Common Deployment

### Initial USB Flash (All Projects)

**Required once per device**:

```bash
# Navigate to project directory
cd temperature-sensor   # or surveillance, solar-monitor

# Build and flash (USB cable required)
pio run -e ENVIRONMENT -t upload --upload-port /dev/ttyUSB0

# Common environments:
# - esp8266 (Temperature Sensor)
# - esp32dev (Temperature Sensor, Solar Monitor)
# - esp32-s3-devkitc-1 (Surveillance)
```

**Find USB port**:
```bash
# Linux/WSL
ls -la /dev/ttyUSB*

# macOS
ls -la /dev/cu.usb*
```

### OTA Updates (After Initial Flash)

**Update over WiFi without USB cable**:

```bash
cd PROJECT_DIR
pio run -e ENVIRONMENT -t upload --upload-port 192.168.0.XXX

# OTA requires:
# 1. Device on same network
# 2. OTA password configured in secrets.h
# 3. Device has HTTP server enabled (not in deep sleep mode)
```

**WSL2 Users**: May need to temporarily disable Windows Firewall for OTA uploads

### Firmware Version Management

All projects include automatic version tracking:

```ini
# platformio.ini
build_flags =
    -D FIRMWARE_VERSION_MAJOR=1
    -D FIRMWARE_VERSION_MINOR=1
    -D FIRMWARE_VERSION_PATCH=0
    -D BUILD_TIMESTAMP=20251223
```

Versions visible in:
- Serial output on boot
- MQTT status messages
- HTTP `/health` endpoint

---

## Common Monitoring

### MQTT Data Stream

**Subscribe to all devices**:
```bash
# All projects
mosquitto_sub -h BROKER -t "#" -v

# Temperature sensors only
mosquitto_sub -h BROKER -t "esp-sensor-hub/#" -v

# Surveillance only
mosquitto_sub -h BROKER -t "surveillance/#" -v

# Solar monitor only
mosquitto_sub -h BROKER -t "solar-monitor/#" -v
```

### Device Health Monitoring

**HTTP endpoints** (when not in deep sleep):
```bash
# Temperature sensor health
curl http://192.168.0.XXX/health | jq

# Returns: {device, uptime, free_heap, wifi_rssi, sensor_healthy, ...}
```

### Serial Monitoring

```bash
# PlatformIO monitor
cd PROJECT_DIR
pio device monitor -b 115200

# Or direct serial
screen /dev/ttyUSB0 115200
```

---

## Common Troubleshooting

### Device Won't Connect to WiFi

1. **Check WiFiManager Portal**:
   - Device creates AP on first boot or after reset
   - Connect to AP and configure credentials
   - Verify SSID/password are correct

2. **Factory Reset**:
   - Double-tap (or triple-tap for S3) reset button
   - Re-enter configuration portal
   - Reconfigure WiFi credentials

3. **Serial Debugging**:
   ```bash
   pio device monitor -b 115200
   # Look for: [WiFi] Connection status, RSSI, failures
   ```

### Device Won't Connect to MQTT

1. **Verify Broker Settings**:
   - Check MQTT_BROKER in secrets.h
   - Ensure port 1883 accessible
   - Test broker: `mosquitto_sub -h BROKER -t "#"`

2. **Check Device Logs**:
   ```bash
   pio device monitor -b 115200
   # Look for: [MQTT] Connection attempts, errors
   ```

3. **Network Issues**:
   - Verify device has WiFi connection first
   - Check broker firewall rules
   - Confirm broker authentication settings

### OTA Upload Fails

1. **Firewall Issues** (WSL2):
   - Temporarily disable Windows Firewall
   - Or add exception for Python/PlatformIO

2. **Device Not Reachable**:
   - Ping device IP: `ping 192.168.0.XXX`
   - Verify HTTP server running (not in deep sleep)
   - Check OTA password matches secrets.h

3. **Network Timeout**:
   - Move device closer to WiFi AP
   - Check WiFi signal strength (RSSI > -80dBm)
   - Reduce upload_speed in platformio.ini

### Memory Issues

**Symptoms**: Device crashes, heap exhaustion, publish failures

1. **Check Free Heap**:
   - MQTT status: `{"free_heap": XXXXX}`
   - HTTP `/health`: Shows current heap
   - Should be > 20KB for stable operation

2. **Reduce Memory Usage**:
   - Disable unused features (HTTP server, OLED)
   - Reduce MQTT_MAX_PACKET_SIZE
   - Check for memory leaks (Strings vs char[])

3. **Platform Differences**:
   - ESP8266: 512 bytes MQTT packets, minimal features
   - ESP32: 2048 bytes MQTT packets, full features
   - ESP32-S3: 2048+ bytes, camera support

---

## Project-Specific Configuration

See individual project documentation for detailed setup:

- **[Temperature Sensor Config](../temperature-sensor/CONFIG.md)** - Deep sleep, OLED display, battery monitoring
- **[Surveillance Config](../surveillance/CONFIG.md)** - Camera settings, motion detection, SD card
- **[Solar Monitor Config](../solar-monitor/CONFIG.md)** - VE.Direct protocol, Victron equipment

---

**Last Updated**: December 24, 2025
**Projects**: Temperature Sensor (8 devices) | Surveillance (1 device) | Solar Monitor (1 device)
