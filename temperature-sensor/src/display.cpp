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

// Screen cycling for temperature-only and network status views
static unsigned long lastScreenSwitch = 0;
static int currentScreen = 0;  // 0 = temperature, 1 = network info
#define SCREEN_CYCLE_MS 3000  // Switch screens every 3 seconds

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
    // Only apply battery-based power saving if battery monitoring is actually enabled
    // Check VALUE of BATTERY_POWERED (not just if defined) AND if BATTERY_MONITOR_ENABLED is defined
    #if BATTERY_POWERED && defined(BATTERY_MONITOR_ENABLED)
    // batteryPercent is -1 when no battery present, 0-100 when battery is monitored
    if (batteryPercent >= 0 && batteryPercent < 50) {
        // Low battery - power off display to conserve power
        if (displayIsPoweredOn) {
            display.setPowerSave(1);
            displayIsPoweredOn = false;
            Serial.println("[OLED] Display powered off - low battery");
        }
        return;
    } else if (batteryPercent >= 50) {
        // Battery recovered - power on display
        if (!displayIsPoweredOn) {
            display.setPowerSave(0);
            displayIsPoweredOn = true;
            Serial.println("[OLED] Display powered on - battery recovered");
        }
    }
    #endif
    
    // Check gating window
    if (!isDisplayOnWindow()) {
        return;  // Skip update if outside display window
    }
    
    // Cycle between screens every SCREEN_CYCLE_MS milliseconds
    unsigned long now = millis();
    if (now - lastScreenSwitch > SCREEN_CYCLE_MS) {
        currentScreen = (currentScreen + 1) % 2;  // Toggle between 0 and 1
        lastScreenSwitch = now;
    }
    
    display.clearBuffer();

    // Parse temperature string and format to 1 decimal place
    char tempCStr[16];
    float tempCVal = atof(tempC);
    snprintf(tempCStr, sizeof(tempCStr), "%.1f", tempCVal);
    strcat(tempCStr, "\xb0");  // degree symbol
    strcat(tempCStr, "C");

    if (currentScreen == 0) {
        // Screen 1: Temperature only (large and centered)
        display.setFont(u8g2_font_logisoso42_tn);  // Extra large 42-pixel font
        int tempCWidth = display.getStrWidth(tempCStr);
        // Center horizontally and position for visibility (Y=11 positions font nicely on 64px display)
        display.drawStr((128 - tempCWidth) / 2, 11, tempCStr);
    } else {
        // Screen 2: Network status
        display.setFont(u8g2_font_9x15B_tf);  // Bold font for better readability
        
        // WiFi status (centered near top)
        const char* statusText;
        if (wifiConnected) {
            statusText = "Connected";
        } else {
            statusText = "Disconnected";
        }
        int statusWidth = display.getStrWidth(statusText);
        display.drawStr((128 - statusWidth) / 2, 15, statusText);
        
        // IP address (centered in middle)
        if (wifiConnected && ipAddress != nullptr && strlen(ipAddress) > 0) {
            display.setFont(u8g2_font_8x13_tf);  // Smaller monospace for IP
            int ipWidth = display.getStrWidth(ipAddress);
            display.drawStr((128 - ipWidth) / 2, 40, ipAddress);
        }
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

void updateDisplay(const char* tempC, const char* tempF, bool wifiConnected, const char* ipAddress, int batteryPercent) {
    // Stub: Do nothing when display is disabled
}

bool isDisplayOnWindow() {
    return true;  // Stub: always return true for disabled display
}

#endif // OLED_ENABLED
