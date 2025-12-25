#!/bin/bash
# Source device configuration from .env file
# This script loads IP addresses and credentials for device management

# Determine .env file location (allow override via ENV_FILE, default to project root)
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "${script_dir}/.." && pwd)"
ENV_FILE="${ENV_FILE:-${project_root}/.env}"

set -a  # Export all variables
if [ -f "$ENV_FILE" ]; then
    source "$ENV_FILE"
else
    echo "ERROR: .env file not found at $ENV_FILE"
    echo "Please copy .env.example to .env and fill in your values:"
    echo "  cp .env.example .env"
    exit 1
fi
set +a  # Stop exporting

# Verify required variables are set
required_vars=("OTA_PASSWORD" "ESP32DEV_IP" "ESP8266_IP" "ESP32_SMALL_GARAGE_IP" "MQTT_BROKER_IP")
for var in "${required_vars[@]}"; do
    if [ -z "${!var}" ]; then
        echo "ERROR: Required variable $var is not set in .env file"
        exit 1
    fi
done

# Export for use in platformio.ini
export OTA_PASSWORD
export ESP32DEV_IP
export ESP8266_IP
export ESP32_SMALL_GARAGE_IP
export MQTT_BROKER_IP
export MQTT_BROKER_PORT

echo "✓ Device configuration loaded from .env"
echo "  - OTA Password: ${OTA_PASSWORD:0:4}****"
echo "  - ESP32 Dev IP: $ESP32DEV_IP"
echo "  - ESP8266 IP: $ESP8266_IP"
echo "  - ESP32-S3 IP: $ESP32_SMALL_GARAGE_IP"
echo "  - MQTT Broker: $MQTT_BROKER_IP:$MQTT_BROKER_PORT"
