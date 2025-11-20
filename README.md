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

1. **Configure Secrets**: Create `include/secrets.h` from template:
   ```bash
   cp include/secrets.h.example include/secrets.h
   # Edit with your WiFi and InfluxDB credentials
   ```
   See [docs/guides/SECRETS_SETUP.md](docs/guides/SECRETS_SETUP.md) for detailed instructions.

2. **Validate Configuration**:
   ```bash
   ./scripts/validate_secrets.sh
   ```

3. **Build & Flash**:
   ```bash
   scripts/flash_device.sh "Device Name" esp8266
   ```

4. **Infrastructure**: See [raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker) for Pi setup

For detailed setup instructions, see [docs/SETUP.md](docs/SETUP.md).

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
- [Secrets Setup Guide](docs/guides/SECRETS_SETUP.md) - **Configure WiFi and InfluxDB credentials**
- [Full Setup Guide](docs/SETUP.md) - Detailed setup instructions
- [Code Structure](docs/architecture/CODE_STRUCTURE.md) - Technical implementation details
- [Device Flashing](docs/guides/) - How to flash and deploy devices
- [API Integration](docs/api/) - InfluxDB and MQTT integration guides
- [Raspberry Pi Infrastructure](https://github.com/aachtenberg/raspberry-pi-docker) - Docker stack setup

## Security Note

This repository does not contain any secrets or credentials. All sensitive configuration is stored in `include/secrets.h`, which is gitignored. To use this project, you must create your own `secrets.h` from the provided template. See [docs/guides/SECRETS_SETUP.md](docs/guides/SECRETS_SETUP.md).

## Building & Flashing

```bash
# Build and flash ESP8266
scripts/flash_device.sh "Device Name" esp8266

# Build and flash ESP32
scripts/flash_device.sh "Device Name" esp32
```

See [docs/SETUP.md](docs/SETUP.md) for detailed build instructions.
