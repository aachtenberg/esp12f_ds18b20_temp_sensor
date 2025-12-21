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
static const char* DEVICE_BOARD = "esp8266";

// Timing constants
static const unsigned long WIFI_CHECK_INTERVAL_MS = 15000;    // Check WiFi connection every 15s
static const unsigned long TEMPERATURE_READ_INTERVAL_MS = 30000;  // Read temperature every 30s
// HTTP timeout: ESP8266 needs shorter timeout to prevent watchdog resets
#ifdef ESP8266
  static const int HTTP_TIMEOUT_MS = 5000;   // 5s timeout for ESP8266
#else
  static const int HTTP_TIMEOUT_MS = 10000;  // 10s timeout for ESP32
#endif

// =============================================================================
// LOW-POWER CONFIGURATION (Per-Board Profiles)
// =============================================================================

// CPU Frequency (MHz) - Lower freq reduces power consumption
#ifdef ESP8266
  #define CPU_FREQ_MHZ 80  // ESP8266: 80 MHz (vs 160 MHz default)
#elif defined(ESP32) || defined(ESP32S3)
  #define CPU_FREQ_MHZ 80  // ESP32/S3: 80 MHz (vs 240 MHz default)
#endif

// WiFi Power Save Mode
#ifdef ESP8266
  #define WIFI_PS_MODE WIFI_LIGHT_SLEEP  // ESP8266: Light sleep mode
#elif defined(ESP32) || defined(ESP32S3)
  #define WIFI_PS_MODE WIFI_PS_MIN_MODEM // ESP32/S3: Minimum modem sleep
#endif

// OLED Display Gating (1 = gate display, 0 = always on)
// Battery-powered devices: gate to 10s on / 50s off per minute (saves ~60% OLED power)
// Mains-powered devices: always on for monitoring
#ifdef BATTERY_POWERED
  #define OLED_GATE_ENABLED 1             // Gate OLED for battery operation
  #define OLED_ON_DURATION_MS 10000       // Display on for 10 seconds
  #define OLED_CYCLE_DURATION_MS 60000    // Full cycle is 60 seconds (on 10s, off 50s)
  #define HTTP_SERVER_ENABLED 0           // Disable HTTP server for battery
#else
  #define OLED_GATE_ENABLED 0             // Always on for mains-powered
  #define HTTP_SERVER_ENABLED 1           // Enable HTTP server for mains-powered
#endif

#endif // DEVICE_CONFIG_H
