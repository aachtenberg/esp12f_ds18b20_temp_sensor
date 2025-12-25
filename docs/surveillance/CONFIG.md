# Surveillance Camera Configuration

## Overview

ESP32-S3 camera firmware for motion detection and video streaming. See [surveillance/README.md](../../surveillance/README.md) for hardware setup.

## Required Files

### secrets.h Setup
Create `surveillance/include/secrets.h`:
```cpp
#ifndef SECRETS_H
#define SECRETS_H

// MQTT Configuration
static const char* MQTT_BROKER = "your.mqtt.broker.com";
static const int MQTT_PORT = 1883;
static const char* MQTT_USER = "";
static const char* MQTT_PASSWORD = "";

// WiFi Configuration (set via WiFiManager portal)
static const char* WIFI_SSID = "";
static const char* WIFI_PASSWORD = "";

#endif
```

## Build & Flash

### Initial Setup
```bash
cd surveillance
pio run -e esp32-s3-devkitc-1 -t upload --upload-port /dev/ttyUSB0
```

### OTA Updates
```bash
# Update via IP address (after initial USB flash)
pio run -e esp32-s3-devkitc-1 -t upload --upload-port 192.168.0.XXX
```

## Configuration

### WiFiManager Portal
1. Device creates AP: "ESP32CAM-Setup"
2. Connect and configure WiFi credentials
3. Device reboots and connects to network

### Camera Settings
- Resolution: Configured via web interface
- Motion detection: Enabled by default
- SD card recording: Optional

## MQTT Topics

| Topic | Payload | Purpose |
|-------|---------|---------|
| `surveillance/DEVICE/motion` | `{detected, timestamp, ...}` | Motion events |
| `surveillance/DEVICE/status` | `{uptime, free_heap, ...}` | Device status |
| `surveillance/DEVICE/command` | `snapshot`, `start_recording`, `stop_recording` | Camera control |

## Troubleshooting

### Camera Not Initializing
1. Check camera ribbon cable connection
2. Verify power supply (5V 2A minimum)
3. Check serial output for camera init errors

### Motion Detection Not Working
1. Verify motion detection enabled in settings
2. Check sensitivity threshold
3. Monitor MQTT for motion events

### SD Card Issues
1. Format SD card as FAT32
2. Ensure card is properly seated
3. Check free space available

## Web Interface

Access camera web interface at: `http://DEVICE_IP/`

Features:
- Live stream view
- Camera settings
- Motion detection configuration
- SD card management
