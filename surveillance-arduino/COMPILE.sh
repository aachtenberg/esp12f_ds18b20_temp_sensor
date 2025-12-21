#!/bin/bash
# Quick compile script for ESP32-CAM Surveillance
# Usage: ./COMPILE.sh

set -e

echo "🔧 Compiling ESP32-CAM Surveillance..."
echo ""

./bin/arduino-cli compile \
  --fqbn esp32:esp32:esp32cam \
  --build-property build.partitions=huge_app \
  ESP32CAM_Surveillance \
  --verify

echo ""
echo "✅ Compilation successful!"
echo ""
echo "To upload to device:"
echo "  ./bin/arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32cam ESP32CAM_Surveillance"
echo ""
echo "To monitor serial output:"
echo "  ./bin/arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200"
