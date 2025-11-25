# ESP Sensor Hub

A collection of ESP32/ESP8266 IoT sensor projects with local InfluxDB data logging to Raspberry Pi.

## Projects

### Temperature Sensor (Active)
Multi-location DS18B20 temperature monitoring with WiFi connectivity and comprehensive event logging.
- **Hardware**: ESP8266/ESP32 + DS18B20
- **Status**: 4 devices deployed
- **PCB**: [USB-powered board](docs/pcb_design/usb-powered/) (v1.0 ordered)
- **Features**: Temperature logging, device monitoring, error tracking, configuration auditing

### Solar Monitor (Planned)
ESP32 monitoring for Victron solar equipment via VE.Direct protocol.
- **Hardware**: ESP32 + Victron SmartShunt + SmartSolar MPPT
- **Status**: Planning phase

## System Overview

All sensor projects share common infrastructure:
- **[raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker)** - Raspberry Pi backend (InfluxDB, Grafana, Home Assistant)

### Architecture
```
ESP Devices → Raspberry Pi (192.168.0.167)
              ├── InfluxDB (data storage)
              ├── Grafana (dashboards)
              ├── Home Assistant (automation)
              └── Cloudflare Tunnel (remote access)
```

See [docs/README.md](docs/README.md) for complete architecture details.

## Quick Start

### Temperature Sensor
```bash
# 1. Configure InfluxDB credentials
cp include/secrets.h.example include/secrets.h
# Edit include/secrets.h with your InfluxDB settings

# 2. Flash device (WiFi configured via portal)
./scripts/flash_device.sh temp

# 3. Configure via WiFiManager portal
# Connect to "ESP-Setup" AP → http://192.168.4.1
```

### Solar Monitor  
```bash
# 1. Same secrets.h setup as above

# 2. Flash solar monitor
./scripts/flash_device.sh solar

# 3. Same WiFiManager portal configuration
```

📖 **Complete Guide**: See [docs/reference/PLATFORM_GUIDE.md](docs/reference/PLATFORM_GUIDE.md)

4. **Configure WiFi**: On first boot (or double-reset), connect to the "Temp-Device-Setup" AP and configure WiFi via the captive portal.

5. **Infrastructure**: See [raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker) for Pi setup

For detailed setup instructions, see [docs/SETUP.md](docs/SETUP.md).

## Project Structure

```
├── src/                    # Temperature sensor firmware
├── include/                # Header files (config, secrets)
├── lib/                    # Local libraries (OneWire, DallasTemperature)
├── test/                   # Test files
├── docs/                   # Documentation (organized by category)
│   ├── api/               # InfluxDB, MQTT integration docs
│   ├── guides/            # How-to guides
│   ├── architecture/      # Technical design
│   ├── pcb_design/        # Custom PCB designs
│   │   ├── usb-powered/   # USB-C powered board (v1.0)
│   │   └── solar-battery/ # Solar variant (planned)
│   └── reference/         # Reference material
├── scripts/               # Utility scripts (flash, deploy)
├── platformio.ini         # PlatformIO configuration
└── .gitignore            # Git ignore rules
```

## Supported Boards

- **ESP32** (esp32dev)
- **ESP8266** (nodemcuv2)

## Documentation

- [**Platform Guide**](docs/reference/PLATFORM_GUIDE.md) - **Main documentation & architecture**
- [Configuration Guide](docs/reference/CONFIG.md) - **InfluxDB setup & deployment commands**
- [Architecture Overview](docs/README.md) - Complete system architecture and data flow
- [Full Setup Guide](docs/SETUP.md) - Detailed setup instructions
- [Event Logging](docs/EVENT_LOGGING.md) - **Device monitoring and diagnostics** - Track boots, errors, config changes
- [Code Structure](docs/architecture/CODE_STRUCTURE.md) - Technical implementation details
- [Device Flashing](docs/guides/) - How to flash and deploy devices
- [API Integration](docs/api/) - InfluxDB and MQTT integration guides
- [PCB Designs](docs/pcb_design/) - Custom PCB schematics and manufacturing files
- [Raspberry Pi Infrastructure](https://github.com/aachtenberg/raspberry-pi-docker) - Docker stack setup

## WiFi Configuration

WiFi credentials are managed via **WiFiManager** captive portal:
- On first boot, device creates an AP named "Temp-{Location}-Setup"
- Connect to the AP and configure WiFi via web interface
- **To reconfigure**: Double-reset the device within 3 seconds

## Security Note

This repository does not contain any secrets or credentials. InfluxDB configuration is stored in `include/secrets.h` (gitignored). WiFi credentials are stored on-device via WiFiManager.

## Building & Flashing

```bash
# Build and flash ESP8266
scripts/flash_device.sh "Device Name" esp8266

# Build and flash ESP32
scripts/flash_device.sh "Device Name" esp32
```

See [docs/SETUP.md](docs/SETUP.md) for detailed build instructions.

## Related Projects

- **[raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker)** - Centralized data collection infrastructure

All IoT sensor projects follow similar patterns and integrate with the Raspberry Pi data collection system.
