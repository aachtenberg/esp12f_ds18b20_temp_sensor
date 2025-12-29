# ESP32-S3 Surveillance - Arduino CLI Build

Build system for ESP32-S3 Surveillance sketch using Arduino CLI. Supports WSL2, Linux, and CI/CD.

**Primary Board:** Freenove ESP32-S3 WROOM (OV3660)  
**Legacy Support:** AI Thinker ESP32-CAM (OV2640)

---

## 🚀 Quick Start

**⚠️ CRITICAL: Firmware and LittleFS must use matching partition tables!**

The standard workflow ensures this by design:
1. `./COMPILE_ESP32S3.sh` → generates huge_app partitions (SPIFFS at 0x310000)
2. `./bin/arduino-cli upload ...` → flashes firmware with huge_app partitions
3. `./UPLOAD_LITTLEFS.sh` → uploads to 0x310000 (matches firmware)

### Compile for ESP32-S3 (Recommended)
```bash
./COMPILE_ESP32S3.sh
```

This script uses `--output-dir ESP32CAM_Surveillance/build` to generate the **huge_app partition scheme** where SPIFFS is at **0x310000 (896KB)**.

### Compile for ESP32-CAM (Legacy)
```bash
./COMPILE_ESP32CAM.sh
```

### Compile for ESP32 DevKit (Testing - no camera)
```bash
./bin/arduino-cli compile --fqbn esp32:esp32:esp32 ESP32CAM_Surveillance
```

---

## ⚙️ Setup

### 1. Configure Credentials

Edit [ESP32CAM_Surveillance/secrets.h](ESP32CAM_Surveillance/secrets.h):

```cpp
#define WIFI_SSID "YourWiFi"
#define WIFI_PASSWORD "YourPassword"
#define MQTT_SERVER "YOUR_MQTT_BROKER_IP"
#define MQTT_USER "username"
#define MQTT_PASSWORD "password"
```

### 2. Compile

```bash
./COMPILE_ESP32S3.sh  # ESP32-S3
# OR
./COMPILE_ESP32CAM.sh  # ESP32-CAM
```

### 3. Upload Firmware

**Linux/Native:**
```bash
./bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=qio ESP32CAM_Surveillance
```

**WSL2:** Requires USB/IP setup. See [ARDUINO_CLI_SETUP.md](ARDUINO_CLI_SETUP.md) for detailed instructions.

### 4. Upload Web Interface (LittleFS)

**Required:** Upload the gzipped web interface to the device's LittleFS partition.

```bash
./UPLOAD_LITTLEFS.sh /dev/ttyACM0 s3      # ESP32-S3
# OR
./UPLOAD_LITTLEFS.sh /dev/ttyUSB0 esp32   # ESP32-CAM
```

⚠️ **Important:** This step deploys:
- Dark-themed web interface (HTML/CSS)
- All camera settings panel (resolution, quality, AWB, AEC, motion detection, etc.)
- Gzipped for flash efficiency (6.7KB vs 27KB uncompressed)

**What it does:**
- Creates LittleFS filesystem image from `data/` folder
- Flashes to partition at address **0x310000** (896KB) - MATCHES huge_app partition from firmware
- Device will serve `index.html.gz` on next boot
- Must use same partition scheme as compiled firmware

### 5. Monitor Serial Output

```bash
./bin/arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

---

## 📋 Deployment Workflow

**Complete setup sequence:**
1. Edit `ESP32CAM_Surveillance/secrets.h` with WiFi/MQTT credentials
2. Run `./COMPILE_ESP32S3.sh` to build firmware
3. Run `./bin/arduino-cli upload -p /dev/ttyACM0 ...` to flash firmware
4. Run `./UPLOAD_LITTLEFS.sh /dev/ttyACM0 s3` to deploy web interface
5. Power cycle or reset device
6. Access web interface at `http://DEVICE_IP`

Both firmware and LittleFS uploads are required for full functionality.

---

## 📋 Board Support

| Board | Camera | PSRAM | SD Card | Status |
|-------|--------|-------|---------|--------|
| **ESP32-S3** | OV3660 | 8MB | 1-bit mode | ✅ Primary |
| ESP32-CAM | OV2640 | None | 2-bit mode | ✅ Legacy |
| ESP32 DevKit | None | Varies | N/A | ✅ Testing |

---

## 📁 Key Files

- **ESP32CAM_Surveillance.ino** - Main sketch with board selection comments
- **camera_config.cpp/h** - Camera initialization
- **device_config.h** - Device settings
- **secrets.h** - WiFi/MQTT credentials (edit this!)
- **COMPILE_ESP32S3.sh** - ESP32-S3 build script
- **COMPILE_ESP32CAM.sh** - ESP32-CAM build script
- **ARDUINO_CLI_SETUP.md** - WSL2 USB/IP configuration

---

## 🔌 Pin Assignments

### ESP32-S3 (Freenove WROOM)
- **SD Card:** CLK=39, CMD=38, D0=40 (1-bit mode)
- **Camera:** OV3660, pins 4-7, 8-13, 15-18 (auto-configured)
- **Status LED:** GPIO 2
- **Flash LED:** Not available on this board

### ESP32-CAM (AI Thinker)
- **SD Card:** Uses 2-bit default mode
- **Camera:** OV2640, pins 0, 5, 18-23, 25-27, 32-39 (auto-configured)
- **Status LED:** GPIO 2
- **Flash LED:** GPIO 4 (built-in)

---

## 🛠️ Troubleshooting

**SD card not mounting?**
- Check macro definition in sketch (ESP32-S3 requires `ARDUINO_FREENOVE_ESP32_S3_WROOM`)
- Verify pin assignments match your board
- Check serial output for SD card errors

**Arduino CLI not found?**
```bash
chmod +x ./bin/arduino-cli
```

**Port not detected?**
- Linux: Check `/dev/ttyACM*` or `/dev/ttyUSB*`
- WSL2: See [ARDUINO_CLI_SETUP.md](ARDUINO_CLI_SETUP.md) for USB/IP setup

**LittleFS upload fails or file not found on device?**
- Check device is connected: `./bin/arduino-cli board list`
- Verify mklittlefs exists: `ls ~/.arduino15/packages/esp32/tools/mklittlefs/*/`
- Ensure `ESP32CAM_Surveillance/data/index.html.gz` exists in sketch directory
- Device must be flashed with firmware before uploading LittleFS
- **CRITICAL**: Ensure firmware and LittleFS use matching partition tables:
  - Firmware compiled with: `./COMPILE_ESP32S3.sh` (uses --output-dir, huge_app scheme, SPIFFS at 0x310000)
  - LittleFS uploaded to: `0x310000` (via UPLOAD_LITTLEFS.sh which reads from build output)
  - Both must match! If partition mismatch, LittleFS will be empty on device

**Serial monitor shows garbage?**
- Ensure baud rate is 115200
- Check USB driver installation

---

## 📚 Further Reading

- [ARDUINO_CLI_SETUP.md](ARDUINO_CLI_SETUP.md) - WSL2 USB/IP, detailed setup
- [ESP32CAM_Surveillance/README.md](ESP32CAM_Surveillance/README.md) - Feature documentation
- Sketch header comments - Board selection and compilation instructions

---

**All build systems produce identical firmware.** Choose based on your environment.
