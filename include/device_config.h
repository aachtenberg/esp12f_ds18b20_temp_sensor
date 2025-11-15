#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

/**
 * Device Configuration
 * Each ESP device can have a unique location name and board type
 * Update DEVICE_LOCATION and DEVICE_BOARD for each device before flashing
 */

// Data wire is connected to GPIO 4 on ESP8266/ESP32
static const int ONE_WIRE_PIN = 4;

// Device location name (appears in InfluxDB, Lambda logs, and Home Assistant)
// Examples: "Big Garage", "Bedroom", "Living Room", "Basement", "Attic", etc.
static const char* DEVICE_LOCATION = "Big Garage";

// Optional: Device ID (useful for Home Assistant entity identification)
// If empty, will use chip ID (MAC address)
static const char* DEVICE_ID = "";

// Device board type - MUST match the physical hardware!
// Options: "esp8266" or "esp32"
// This is set automatically by flash_device.sh, but you can override here if needed
static const char* DEVICE_BOARD = "esp8266";

// Device timezone offset (hours from UTC)
// Used for logging timestamps
static const int TIMEZONE_OFFSET = -5;  // EST/CDT

// Network timing constants
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;
static const unsigned long WIFI_CHECK_INTERVAL_MS = 30000;
static const unsigned long TEMPERATURE_READ_INTERVAL_MS = 30000;
static const int MAX_WIFI_ATTEMPTS = 20;
static const int HTTP_TIMEOUT_MS = 10000;

// Exponential backoff constants
static const unsigned long MIN_BACKOFF_MS = 1000;      // 1 second
static const unsigned long MAX_BACKOFF_MS = 300000;    // 5 minutes
static const int MAX_CONSECUTIVE_FAILURES = 10;

#endif // DEVICE_CONFIG_H
