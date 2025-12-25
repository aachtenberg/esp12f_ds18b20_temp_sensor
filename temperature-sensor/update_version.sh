#!/bin/bash
# Version Update Script for ESP32 Temperature Sensor
# Run this script before building a new OTA release
# Usage: ./update_version.sh [--patch|--minor|--major]

# Get current date in YYYYMMDD format
CURRENT_DATE=$(date +%Y%m%d)

# Read current platformio.ini
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLATFORMIO_INI="$SCRIPT_DIR/platformio.ini"

# Extract current version components (handle both -D format)
MAJOR=$(grep "FIRMWARE_VERSION_MAJOR" "$PLATFORMIO_INI" | head -1 | grep -o '[0-9]*$')
MINOR=$(grep "FIRMWARE_VERSION_MINOR" "$PLATFORMIO_INI" | head -1 | grep -o '[0-9]*$')
PATCH=$(grep "FIRMWARE_VERSION_PATCH" "$PLATFORMIO_INI" | head -1 | grep -o '[0-9]*$')

echo "Current version: $MAJOR.$MINOR.$PATCH"

# Handle version bumping
BUMP_TYPE="${1:-patch}"
case "$BUMP_TYPE" in
  --patch)
    PATCH=$((PATCH + 1))
    ;;
  --minor)
    MINOR=$((MINOR + 1))
    PATCH=0
    ;;
  --major)
    MAJOR=$((MAJOR + 1))
    MINOR=0
    PATCH=0
    ;;
esac

echo "New version: $MAJOR.$MINOR.$PATCH"
echo "New build timestamp: $CURRENT_DATE"

# Update all version components in platformio.ini
sed -i "s/-D FIRMWARE_VERSION_MAJOR=[0-9]*/-D FIRMWARE_VERSION_MAJOR=$MAJOR/g" "$PLATFORMIO_INI"
sed -i "s/-D FIRMWARE_VERSION_MINOR=[0-9]*/-D FIRMWARE_VERSION_MINOR=$MINOR/g" "$PLATFORMIO_INI"
sed -i "s/-D FIRMWARE_VERSION_PATCH=[0-9]*/-D FIRMWARE_VERSION_PATCH=$PATCH/g" "$PLATFORMIO_INI"
sed -i "s/-D BUILD_TIMESTAMP=[0-9]*/-D BUILD_TIMESTAMP=$CURRENT_DATE/g" "$PLATFORMIO_INI"

echo "Version updated. Ready for build."
echo "New version will be: $MAJOR.$MINOR.$PATCH-build$CURRENT_DATE"