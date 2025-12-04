# ESP32-S3 Surveillance Camera

ESP32-S3 WROOM based surveillance camera system with MQTT integration and web interface.

## Features

- 📷 High-quality JPEG image capture using ESP32-S3 camera
- 🌐 Web interface for live preview and settings
- 📡 MQTT integration for remote control and image publishing
- 🔄 OTA (Over-The-Air) firmware updates
- 📱 WiFi configuration portal (WiFiManager)
- 🎯 Motion detection support (configurable)
- 💾 PSRAM support for high-resolution images

## Hardware Requirements

- ESP32-S3 WROOM module with camera support
- Camera module (OV2640 or compatible)
- Power supply (5V, 1A minimum)
- PSRAM (required for high resolution)

### Camera Pin Configuration

Default pins (adjust in `camera_config.h` for your module):

- XCLK: GPIO 15
- SIOD: GPIO 4
- SIOC: GPIO 5
- Y9-Y2: GPIOs 16, 17, 18, 12, 10, 8, 9, 11
- VSYNC: GPIO 6
- HREF: GPIO 7
- PCLK: GPIO 13

## Software Setup

### 1. Configuration

Copy the example secrets file:
```bash
cp include/secrets.h.example include/secrets.h
```

```

### 3. Monitor Serial Output
pio device monitor -e esp32-s3-devkitc-1
```

## Usage

### Initial WiFi Setup

### Web Interface

- `/capture` - Capture single image
- `/stream` - MJPEG stream (planned)
- `/status` - Device status JSON
- `/update` - OTA update interface

### MQTT Commands

Publish to `surveillance/command`:

```json
{"command": "capture"}  // Capture and publish image
{"command": "status"}   // Publish device status
{"command": "restart"}  // Restart device
```

Subscribe to topics:
- `surveillance/status` - Device status updates
- `surveillance/image` - Image metadata
- `surveillance/motion` - Motion detection events

- MQTT topics
- Capture interval
- Motion detection settings
- Web server port

## Troubleshooting

### Camera Initialization Failed
- Check camera module connection
- Review serial output for error codes
- Use WiFiManager portal to reconfigure
- Double reset within 10 seconds to force config portal
- Check signal strength

### MQTT Connection Failed

- Verify broker address and credentials in `secrets.h`
- Check network connectivity
- Review serial output for error codes

### OTA Update Failed

- Ensure stable WiFi connection
- Check partition table supports OTA
- Verify firmware binary size

## Development

### Project Structure

```
surveillance/
├── platformio.ini          # PlatformIO configuration
├── include/
│   ├── camera_config.h     # Camera pin and settings
│   ├── device_config.h     # Device configuration
│   ├── secrets.h           # Credentials (gitignored)
│   └── secrets.h.example   # Secrets template
├── src/
│   ├── main.cpp            # Main application
│   └── camera_config.cpp   # Camera implementation
└── data/                   # Web interface files (future)
```

### Dependencies

- esp32-camera
- PubSubClient (MQTT)
- ArduinoJson
- WiFiManager
- ESPAsyncWebServer
- AsyncElegantOTA

## Future Enhancements

- [ ] Motion detection with event triggering
- [ ] Image upload to cloud storage (S3, etc.)
- [ ] MJPEG streaming implementation
- [ ] Time-lapse recording
- [ ] Multiple camera support
- [ ] Person detection using TensorFlow Lite
- [ ] Telegram/Discord notifications
- [ ] SD card storage

## License

See main repository LICENSE file.