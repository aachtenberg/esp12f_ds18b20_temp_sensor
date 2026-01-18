# Firmware Version Tracking

## Overview

The ESP32 temperature sensor now includes automatic firmware version tracking that updates with each build. This allows you to track which firmware version is running on each device and monitor OTA update success.

## Version Format

Versions follow the format: `MAJOR.MINOR.PATCH-buildYYYYMMDD`

Example: `1.0.48-build20260118`

- **MAJOR**: Major version (breaking changes)
- **MINOR**: Minor version (new features)
- **PATCH**: Patch version (bug fixes)
- **buildYYYYMMDD**: Build timestamp (automatically updated)

## Where Versions Appear

### MQTT Messages

All MQTT messages now include the firmware version:

**Temperature Data:**
```json
{
  "device": "Temp Sensor",
  "firmware_version": "1.0.48-build20260118",
  "current_temp_c": 23.5,
  "current_temp_f": 74.3
}
```

**Status Updates:**
```json
{
  "device": "Temp Sensor",
  "firmware_version": "1.0.48-build20260118",
  "wifi_connected": true,
  "uptime_seconds": 3600
}
```

**OTA Events:**
```json
{
  "device": "Temp Sensor",
  "firmware_version": "1.0.48-build20260118",
  "event": "ota_start",
  "message": "OTA update starting (sketch)"
}
```

## Updating Versions

### Automatic Build Timestamp

Run the version update script before each build:

```bash
cd temperature-sensor
./update_version.sh
```

This updates the build timestamp to the current date.

### Manual Version Bumps

For major/minor/patch version changes, edit `platformio.ini`:

```ini
build_flags =
    -D CPU_FREQ_MHZ=80
    -D WIFI_PS_MODE=WIFI_PS_MIN_MODEM
    -D FIRMWARE_VERSION_MAJOR=1
    -D FIRMWARE_VERSION_MINOR=0
    -D FIRMWARE_VERSION_PATCH=48  # Increment for bug fixes
    -D BUILD_TIMESTAMP=20260118
```

## OTA Version Tracking

When you perform an OTA update:

1. **Before OTA**: Device reports old version (e.g., `1.0.47-build20260115`)
2. **OTA Start**: Device publishes `ota_start` event with old version
3. **OTA Complete**: Device reboots with new firmware
4. **After OTA**: Device reports new version (e.g., `1.0.48-build20260118`)

This allows you to verify OTA success by checking version changes in MQTT messages.

## Implementation Details

- **version.h**: Defines version string construction from build flags
- **platformio.ini**: Contains version components as build flags
- **main.cpp**: Uses `getFirmwareVersion()` function in all MQTT messages
- **update_version.sh**: Script to update build timestamps

## Benefits

- **OTA Verification**: Confirm updates succeeded by checking version changes
- **Device Inventory**: Track which firmware versions are deployed
- **Debugging**: Correlate issues with specific firmware versions
- **Compliance**: Track software versions for regulatory requirements