# Multi-Device Temperature Sensor Setup

## Based On

This project's DS18B20 implementation is based on [ESP32 DS18B20 Temperature Arduino IDE](https://randomnerdtutorials.com/esp32-ds18b20-temperature-arduino-ide/) by Rui Santos.

## System Overview

Each device sends data to:
- **InfluxDB** - Time-series storage on Raspberry Pi
- **Local Web Server** - Direct device access via HTTP

## Quick Start

### 1. Configure InfluxDB Credentials

```bash
cp include/secrets.h.example include/secrets.h
# Edit with your InfluxDB URL, org, bucket, and token
```

See [SECRETS_SETUP.md](guides/SECRETS_SETUP.md) for InfluxDB setup details.

### 2. Set Device Location

Edit `include/device_config.h`:
```cpp
static const char* DEVICE_LOCATION = "Big Garage";
static const char* DEVICE_BOARD = "esp8266";  // or "esp32"
```

### 3. Flash Device

```bash
scripts/flash_device.sh "Big Garage" esp8266
```

### 4. Configure WiFi

On first boot, the device creates a WiFi access point:
- **AP Name**: `Temp-Big-Garage-Setup`
- Connect to the AP with your phone/laptop
- A captive portal opens automatically
- Select your WiFi network and enter password
- Device reboots and connects to your network

**To reconfigure WiFi later**: Double-reset the device within 3 seconds.

## Data Flow

```
┌─────────────────────────────────────────────────────┐
│         ESP8266 Temperature Sensor                  │
│  (Device Location: "Big Garage", "Bedroom", etc.)   │
└──────────┬──────────────────────────────────────────┘
           │ HTTP POST (every 15s)
           ▼
┌──────────────────────────────────────────────────────┐
│  Raspberry Pi (192.168.0.167)                        │
│  ├── InfluxDB  - Time-series storage                 │
│  ├── Grafana   - Dashboards                          │
│  └── Home Assistant - Automation                     │
└──────────────────────────────────────────────────────┘
```

## Flashing Multiple Devices

For each device:

1. **Update device_config.h** with new location name
2. **Connect device via USB**
3. **Run flash script**:
   ```bash
   scripts/flash_device.sh "Bedroom" esp8266
   ```
4. **Configure WiFi** via the captive portal

## Troubleshooting

### Device Not Connecting to WiFi
- Double-reset to enter configuration mode
- Connect to device's AP and reconfigure

### Temperature Shows "--"
- Check DS18B20 wiring (GPIO 4)
- Verify sensor is properly connected

### InfluxDB Not Receiving Data
- Check InfluxDB token in `secrets.h`
- Verify InfluxDB is running on Pi
- Check serial monitor for error messages

### Device Keeps Rebooting
- Check power supply (adequate 5V)
- Check DS18B20 wiring
- Monitor console for error messages

## Web Endpoints

Each device serves:
- `/` - HTML dashboard with live temperature
- `/temperaturec` - Temperature in Celsius (plain text)
- `/temperaturef` - Temperature in Fahrenheit (plain text)
- `/health` - JSON health/metrics endpoint

## Resources

- [InfluxDB Docs](https://docs.influxdata.com/influxdb/v2.7/)
- [PlatformIO Docs](https://docs.platformio.org/)
- [Raspberry Pi Docker Stack](https://github.com/aachtenberg/raspberry-pi-docker)
