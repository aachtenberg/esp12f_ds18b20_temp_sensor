# Arduino CLI Setup for WSL2 - ESP32-CAM Surveillance

## ✅ Setup Complete!

The ESP32-CAM Surveillance project is now fully configured for Arduino CLI compilation in WSL2.

### Installation Summary

**Arduino CLI**: v1.4.0
**Location**: `./bin/arduino-cli`
**ESP32 Core**: v3.3.3
**Target Board**: AI Thinker ESP32-CAM (`esp32:esp32:esp32cam`)

### Installed Libraries

| Library | Version | Source |
|---------|---------|--------|
| WiFiManager | 2.0.17 | Arduino Library Manager |
| ArduinoJson | 7.4.2 | Arduino Library Manager |
| PubSubClient | 2.8.0 | Arduino Library Manager |
| ESPAsyncWebServer | 3.6.0 | GitHub (cloned manually) |
| AsyncTCP | 3.3.2 | GitHub (cloned manually) |

Built-in ESP32 libraries (automatically included):
- WiFi, Update, WebServer, FS, DNSServer, AsyncUDP
- ArduinoOTA, LittleFS, SD_MMC, Preferences
- Hash, ESPmDNS

### Compilation Results

```
✅ Compilation Successful!

Sketch uses:     1,420,855 bytes (45%) of program storage
Global variables:   64,924 bytes (19%) of dynamic memory
Remaining RAM:     262,756 bytes for local variables

Maximum flash:   3,145,728 bytes
Maximum RAM:       327,680 bytes
```

### Project Files

```
ESP32CAM_Surveillance/
├── ESP32CAM_Surveillance.ino  # Main sketch (2500 lines)
├── camera_config.h             # Camera pin definitions
├── camera_config.cpp           # Camera initialization
├── device_config.h             # Device settings
├── secrets.h                   # WiFi/MQTT credentials
├── trace.h                     # Distributed tracing
├── trace.cpp                   # Trace implementation
└── README.md                   # User documentation
```

### Compilation Commands

#### For AI Thinker ESP32-CAM (OV2640)

**Compile only:**
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance
```

**Compile with verification:**
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance --verify
```

**Upload to device (requires USB connection):**
```bash
./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance
```

**Compile with verbose output:**
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance -v
```

#### For Freenove ESP32-S3 WROOM (OV3660)

If you have the Freenove ESP32-S3 with camera (your current hardware!):

**Compile for ESP32-S3:**
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app ESP32CAM_Surveillance
```

**Upload to ESP32-S3:**
```bash
./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app ESP32CAM_Surveillance
```

**Note:** The `PartitionScheme=huge_app` parameter is **required** for ESP32-S3 to allocate enough flash space (3MB) for the application.

**Note:** The code auto-detects the board:
- Checks for `ARDUINO_FREENOVE_ESP32_S3_WROOM` or `ARDUINO_ESP32S3_DEV`
- Uses appropriate camera pins and settings
- OV3660 camera with 8MB PSRAM
- See [camera_config.h](ESP32CAM_Surveillance/camera_config.h) for pin definitions

#### For Generic ESP32 Dev Module (Testing/Development)

If you don't have the ESP32-CAM hardware yet and want to test compilation on a standard ESP32 DevKit:

**Compile for ESP32 Dev Module:**
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32 ESP32CAM_Surveillance
```

**Upload to ESP32 DevKit:**
```bash
./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 ESP32CAM_Surveillance
```

**Note:** When using a standard ESP32 DevKit without camera hardware:
- The code will compile successfully
- Camera initialization will fail at runtime (expected)
- Web server and MQTT features will still work
- Motion detection will be disabled (no camera sensor)
- Use this for testing WiFi, MQTT, and web interface logic

#### Other Supported ESP32 Boards

**DOIT ESP32 DEVKIT V1:**
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 ESP32CAM_Surveillance
```

**ESP32 Wrover Module:**
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32wrover ESP32CAM_Surveillance
```

**NodeMCU-32S:**
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:nodemcu-32s ESP32CAM_Surveillance
```

To see all available ESP32 boards:
```bash
./bin/arduino-cli board listall | grep esp32
```

### WSL2 USB Access (for uploading firmware)

Since WSL2 doesn't natively support USB, you have two options:

#### Option 1: USB/IP (Recommended for WSL2)

**On Windows (PowerShell as Administrator):**
```powershell
# Install usbipd-win from: https://github.com/dorssel/usbipd-win/releases

# List USB devices
usbipd list

# Bind the ESP32-CAM device (one-time setup)
usbipd bind --busid <BUSID>

# Attach to WSL2 (every time you want to upload)
usbipd attach --wsl --busid <BUSID>
```

**In WSL2:**
```bash
# Install USB/IP tools
sudo apt install linux-tools-generic hwdata
sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*-generic/usbip 20

# Verify device is attached
ls /dev/ttyUSB*

# Upload firmware
./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance
```

#### Option 2: Compile in WSL2, Upload from Windows

1. Compile in WSL2 as shown above
2. Export the compiled binary:
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance --export-binaries
```
3. Find the .bin file in the build output directory
4. Use esptool.py on Windows to upload:
```cmd
esptool.py --port COM3 write_flash 0x10000 firmware.bin
```

### Board Configuration

#### AI Thinker ESP32-CAM (Production - OV2640)

The compilation uses these settings (matching PlatformIO):

- **Board**: AI Thinker ESP32-CAM (`esp32:esp32:esp32cam`)
- **Partition Scheme**: Huge APP (3MB No OTA/1MB SPIFFS)
- **PSRAM**: Enabled (required for camera)
- **CPU Frequency**: 240MHz
- **Flash Frequency**: 80MHz
- **Upload Speed**: 921600 baud
- **Camera Model**: OV2640 (built-in)
- **Flash LED**: GPIO 4

#### Freenove ESP32-S3 WROOM (Production - OV3660)

For your current hardware:

- **Board**: ESP32-S3 Dev Module (`esp32:esp32:esp32s3`)
- **Partition Scheme**: Huge APP (compatible)
- **PSRAM**: 8MB Octal SPI (auto-configured)
- **CPU Frequency**: 240MHz
- **Upload Speed**: 921600 baud
- **Camera Model**: OV3660 (higher resolution than OV2640)
- **Flash LED**: None (no GPIO 4 on S3)
- **SD Card**: 4-bit mode (better performance)
- **Reset Detection**: NVS-based (survives hardware resets)

#### ESP32 DevKit (Testing/Development)

For testing without camera hardware:

- **Board**: ESP32 Dev Module (`esp32:esp32:esp32`)
- **Partition Scheme**: Default (can use standard partition)
- **PSRAM**: Optional (enables if available)
- **CPU Frequency**: 240MHz
- **Upload Speed**: 921600 baud
- **Camera Model**: None (camera init will fail gracefully)
- **Flash LED**: Not available

**What works on ESP32 DevKit:**
- ✅ WiFi connectivity and WiFiManager
- ✅ MQTT client and telemetry
- ✅ Web server and REST API
- ✅ OTA updates
- ✅ LittleFS filesystem
- ❌ Camera capture (no hardware)
- ❌ Motion detection (no camera)
- ❌ SD card (no SD_MMC pins)

**Use cases for ESP32 DevKit:**
- Development and testing of WiFi/MQTT logic
- Web interface development
- API endpoint testing
- Quick prototyping before deploying to ESP32-CAM

### Fixes Applied During Setup

1. **Added `#include <Arduino.h>`** to `camera_config.cpp` and `trace.cpp`
   - Arduino CLI requires explicit Arduino.h inclusion for Serial, delay(), etc.

2. **Completed missing code** in ESP32CAM_Surveillance.ino
   - Original conversion from PlatformIO was incomplete (stopped at line 1182)
   - Appended remaining 1300+ lines with MQTT, web server, and handler functions

### Next Steps

1. **Configure secrets.h** with your WiFi and MQTT credentials
2. **Test compilation** works in your environment
3. **Set up USB/IP** if you want to upload from WSL2
4. **Upload firmware** to ESP32-CAM
5. **Monitor serial output** to verify boot

### Useful Arduino CLI Commands

```bash
# List all installed boards
./bin/arduino-cli board listall | grep esp32

# List connected boards
./bin/arduino-cli board list

# Install additional libraries
./bin/arduino-cli lib install <library-name>

# Search for libraries
./bin/arduino-cli lib search <keyword>

# Update package index
./bin/arduino-cli core update-index

# Monitor serial output
./bin/arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### Troubleshooting

**Error: "psramFound was not declared"**
- Fixed by adding `#include <Arduino.h>` to .cpp files

**Error: "undefined reference to function"**
- Fixed by completing the .ino file with all function implementations

**USB device not found in WSL2:**
- Install and configure USB/IP as described above
- Make sure device is attached with `usbipd attach --wsl`

**Compilation errors about missing libraries:**
- Ensure AsyncTCP and ESPAsyncWebServer are cloned to ~/Arduino/libraries/

---

**Date**: 2025-12-20
**Arduino CLI Version**: 1.4.0
**ESP32 Core**: 3.3.3
**Status**: ✅ Ready for deployment
