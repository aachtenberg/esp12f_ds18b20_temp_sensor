# ESP12F DS18B20 Temperature Sensor

Multi-board temperature sensor project supporting ESP32 and ESP8266 with local InfluxDB data logging to Raspberry Pi.

## System Overview

This project consists of two repositories:
- **This repo** - ESP8266/ESP32 firmware for temperature sensors
- **[raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker)** - Raspberry Pi infrastructure (InfluxDB, Grafana, Home Assistant, etc.)

### Architecture
```
ESP Devices (4 deployed) → Raspberry Pi (192.168.0.167)
                          ├── InfluxDB (data storage)
                          ├── Grafana (dashboards)
                          ├── Home Assistant (automation)
                          └── Cloudflare Tunnel (remote access)
```

See [docs/README.md](docs/README.md) for complete architecture details.

## Quick Start

1. **Setup**: See [docs/SETUP.md](docs/SETUP.md)
2. **Flash Device**: See [docs/guides/DEVICE_FLASHING_QUICK_GUIDE.md](docs/guides/DEVICE_FLASHING_QUICK_GUIDE.md)
3. **Scripts**: All flashing/deployment scripts in `scripts/`
4. **Infrastructure**: See [raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker) for Pi setup

## Project Structure

```
├── src/                    # Main application code
├── include/                # Header files (config, secrets)
├── lib/                    # Local libraries (OneWire, DallasTemperature)
├── test/                   # Test files
├── docs/                   # Documentation (organized by category)
│   ├── api/               # AWS Lambda, InfluxDB, MQTT docs
│   ├── guides/            # How-to guides
│   ├── architecture/      # Technical design
│   └── reference/         # Reference material
├── scripts/               # Utility scripts (flash, deploy, CDK)
├── platformio.ini         # PlatformIO configuration
└── .gitignore            # Git ignore rules
```

## Supported Boards

- **ESP32** (esp32dev)
- **ESP8266** (nodemcuv2)

## Documentation

- [Architecture Overview](docs/README.md) - Complete system architecture and data flow
- [Full Setup Guide](docs/SETUP.md) - Detailed setup instructions
- [Code Structure](docs/architecture/CODE_STRUCTURE.md) - Technical implementation details
- [Device Flashing](docs/guides/) - How to flash and deploy devices
- [API Integration](docs/api/) - InfluxDB and MQTT integration guides
- [Raspberry Pi Infrastructure](https://github.com/aachtenberg/raspberry-pi-docker) - Docker stack setup

## Building & Flashing

```bash
# Build and flash ESP8266
scripts/flash_device.sh "Device Name" esp8266

# Build and flash ESP32
scripts/flash_device.sh "Device Name" esp32
```

See [docs/SETUP.md](docs/SETUP.md) for detailed build instructions.
