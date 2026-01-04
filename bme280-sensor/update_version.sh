#!/bin/bash

# BME280 Sensor - Firmware Version Update Script
# Updates version numbers in platformio.ini and sets build timestamp

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INI_FILE="$SCRIPT_DIR/platformio.ini"

# Parse arguments
BUMP_TYPE="${1:-timestamp}"  # Default: update timestamp only

# Get current version from platformio.ini
MAJOR=$(grep "FIRMWARE_VERSION_MAJOR=" "$INI_FILE" | head -1 | cut -d'=' -f2 | xargs)
MINOR=$(grep "FIRMWARE_VERSION_MINOR=" "$INI_FILE" | head -1 | cut -d'=' -f2 | xargs)
PATCH=$(grep "FIRMWARE_VERSION_PATCH=" "$INI_FILE" | head -1 | cut -d'=' -f2 | xargs)

echo "Current version: $MAJOR.$MINOR.$PATCH"

# Handle version bumping
case "$BUMP_TYPE" in
  --major)
    MAJOR=$((MAJOR + 1))
    MINOR=0
    PATCH=0
    echo "Bumped to: $MAJOR.$MINOR.$PATCH (major)"
    ;;
  --minor)
    MINOR=$((MINOR + 1))
    PATCH=0
    echo "Bumped to: $MAJOR.$MINOR.$PATCH (minor)"
    ;;
  --patch)
    PATCH=$((PATCH + 1))
    echo "Bumped to: $MAJOR.$MINOR.$PATCH (patch)"
    ;;
  timestamp|"")
    echo "Updating timestamp only"
    ;;
  *)
    echo "Usage: $0 [--major|--minor|--patch|timestamp]"
    exit 1
    ;;
esac

# Update build timestamp to current date (YYYYMMDD format)
BUILD_DATE=$(date +%Y%m%d)

echo "Updating BUILD_TIMESTAMP to $BUILD_DATE"

# Update all occurrences in platformio.ini
# Use portable sed in-place command that works on both GNU sed (Linux) and BSD sed (macOS)
if sed --version 2>/dev/null | grep -q "GNU sed"; then
  SED_INPLACE=(sed -i -E)
else
  # macOS / BSD sed requires an explicit backup extension argument for -i
  SED_INPLACE=(sed -i '' -E)
fi

"${SED_INPLACE[@]}" "s/(FIRMWARE_VERSION_MAJOR=).*/\1$MAJOR/g" "$INI_FILE"
"${SED_INPLACE[@]}" "s/(FIRMWARE_VERSION_MINOR=).*/\1$MINOR/g" "$INI_FILE"
"${SED_INPLACE[@]}" "s/(FIRMWARE_VERSION_PATCH=).*/\1$PATCH/g" "$INI_FILE"
"${SED_INPLACE[@]}" "s/(BUILD_TIMESTAMP=).*/\1$BUILD_DATE/g" "$INI_FILE"

echo "✓ Updated version to $MAJOR.$MINOR.$PATCH-build$BUILD_DATE"
echo "✓ All environments updated"
