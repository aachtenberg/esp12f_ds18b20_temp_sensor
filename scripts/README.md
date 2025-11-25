# Flash Scripts

Automated flashing tools for ESP Temperature Sensor and Solar Monitor projects.

## Quick Start

### Single Device
```bash
# Flash temperature sensor (ESP8266, default)
./flash_device.sh

# Flash solar monitor (ESP32)  
./flash_device.sh solar

# Flash temperature sensor on ESP32
./flash_device.sh temp esp32
```

### Multiple Devices
```bash
# Flash multiple temperature sensors
python3 flash_multiple.py --project temp --all

# Flash multiple solar monitors
python3 flash_multiple.py --project solar --all
```

### Windows (WSL2)
```powershell
# Run as Administrator
.\attach-and-flash.ps1
```

## Scripts Overview

| Script | Purpose | Status |
|--------|---------|--------|
| `flash_device.sh` | Single device flashing (temp/solar) | ✅ Updated |
| `flash_multiple.py` | Bulk device flashing | ✅ Updated |
| `validate_secrets.sh` | Validate InfluxDB configuration | ✅ Current |
| `attach-and-flash.ps1` | Windows/WSL2 USB + flash | ✅ Updated |
| `attach-usb-wsl2.ps1` | Windows/WSL2 USB management | ✅ Current |

## Device Configuration

Device names are now configured through **WiFiManager portal** after flashing:

1. Flash device with script
2. Device creates WiFi AP (`Temp-*-Setup` or `SolarMonitor-Setup`)
3. Connect to AP and configure WiFi + device name
4. Device connects to your WiFi and logs events to InfluxDB

No need to modify `device_config.h` - the old `DEVICE_LOCATION` approach is deprecated.

## Examples

### Temperature Sensor
```bash
# ESP8266 (most common)
./flash_device.sh temp esp8266

# ESP32 variant  
./flash_device.sh temp esp32
```

### Solar Monitor
```bash
# Standard ESP32 build
./flash_device.sh solar esp32

# With optional device name hint
./flash_device.sh solar esp32 "Main Solar System"
```

### Bulk Deployment
```bash
# Flash 4 temperature sensors
python3 flash_multiple.py --project temp --env esp8266

# Flash 2 solar monitors  
python3 flash_multiple.py --project solar --env esp32dev --ports /dev/ttyUSB0 /dev/ttyUSB1
```

## Validation

Before flashing, validate your `include/secrets.h`:
```bash
./validate_secrets.sh
```

Checks for:
- ✅ File exists and is gitignored
- ✅ No placeholder values (YOUR_*)
- ✅ Valid InfluxDB URL and token
- ✅ WiFi networks configured