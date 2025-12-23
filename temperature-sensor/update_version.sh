#!/bin/bash
# Version Update Script for ESP32 Temperature Sensor
# Run this script before building a new OTA release

# Get current date in YYYYMMDD format
CURRENT_DATE=$(date +%Y%m%d)

# Read current platformio.ini
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLATFORMIO_INI="$SCRIPT_DIR/platformio.ini"

# Extract current version components
MAJOR=$(grep "FIRMWARE_VERSION_MAJOR" "$PLATFORMIO_INI" | cut -d'=' -f2 | tr -d ' ')
MINOR=$(grep "FIRMWARE_VERSION_MINOR" "$PLATFORMIO_INI" | cut -d'=' -f2 | tr -d ' ')
PATCH=$(grep "FIRMWARE_VERSION_PATCH" "$PLATFORMIO_INI" | cut -d'=' -f2 | tr -d ' ')

echo "Current version: $MAJOR.$MINOR.$PATCH"
echo "New build timestamp: $CURRENT_DATE"

# Update the build timestamp
sed -i "s/-D BUILD_TIMESTAMP=[0-9]*/-D BUILD_TIMESTAMP=$CURRENT_DATE/" "$PLATFORMIO_INI"

echo "Version updated. Ready for build."
echo "New version will be: $MAJOR.$MINOR.$PATCH-build$CURRENT_DATE"