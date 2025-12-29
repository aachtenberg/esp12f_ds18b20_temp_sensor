#!/bin/bash
# Compile script for Freenove ESP32-S3 WROOM (OV3660 camera)
# Usage: ./COMPILE_ESP32S3.sh
#
# This script compiles the ESP32-CAM Surveillance sketch for the Freenove ESP32-S3 WROOM board.
# The board macro ARDUINO_FREENOVE_ESP32_S3_WROOM is force-defined in the sketch code because
# Arduino CLI does not automatically set it even when using --fqbn esp32:esp32:esp32s3.
#
# For other boards:
#   - ESP32-CAM (legacy): ./COMPILE_ESP32CAM.sh
#   - ESP32 DevKit (testing): ./bin/arduino-cli compile --fqbn esp32:esp32:esp32 ESP32CAM_Surveillance

set -e

echo "🔧 Compiling ESP32-CAM Surveillance for ESP32-S3..."
echo "   Board: Freenove ESP32-S3 WROOM"
echo "   Camera: OV3660 (auto-detected)"
echo ""

./bin/arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=qio --output-dir ESP32CAM_Surveillance/build \
  ESP32CAM_Surveillance \
  --verify

echo ""
echo "✅ Compilation successful for ESP32-S3!"
echo ""
echo "📊 ESP32-S3 Features:"
echo "   - OV3660 camera (higher resolution)"
echo "   - 8MB PSRAM support (QVGA fallback to DRAM if unavailable)"
echo "   - NVS-based triple-reset detection"
echo "   - SD_MMC in 1-bit mode (D1/D2/D3 not connected on Freenove board)"
echo ""
echo "To upload to device:"
echo "  ./bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=qio ESP32CAM_Surveillance"
echo ""
echo "To monitor serial output:"
echo "  ./bin/arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200"
