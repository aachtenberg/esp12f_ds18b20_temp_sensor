# ESP32-S3 Surveillance - Build System Guide

This project supports **three independent build systems** for the Freenove ESP32-S3 WROOM board and other ESP32 variants.

## Quick Reference

| Build System | Use Case | Platform | Status |
|--------------|----------|----------|--------|
| **PlatformIO** | Production, development, debugging | Windows, macOS, Linux | ✅ Primary |
| **Arduino IDE** | Community, beginners, manual builds | Windows, macOS, Linux | ✅ Supported |
| **Arduino CLI** | WSL2, Linux, CI/CD, automation | Linux, WSL2, macOS | ✅ Supported |

---

## 1. PlatformIO (Recommended for Development)

**Best for:** Professional development, debugging, team projects

### Setup
```bash
# Install PlatformIO Core
pip install platformio

# Build
cd surveillance
pio run

# Upload
pio run --target upload

# Monitor
pio device monitor
```

### Features
- ✅ Automatic dependency management
- ✅ Built-in debugger support
- ✅ Multi-board configuration
- ✅ VSCode integration
- ✅ Unit testing framework

### Configuration
See [surveillance/platformio.ini](../surveillance/platformio.ini)

---

## 2. Arduino IDE (Easiest for Beginners)

**Best for:** Quick testing, community sharing, simple uploads

### Setup
1. Install Arduino IDE 2.x from [arduino.cc](https://www.arduino.cc/en/software)
2. Add ESP32 board support:
   - Open **File → Preferences**
   - Add to **Additional Boards Manager URLs**:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
3. Install **esp32 by Espressif Systems** (version 2.0.11+)
4. Install libraries via **Tools → Manage Libraries**:
   - WiFiManager (2.0.17)
   - ArduinoJson (7.x)
   - PubSubClient (2.8.0)
5. Manually install from GitHub:
   - [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
   - [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)

### Board Configuration (ESP32-S3)
- **Board**: ESP32-S3 Dev Module
- **Partition Scheme**: Huge APP (3MB No OTA/1MB SPIFFS)
- **PSRAM**: Enabled (PSRAM V0)
- **Flash Mode**: QIO
- **Upload Speed**: 921600

### Build
1. Open `ESP32CAM_Surveillance.ino`
2. Configure `secrets.h` with your credentials
3. Click **Upload** (requires FTDI adapter)

### Features
- ✅ Simple graphical interface
- ✅ Built-in serial monitor
- ✅ Easy library management
- ✅ Wide community support

### Documentation
See [ESP32CAM_Surveillance/README.md](ESP32CAM_Surveillance/README.md)

---

## 3. Arduino CLI (Best for WSL2/Linux/CI)

**Best for:** Linux development, WSL2, CI/CD pipelines, automation

### Setup
```bash
# Arduino CLI is already installed in ./bin/
cd surveillance-arduino

# Verify installation
./bin/arduino-cli version

# Libraries are pre-installed, but to reinstall:
./bin/arduino-cli lib install WiFiManager ArduinoJson PubSubClient
```

### Build Commands

**For ESP32-S3 WROOM (Primary):**
```bash
# Quick compile with correct flags
./COMPILE_ESP32S3.sh

# Manual compile
./bin/arduino-cli compile --fqbn esp32:esp32:esp32s3 -e \
  -S compiler.cpp.extra_flags="-DARDUINO_FREENOVE_ESP32_S3_WROOM" \
  ESP32CAM_Surveillance
```

**For ESP32 DevKit (Testing - no camera):**
```bash
# Manual compile
./bin/arduino-cli compile --fqbn esp32:esp32:esp32 ESP32CAM_Surveillance
```

**For ESP32-CAM (Legacy):**
```bash
# Manual compile
./bin/arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance
```

### Upload
```bash
# For Linux/Native Linux:
./bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3 ESP32CAM_Surveillance

# For WSL2 (requires USB/IP - see ARDUINO_CLI_SETUP.md):
./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3 ESP32CAM_Surveillance
```

### Features
- ✅ Command-line automation
- ✅ CI/CD integration ready
- ✅ Scriptable builds
- ✅ Works in WSL2/Linux
- ✅ No GUI required

### Documentation
See [ARDUINO_CLI_SETUP.md](ARDUINO_CLI_SETUP.md)

---

## Build Output Comparison

### Binary Sizes

| Build System | Target | Binary Size | Flash Usage | RAM Usage |
|--------------|--------|-------------|-------------|-----------|
| Arduino CLI | **ESP32-S3** | 1,372,503 bytes | **43%** | ~63KB |
| Arduino CLI | ESP32-CAM | 1,420,855 bytes | 45% | 64,924 bytes |
| Arduino CLI | ESP32 DevKit | 1,420,855 bytes | 45% | 64,924 bytes |

**Note:** PlatformIO produces smaller binaries due to more aggressive optimization.

---

## Testing on Other Boards

### ESP32 DevKit (Without Camera)
All three build systems support compilation for ESP32 DevKit for development without camera hardware:

### What Works
- ✅ WiFi connectivity
- ✅ WiFiManager configuration portal
- ✅ MQTT client and telemetry
- ✅ Web server and REST API
- ✅ OTA updates
- ✅ LittleFS filesystem

### What Doesn't Work
- ❌ Camera initialization (fails gracefully)
- ❌ Motion detection (no sensor)
- ❌ SD card storage (different pins, no SD card slot)
- ❌ Flash LED (GPIO configuration different)

### Use Cases
- Develop WiFi/MQTT logic without camera
- Test web interface on cheaper hardware
- Rapid prototyping of new features
- Debugging network issues

---

## Choosing a Build System

### Use PlatformIO if you:
- Want professional development tools
- Need debugging capabilities
- Work on a team
- Want automated testing
- Prefer VSCode integration

### Use Arduino IDE if you:
- Are new to ESP32 development
- Want a simple graphical interface
- Need quick one-off builds
- Share code with the community
- Don't need advanced features

### Use Arduino CLI if you:
- Work in WSL2 or Linux
- Need CI/CD integration
- Want scriptable builds
- Prefer command-line tools
- Build automation scripts

---

## Migration Between Systems

### PlatformIO → Arduino IDE
1. Copy files from `surveillance-arduino/ESP32CAM_Surveillance/`
2. Install libraries manually
3. Configure board settings
4. Upload via Arduino IDE

### PlatformIO → Arduino CLI
1. Files already prepared in `surveillance-arduino/`
2. Run `./COMPILE.sh`
3. Upload via `arduino-cli upload`

### Arduino IDE → PlatformIO
1. Use existing `surveillance/` directory
2. Run `pio run`

---

## CI/CD Integration

Arduino CLI is perfect for automated builds:

```yaml
# Example GitHub Actions
name: Build Firmware
on: [push]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install Arduino CLI
        run: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
      - name: Install ESP32 core
        run: arduino-cli core install esp32:esp32
      - name: Compile
        run: arduino-cli compile --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance
```

---

## Support & Documentation

- **Arduino IDE**: See [ESP32CAM_Surveillance/README.md](ESP32CAM_Surveillance/README.md)
- **Arduino CLI**: See [ARDUINO_CLI_SETUP.md](ARDUINO_CLI_SETUP.md)
- **Hardware Setup**: See [../docs/hardware/](../docs/hardware/) for board pinouts and SD card configuration
- **Build Scripts**: See [COMPILE_ESP32S3.sh](COMPILE_ESP32S3.sh) for ESP32-S3 specific build flags

---

**All three build systems produce functionally identical firmware for ESP32-S3!** Choose based on your workflow preferences and development environment. For ESP32-CAM or other boards, adjust the `--fqbn` parameter accordingly.
