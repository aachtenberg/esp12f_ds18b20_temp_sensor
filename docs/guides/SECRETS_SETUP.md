# Secrets Configuration Guide

This guide explains how to configure your `secrets.h` file for ESP temperature sensors.

## Quick Setup

1. **Copy the template**:
   ```bash
   cp include/secrets.h.example include/secrets.h
   ```

2. **Edit `include/secrets.h`** with your actual credentials

3. **Verify it's gitignored**:
   ```bash
   git status
   # Should NOT show include/secrets.h as changed/untracked
   ```

## Required Secrets

### WiFi Credentials

**Primary Network** (required):
```cpp
static const WiFiNetwork wifi_networks[NUM_WIFI_NETWORKS] = {
  {"YourWiFiSSID", "YourWiFiPassword"},  // Replace these
  // ...
};
```

**Fallback Networks** (optional but recommended):
- Configure 2-3 networks for reliability
- ESP will try each network in order until one connects
- Open networks can use empty password: `{"OpenNetwork", ""}`

### InfluxDB Configuration

**Local InfluxDB** (recommended for home use):

1. **Get your InfluxDB Organization ID**:
   ```bash
   # SSH to your Pi
   ssh user@192.168.1.100
   
   # Query InfluxDB
   docker exec -it influxdb influx org list
   # Copy the "ID" column value
   ```

2. **Generate API Token**:
   - Open InfluxDB UI: `http://192.168.1.100:8086`
   - Login → Data → API Tokens → Generate API Token → Read/Write Token
   - Select your bucket (e.g., "sensor_data")
   - Copy the generated token

3. **Update secrets.h**:
   ```cpp
   static const char* INFLUXDB_URL = "http://192.168.1.100:8086";  // Your Pi's IP
   static const char* INFLUXDB_ORG = "abc123def456";  // Org ID from step 1
   static const char* INFLUXDB_BUCKET = "sensor_data";
   static const char* INFLUXDB_TOKEN = "your_token_from_step_2";
   ```

**InfluxDB Cloud** (optional):
```cpp
#define USE_LOCAL_INFLUXDB false

static const char* INFLUXDB_URL = "https://us-east-1-1.aws.cloud2.influxdata.com";
static const char* INFLUXDB_BUCKET = "sensor_data";
static const char* INFLUXDB_TOKEN = "your_cloud_token";
```

### Optional: MQTT Configuration

If using MQTT for logging (optional):

```cpp
static const char* MQTT_BROKER = "192.168.1.100";  // Your MQTT broker IP
static const int MQTT_PORT = 1883;
static const char* MQTT_TOPIC = "esp12f/logs";
static const char* MQTT_USER = "your_mqtt_user";       // If authentication enabled
static const char* MQTT_PASSWORD = "your_mqtt_password"; // If authentication enabled
```

## Device-Specific Configuration

### Device Name and Location

Edit `include/device_config.h` (separate from secrets):

```cpp
static const char* DEVICE_NAME = "Big Garage Temperature";
static const char* DEVICE_LOCATION = "garage";
```

These appear in InfluxDB tags for filtering/grouping.

### Page Title

In `secrets.h`, customize the web dashboard title:

```cpp
static const char* PAGE_TITLE = "Big Garage Temperature";
```

### Static IP (Optional)

For devices that need fixed IPs:

```cpp
static const char* STATIC_IP = "192.168.1.50";
static const char* STATIC_GATEWAY = "192.168.1.1";
static const char* STATIC_SUBNET = "255.255.255.0";
static const char* STATIC_DNS = "8.8.8.8";
```

Leave empty for DHCP (recommended):

```cpp
static const char* STATIC_IP = "";
```

## Security Best Practices

### Do NOT:
- ❌ Commit `include/secrets.h` to Git
- ❌ Share screenshots showing your secrets
- ❌ Use weak passwords (< 12 characters)
- ❌ Use the same password across multiple services
- ❌ Store secrets in public pastebins or forums

### DO:
- ✅ Keep `secrets.h` only on your local machine
- ✅ Use strong, unique passwords
- ✅ Regenerate tokens if accidentally exposed
- ✅ Use different InfluxDB tokens for each environment
- ✅ Enable WPA2/WPA3 encryption on WiFi networks

## Validation

Before flashing your device, validate your configuration:

```bash
# Run validation script (checks syntax and placeholders)
./scripts/validate_secrets.sh
```

Expected output:
```
✅ secrets.h exists
✅ No placeholder values found (YOUR_*, YOUR-*)
✅ WiFi networks configured: 3
✅ InfluxDB URL configured
✅ InfluxDB token length: 88 characters (looks valid)
✅ Configuration looks good!
```

## Troubleshooting

### Error: "secrets.h: No such file or directory"

You haven't created `secrets.h` yet:

```bash
cp include/secrets.h.example include/secrets.h
# Then edit include/secrets.h with your credentials
```

### Error: "401 Unauthorized" when sending to InfluxDB

- **Check token**: Make sure `INFLUXDB_TOKEN` is correct
- **Check org ID**: Verify `INFLUXDB_ORG` matches your InfluxDB setup
- **Check bucket**: Ensure `INFLUXDB_BUCKET` exists in InfluxDB

To verify in InfluxDB UI:
1. Go to `http://YOUR_PI_IP:8086`
2. Data → Buckets → Verify "sensor_data" exists
3. Data → API Tokens → Check your token has read/write permissions

### WiFi Not Connecting

- **Check SSID**: WiFi names are case-sensitive
- **Check password**: Must be exact (no extra spaces)
- **Check signal**: Device must be within WiFi range
- **Try fallback**: Temporarily put your phone hotspot as first network for testing

### InfluxDB URL Wrong Format

**Correct**:
```cpp
"http://192.168.1.100:8086"  // Include http:// and port
```

**Incorrect**:
```cpp
"192.168.1.100"        // Missing http:// and port
"http://192.168.1.100" // Missing port :8086
```

## Multiple Devices

### Strategy 1: Separate secrets.h per device (recommended)

Keep different `secrets.h` files for each device:

```bash
# Save device-specific versions
cp include/secrets.h include/secrets.h.big-garage
cp include/secrets.h include/secrets.h.small-garage

# When flashing:
cp include/secrets.h.big-garage include/secrets.h
platformio run --target upload -e esp8266
```

### Strategy 2: Only change device_config.h

Keep same `secrets.h` (WiFi, InfluxDB) across all devices, only change:

```bash
# Edit for each device before flashing
vim include/device_config.h
# Change DEVICE_NAME and DEVICE_LOCATION
```

## Rotating Credentials

### If WiFi Password Changes:

1. Update `secrets.h` with new password
2. Rebuild and flash all devices:
   ```bash
   scripts/deploy_all_devices.sh
   ```

### If InfluxDB Token Compromised:

1. **Revoke old token** in InfluxDB UI:
   - Data → API Tokens → Find token → Delete

2. **Generate new token**:
   - Data → API Tokens → Generate API Token

3. **Update all devices**:
   ```bash
   # Edit secrets.h with new token
   vim include/secrets.h
   
   # Reflash all devices
   scripts/deploy_all_devices.sh
   ```

## Environment-Specific Configurations

### Development Setup

```cpp
#define USE_LOCAL_INFLUXDB true
static const char* INFLUXDB_URL = "http://localhost:8086";  // Local InfluxDB
static const char* INFLUXDB_BUCKET = "sensor_data_dev";     // Separate dev bucket
```

### Production Setup

```cpp
#define USE_LOCAL_INFLUXDB true
static const char* INFLUXDB_URL = "http://192.168.1.100:8086";
static const char* INFLUXDB_BUCKET = "sensor_data";
```

## Getting Help

If you're stuck:

1. Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. Run validation script: `./scripts/validate_secrets.sh`
3. Check serial monitor: `platformio device monitor -b 115200`
4. Verify InfluxDB is running: `docker ps | grep influxdb`

## Example Configuration

Here's a complete working example (with fake credentials):

```cpp
#define NUM_WIFI_NETWORKS 3
static const WiFiNetwork wifi_networks[NUM_WIFI_NETWORKS] = {
  {"MyHomeWiFi", "SuperSecret123!"},
  {"MyHomeWiFi-5G", "SuperSecret123!"},
  {"MyPhone_Hotspot", "Hotspot2024"}
};

static const char* PAGE_TITLE = "Big Garage Temperature";

#define USE_LOCAL_INFLUXDB true
static const char* INFLUXDB_URL = "http://192.168.0.167:8086";
static const char* INFLUXDB_ORG = "d990ccd978a70382";
static const char* INFLUXDB_BUCKET = "sensor_data";
static const char* INFLUXDB_TOKEN = "Mqj3XYZ_example_token_abc123==";

static const char* MQTT_BROKER = "192.168.0.167";
static const int MQTT_PORT = 1883;
```

Remember to replace all values with your actual credentials!
