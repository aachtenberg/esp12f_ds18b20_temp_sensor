#include "display.h"

#if OLED_ENABLED

#include <Wire.h>
#include <U8g2lib.h>

// =============================================================================
// DISPLAY INSTANCE
// =============================================================================

// U8G2 instance for SSD1306 128x64 I2C display
// Using full frame buffer mode (_F) for smooth updates
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0,              // no rotation
    /* reset=*/ U8X8_PIN_NONE,  // no reset pin
    DISPLAY_SCL_PIN,      // SCL pin
    DISPLAY_SDA_PIN       // SDA pin
);

// =============================================================================
// DISPLAY GATING STATE (for power saving)
// =============================================================================

#if OLED_GATE_ENABLED
static unsigned long lastDisplayOnTime = 0;  // Track when display was last turned on
static bool displayShouldBeOn = true;        // Track current state
#endif

// Track display power state to avoid unnecessary I2C traffic
static bool displayIsPoweredOn = true;

// =============================================================================
// INITIALIZATION
// =============================================================================

void initDisplay() {
    Serial.println("[OLED] Initializing display...");

    // Initialize I2C with custom pins
    Wire.begin(DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);

    // Initialize U8G2 display
    if (!display.begin()) {
        Serial.println("[OLED] ERROR: Display initialization failed!");
        return;
    }

    // Configure display settings
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);  // Default font
    display.setFontRefHeightExtendedText();
    display.setDrawColor(1);  // White/on
    display.setFontPosTop();  // Position font at top
    display.setFontDirection(0);  // Left-to-right

    // Show startup screen
    display.setFont(u8g2_font_7x13B_tf);
    display.drawStr(20, 20, "Temperature");
    display.drawStr(30, 35, "Sensor");
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(35, 50, "Starting...");
    display.sendBuffer();

    #if OLED_GATE_ENABLED
    lastDisplayOnTime = millis();
    displayShouldBeOn = true;
    Serial.println("[OLED] Display gating enabled (10s on / 50s off per cycle)");
    #else
    Serial.println("[OLED] Display initialized successfully");
    #endif
}

// =============================================================================
// DISPLAY GATING LOGIC
// =============================================================================

bool isDisplayOnWindow() {
    #if !OLED_GATE_ENABLED
    return true;  // Always on if gating disabled
    #else
    unsigned long now = millis();
    unsigned long cyclePosition = now % OLED_CYCLE_DURATION_MS;
    
    // Display is on during first OLED_ON_DURATION_MS of each cycle
    if (cyclePosition < OLED_ON_DURATION_MS) {
        if (!displayShouldBeOn) {
            displayShouldBeOn = true;
            lastDisplayOnTime = now;
            Serial.println("[OLED] Display turned on");
        }
        return true;
    } else {
        if (displayShouldBeOn) {
            displayShouldBeOn = false;
            Serial.println("[OLED] Display turned off (power save)");
        }
        return false;
    }
    #endif
}

// =============================================================================
// DISPLAY UPDATE
// =============================================================================

void updateDisplay(const char* tempC, const char* tempF, bool wifiConnected, const char* ipAddress, int batteryPercent) {
    // Check battery level - disable display if battery powered and below 50%
    #ifdef BATTERY_POWERED
    if (batteryPercent >= 0 && batteryPercent < 50) {
        if (displayIsPoweredOn) {
            display.setPowerSave(1);  // Power off display to save battery
            displayIsPoweredOn = false;
        }
        return;
    } else if (batteryPercent >= 50) {
        if (!displayIsPoweredOn) {
            display.setPowerSave(0);  // Power on display when battery recovers
            displayIsPoweredOn = true;
        }
    }
    #endif
    
    // Check gating window
    if (!isDisplayOnWindow()) {
        return;  // Skip update if outside display window
    }
    
    display.clearBuffer();

    // Parse temperature string and format to 1 decimal place
    char tempCStr[16];

    // Convert input string to float and back to get 1 decimal precision
    float tempCVal = atof(tempC);

    // Format with 1 decimal place
    snprintf(tempCStr, sizeof(tempCStr), "%.1f", tempCVal);

    // Add degree symbol and unit
    strcat(tempCStr, "\xb0");  // degree symbol
    strcat(tempCStr, "C");

    // Draw Celsius temperature (extra large, centered, positioned below yellow zone)
    // Y position 18 keeps the 32-pixel tall font mostly in the blue zone (starts at row 18)
    display.setFont(u8g2_font_logisoso32_tn);  // Extra large number font (32 pixels tall)
    int tempCWidth = display.getStrWidth(tempCStr);
    display.drawStr((128 - tempCWidth) / 2, 18, tempCStr);

    // Draw WiFi status (bottom left)
    display.setFont(u8g2_font_5x8_tf);  // Small text font
    if (wifiConnected) {
        display.drawStr(4, 54, "WiFi: OK");
    } else {
        display.drawStr(4, 54, "WiFi: --");
    }

    // Draw IP address (if connected, centered at bottom)
    if (wifiConnected && ipAddress != nullptr && strlen(ipAddress) > 0) {
        // Truncate IP if too long (shouldn't happen for normal IPs)
        char ipStr[24];
        strncpy(ipStr, ipAddress, sizeof(ipStr) - 1);
        ipStr[sizeof(ipStr) - 1] = '\0';

        int ipWidth = display.getStrWidth(ipStr);
        // Center the IP address at bottom
        display.drawStr((128 - ipWidth) / 2, 54, ipStr);
    }

    display.sendBuffer();
}

#else

// =============================================================================
// STUB FUNCTIONS (when OLED_ENABLED = 0)
// =============================================================================

void initDisplay() {
    // Stub: Do nothing when display is disabled
    Serial.println("[OLED] Display disabled (OLED_ENABLED = 0)");
}

void updateDisplay(const char* tempC, const char* tempF, bool wifiConnected, const char* ipAddress) {
    // Stub: Do nothing when display is disabled
}

bool isDisplayOnWindow() {
    return true;  // Stub: always return true for disabled display
}

#endif // OLED_ENABLED
