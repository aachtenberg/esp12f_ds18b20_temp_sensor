# GitHub Copilot Project Summary: ESP Multi-Device Temperature Sensor

This document outlines the work completed by GitHub Copilot to build, debug, and refactor a temperature sensor project designed to run on both ESP8266 and ESP32 microcontrollers. The project evolved from a single-device setup to a robust, multi-board solution with a streamlined flashing process and several critical bug fixes.

## 1. Project Evolution & Key Fixes

The project underwent several major iterations to resolve bugs and add features.

### Initial State:
- The project was initially configured for a single board type.
- The build process was failing due to incorrect library dependencies and missing Python packages.
- The device was experiencing runtime crashes, particularly on the ESP8266, due to memory issues.

### Phase 1: Build & Dependency Resolution
- **Problem**: The initial build failed because the ESP32 requires a different asynchronous TCP library (`AsyncTCP`) than the ESP8266 (`ESPAsyncTCP`).
- **Solution**:
    1.  Modified `platformio.ini` to use `me-no-dev/AsyncTCP` for the ESP32.
    2.  Corrected a missing Python dependency (`intelhex`) required by PlatformIO's `esptool.py` by running `pip install intelhex`.
    3.  Fixed conditional compilation flags in `main.cpp` to include the correct headers (`HTTPClient` for ESP32) and API calls (`WiFi.setSleep(false)` for ESP32 vs. `WiFi.setSleepMode()` for ESP8266).

### Phase 2: Multi-Board Architecture
- **Problem**: The project needed to support both ESP8266 and ESP32 devices from the same codebase without manual configuration changes for each flash.
- **Solution**:
    1.  **`platformio.ini` Refactor**: Created two distinct environments: `[env:esp32dev]` and `[env:esp8266]`, each with the correct platform and board settings.
    2.  **`flash_device.sh` Script**: Heavily modified the script to accept a board type (`esp32` or `esp8266`) as an argument. The script now automatically selects the correct PlatformIO environment.
    3.  **`device_config.h`**: Added a `DEVICE_BOARD` variable, which the flash script updates automatically. This allows the C++ code to know which board it's running on.

### Phase 3: Fixing Runtime Errors (Lambda 500 & Memory Crashes)
- **Problem**: The ESP8266 device was crashing and rebooting. When it did run, it sent a malformed JSON payload to AWS Lambda, resulting in an HTTP 500 error.
- **Solution**:
    1.  **JSON Payload Fix**: Corrected a missing comma in the JSON string being built in `sendToLambda()`.
    2.  **Memory Optimization**: The primary cause of crashes was memory exhaustion on the ESP8266.
        - The `sendToLambda()` function was rewritten to use `snprintf` instead of `String` concatenation, which dramatically reduced heap fragmentation.
        - The large, buffered log payload was removed in favor of a simple, direct temperature payload.
        - The MQTT client, which consumes significant RAM, was disabled for the ESP8266 build.
    3.  **Logging Reduction**: Removed verbose `Serial.println` statements from the main loop to clean up the output and reduce overhead.

## 2. Final Architecture & How to Use

The project is now stable, configurable, and easy to manage.

### Configuration Files
- **`include/secrets.h`**: Stores all sensitive information like WiFi credentials, InfluxDB tokens, and AWS keys. **This file should not be committed to version control.**
- **`include/device_config.h`**: Stores the device's location and board type. These values are **set automatically** by the `flash_device.sh` script.

### Flashing a Device
The `flash_device.sh` script is the primary tool for flashing.

**Usage**:
```bash
./flash_device.sh "Device Location" [board_type]
```
- `Device Location`: A string that will identify the device (e.g., "Pump House").
- `board_type`: Either `esp32` or `esp8266`.

**Examples**:
```bash
# Flash an ESP32 device for the garage
./flash_device.sh "Big Garage" esp32

# Flash an ESP8266 device for the pump house
./flash_device.sh "Pump House" esp8266
```

### Monitoring
After flashing, the script will prompt you to monitor the serial output. You can also do this manually:
```bash
pio device monitor -b 115200
```

### Accessing the Web Interface
Once a device is connected to WiFi, you can view its temperature by navigating to its IP address in a web browser (e.g., `http://192.168.0.204`). The IP address is printed in the serial monitor on boot.

## 3. Infrastructure Setup

### Home Assistant & Cloudflare Integration
The project includes a complete Home Assistant setup with Cloudflare tunnels running on a Raspberry Pi (192.168.0.167).

**Docker Setup on Raspberry Pi** (`docker-compose-homeassistant.yml`):
- **Home Assistant**: Running in host network mode for device discovery
- **Cloudflared**: Cloudflare tunnel service for secure external access
- **Nginx Proxy Manager**: Reverse proxy for local routing and SSL
- All services run in Docker containers on the Raspberry Pi
- Services restart automatically and use host networking

**Key Infrastructure Components (All on Raspberry Pi 192.168.0.167)**:
- **Local InfluxDB**: Running at 192.168.0.167:8086 in Docker
- **Nginx Proxy Manager**: Reverse proxy for local services, Docker port 81 (admin)
- **Cloudflare Tunnel (cloudflared)**: Secure external access via Docker container
- **Home Assistant**: Dockerized with persistent config volume
- **Prometheus**: Monitoring stack with cAdvisor and Node Exporter
- **Grafana**: Metrics visualization at port 3000

### Data Flow Architecture
```
ESP8266/ESP32 → Local InfluxDB (192.168.0.167) → Home Assistant → Cloudflare Tunnel → External Access
                     ↓
               Local nginx (reverse proxy)
```

**Network Configuration**:
- Devices send data to local InfluxDB on internal network (192.168.0.x)
- Home Assistant reads from InfluxDB via local network connection
- External access secured through Cloudflare tunnels (no port forwarding)
- nginx provides reverse proxy services for local infrastructure

### Home Assistant InfluxDB Integration
For devices sending data to local InfluxDB, configure Home Assistant with:
```yaml
influxdb:
  api_version: 2
  ssl: false  # Local network connection
  host: 192.168.0.167
  port: 8086
  token: "y67e6bowEepW8C20Lr6-Dvxts5N7XMMMBINqcazRKkoiNLVSTWPuqeC1xdneQ96cu3QNpnKD3O4OgmWHrR5dqw=="
  organization: "d990ccd978a70382"
  bucket: "sensor_data"
```

## 4. Complete Device Setup & Integration Procedure

This procedure documents the complete workflow for adding a new ESP temperature sensor device to your infrastructure, including Cloudflare tunnel and Nginx Proxy Manager configuration.

### Step 1: Flash the Device

Flash the ESP device with location and board type:

```bash
cd /home/aachten/PlatformIO/esp12f_ds18b20_temp_sensor
./scripts/flash_device.sh "Device Name" esp32
# or for ESP8266:
./scripts/flash_device.sh "Device Name" esp8266
```

**Note for WSL2 Users**: USB devices require Windows-side attachment using `usbipd`:
```powershell
# Run in Windows PowerShell as Administrator
usbipd list  # Find your device BUSID (e.g., 2-11)
usbipd bind --busid 2-11  # One-time share
usbipd attach --wsl --busid 2-11  # Connect to WSL
```

After flashing, note the device's IP address from the serial monitor output.

### Step 2: Verify Device Operation

Test the device is responding:
```bash
curl http://192.168.0.XXX
```

You should see the HTML temperature display interface.

### Step 3: Update Cloudflare Tunnel Configuration

**3a. Get current tunnel configuration:**
```bash
curl -s -X GET "https://api.cloudflare.com/client/v4/accounts/ea26fdf5a9eaa636f5f3975694a1014f/cfd_tunnel/72371ad4-7b0e-43e3-91e4-e21463cc85e5/configurations" \
  -H "Authorization: Bearer pedZT_ZRT5ingIpcdJbpI-b75XLsLcFymQiv2PUa"
```

**3b. Create updated configuration JSON** with new device:
```bash
cat > /tmp/cf_config.json << 'EOF'
{
  "config": {
    "ingress": [
      {"hostname": "ha.xgrunt.com", "service": "http://localhost:8123"},
      {"hostname": "pumphouse.xgrunt.com", "service": "http://192.168.0.122:80", "originRequest": {}},
      {"hostname": "smallgarage.xgrunt.com", "service": "http://192.168.0.109:80", "originRequest": {}},
      {"hostname": "biggarage.xgrunt.com", "service": "http://192.168.0.103:80", "originRequest": {}},
      {"hostname": "maincottage.xgrunt.com", "service": "http://192.168.0.139:80", "originRequest": {}},
      {"hostname": "NEWDEVICE.xgrunt.com", "service": "http://192.168.0.XXX:80", "originRequest": {}},
      {"hostname": "grafana.xgrunt.com", "service": "http://192.168.0.167:3000", "originRequest": {}},
      {"service": "http_status:404"}
    ]
  }
}
EOF
```

**3c. Update tunnel configuration:**
```bash
curl -s -X PUT "https://api.cloudflare.com/client/v4/accounts/ea26fdf5a9eaa636f5f3975694a1014f/cfd_tunnel/72371ad4-7b0e-43e3-91e4-e21463cc85e5/configurations" \
  -H "Authorization: Bearer pedZT_ZRT5ingIpcdJbpI-b75XLsLcFymQiv2PUa" \
  -H "Content-Type: application/json" \
  --data @/tmp/cf_config.json
```

### Step 4: Create DNS Record

Get Zone ID and create CNAME record:
```bash
# Get Zone ID (one-time)
curl -s -X GET "https://api.cloudflare.com/client/v4/zones?name=xgrunt.com" \
  -H "Authorization: Bearer 72NkXVJpmPgeBFuEN5Md-SM0_EfYrMMMgcSOBuC8" | grep -o '"id":"[^"]*"' | head -1

# Create DNS CNAME (replace ZONE_ID)
curl -s -X POST "https://api.cloudflare.com/client/v4/zones/eab29b035cefa6e89348de6b4e0a66f5/dns_records" \
  -H "Authorization: Bearer 72NkXVJpmPgeBFuEN5Md-SM0_EfYrMMMgcSOBuC8" \
  -H "Content-Type: application/json" \
  --data '{"type":"CNAME","name":"DEVICENAME","content":"72371ad4-7b0e-43e3-91e4-e21463cc85e5.cfargotunnel.com","proxied":true,"ttl":1}'
```

### Step 5: Update Nginx Proxy Manager

**Note**: Nginx Proxy Manager runs in a Docker container on the Raspberry Pi (192.168.0.167), not on WSL2.

**5a. Create Nginx configuration file** (increment number from last config):
```bash
ssh aachten@192.168.0.167 'sudo docker exec nginx-proxy-manager bash -c "cat > /data/nginx/proxy_host/X.conf << '\''EOF'\''
# ------------------------------------------------------------
# DEVICENAME.xgrunt.com
# ------------------------------------------------------------

map \$scheme \$hsts_header {
    https   \"max-age=63072000; preload\";
}

server {
  set \$forward_scheme http;
  set \$server         \"192.168.0.XXX\";
  set \$port           80;

  listen 80;
listen [::]:80;

  server_name DEVICENAME.xgrunt.com;
http2 off;

  # Block Exploits
  include conf.d/include/block-exploits.conf;

  access_log /data/logs/proxy-host-X_access.log proxy;
  error_log /data/logs/proxy-host-X_error.log warn;

  location / {
    # Proxy!
    include conf.d/include/proxy.conf;
  }

  # Custom
  include /data/nginx/custom/server_proxy[.]conf;
}
EOF
"'
```

**5b. Test and reload Nginx:**
```bash
ssh aachten@192.168.0.167 'sudo docker exec nginx-proxy-manager nginx -t && sudo docker exec nginx-proxy-manager nginx -s reload'
```

### Step 6: Restart Cloudflare Tunnel

**Note**: The Cloudflare tunnel runs in a Docker container on the Raspberry Pi (192.168.0.167), alongside Home Assistant.

```bash
ssh aachten@192.168.0.167 'cd /home/aachten/homeassistant && sudo docker compose restart cloudflared'
```

Verify logs show new configuration:
```bash
ssh aachten@192.168.0.167 'sudo docker logs cloudflared --tail 20'
```

### Step 7: Update Documentation

**Note**: All infrastructure documentation is stored on the Raspberry Pi and backed up to git.

**7a. Update CLOUDFLARE_TUNNEL.md:**
```bash
ssh aachten@192.168.0.167 'nano /home/aachten/homeassistant-backup/homeassistant/CLOUDFLARE_TUNNEL.md'
```

Add the new device to:
- **Current Routes** section
- **ESP Device IPs** section
- **API example** in the curl commands

**7b. Commit to git:**
```bash
ssh aachten@192.168.0.167 'cd /home/aachten/homeassistant-backup && \
  git add homeassistant/CLOUDFLARE_TUNNEL.md && \
  git commit -m "Add DEVICENAME ESP device (192.168.0.XXX) to configuration" && \
  git push'
```

### Step 8: Add to Home Assistant

**Note**: Home Assistant runs in a Docker container on the Raspberry Pi (192.168.0.167).

Add REST sensor to Home Assistant configuration.yaml:
```yaml
sensor:
  - platform: rest
    name: "Device Name Temperature"
    resource: http://192.168.0.XXX/temperaturec
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement
    scan_interval: 30
    
  - platform: rest
    name: "Device Name Temperature F"
    resource: http://192.168.0.XXX/temperaturef
    unit_of_measurement: "°F"
    device_class: temperature
    state_class: measurement
    scan_interval: 30
```

Restart Home Assistant to load the new sensors.

### Step 9: Verify External Access

Test the device is accessible externally (wait 1-2 minutes for DNS propagation):
```bash
curl -I https://DEVICENAME.xgrunt.com
```

### Summary of Configured Devices

| Device | IP | External URL | Status |
|--------|----|--------------|---------
| Big Garage | 192.168.0.103 | https://biggarage.xgrunt.com | ✅ Active |
| Small Garage | 192.168.0.109 | https://smallgarage.xgrunt.com | ✅ Active |
| Pump House | 192.168.0.122 | https://pumphouse.xgrunt.com | ✅ Active |
| Main Cottage | 192.168.0.139 | https://maincottage.xgrunt.com | ✅ Active |

### API Credentials Reference

- **Cloudflare Tunnel Management**: `pedZT_ZRT5ingIpcdJbpI-b75XLsLcFymQiv2PUa`
- **Cloudflare DNS Management**: `72NkXVJpmPgeBFuEN5Md-SM0_EfYrMMMgcSOBuC8`
- **Cloudflare Account ID**: `ea26fdf5a9eaa636f5f3975694a1014f`
- **Cloudflare Tunnel ID**: `72371ad4-7b0e-43e3-91e4-e21463cc85e5`
- **Cloudflare Zone ID (xgrunt.com)**: `eab29b035cefa6e89348de6b4e0a66f5`

````
