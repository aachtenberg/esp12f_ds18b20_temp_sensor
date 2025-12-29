#!/bin/bash
# Upload LittleFS filesystem to ESP32 devices
# Usage: ./UPLOAD_LITTLEFS.sh [port] [chip]
#
# Examples:
#   ./UPLOAD_LITTLEFS.sh                    # ESP32-CAM on /dev/ttyUSB0
#   ./UPLOAD_LITTLEFS.sh /dev/ttyACM0 s3    # ESP32-S3 on /dev/ttyACM0
#   ./UPLOAD_LITTLEFS.sh /dev/ttyUSB0 esp32 # ESP32-CAM on /dev/ttyUSB0
#
# This uploads the data/ folder contents to the LittleFS partition
# Required for serving gzipped web content (reduces 27KB to 6.7KB)

set -e

PORT=${1:-/dev/ttyUSB0}
CHIP=${2:-esp32}
SKETCH_DIR="ESP32CAM_Surveillance"
DATA_DIR="$SKETCH_DIR/data"

# Tool paths (installed with ESP32 Arduino core)
MKLITTLEFS="$HOME/.arduino15/packages/esp32/tools/mklittlefs/4.0.2-db0513a/mklittlefs"
ESPTOOL="$HOME/.arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool"

# Partition layout (huge_app scheme):
# - APP: 0x10000, size 3MB (3072KB)
# - SPIFFS/LittleFS: 0x310000, size 0xE0000 (896KB)
LITTLEFS_ADDR="0x310000"
LITTLEFS_SIZE="0xE0000"  # 896KB

# Normalize chip type
case "$CHIP" in
    s3|S3|esp32s3|ESP32S3|esp32-s3)
        CHIP="esp32s3"
        BAUD=921600
        CHIP_NAME="ESP32-S3"
        ;;
    *)
        CHIP="esp32"
        BAUD=460800
        CHIP_NAME="ESP32-CAM"
        ;;
esac

echo "📁 LittleFS Upload for $CHIP_NAME"
echo "   Port: $PORT"
echo "   Chip: $CHIP"
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
$ESPTOOL --chip $CHIP --port "$PORT" --baud $BAUD \
    write-flash $LITTLEFS_ADDR "$LITTLEFS_BIN"

echo ""
echo "✅ LittleFS upload complete!"
echo ""
echo "The device will now serve the gzipped web page from LittleFS."
echo "Reset the device or power cycle to apply changes."
