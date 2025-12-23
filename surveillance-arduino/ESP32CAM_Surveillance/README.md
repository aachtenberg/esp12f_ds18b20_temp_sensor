# ESP32-CAM Surveillance System - Arduino IDE Version

A modern, feature-rich ESP32-CAM surveillance system with camera-based motion detection, MQTT integration, and a dark glassmorphism web UI. Converted from PlatformIO for easy use with Arduino IDE.

## Features

- 📷 **Camera-based motion detection** (JPEG decode with pixel comparison)
- 🌐 **Modern web interface** with dark glassmorphism design
- 📡 **MQTT integration** for status publishing and remote control
- 💾 **SD card storage** for captured images
- 🔦 **Flash LED control** for illumination and motion indicator
- 📱 **Responsive UI** with mobile support
- 🔧 **Triple-reset recovery** to enter WiFi configuration portal
- 🔄 **Crash loop detection** and automatic recovery
- ⚙️ **Full camera control** (resolution, quality, brightness, contrast, etc.)

## Hardware Requirements

### Supported Boards
- **ESP32-CAM (AI-Thinker)** with OV2640 camera ✅ Recommended
- **ESP32-S3-CAM (Freenove)** with OV3660 camera

### Required Components
- ESP32-CAM module
- FTDI programmer (for initial upload)
- MicroSD card (optional, for image storage)
- 5V power supply (1A minimum)

### Pin Connections
- **GPIO 4**: Flash LED (AI-Thinker ESP32-CAM only)
- **GPIO 13**: PIR motion sensor (optional, currently disabled)
- **SD Card**: Uses SD_MMC in 1-bit mode (built-in slot)

## Arduino IDE Setup

### 1. Install ESP32 Board Support

1. Open **File → Preferences**
2. Add this URL to **Additional Boards Manager URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**
4. Search for "ESP32" and install **esp32 by Espressif Systems** (version 2.0.11 or newer recommended)

### 2. Install Required Libraries

Open **Tools → Manage Libraries** and install:

| Library | Version | Purpose |
|---------|---------|---------|
| WiFiManager | 2.0.17 | WiFi credential management |
| AsyncTCP | 1.1.1 | Asynchronous TCP for web server |
| ESPAsyncWebServer | 1.2.4 | Async web server |
| ArduinoJson | 7.x | JSON serialization |
| PubSubClient | 2.8.0 | MQTT communication |
| ESP_DoubleResetDetector | 1.3.2 | Triple-reset detection |

**Note**: Some libraries may need manual installation from GitHub:
- **AsyncTCP**: https://github.com/me-no-dev/AsyncTCP
- **ESPAsyncWebServer**: https://github.com/me-no-dev/ESPAsyncWebServer

### 3. Configure Board Settings

1. **Tools → Board**: Select **AI Thinker ESP32-CAM**
2. **Tools → Partition Scheme**: Select **Huge APP (3MB No OTA/1MB SPIFFS)**
3. **Tools → PSRAM**: **Enabled**
4. **Tools → Upload Speed**: **115200** (or 921600 if supported)
5. **Tools → CPU Frequency**: **240MHz**
6. **Tools → Flash Frequency**: **80MHz**
7. **Tools → Core Debug Level**: **None** (or "Info" for debugging)

### 4. Configure Secrets

1. Open `secrets.h` tab in Arduino IDE
2. Fill in your credentials:
   ```cpp
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASSWORD "YourWiFiPassword"
   #define MQTT_SERVER "YOUR_MQTT_BROKER_IP"  // Your MQTT broker IP
   #define MQTT_USER "mqtt_username"
   #define MQTT_PASSWORD "mqtt_password"
   ```
3. **Save the file**

### 5. Upload Firmware

#### First Upload (Using FTDI Programmer):
1. Connect FTDI to ESP32-CAM:
   - FTDI TX → ESP32 RX (GPIO 3)
   - FTDI RX → ESP32 TX (GPIO 1)
   - FTDI GND → ESP32 GND
   - FTDI 5V → ESP32 5V
2. **Connect GPIO 0 to GND** (boot mode)
3. Press **Reset** button on ESP32-CAM
4. Click **Upload** in Arduino IDE
5. After upload completes, **disconnect GPIO 0 from GND**
6. Press **Reset** again to start normally

#### Subsequent Uploads (OTA):
OTA is supported and can be enabled by setting a secure `OTA_PASSWORD` in `secrets.h`.

- Edit `secrets.h` and set `#define OTA_PASSWORD "YOUR_OTA_PASSWORD"` (replace with a strong password).
- The firmware will enable OTA at runtime only when `OTA_PASSWORD` is set to a non-default/secure value.
- When enabled, use `espota.py` or Arduino IDE network upload to perform OTA uploads (ensure your host can reach the device on port 3232 and firewall rules allow it).

> Note: For first-time uploads or recovery, continue to use the FTDI programmer as described above.

## First-Time Configuration

### WiFi Setup

On first boot, the device will create a WiFi access point:
- **AP Name**: `Cam-Surveillance-Cam-Setup`
- **IP Address**: 192.168.4.1

1. Connect to the AP with your phone/computer
2. Open browser to `http://192.168.4.1`
3. Select your WiFi network and enter password
4. **Optionally** set a custom device name
5. Click **Save** - device will reboot and connect

### Triple-Reset Recovery

If you need to reconfigure WiFi:
1. Press **Reset** button 3 times within 2 seconds
2. Device enters configuration portal
3. Connect to AP and reconfigure

## Web Interface

Once connected to WiFi, access the web interface at the device IP address (shown in Serial Monitor).

### Features:
- **Live MJPEG stream**
- **Single image capture**
- **Camera controls** (resolution, quality, brightness, etc.)
- **Motion detection** toggle
- **Flash LED** manual control
- **SD card management** (view usage, clear files, format)
- **Responsive design** (works on mobile)

## Serial Monitor Commands

Set baud rate to **115200** to view debug output:
- Boot reason (normal, triple-reset, crash recovery)
- WiFi connection status
- Camera initialization
- Motion detection events
- MQTT connection status

## Troubleshooting

### Camera Init Failed
```
Camera init failed with error 0x105
```
**Solution**: Check power supply (1A minimum), try lowering `xclk_freq_hz` in `camera_config.cpp`

### WiFi Connection Issues
- Use triple-reset to enter config portal
- Check SSID/password in `secrets.h`
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)

### Web Interface Not Loading
- Check device IP in Serial Monitor
- Clear browser cache (Ctrl+Shift+R)
- Ensure device and computer on same network

### SD Card Not Detected
- Format card as FAT32
- Ensure card is inserted before power-on
- Try different SD card (Class 10 recommended)

### MQTT Not Connecting
- Verify broker IP in `secrets.h`
- Check broker allows anonymous connections or set credentials
- Ensure broker port is 1883 (default)

### Brown-out Detector Reset
```
Brownout detector was triggered
```
**Solution**: Use better 5V power supply (1A minimum), add 100µF capacitor near ESP32-CAM

## MQTT Topics

The device publishes to device-specific topics:

- `surveillance/<device_name>/status` - Periodic status (30s interval)
- `surveillance/<device_name>/motion` - Motion detection events
- `surveillance/<device_name>/metrics` - System metrics (60s interval)
- `surveillance/<device_name>/events` - System events (boot, errors, etc.)
- `surveillance/<device_name>/image` - Image metadata

Command topic (subscribe):
- `surveillance/<device_name>/command` - Remote commands

### Available Commands (JSON):
```json
{"command": "capture"}        // Trigger image capture
{"command": "status"}          // Request status update
{"command": "restart"}         // Reboot device
{"command": "capture_with_image"} // Capture with base64 image
```

## Project Structure

```
ESP32CAM_Surveillance/
├── ESP32CAM_Surveillance.ino  # Main sketch
├── camera_config.h             # Camera pin definitions
├── camera_config.cpp           # Camera initialization
├── device_config.h             # Device settings and constants
├── secrets.h                   # WiFi/MQTT credentials (EDIT THIS!)
├── trace.h                     # Distributed tracing utilities
├── trace.cpp                   # Trace implementation
└── README.md                   # This file
```

## Performance Notes

### AI-Thinker ESP32-CAM (OV2640):
- **Resolution**: VGA (640×480) recommended for balance
- **Quality**: 10 (lower = better quality, higher = smaller file)
- **Frame Rate**: ~10-15 fps with MJPEG stream
- **PSRAM**: 4MB - essential for stable operation

### Freenove ESP32-S3 (OV3660):
- **Resolution**: SVGA (800×600) supported
- **Quality**: 10 (matches working Freenove sketch)
- **Frame Rate**: ~8-12 fps
- **PSRAM**: 8MB - allows higher resolutions

## Motion Detection Algorithm

Uses JPEG decode to RGB565, then grayscale conversion:
1. Capture frame and decode JPEG at 8× downscale (96×96)
2. Convert RGB565 to grayscale
3. Compare with previous frame pixel-by-pixel
4. Trigger if ≥25 pixels changed above threshold
5. Publish to MQTT and save to SD card

**Adjustable in `device_config.h`**:
- `MOTION_THRESHOLD` (default: 25) - Pixel difference sensitivity
- `MOTION_CHANGED_BLOCKS` (default: 25) - Minimum changed pixels
- `MOTION_CHECK_INTERVAL` (default: 3000ms) - Check frequency

## Credits

- **Original PlatformIO Version**: Created for professional IoT deployment
- **Arduino IDE Port**: For easier community adoption
- **Web UI Design**: Modern dark glassmorphism aesthetic
- **Motion Detection**: Based on MJPEG2SD algorithm

## License

This project is provided as-is for educational and personal use.

## Support

For issues or questions:
1. Check Serial Monitor output (baud 115200)
2. Verify all library versions match requirements
3. Try triple-reset recovery if WiFi issues
4. Ensure adequate power supply (brown-out is common issue)

---

**Happy Surveilling! 📹**
