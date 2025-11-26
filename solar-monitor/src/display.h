/**
 * OLED Display Module for ESP32 Solar Monitor
 * 
 * Hardware: SSD1306 0.96" 128x64 I2C OLED Display
 * Library: U8g2
 * 
 * I2C Connections:
 * - SDA: GPIO 21
 * - SCL: GPIO 22
 * - VCC: 3.3V
 * - GND: GND
 */

#ifndef DISPLAY_H
#define DISPLAY_H

// ============================================================================
// OLED Enable/Disable Flag
// Set to 0 to disable OLED (when hardware not connected)
// Set to 1 to enable OLED display
// ============================================================================
#define OLED_ENABLED 0

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// Display configuration
#define DISPLAY_I2C_ADDRESS 0x3C
#define DISPLAY_SDA_PIN 21
#define DISPLAY_SCL_PIN 22

// Display update interval (ms)
#define DISPLAY_UPDATE_INTERVAL 1000

// Display pages for cycling through data
enum DisplayPage {
    PAGE_BATTERY,
    PAGE_SOLAR,
    PAGE_SYSTEM,
    PAGE_COUNT
};

// Display instance - SSD1306 128x64 I2C
// Using hardware I2C (U8G2_R0 = no rotation)
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C display;

// Current display page
extern DisplayPage currentPage;
extern unsigned long lastPageChange;
extern const unsigned long PAGE_CYCLE_INTERVAL;

// Function declarations
void initDisplay();
void updateDisplay(float batteryPercent, float batteryVoltage, float batteryCurrent,
                   float solarPower1, float solarPower2, bool wifiConnected, const char* ipAddress);
void drawBatteryPage(float percent, float voltage, float current);
void drawSolarPage(float power1, float power2);
void drawSystemPage(bool wifiConnected, const char* ipAddress, unsigned long uptimeMs);
void drawProgressBar(int x, int y, int width, int height, int percent);
void nextDisplayPage();

#endif // DISPLAY_H
