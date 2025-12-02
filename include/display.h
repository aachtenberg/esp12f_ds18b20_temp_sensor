#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// Hardware: SSD1306 0.96" 128x64 I2C OLED Display
// I2C Connections for ESP8266:
// - SDA: GPIO 0 (D3)
// - SCL: GPIO 5 (D1)
// - VCC: 3.3V
// - GND: GND
//
// Note: DS18B20 temperature sensor uses GPIO 4 (D2)

// =============================================================================
// CONFIGURATION
// =============================================================================

// Set to 1 to enable OLED display, 0 to disable
// When disabled, stub functions are used to prevent crashes if hardware is not connected
#define OLED_ENABLED 0

#if OLED_ENABLED

// Display Hardware Configuration
#define DISPLAY_I2C_ADDRESS 0x3C
#define DISPLAY_SDA_PIN 0  // GPIO 0 (D3)
#define DISPLAY_SCL_PIN 5  // GPIO 5 (D1)

// Display Update Timing
#define DISPLAY_UPDATE_INTERVAL 1000  // milliseconds

#endif // OLED_ENABLED

// =============================================================================
// PUBLIC API
// =============================================================================

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
 */
void updateDisplay(const char* tempC, const char* tempF, bool wifiConnected, const char* ipAddress);

#endif // DISPLAY_H
