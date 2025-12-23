#!/bin/bash
# Upload LittleFS filesystem to ESP32-CAM
# Usage: ./UPLOAD_LITTLEFS.sh [port]
#
# This uploads the data/ folder contents to the LittleFS partition
# Required for serving gzipped web content (reduces 27KB to 6.7KB)

set -e

PORT=${1:-/dev/ttyUSB0}
SKETCH_DIR="ESP32CAM_Surveillance"
DATA_DIR="$SKETCH_DIR/data"

# Tool paths (installed with ESP32 Arduino core)
MKLITTLEFS="$HOME/.arduino15/packages/esp32/tools/mklittlefs/4.0.2-db0513a/mklittlefs"
ESPTOOL="$HOME/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool"

# ESP32-CAM partition layout (huge_app scheme):
# - App: 0x10000, size 3MB
# - SPIFFS/LittleFS: 0x290000, size 0xF0000 (960KB)
LITTLEFS_ADDR="0x290000"
LITTLEFS_SIZE="0xF0000"  # 960KB

echo "📁 LittleFS Upload for ESP32-CAM"
echo "   Port: $PORT"
echo "   Data folder: $DATA_DIR"
echo ""

# Check data folder exists
if [ ! -d "$DATA_DIR" ]; then
    echo "❌ Error: Data folder not found: $DATA_DIR"
    echo "   Create the folder and add files to upload"
    exit 1
fi

# Check tools exist
if [ ! -f "$MKLITTLEFS" ]; then
    echo "❌ Error: mklittlefs not found at $MKLITTLEFS"
    echo "   Make sure ESP32 Arduino core is installed"
    exit 1
fi

if [ ! -f "$ESPTOOL" ]; then
    echo "❌ Error: esptool not found at $ESPTOOL"
    echo "   Make sure ESP32 Arduino core is installed"
    exit 1
fi

# List files to upload
echo "📦 Files to upload:"
ls -lah "$DATA_DIR"
echo ""

# Create LittleFS image
LITTLEFS_BIN="$SKETCH_DIR/littlefs.bin"
echo "🔧 Creating LittleFS image..."
$MKLITTLEFS -c "$DATA_DIR" -s $LITTLEFS_SIZE -p 256 -b 4096 "$LITTLEFS_BIN"
echo "   Created: $LITTLEFS_BIN ($(du -h "$LITTLEFS_BIN" | cut -f1))"
echo ""

# Upload to device
echo "📤 Uploading to $PORT at address $LITTLEFS_ADDR..."
$ESPTOOL --chip esp32 --port "$PORT" --baud 460800 \
    write_flash $LITTLEFS_ADDR "$LITTLEFS_BIN"

echo ""
echo "✅ LittleFS upload complete!"
echo ""
echo "The device will now serve the gzipped web page from LittleFS."
echo "Reset the device or power cycle to apply changes."
