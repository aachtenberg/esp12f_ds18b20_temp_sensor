# Surveillance Camera Configuration

## Overview

ESP32-S3 camera firmware for motion detection and video streaming. See [surveillance/README.md](../../surveillance/README.md) for hardware setup.

## Required Files

### secrets.h Setup
Create `surveillance/include/secrets.h`:
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

### Using Arduino CLI (Recommended for ESP32-S3)

**Location**: `surveillance-arduino/` directory

**⚠️ CRITICAL: Partition Table Alignment**
The compile, firmware upload, and LittleFS upload must all use the **same partition scheme**. The standard workflow ensures this:

```bash
cd surveillance-arduino

# Step 1: Compile firmware (generates huge_app partition scheme with SPIFFS at 0x310000)
./COMPILE_ESP32S3.sh

# Step 2: Upload firmware to device
./bin/arduino-cli upload -p /dev/ttyACM0 \
  --fqbn esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=qio \
  ESP32CAM_Surveillance

# Step 3: Deploy web interface to LittleFS (REQUIRED - uploads to 0x310000 to match firmware)
./UPLOAD_LITTLEFS.sh /dev/ttyACM0 s3

# Step 4: Monitor device boot
./bin/arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

⚠️ **Important**: 
- **Both firmware AND LittleFS uploads are required** for full functionality
- COMPILE_ESP32S3.sh uses `--output-dir` to generate huge_app partition (SPIFFS at 0x310000)
- UPLOAD_LITTLEFS.sh automatically uses 0x310000 address
- Do NOT mix partition schemes

### Using PlatformIO (Legacy ESP32-CAM only)

```bash
cd surveillance

# Build and upload firmware
pio run -e esp32cam --target upload --upload-port /dev/ttyUSB0

# Monitor serial output
pio device monitor -e esp32cam --baud 74880
```

### OTA Updates (After Initial Deployment)

```bash
# From surveillance-arduino/ directory
export PLATFORMIO_UPLOAD_FLAGS="--auth=YOUR_OTA_PASSWORD"
./bin/arduino-cli upload -p 192.168.0.XXX \
  --fqbn esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=qio \
  ESP32CAM_Surveillance

# Then update LittleFS if HTML changes
./UPLOAD_LITTLEFS.sh 192.168.0.XXX s3
```

## Configuration

### WiFiManager Portal
1. **Factory Reset**: Triple-tap reset button to enter config mode
2. Device creates AP: `{DEVICE_NAME}-Setup` (e.g., "Surveillance-Cam-Setup")
3. Connect to AP (no password required)
4. Open browser to `http://192.168.4.1` for captive portal
5. Configure WiFi network and device name
6. Device reboots and connects to your network

### Web Interface

**Access**: `http://{DEVICE_IP}/` after WiFi connection

**Features**:
- Live MJPEG stream with real-time bitrate
- **Camera Settings**:
  - Resolution (VGA, SVGA, XGA, HD)
  - Quality (JPEG compression)
  - Brightness, contrast, saturation
  - **AWB (Auto White Balance)** - toggle
  - **AEC (Auto Exposure Control)** - toggle
  - Special effects (none, negative, grayscale, sepia, etc.)
- **Motion Detection**: Enable/disable with real-time threshold tuning
- **SD Card Recording**: Automatic capture on motion (if SD card present)
- **SFTP Upload**: Upload motion captures to remote server
- **Flashlight**: Manual and motion-indicator modes (ESP32-CAM only)

### Camera Settings
- **Resolution**: Configured via web interface
- **Motion Detection**: Enabled by default, configurable via web UI
- **AWB**: Enabled by default for automatic white balance
- **AEC**: Enabled by default for automatic exposure

## Troubleshooting

### Web Interface Not Loading
- Verify LittleFS was uploaded: `./UPLOAD_LITTLEFS.sh /dev/ttyACM0 s3`
- Check device IP address in serial monitor
- Clear browser cache: `Ctrl+Shift+Del` and retry
- Try accessing `/` directly if other paths fail

### LittleFS Upload Fails
- Verify Arduino CLI core is installed: `./bin/arduino-cli core list | grep esp32`
- Check mklittlefs exists: `ls ~/.arduino15/packages/esp32/tools/mklittlefs/*/`
- Device must be flashed with firmware before uploading LittleFS
- Ensure `ESP32CAM_Surveillance/data/index.html.gz` exists
- Try different baud rate: Edit `UPLOAD_LITTLEFS.sh` (ESP32-S3 = 921600, ESP32-CAM = 460800)
- **CRITICAL**: Verify partition table alignment:
  - COMPILE_ESP32S3.sh must use `--output-dir` (generates huge_app with SPIFFS at 0x310000)
  - UPLOAD_LITTLEFS.sh must upload to 0x310000 (not 0x290000)
  - If device shows `[SETUP] /index.html.gz NOT FOUND!`, partition mismatch is likely cause

### Camera Not Initializing
- Check camera ribbon cable connection
- Verify correct board selected (Freenove ESP32-S3 vs AI Thinker ESP32-CAM)
- Look for camera init errors in serial monitor
- Try re-flashing firmware and LittleFS

### Motion Detection Not Working
- Enable via web interface: Settings → Motion Detection toggle
- Check serial output for motion detection logs
- Verify camera is properly initialized first

### WiFi Connection Issues
- Triple-tap reset to re-enter WiFiManager portal
- Verify WiFi SSID is visible and password is correct
- Check if 2.4GHz WiFi is available (5GHz not supported)
- Restart WiFi router and try again
````
- SD card recording: Optional

## MQTT Topics

| Topic | Payload | Purpose |
|-------|---------|---------|
| `surveillance/DEVICE/motion` | `{detected, timestamp, ...}` | Motion events |
| `surveillance/DEVICE/status` | `{uptime, free_heap, ...}` | Device status |
| `surveillance/DEVICE/command` | `snapshot`, `start_recording`, `stop_recording` | Camera control |

## Troubleshooting

### Camera Not Initializing
1. Check camera ribbon cable connection
2. Verify power supply (5V 2A minimum)
3. Check serial output for camera init errors

### Motion Detection Not Working
1. Verify motion detection enabled in settings
2. Check sensitivity threshold
3. Monitor MQTT for motion events

### SD Card Issues
1. Format SD card as FAT32
2. Ensure card is properly seated
3. Check free space available

## Web Interface

Access camera web interface at: `http://DEVICE_IP/`

Features:
- Live stream view
- Camera settings
- Motion detection configuration
- SD card management
