#!/bin/bash
# Compile script for AI Thinker ESP32-CAM (OV2640)
# Usage: ./COMPILE_ESP32CAM.sh
# 
# NOTE: This is legacy support. For new projects, use ESP32-S3 (COMPILE_ESP32S3.sh)

set -e

echo "🔧 Compiling ESP32-CAM Surveillance for ESP32-CAM..."
echo "   Board: AI Thinker ESP32-CAM"
echo "   Camera: OV2640 (legacy)"
echo ""

./bin/arduino-cli compile \
  --fqbn esp32:esp32:esp32cam \
  ESP32CAM_Surveillance \
  --verify

echo ""
echo "✅ Compilation successful for ESP32-CAM!"
echo ""
echo "📊 ESP32-CAM Features:"
echo "   - OV2640 camera (lower resolution)"
echo "   - No PSRAM (uses DRAM only)"
echo "   - SD_MMC in 2-bit mode (default)"
echo "   - Triple-reset detection via RTC memory"
echo ""
echo "To upload to device:"
echo "  ./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance"
echo ""
echo "To monitor serial output:"
echo "  ./bin/arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200"
