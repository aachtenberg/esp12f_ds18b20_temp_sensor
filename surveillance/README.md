# ESP32-CAM Surveillance Camera

ESP32-CAM surveillance system with MQTT integration, motion detection, and web interface.

## Hardware Support

### ✅ Fully Functional
- **ESP32-CAM (AI-Thinker)** with MB programmer board
  - OV2640 camera module
  - 4MB PSRAM
  - VGA resolution @ 640x480
  - Proven stable for production use
  - Flash LED on GPIO 4 for capture illumination

### 🚧 In Testing
- **ESP32-S3 WROOM Dev Kit**
  - OV3660 camera module (higher resolution)
  - SVGA resolution @ 800x600
  - Requires stabilized 10 MHz XCLK
  - No built-in flash LED
  - Still being optimized for production use

## Camera Performance Notes

Both boards use **10 MHz XCLK clock** (stabilized from earlier 25 MHz) to ensure reliable frame capture across different environmental conditions. This matches the working Freenove reference implementation.

**Board-Specific Camera Settings:**
- **ESP32-CAM**: OV2640 encoder is forgiving, uses default sensor settings
- **ESP32-S3**: OV3660 requires specific configuration (vflip, brightness, saturation)

See `camera_config.cpp` and `PERFORMANCE_OPTIMIZATIONS.md` for detailed tuning.

## Features

- 📷 MJPEG streaming with async web server
- 🎯 **Hardware-accelerated motion detection** using JPEG decoder
- 🌐 Responsive web interface (desktop/tablet/mobile)
- 📡 MQTT integration for remote monitoring
- 💾 SD card capture storage with graceful shutdown
- 🔦 Configurable flash LED (manual/capture modes - board-dependent)
- 🔄 OTA firmware updates
- 📱 WiFi configuration portal with **NVS-based triple-reset detection** (ESP32-S3)
- 🛡️ Crash loop recovery and WiFi fallback AP
- 💾 PSRAM support for smooth streaming
- 📊 Board-specific optimizations (ESP32-CAM vs ESP32-S3)

### ESP32-CAM Pin Configuration (AI-Thinker)

**Camera Interface:**
- PWDN: GPIO 32
- RESET: -1 (not used)
- XCLK: GPIO 0
- SIOD (SDA): GPIO 26
- SIOC (SCL): GPIO 27
- Y9-Y2: GPIOs 35, 34, 39, 36, 21, 19, 18, 5
- VSYNC: GPIO 25
- HREF: GPIO 23
- PCLK: GPIO 22

**Additional Pins:**
- Flash LED: GPIO 4 (built-in on AI-Thinker board)
- SD Card: 1-bit mode (CLK=39, CMD=38, D0=40) - default pins for ESP32-S3

### ESP32-S3 Pin Configuration (Dev Kit - WIP)

See `camera_config.h` for ESP32-S3 pinout (under development).

## Software Setup

### 1. Configuration

Copy the example secrets file:
```bash
cp include/secrets.h.example include/secrets.h
```

Edit `include/secrets.h` with your credentials:
- WiFi SSID/password
- MQTT broker address and credentials
- Optional: InfluxDB, OTA password

### 2. Build and Upload

**For ESP32-CAM (AI-Thinker):**
```bash
pio run -e esp32cam --target upload --upload-port /dev/ttyUSB0
```

**For ESP32-S3 (WIP):**
```bash
pio run -e esp32-s3-devkitc-1 --target upload
```

### 3. Monitor Serial Output

**ESP32-CAM (74880 baud for boot messages):**
```bash
pio device monitor -e esp32cam --baud 74880
```

**ESP32-S3:**
```bash
pio device monitor -e esp32-s3-devkitc-1
```

## Usage

### Initial WiFi Setup

1. **Triple-reset detection**: Reset device 3 times within 2 seconds to enter config portal
   - **ESP32-S3**: Uses NVS (non-volatile storage) to persist counters across hardware resets
   - **ESP8266/ESP32**: Uses RTC memory with ESP_DoubleResetDetector library
2. **Fallback AP**: If WiFi disconnects for 60+ seconds, fallback AP starts automatically
3. Connect to AP: `Cam-[DeviceName]-Setup` (config portal) or `Cam-[DeviceName]-Fallback` (fallback mode)
4. Open browser to `http://192.168.4.1` for WiFiManager captive portal
5. Configure WiFi credentials and device name
6. Device saves settings to NVS/flash and connects to your network

### Web Interface

Access via `http://[device-ip]` after WiFi connection:

- **`/`** - Full web UI with controls and live stream
- **`/capture`** - Capture single JPEG image
- **`/stream`** - MJPEG live stream
- **`/status`** - Device status JSON
- **`/control`** - Camera settings (brightness, contrast, etc.)
- **`/motion-control`** - Enable/disable motion detection
- **`/flash-control`** - Manual flashlight control
- **`/update`** - OTA firmware update interface

### Motion Detection

- **Algorithm**: JPEG hardware decoder → RGB565 → Grayscale comparison
- **Resolution**: 96x96 pixels (downsampled from VGA)
- **Thresholds**: 
  - Pixel difference: 25 (0-255 scale)
  - Changed blocks: 25 blocks minimum to trigger
- **Check interval**: Every 3 seconds
- **Flash indicator**: Disabled by default (too bright for continuous use)
- **Storage**: Detected motion images saved to SD card automatically (if mounted)

### MQTT Topics

**Subscribe to:**
- `surveillance/[device-name]/status` - Device status updates
- `surveillance/[device-name]/motion` - Motion detection events
- `surveillance/[device-name]/metrics` - System metrics (heap, uptime)
- `surveillance/[device-name]/events` - System events (boot, errors)

**Publish to:**
- `surveillance/[device-name]/command` - Remote commands

**Command examples:**
```json
{"command": "capture"}    // Capture and publish image
{"command": "status"}     // Publish device status
{"command": "restart"}    // Restart device
```

### SD Card Storage

When an SD card is mounted, the device automatically saves captures:
- **Motion detection images**: Saved with `motion` tag and timestamp
- **Manual captures**: Saved with `capture` tag and timestamp
- **Full-frame images**: Saved with `full` tag (from `captureAndPublishWithImage()`)
- **Storage format**: JPEG files in `/captures` directory
- **Web UI feedback**: Capture endpoint returns `X-SD-Saved` header indicating success/failure

**Configuration:**
- Directory: `/captures` (automatically created on SD card)
- File naming: `{timestamp}_{reason}.jpg`
- Graceful unmount: Device properly unmounts SD before reboot to prevent corruption
- Fallback: If SD card unavailable, captures still work and stream normally

### Configuration Options

Edit `include/device_config.h` to customize:
- Motion detection thresholds
- MQTT topics and intervals
- Flash LED behavior
- Capture intervals
- Web server port
- Recovery timeouts

## Troubleshooting

### Camera Initialization Failed
- **Check connections**: Ensure camera module is properly seated
- **Power supply**: Use quality 5V 1A+ power supply (USB power may be insufficient)
- **Serial output**: Monitor at 74880 baud to see ESP32 boot messages
- **PSRAM check**: Device requires PSRAM for camera operation

### Motion Detection Issues
- **False positives**: Increase `MOTION_THRESHOLD` (default: 25) or `MOTION_CHANGED_BLOCKS` (default: 25)
- **Not detecting**: Decrease thresholds in `device_config.h`
- **Check logs**: Serial output shows pixel change percentage on each check

### WiFi Configuration
- **Triple-reset**: Reset device 3 times within 2 seconds to force config portal
  - Counters persist via **NVS (Preferences)** on ESP32-S3 for reliable hardware reset detection
  - Window timeout configurable in `device_config.h` (`RESET_DETECT_TIMEOUT`, default: 2 seconds)
- **Fallback AP**: Wait 60 seconds after WiFi loss for automatic AP mode
- **Portal timeout**: Config portal times out after 2 minutes
- **Remote reset**: Use `/wifi-reset?token=[SECRET]` endpoint if configured
- **AP addresses**: Config portal at `http://192.168.4.1`, fallback AP displays IP on serial

### MQTT Connection Failed
- Verify broker address and port in `secrets.h`
- Check credentials (username/password)
- Test broker with mosquitto client: `mosquitto_sub -h [broker] -t surveillance/# -v`
- Review serial output for connection error codes

### SD Card Issues
- **Mount failed**: Ensure SD card is properly inserted and contacts are clean
- **Not saving**: Check SD card has write permissions and free space
- **Slow saves**: Normal - JPEG write speed depends on card class (use Class 10+)
- **Corruption**: Device uses graceful shutdown; wait for system to fully boot before power loss
- **Check status**: `/status` endpoint shows `sd_ready` and card size in MB

### OTA Update Failed
- Ensure stable WiFi connection (strong signal)
- Check firmware size fits partition (use `huge_app.csv`)
- Verify .bin file is for correct board (esp32cam vs esp32-s3)
- Wait for device to fully boot before updating

### Performance Issues
- **Streaming lag**: Reduce resolution or increase JPEG quality value
- **Low memory**: Check free heap in `/status` endpoint
- **Slow response**: WiFi power save is disabled automatically
- **Crashes**: Enable crash loop recovery (automatic after 5 crashes)

## Development

### Project Structure

```
surveillance/
├── platformio.ini              # PlatformIO configuration (2 environments)
├── include/
│   ├── camera_config.h         # Camera pin definitions & auto-detection
│   ├── device_config.h         # System configuration & thresholds
│   ├── secrets.h               # Credentials (gitignored)
│   └── secrets.h.example       # Template for credentials
├── src/
│   ├── main.cpp                # Main application (~2350 lines)
│   └── camera_config.cpp       # Camera initialization & control
└── data/                       # (Future: LittleFS web assets)
```

### Dependencies

- **espressif/esp32-camera** ^2.0.4 - Camera driver
- **knolleary/PubSubClient** ^2.8.0 - MQTT client
- **bblanchon/ArduinoJson** ^7.0.0 - JSON parsing
- **WiFiManager** (tzapu) - WiFi configuration portal
- **me-no-dev/AsyncTCP** ^1.1.1 - Async networking
- **me-no-dev/ESPAsyncWebServer** ^1.2.3 - Web server
- **khoih-prog/ESP_DoubleResetDetector** ^1.3.2 - Reset detection (ESP8266/ESP32 fallback)
- **Preferences** (built-in) - NVS storage for ESP32-S3 reset/crash tracking

### Build Environments

**esp32cam** (Production):
- Board: ESP32-CAM (AI-Thinker)
- Camera: OV2640
- Build flags: `-DCAMERA_MODEL_AI_THINKER`
- Monitor baud: 74880

**esp32-s3-devkitc-1** (WIP):
- Board: Freenove ESP32-S3 WROOM
- Camera: TBD
- Build flags: `-DCAMERA_MODEL_ESP32S3_EYE`
- Monitor baud: 115200

### Key Technical Details

**Motion Detection Algorithm:**
1. Capture JPEG frame from camera
2. Decode to RGB565 using `jpg2rgb565()` with `JPG_SCALE_8X`
3. Convert RGB565 to 8-bit grayscale
4. Compare with previous frame pixel-by-pixel
5. Count pixels exceeding threshold
6. Trigger if changed pixels ≥ minimum

**SD Card Management:**
- Simple mount using `SD_MMC.begin()` in 1-bit mode
- Automatic `/captures` directory creation on first mount
- Saves JPEG frames with timestamp and reason tag
- Graceful unmount before reboot to prevent corruption
- Web UI provides feedback on save status via HTTP headers

**Recovery Systems:**
- Triple-reset detection (within 2 seconds) using NVS persistence
- Crash loop detection (5 incomplete boots) with NVS tracking
- WiFi fallback AP (after 60 seconds) with automatic STA/AP mode switching
- **ESP32-S3**: Uses Preferences (NVS) for reliable reset/crash tracking across hardware resets
- **ESP8266/ESP32**: Uses RTC memory with ESP_DoubleResetDetector library

## Future Enhancements

- [x] Motion detection with JPEG hardware decoder
- [x] MJPEG streaming implementation
- [x] SD card storage with graceful shutdown
- [ ] Time-lapse recording mode
- [ ] Multi-camera MQTT discovery
- [ ] Person detection using TensorFlow Lite
- [ ] Telegram/Discord notifications via webhook
- [ ] Cloud storage integration (S3, MinIO)
- [ ] Region-of-interest motion zones
- [ ] H.264 video recording
- [ ] ESP32-S3 full support

## License

See main repository LICENSE file.