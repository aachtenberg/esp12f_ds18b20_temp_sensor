#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "device_config.h"

// Hardware: SSD1306 0.96" 128x64 I2C OLED Display
// I2C Connections:
//   ESP8266: SDA=GPIO 0 (D3), SCL=GPIO 5 (D1)
//   ESP32:   SDA=GPIO 21, SCL=GPIO 22
// - VCC: 3.3V
// - GND: GND
//
// Note: DS18B20 temperature sensor uses GPIO 4 (D2)

// =============================================================================
// CONFIGURATION
// =============================================================================

// Set to 1 to enable OLED display, 0 to disable
// When disabled, stub functions are used to prevent crashes if hardware is not connected
// OLED_ENABLED is set in device_config.h - don't override it here
// Default to 1 only if not already defined elsewhere
#if !defined(OLED_ENABLED)
#define OLED_ENABLED 1
#endif

#if OLED_ENABLED

// Display Hardware Configuration
#define DISPLAY_I2C_ADDRESS 0x3C
#if defined(ESP8266)
#define DISPLAY_SDA_PIN 0   // GPIO 0 (D3) on ESP8266
#define DISPLAY_SCL_PIN 5   // GPIO 5 (D1) on ESP8266
#else
#define DISPLAY_SDA_PIN 21  // GPIO 21 (ESP32 standard I2C SDA)
#define DISPLAY_SCL_PIN 22  // GPIO 22 (ESP32 standard I2C SCL)
#endif

// Display Update Timing
#define DISPLAY_UPDATE_INTERVAL 1000  // milliseconds

// Display Gating for Power Saving
// When gating is enabled, display only shows for 10s per minute (saves ~60% OLED power)
// Import OLED_GATE_ENABLED from device_config.h
#ifndef OLED_GATE_ENABLED
#define OLED_GATE_ENABLED 0  // Default: always on if not specified
#endif

#if OLED_GATE_ENABLED
#ifndef OLED_ON_DURATION_MS
#define OLED_ON_DURATION_MS 10000      // Display on for 10 seconds
#endif
#ifndef OLED_CYCLE_DURATION_MS
#define OLED_CYCLE_DURATION_MS 60000   // Full cycle is 60 seconds
#endif
#endif  // OLED_GATE_ENABLED

#endif // OLED_ENABLED

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * Check if OLED display should be on (respects gating schedule)
 * Call this before updateDisplay() to honor power-save gating
 */
bool isDisplayOnWindow();

/**
 * Initialize the OLED display
 * Call this once in setup() after initializing other hardware
 */
void initDisplay();

/**
 * Update the OLED display with current temperature and status
 *
 * @param tempC Temperature in Celsius (as string, e.g., "22.50")
 * @param tempF Temperature in Fahrenheit (as string, e.g., "72.50")
 * @param wifiConnected WiFi connection status (true = connected)
 * @param ipAddress IP address string (e.g., "192.168.1.100") or empty if not connected
 * @param batteryPercent Battery percentage (-1 if not available or not monitoring)
 */
void updateDisplay(const char* tempC, const char* tempF, bool wifiConnected, const char* ipAddress, int batteryPercent);

#endif // DISPLAY_H
