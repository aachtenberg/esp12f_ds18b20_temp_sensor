# Build & Flash Scripts

Helper scripts for building and flashing ESP devices.

## MQTT Device Control

### Unified Device Control Script

**`mqtt_device_control.sh`** - Comprehensive MQTT command interface for all ESP32 devices:

```bash
cd /home/aachten/PlatformIO/esp12f_ds18b20_temp_sensor/scripts

# Enable deep sleep (30 second interval)
./mqtt_device_control.sh deepsleep 30

# Disable deep sleep (continuous operation)
./mqtt_device_control.sh disable-sleep

# Enable deep sleep (shortcut)
./mqtt_device_control.sh enable-sleep 30

# Request device status
./mqtt_device_control.sh status

# Restart device
./mqtt_device_control.sh restart

# Monitor all device topics (temperature, status, events)
./mqtt_device_control.sh monitor

# Control different device
./mqtt_device_control.sh -d Greenhouse status

# Use different MQTT broker
./mqtt_device_control.sh -b 192.168.1.100 deepsleep 30
```

**Features:**
- Automatic retry with configurable intervals (catches device wake windows)
- Visual confirmation of command success
- Colored output for easy reading
- Support for all MQTT commands
- Environment variable configuration (`MQTT_BROKER`, `DEVICE_NAME`)
- Multi-topic monitoring

**Available Commands:**
- `deepsleep <seconds>` - Configure deep sleep (0-3600 seconds, 0=disable)
- `disable-sleep` - Disable deep sleep (shortcut)
- `enable-sleep <seconds>` - Enable deep sleep (shortcut)
- `status` - Request device status update
- `restart` - Restart the device
- `monitor` - Monitor all device topics in real-time

**Options:**
- `-b, --broker <ip>` - MQTT broker IP
- `-d, --device <name>` - Device name
- `-r, --retry <count>` - Maximum retry attempts (default: 15)
- `-i, --interval <sec>` - Retry interval (default: 2)

### Legacy Scripts

**`disable_sleep_mqtt.sh`** - Original deep sleep disable script (still functional)
**`monitor_sleep_cycle.sh`** - Monitor device wake/sleep cycles via HTTP health endpoint

## Device Inventory Database Update

### Update PostgreSQL Database

Updates device information from `DEVICE_INVENTORY.md` to PostgreSQL database on raspberrypi2:

```bash
cd /home/aachten/PlatformIO/esp12f_ds18b20_temp_sensor/scripts

# Install dependencies (once)
pip3 install psycopg2-binary

# Update database with device info
python3 update_devices.py
```

**What it does:**
- Reads `../DEVICE_INVENTORY.md`
- Parses all device information
- Updates PostgreSQL database on raspberrypi2
- Creates `devices` and `device_groups` tables automatically

**Database location:** raspberrypi2 (YOUR_DB_HOST), database: `camera_db`

## Build Systems

### PlatformIO (Temperature Sensor & Solar Monitor)

```bash
cd temperature-sensor
pio run -e esp8266 --target upload --upload-port /dev/ttyUSB0
pio run -e esp32dev --target upload --upload-port /dev/ttyUSB0

cd solar-monitor
pio run --target upload --upload-port /dev/ttyUSB0
```

### Arduino CLI (Surveillance Camera)

```bash
cd surveillance-arduino
./COMPILE.sh              # ESP32-CAM
./COMPILE_ESP32S3.sh      # ESP32-S3

# Upload
./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance
./bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3 ESP32CAM_Surveillance
```

## Device Configuration

All devices use **WiFiManager captive portal** for configuration:

1. Flash device using appropriate build system
2. Device creates AP (e.g., `ESP-Setup`)
3. Connect to AP and open captive portal
4. Configure WiFi credentials and device name
5. Settings persist in flash/NVS across reboots

## Board Support

| Board | Project | Port | Build System | FQBN | Build Flags |
|-------|---------|------|--------------|------|-------------|
| ESP8266 (NodeMCU) | Temperature Sensor | `/dev/ttyUSB*` | PlatformIO | `esp8266` | `OLED_ENABLED=0, BATTERY_POWERED, API_ENDPOINTS_ONLY, CPU_FREQ_MHZ=80, WIFI_PS_MODE=WIFI_LIGHT_SLEEP` |
| ESP32 (WROOM) | Temperature Sensor / Solar Monitor | `/dev/ttyUSB*` | PlatformIO | `esp32dev` | `OLED_ENABLED=1, API_ENDPOINTS_ONLY, CPU_FREQ_MHZ=80, WIFI_PS_MODE=WIFI_PS_MIN_MODEM` |
| ESP32-CAM (AI-Thinker) | Surveillance | `/dev/ttyUSB*` | Arduino CLI | `esp32:esp32:esp32cam` | Camera=1, Motion=1, Webserver=1, MQTT=1 |
| ESP32-S3 (Freenove) | Surveillance | `/dev/ttyACM*` | Arduino CLI | `esp32:esp32:esp32s3` | Camera=1, Motion=1, Webserver=1, MQTT=1 |

**PlatformIO Build Flags:**
- **ESP8266 Temperature:** `OLED_ENABLED=0, BATTERY_POWERED, API_ENDPOINTS_ONLY, CPU_FREQ_MHZ=80, WIFI_PS_MODE=WIFI_LIGHT_SLEEP`
- **ESP32 Temperature:** `OLED_ENABLED=1, API_ENDPOINTS_ONLY, CPU_FREQ_MHZ=80, WIFI_PS_MODE=WIFI_PS_MIN_MODEM`
- **ESP32-S3 Temperature:** `CPU_FREQ_MHZ=80, WIFI_PS_MODE=WIFI_PS_MIN_MODEM` (OLED auto-detected)
- **ESP32 Solar Monitor:** `CORE_DEBUG_LEVEL=0, ARDUINO_ESP32_DEV`

**Arduino CLI Build Flags:**
- Surveillance projects use Arduino IDE build system with camera libraries and motion detection enabled by default

## WSL2 USB Support

For Windows users with WSL2:

```powershell
# Windows PowerShell (as Administrator)
usbipd list
usbipd bind --busid 2-11
usbipd attach --wsl --busid 2-11
```

Then use the device normally in WSL2 terminal.