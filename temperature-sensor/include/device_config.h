#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

/**
 * Device Configuration
 * Per-device hardware settings and timing defaults
 */

// Data wire is connected to GPIO 4 on ESP8266/ESP32
static const int ONE_WIRE_PIN = 4;

// Device board type - MUST match the physical hardware!
// Options: "esp8266", "esp32", or "esp32s3"
static const char* DEVICE_BOARD = "esp32";

// Timing constants
static const unsigned long WIFI_CHECK_INTERVAL_MS = 15000;    // Check WiFi connection every 15s
static const unsigned long TEMPERATURE_READ_INTERVAL_MS = 15000;  // Read temperature every 15s
static const int HTTP_TIMEOUT_MS = 10000;  // HTTP request timeout

#endif // DEVICE_CONFIG_H
