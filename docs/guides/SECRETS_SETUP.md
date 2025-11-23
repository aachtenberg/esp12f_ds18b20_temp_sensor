# Secrets Configuration Guide

This guide explains how to configure `secrets.h` for ESP sensor projects.

## Quick Setup

1. **Copy the template**:
   ```bash
   cp include/secrets.h.example include/secrets.h
   ```

2. **Edit `include/secrets.h`** with your InfluxDB credentials

3. **Verify it's gitignored**:
   ```bash
   git status
   # Should NOT show include/secrets.h
   ```

## WiFi Configuration

**WiFi credentials are NOT in secrets.h** - they're managed via WiFiManager:
- On first boot, device creates an AP: "Temp-{Location}-Setup"
- Connect to the AP and configure WiFi via captive portal
- **To reconfigure WiFi**: Double-reset within 3 seconds

## InfluxDB Configuration

### Local InfluxDB (Recommended)

1. **Get your InfluxDB Organization ID**:
   ```bash
   ssh user@192.168.0.167
   docker exec -it influxdb influx org list
   # Copy the "ID" column value
   ```

2. **Generate API Token**:
   - Open InfluxDB UI: `http://192.168.0.167:8086`
   - Data → API Tokens → Generate API Token → Read/Write Token
   - Select your bucket (e.g., "sensor_data")

3. **Update secrets.h**:
   ```cpp
   #define USE_LOCAL_INFLUXDB true

   static const char* INFLUXDB_URL = "http://192.168.0.167:8086";
   static const char* INFLUXDB_ORG = "your_org_id";
   static const char* INFLUXDB_BUCKET = "sensor_data";
   static const char* INFLUXDB_TOKEN = "your_token";
   ```

### InfluxDB Cloud (Optional)

```cpp
#define USE_LOCAL_INFLUXDB false

static const char* INFLUXDB_URL = "https://us-east-1-1.aws.cloud2.influxdata.com";
static const char* INFLUXDB_ORG = "";  // Not needed for cloud
static const char* INFLUXDB_BUCKET = "sensor_data";
static const char* INFLUXDB_TOKEN = "your_cloud_token";
```

## Device Configuration

Device-specific settings are in `include/device_config.h` (not secrets.h):

```cpp
static const char* DEVICE_LOCATION = "Big Garage";
static const char* DEVICE_BOARD = "esp8266";  // or "esp32"
```

## Troubleshooting

### Error: "401 Unauthorized" from InfluxDB
- Verify `INFLUXDB_TOKEN` is correct
- Verify `INFLUXDB_ORG` matches your InfluxDB setup
- Ensure `INFLUXDB_BUCKET` exists

### WiFi Not Connecting
- Double-reset to enter configuration mode
- Connect to the device's AP and reconfigure

### Error: "secrets.h: No such file or directory"
```bash
cp include/secrets.h.example include/secrets.h
# Then edit with your credentials
```

## Security Best Practices

- Never commit `include/secrets.h` to Git
- Use strong, unique InfluxDB tokens
- Regenerate tokens if accidentally exposed
