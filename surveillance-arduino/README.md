# ESP32-S3 Surveillance - Arduino CLI Build

Build system for ESP32-S3 Surveillance sketch using Arduino CLI. Supports WSL2, Linux, and CI/CD.

**Primary Board:** Freenove ESP32-S3 WROOM (OV3660)  
**Legacy Support:** AI Thinker ESP32-CAM (OV2640)

---

## 🚀 Quick Start

### Compile for ESP32-S3 (Recommended)
```bash
./COMPILE_ESP32S3.sh
```

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
#define MQTT_SERVER "192.168.1.100"
#define MQTT_USER "username"
#define MQTT_PASSWORD "password"
```

### 2. Compile

```bash
./COMPILE_ESP32S3.sh  # ESP32-S3
# OR
./COMPILE_ESP32CAM.sh  # ESP32-CAM
```

### 3. Upload

**Linux/Native:**
```bash
./bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3:PSRAM=enabled,PartitionScheme=huge_app,FlashMode=qio ESP32CAM_Surveillance
```

**WSL2:** Requires USB/IP setup. See [ARDUINO_CLI_SETUP.md](ARDUINO_CLI_SETUP.md) for detailed instructions.

### 4. Monitor Serial Output

```bash
./bin/arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

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
