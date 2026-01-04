#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

/**
 * Device Configuration
 * Per-device hardware settings and timing defaults
 */

// Data wire is connected to GPIO 4 on ESP8266/ESP32
static const int ONE_WIRE_PIN = 4;

// Device board type - Auto-detected from build environment
// Options: "esp8266", "esp32", or "esp32s3"
#if defined(ESP32S3)
  static const char* DEVICE_BOARD = "esp32s3";
#elif defined(ESP32)
  static const char* DEVICE_BOARD = "esp32";
#else
  static const char* DEVICE_BOARD = "esp8266";
#endif

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
// BATTERY MONITORING (Optional)
// =============================================================================
// Uncomment to enable battery voltage monitoring (ESP32 only)
// Requires voltage divider on GPIO 34: Battery+ -> 10K -> GPIO34 -> 10K -> GND
// Complete battery setup with TP4056 charger: see docs/hardware/BATTERY_SETUP_GUIDE.md
// #define BATTERY_MONITOR_ENABLED

// Only enable battery monitoring on ESP32
#ifdef ESP32
  #define BATTERY_MONITOR_ENABLED
#endif

// BATTERY_POWERED: Set via build flags in platformio.ini (per-device)
#ifndef BATTERY_POWERED
  #define BATTERY_POWERED 0
#endif

// OLED display: enabled via build flag for specific devices (default disabled for battery saving)
#ifndef OLED_ENABLED
  #define OLED_ENABLED 0
#endif

// API_ENDPOINTS_ONLY: Set via build flags in platformio.ini (per-device)
// When enabled, HTTP server provides only JSON endpoints (no HTML dashboard)
#ifndef API_ENDPOINTS_ONLY
  #define API_ENDPOINTS_ONLY 0
#endif

#ifdef BATTERY_MONITOR_ENABLED
  #ifdef ESP32
    static const int BATTERY_PIN = 34;           // ADC pin for battery voltage
    static const float VOLTAGE_DIVIDER = 2.0;    // Voltage divider ratio (R1/(R1+R2))
    static const float CALIBRATION = 1.134;      // Calibration factor based on actual measurements
    static const float ADC_MAX = 4095.0;         // 12-bit ADC
    static const float REF_VOLTAGE = 3.3;        // ESP32 reference voltage
    static const float BATTERY_MIN_V = 3.0;      // 0% battery voltage
    static const float BATTERY_MAX_V = 4.2;      // 100% battery voltage
  #else
    #error "Battery monitoring is only supported on ESP32"
  #endif
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
#if BATTERY_POWERED && !defined(OLED_ALWAYS_ON)
  #define OLED_GATE_ENABLED 1             // Gate OLED for battery operation
  #define OLED_ON_DURATION_MS 10000       // Display on for 10 seconds
  #define OLED_CYCLE_DURATION_MS 60000    // Full cycle is 60 seconds (on 10s, off 50s)
  #define HTTP_SERVER_ENABLED 0           // Disable HTTP server for battery
#else
  #define OLED_GATE_ENABLED 0             // Always on for mains-powered or OLED_ALWAYS_ON
  #define HTTP_SERVER_ENABLED 1           // Enable HTTP server for mains-powered
#endif

// Override HTTP server for API-only mode (if requested)
#if API_ENDPOINTS_ONLY
  #undef HTTP_SERVER_ENABLED
  #define HTTP_SERVER_ENABLED 1           // Enable HTTP for JSON endpoints only
#endif

// HTTP API Mode (only used if HTTP_SERVER_ENABLED = 1)
// When enabled, serves only JSON endpoints (/health, /temperaturec, /temperaturef)
// Disables HTML dashboard (/). Saves memory and reduces bandwidth.
// #define API_ENDPOINTS_ONLY

#endif // DEVICE_CONFIG_H
