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

### Temperature Sensor Hardware Issues
**Symptom**: Device shows `sensor_healthy: false` and `sensor_read_failures > 0` in MQTT status

**Root Cause**: DS18B20 temperature sensor disconnected, faulty, or wiring issue

**Symptoms**:
- Device online and MQTT working (status messages appear)
- Temperature readings show "--" or invalid values
- Web endpoints return valid readings but MQTT temperature messages missing
- `sensor_read_failures` counter increasing in status payloads

**Troubleshooting Steps**:
1. **Check sensor connection**: Verify DS18B20 connected to GPIO 4 with 4.7kΩ pull-up resistor
2. **Test web endpoint**: `curl http://device-ip/temperaturec` - if returns valid temp, sensor works but MQTT failing
3. **Monitor MQTT specifically**: `mosquitto_sub -h broker -t "esp-sensor-hub/+/temperature" -v`
4. **Check device logs**: Look for `[MQTT] Publishing temperature...` vs actual publishes

**Hardware Verification**:
- **Power**: 3.3V to sensor VCC, GND to GND
- **Data**: GPIO 4 with 4.7kΩ pull-up to 3.3V
- **Multiple sensors**: Code supports multiple DS18B20 on same bus
- **Parasitic power**: Sensor can operate without separate power pin

**Recovery**: Replace DS18B20 sensor or fix wiring - device firmware is correct

### ESP8266 Deep Sleep Hardware Requirement
**Critical**: ESP8266 deep sleep requires hardware modification for timer wake-up

**Symptom**: Device enters deep sleep but never wakes up (permanent sleep)

**Root Cause**: ESP8266 timer wake-up requires GPIO 16 connected to RST pin

**Required Hardware Modification**:
```
ESP8266 RST pin ──► 10KΩ resistor ──► ESP8266 GPIO 16 (D0)
                                      │
                                      └─► 0.1µF capacitor ──► GND
```

**Why Required**:
- ESP8266 deep sleep timer uses RTC to trigger wake-up
- RTC needs to pulse RST pin to wake the device
- GPIO 16 is the only pin that can be used for this purpose
- Without this connection, device sleeps forever

**Implementation**:
- Connect RST to GPIO 16 via 10KΩ resistor (prevents short circuit)
- Add 0.1µF capacitor from GPIO 16 to GND (debouncing)
- Device will show warning in serial: `GPIO 16 must be connected to RST for wake-up`

**Testing**: After hardware mod, device will wake up reliably from deep sleep

### ESP8266/ESP32 MQTT 12-Hour Reconnection Timeout
**Symptom**: Devices lose MQTT connection after 12+ hours of operation and never reconnect

**Root Cause**: Complex exponential backoff state machine with multiple variables that corrupts after extended operation

**Fixed in v1.1.0** (Dec 23, 2025):
- Simplified to fixed 5-second reconnection retry interval
- Removed 6 state variables (`mqttReconnectInterval`, `mqttBackoffResetTime`, etc)
- Added heap monitoring for ESP8266 memory exhaustion detection
- All devices now use predictable reconnection with no exponential backoff

**Changes**:
```cpp
// Old: Complex backoff (BROKEN)
if (!mqttClient.connected() && (now - lastMqttReconnectAttempt) >= mqttReconnectInterval) {
  ensureMqttConnected();  // mqttReconnectInterval grows exponentially
}

// New: Simple fixed interval (RELIABLE)
if (!mqttClient.connected() && (now - lastMqttReconnectAttempt) >= MQTT_RECONNECT_INTERVAL_MS) {
  ensureMqttConnected();  // Always 5 seconds, no state corruption
}
```

**Verification**: Check firmware version is `1.1.0-build20251223` or later
```bash
curl http://<device-ip>/health | jq '.firmware_version'
```

**Testing**: Device should maintain MQTT connection indefinitely with 5-second reconnection on failure

### MQTT Buffer Size Issues
**Symptom**: Device reports successful MQTT publishes but messages don't appear in broker

**Root Cause**: PubSubClient default buffer (128 bytes) too small for JSON payloads (~350+ bytes)

**Solution**: PlatformIO environments include increased buffer sizes:
- **ESP32**: `-D MQTT_MAX_PACKET_SIZE=2048` (increased from 512 for large payloads with battery monitoring)
- **ESP8266**: `-D MQTT_MAX_PACKET_SIZE=512` (sufficient for smaller payloads)

**Symptoms of Buffer Issues**:
- Device health shows `mqtt_publish_failures: 0` (no recorded failures)
- Status messages work but temperature messages missing
- Device appears connected but data not reaching broker

**Verification**: Check MQTT buffer size in platformio.ini build_flags for all environments

### High Memory Usage
1. Monitor `free_heap` value in status payload
2. If < 20KB, device may be unstable or dropping publishes
3. Reduce publish cadence or disable OLED display to free memory
4. Check for memory leaks: restart device and monitor heap over time

### OTA Upload Failures (WSL2/Windows)
**Symptom**: `pio run -t upload` fails with "No response from device" after authentication succeeds

**Root Cause**: Windows Firewall blocks ESP32 OTA port (3232) from WSL2

**Solution Options**:

1. **Temporary Fix (Recommended for testing)**:
   - Open Windows Security → Firewall & network protection
   - Turn off "Private network" firewall temporarily
   - Run OTA upload: `pio run -e esp32dev -t upload`
   - Re-enable firewall after upload completes

2. **Permanent Fix (Create Firewall Rule)**:
   - Open Windows Firewall with Advanced Security
   - Create new Inbound Rule:
     - Rule Type: Port
     - Protocol: TCP, Port: 3232
     - Action: Allow connection
     - Profile: Private (or all profiles)
     - Name: ESP32 OTA

**Verification**:
- Device shows "OTA:ready" in serial monitor
- Ping works: `ping DEVICE_IP`
- Port accessible after firewall disabled: `nc -zv DEVICE_IP 3232`

**PlatformIO Configuration** (already correct):
```ini
[env:esp32dev]
upload_protocol = espota
upload_port = DEVICE_IP
upload_flags = --auth=YOUR_OTA_PASSWORD --port=3232
```

### Device-Specific Build Environments
**Choose correct environment based on hardware**:

#### ESP32 with Display (esp32dev)
```bash
pio run -e esp32dev -t upload  # Full-featured with OLED display
```

#### ESP32 API-Only (esp32dev-serial)  
```bash
pio run -e esp32dev-serial -t upload  # Serial upload, API-only mode
```

#### ESP8266 API-Only (esp8266)
```bash
pio run -e esp8266 -t upload  # USB-powered, no display, API endpoints only
```

**Environment Differences**:
- **esp32dev**: OLED display, full web interface, battery monitoring
- **esp32dev-serial**: Same as esp32dev but serial upload only  
- **esp8266**: API-only, no HTML interface, optimized for USB power

### OTA Update Procedures
**For devices with known IP addresses**:

1. **Update platformio.ini upload_port**:
   ```ini
   upload_port = 192.168.0.196  # Target device IP
   ```

2. **Verify device is ready**:
   ```bash
   ping 192.168.0.196
   curl http://192.168.0.196/health | jq '.uptime_seconds'
   ```

3. **Handle WSL2 firewall** (if applicable):
   ```powershell
   # Temporarily disable Windows Firewall
   Set-NetFirewallProfile -Profile Private -Enabled False
   ```

4. **Upload firmware**:
   ```bash
   pio run -e esp32dev -t upload
   ```

5. **Verify update**:
   ```bash
   curl http://192.168.0.196/health | jq '.firmware_version'
   mosquitto_sub -h broker -t "esp-sensor-hub/+/events" -v -C 5
   ```

6. **Re-enable firewall** (if disabled):
   ```powershell
   Set-NetFirewallProfile -Profile Private -Enabled True
   ```

### Firmware Version Tracking

**All devices include automatic firmware version tracking** for deployment management and OTA verification.

#### Version Format
```
MAJOR.MINOR.PATCH-buildYYYYMMDD
Example: 1.0.3-build20251222
```

#### MQTT Version Fields
All MQTT messages include `firmware_version`:
```json
{
  "device": "Temp Sensor",
  "firmware_version": "1.0.3-build20251222",
  "current_temp_c": 23.5,
  "event": "ota_start"
}
```

#### Version Update Process
```bash
# Update build timestamp before deployment
cd temperature-sensor
./update_version.sh

# Build and upload
pio run -e esp32dev -t upload
```

#### OTA Version Tracking
- **Before OTA**: Device reports current version
- **OTA Start**: Publishes `ota_start` event with current version  
- **OTA Complete**: Publishes `ota_complete` event with new version
- **After Reboot**: All messages show updated version

#### Manual Version Bumps
For major/minor/patch changes, edit `platformio.ini`:
```ini
-D FIRMWARE_VERSION_PATCH=3  # Increment for bug fixes
```

### Compilation Errors
1. Ensure `temperature-sensor/include/secrets.h` exists
2. Copy from `temperature-sensor/include/secrets.h.example` if needed
3. Verify MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD are defined
4. Check PlatformIO environment matches board type (esp8266 vs esp32dev)

### Build Verification Steps
**Before deploying firmware, verify build configuration**:

1. **Check MQTT buffer sizes** (critical for ESP32):
   ```bash
   grep "MQTT_MAX_PACKET_SIZE" temperature-sensor/platformio.ini
   # Should show: -D MQTT_MAX_PACKET_SIZE=2048 for ESP32, =512 for ESP8266
   ```

2. **Verify firmware version**:
   ```bash
   cd temperature-sensor && ./update_version.sh  # Updates build timestamp
   grep "BUILD_TIMESTAMP" platformio.ini
   ```

3. **Check upload configuration**:
   ```bash
   grep "upload_port" temperature-sensor/platformio.ini
   # Should match target device IP for OTA uploads
   ```

4. **Test compilation**:
   ```bash
   pio run -e esp32dev  # Or esp8266
   # Should complete without errors
   ```

### Device Identification and IP Tracking
**Track device IPs and chip IDs for troubleshooting**:

1. **Monitor MQTT for device discovery**:
   ```bash
   mosquitto_sub -h your.mqtt.broker.com -t "esp-sensor-hub/+/status" -v -C 10
   # Shows all active devices with chip_id and device names
   ```

2. **Check device health endpoints**:
   ```bash
   curl http://device-ip/health | jq '.device, .chip_id, .firmware_version'
   ```

3. **Update DEVICE_INVENTORY.md** after deployments:
   - Record IP addresses for OTA access
   - Track chip IDs for device identification
   - Note firmware versions and update dates

4. **Network scanning** for unknown devices:
   ```bash
   nmap -sn 192.168.0.0/24 | grep "ESP"
   # Find devices on network
   ```

## ESP8266 API-Only Configuration

**ESP8266 temperature sensors can be configured for API-only operation** - no HTML web interface, optimized for USB-powered headless deployment.

### Configuration
ESP8266 environment automatically enables API-only mode:
```ini
[env:esp8266]
build_flags =
  -D API_ENDPOINTS_ONLY    # Disables HTML interface
  -D OLED_ENABLED=0        # No display support
  -D BATTERY_POWERED       # USB power optimization
```

### Available Endpoints
When `API_ENDPOINTS_ONLY` is defined, only these API endpoints are available:

- **GET `/temperaturec`** - Current temperature in Celsius (plain text)
- **GET `/temperaturef`** - Current temperature in Fahrenheit (plain text)  
- **GET `/health`** - Device health status (JSON)

**HTML interface (`/`) is disabled** - returns 404 Not Found

### Use Cases
- **Headless sensors**: USB-powered ESP8266 devices without displays
- **API integration**: Direct machine-to-machine communication
- **Resource optimization**: Reduced flash usage, faster boot
- **Security**: No web interface reduces attack surface

### Deployment
```bash
# Build ESP8266 API-only firmware
pio run -e esp8266

# Flash to device
pio run -e esp8266 -t upload --upload-port /dev/ttyUSB0
```

### MQTT Operation
ESP8266 devices operate identically to ESP32:
- Publish temperature readings every 30 seconds
- Publish status updates with device health
- Include firmware version in all messages
- Support OTA updates via MQTT commands

---

**Key Points**:
- ✅ Only MQTT broker details need compile-time configuration
- ✅ WiFi configured via WiFiManager captive portal (no hardcoded credentials)
- ✅ Device names set via portal, published in MQTT `device` field
- ✅ All data publishing visible in serial logs for debugging