# ESP Sensor Hub - Documentation

This repository contains ESP32/ESP8266 IoT sensor projects that send data to a local Raspberry Pi infrastructure for storage and visualization.

## Current Projects

- **Temperature Sensor** - DS18B20 temperature monitoring (4 devices deployed)
- **Solar Monitor** - Victron equipment monitoring via VE.Direct (planned)

## Current Architecture

```
┌─────────────────┐
│  ESP8266/ESP32  │  (4 deployed devices)
│   + DS18B20     │
└────────┬────────┘
         │ HTTP (every 15s)
         │ InfluxDB Line Protocol
         ▼
┌─────────────────┐
│  Raspberry Pi   │  (192.168.0.167)
│  192.168.0.167  │
└─────────────────┘
         │
    ┌────┴────┬──────────┬─────────────┐
    ▼         ▼          ▼             ▼
┌─────────┐ ┌────────┐ ┌─────────┐ ┌──────────┐
│ InfluxDB│ │Grafana │ │Home Asst│ │Cloudflare│
│  :8086  │ │ :3000  │ │  :8123  │ │  Tunnel  │
└─────────┘ └────────┘ └─────────┘ └──────────┘
```

### Components

- **ESP Devices** (4 deployed):
  - Big Garage Temperature
  - Small Garage Temperature
  - Pump House Temperature
  - Main Cottage Temperature

- **Raspberry Pi Docker Stack** ([raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker)):
  - **InfluxDB 2.7** - Time-series database (primary data storage)
  - **Grafana** - Dashboards and visualization
  - **Home Assistant** - Reads HTTP endpoints from ESP devices
  - **Prometheus Stack** - System monitoring (Prometheus, Loki, Promtail, Node Exporter, cAdvisor)
  - **Mosquitto** - MQTT broker
  - **Nginx Proxy Manager** - HTTP → HTTPS reverse proxy
  - **Cloudflare Tunnel** - Remote access via xgrunt.com

## Quick Start

1. **Hardware Setup**: Connect DS18B20 sensor to ESP device (GPIO 4)
2. **Configure**: Copy `include/secrets.h.example` to `include/secrets.h` and update credentials
3. **Build & Flash**: Use scripts in `scripts/` directory
4. **Verify**: Check InfluxDB for incoming data

See [SETUP.md](SETUP.md) for detailed instructions.

## Documentation Structure

```
docs/
├── README.md                    # This file (architecture overview)
├── SETUP.md                     # Complete setup guide
├── api/                         # Integration documentation
│   ├── INFLUXDB_INTEGRATION.md  # InfluxDB Line Protocol setup
│   └── MQTT_INTEGRATION.md      # MQTT broker integration
├── guides/                      # How-to guides
│   ├── DEVICE_FLASHING_QUICK_GUIDE.md
│   └── TROUBLESHOOTING.md
├── architecture/                # Technical design docs
│   ├── CODE_STRUCTURE.md
│   └── WIFI_FALLBACK.md
├── pcb_design/                  # Custom PCB designs
│   ├── README.md               # PCB overview and comparison
│   ├── usb-powered/            # USB-C powered board (v1.0)
│   │   ├── SCHEMATIC.md
│   │   ├── BOARD_LAYOUT.md
│   │   ├── BOM.md
│   │   └── ORDERING_GUIDE.md
│   └── solar-battery/          # Solar/battery variant (planned)
└── reference/                   # Reference material
    └── COPILOT_INSTRUCTIONS.md
```

## Current Data Flow

### 1. ESP Device Boot
```
ESP powers on
  → Connect to WiFi (with fallback support)
  → Initialize DS18B20 sensor
  → Start AsyncWebServer (port 80)
  → Begin temperature reading loop
```

### 2. Temperature Reading (Every 15 seconds)
```
Read DS18B20 sensor
  → Convert to °C and °F
  → Send to InfluxDB via HTTP POST (Line Protocol)
  → Serve via HTTP endpoints (/temperaturec, /temperaturef)
  → Log to serial for debugging
```

### 3. Data Storage
```
InfluxDB receives data
  → Stores in "sensor_data" bucket
  → Organization: d990ccd978a70382
  → Retention: configurable (default: infinite)
```

### 4. Visualization
```
Grafana queries InfluxDB
  → Displays dashboards with temperature trends
  → Home Assistant reads HTTP endpoints
  → Remote access via Cloudflare Tunnel (xgrunt.com)
```

## Configuration Files

### ESP Device Configuration

- **[include/secrets.h](../include/secrets.h)** - WiFi, InfluxDB, and service credentials
- **[include/device_config.h](../include/device_config.h)** - Device-specific settings (name, location)
- **[platformio.ini](../platformio.ini)** - Build configuration for ESP8266/ESP32

### Raspberry Pi Configuration

See [raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker) repository for:
- Docker Compose configuration
- InfluxDB setup
- Grafana dashboards
- Home Assistant configuration

## Key Features

### Multi-Network WiFi Fallback
Devices attempt multiple WiFi networks in order (see [WIFI_FALLBACK.md](architecture/WIFI_FALLBACK.md)):
```cpp
static const WiFiNetwork wifi_networks[NUM_WIFI_NETWORKS] = {
  {"AA229-2G", "password"},      // Primary
  {"AA225-2G-OD", "password"},   // Fallback 1
  {"AASTAR", "password"}         // Fallback 2
};
```

### Health Monitoring
- **Exponential Backoff**: Automatic retry with increasing delays on failures
- **Connection Tracking**: WiFi reconnection counter in InfluxDB tags
- **Error Logging**: All errors logged to InfluxDB and serial

### Lightweight Web Server
- **AsyncWebServer**: Non-blocking HTTP server for ESP devices
- **Endpoints**:
  - `/` - HTML dashboard with live temperature
  - `/temperaturec` - Plain text temperature in Celsius
  - `/temperaturef` - Plain text temperature in Fahrenheit

## Deployment

### Single Device
```bash
scripts/flash_device.sh "Device Name" esp8266
```

### All Devices (Bulk Deployment)
```bash
scripts/deploy_all_devices.sh
```

See [guides/DEVICE_FLASHING_QUICK_GUIDE.md](guides/DEVICE_FLASHING_QUICK_GUIDE.md) for detailed flashing instructions.

## Troubleshooting

### Device Not Sending Data
1. Check WiFi connection (serial monitor shows connection status)
2. Verify InfluxDB is running: `ssh aachten@192.168.0.167 "docker ps | grep influxdb"`
3. Check InfluxDB token in `include/secrets.h` matches Pi configuration
4. Query InfluxDB to see if data is arriving:
   ```bash
   influx query 'from(bucket: "sensor_data") |> range(start: -1h)'
   ```

### Sensor Reading "85.0°C" or "--"
- **85.0°C** = Sensor not ready (wait for next reading)
- **"--"** = Sensor not found (check GPIO 4 wiring)

### WiFi Reconnections
Check reconnection count in InfluxDB data:
```bash
influx query 'from(bucket: "sensor_data") |> range(start: -24h) |> filter(fn: (r) => r._field == "wifi_reconnects")'
```

See [guides/TROUBLESHOOTING.md](guides/TROUBLESHOOTING.md) for more solutions.

## Hardware

### Custom PCB Designs

This project includes custom PCB designs for production-ready deployments:

- **[USB-Powered Board](pcb_design/usb-powered/)** - Compact USB-C powered board for indoor use (v1.0, ordered)
- **Solar-Battery Board** - Solar/battery powered variant for outdoor use (planned)

See [pcb_design/README.md](pcb_design/README.md) for design comparison and manufacturing instructions.

## Contributing

When adding features or fixing bugs:
1. Test on both ESP8266 and ESP32 platforms
2. Update documentation if changing configuration or architecture
3. Follow existing code structure (see [architecture/CODE_STRUCTURE.md](architecture/CODE_STRUCTURE.md))
4. Ensure serial logging provides useful debug information

## Support

- **ESP Project**: This repository
- **Pi Infrastructure**: [raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker)
- **InfluxDB Docs**: https://docs.influxdata.com/influxdb/v2.7/
- **PlatformIO Docs**: https://docs.platformio.org/

## Credits

Based on [ESP32 DS18B20 Temperature Tutorial](https://randomnerdtutorials.com/esp32-ds18b20-temperature-arduino-ide/) by Random Nerd Tutorials.

Extended with multi-board support, cloud logging, exponential backoff, and production reliability features.
