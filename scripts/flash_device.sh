#!/bin/bash
# Flash ESP device (Temperature Sensor or Solar Monitor)
# Usage: ./flash_device.sh [project_type] [board_type] [device_name]
# project_type: temp or solar (default: temp)
# board_type: esp8266 or esp32 (default: auto-detect from project)
# device_name: optional, will be configurable via WiFiManager portal

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Show usage if needed (check first)
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "Usage: $0 [project_type] [board_type] [device_name]"
    echo ""
    echo "Arguments:"
    echo "  project_type: temp or solar (default: temp)"
    echo "  board_type:   esp8266 or esp32 (default: auto-detect)"
    echo "  device_name:  optional device name (configurable via WiFi portal)"
    echo ""
    echo "Examples:"
    echo "  $0                           # Flash temperature sensor (ESP8266)"
    echo "  $0 solar                     # Flash solar monitor (ESP32)"
    echo "  $0 temp esp32                # Flash temp sensor on ESP32"
    echo "  $0 solar esp32 'Solar House' # Flash solar monitor with name"
    echo ""
    echo "Note: Device names can be set through WiFiManager portal after flashing"
    exit 0
fi

PROJECT_TYPE="${1:-temp}"
BOARD="${2:-}"
DEVICE_NAME="${3:-}"

# Validate project type
if [ "$PROJECT_TYPE" != "temp" ] && [ "$PROJECT_TYPE" != "solar" ]; then
    echo "❌ Invalid project type: $PROJECT_TYPE"
    echo "Valid options: temp, solar"
    exit 1
fi

# Set project directory and default board based on project type
if [ "$PROJECT_TYPE" = "solar" ]; then
    BUILD_DIR="$PROJECT_DIR/solar-monitor"
    DEFAULT_BOARD="esp32"
    PROJECT_NAME="Solar Monitor"
elif [ "$PROJECT_TYPE" = "surveillance" ]; then
    BUILD_DIR="$PROJECT_DIR/surveillance"
    DEFAULT_BOARD="esp32s3"
    PROJECT_NAME="Surveillance Camera"
else
    BUILD_DIR="$PROJECT_DIR/temperature-sensor"
    DEFAULT_BOARD="esp8266" 
    PROJECT_NAME="Temperature Sensor"
fi

# Use provided board or default
BOARD="${BOARD:-$DEFAULT_BOARD}"

# Validate board type
if [ "$BOARD" != "esp8266" ] && [ "$BOARD" != "esp32" ]; then
    echo "❌ Invalid board type: $BOARD"
    echo "Valid options: esp8266, esp32"
    exit 1
fi

# Map board type to PlatformIO environment
if [ "$BOARD" = "esp8266" ]; then
    ENV="esp8266"
else
    ENV="esp32dev"
fi

echo "================================"
echo "ESP $PROJECT_NAME Flasher"
echo "================================"
echo ""
echo "Project Type: $PROJECT_TYPE ($PROJECT_NAME)"
echo "Board Type: $BOARD"
echo "Environment: $ENV"
echo "Build Directory: $BUILD_DIR"
if [ -n "$DEVICE_NAME" ]; then
    echo "Device Name: $DEVICE_NAME (configurable via WiFi portal)"
fi
echo ""

# Note: Device names are now configured via WiFiManager portal
# No need to modify device_config.h - that's the old approach

echo ""
echo "================================"
echo "Building firmware..."
echo "================================"
cd "$BUILD_DIR"
platformio run -e "$ENV" 2>&1 | tail -5

if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo ""
echo "================================"
echo "Checking for device..."
echo "================================"

PORTS=($(ls /dev/ttyUSB* 2>/dev/null))

if [ ${#PORTS[@]} -eq 0 ]; then
    echo "❌ No USB device found!"
    echo "Connect ESP device and try again"
    exit 1
fi

if [ ${#PORTS[@]} -eq 1 ]; then
    PORT="${PORTS[0]}"
    echo "✅ Found device: $PORT"
else
    echo "Found multiple devices:"
    for i in "${!PORTS[@]}"; do
        echo "  $((i+1)). ${PORTS[$i]}"
    done
    echo ""
    read -p "Select device number: " num
    PORT="${PORTS[$((num-1))]}"
fi

echo ""
echo "================================"
echo "Uploading firmware..."
echo "Project: $PROJECT_NAME ($BOARD)"
echo "Port: $PORT"
echo "================================"
platformio run --target upload -e "$ENV" --upload-port "$PORT" 2>&1 | tail -10

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ SUCCESS! $PROJECT_NAME flashed successfully ($BOARD)"
    echo ""
    if [ "$PROJECT_TYPE" = "solar" ]; then
        echo "Device will create 'SolarMonitor-Setup' WiFi AP for configuration"
    else
        echo "Device will create 'Temp-*-Setup' WiFi AP for configuration"
    fi
    echo "Connect to the AP and configure WiFi + device name via web portal"
    echo ""
    read -p "Monitor serial output? (y/n): " monitor
    if [ "$monitor" = "y" ]; then
        echo ""
        echo "Opening serial monitor (Ctrl+C to exit)..."
        sleep 2
        platformio device monitor -p "$PORT" -b 115200
    fi
else
    echo ""
    echo "❌ Upload failed"
    exit 1
fi
