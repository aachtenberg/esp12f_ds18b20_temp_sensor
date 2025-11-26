/**
 * OLED Display Module Implementation
 * 
 * Displays battery status, solar production, and system info
 * on a 0.96" SSD1306 OLED display.
 */

#include "display.h"

#if OLED_ENABLED

// Display instance - SSD1306 128x64 I2C with hardware I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, DISPLAY_SCL_PIN, DISPLAY_SDA_PIN);

// Page cycling
DisplayPage currentPage = PAGE_BATTERY;
unsigned long lastPageChange = 0;
const unsigned long PAGE_CYCLE_INTERVAL = 5000;  // 5 seconds per page

// ============================================================================
// Display Initialization
// ============================================================================

void initDisplay() {
    Serial.println("[OLED] Initializing display...");
    
    // Initialize I2C with custom pins
    Wire.begin(DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);
    
    // Initialize display
    if (!display.begin()) {
        Serial.println("[OLED] ERROR: Display initialization failed!");
        return;
    }
    
    // Clear and set up display
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.setFontRefHeightExtendedText();
    display.setDrawColor(1);
    display.setFontPosTop();
    display.setFontDirection(0);
    
    // Show startup screen
    display.drawStr(20, 10, "Solar Monitor");
    display.drawStr(35, 30, "Starting...");
    display.sendBuffer();
    
    Serial.println("[OLED] Display initialized successfully");
}

// ============================================================================
// Main Display Update Function
// ============================================================================

void updateDisplay(float batteryPercent, float batteryVoltage, float batteryCurrent,
                   float solarPower1, float solarPower2, bool wifiConnected, const char* ipAddress) {
    
    // Auto-cycle pages
    if (millis() - lastPageChange >= PAGE_CYCLE_INTERVAL) {
        nextDisplayPage();
        lastPageChange = millis();
    }
    
    display.clearBuffer();
    
    // Draw based on current page
    switch (currentPage) {
        case PAGE_BATTERY:
            drawBatteryPage(batteryPercent, batteryVoltage, batteryCurrent);
            break;
        case PAGE_SOLAR:
            drawSolarPage(solarPower1, solarPower2);
            break;
        case PAGE_SYSTEM:
            drawSystemPage(wifiConnected, ipAddress, millis());
            break;
        default:
            drawBatteryPage(batteryPercent, batteryVoltage, batteryCurrent);
            break;
    }
    
    // Draw page indicator dots at bottom
    int dotY = 60;
    int dotSpacing = 10;
    int startX = 64 - (PAGE_COUNT * dotSpacing / 2);
    for (int i = 0; i < PAGE_COUNT; i++) {
        int x = startX + (i * dotSpacing);
        if (i == currentPage) {
            display.drawDisc(x, dotY, 2);  // Filled circle for current page
        } else {
            display.drawCircle(x, dotY, 2);  // Empty circle for other pages
        }
    }
    
    display.sendBuffer();
}

// ============================================================================
// Battery Status Page
// ============================================================================

void drawBatteryPage(float percent, float voltage, float current) {
    // Title
    display.setFont(u8g2_font_7x13B_tf);
    display.drawStr(0, 0, "BATTERY");
    
    // WiFi indicator in top right (small)
    display.setFont(u8g2_font_5x7_tf);
    
    // Large battery percentage
    display.setFont(u8g2_font_logisoso22_tn);
    char percentStr[8];
    snprintf(percentStr, sizeof(percentStr), "%.0f%%", percent);
    
    // Center the percentage
    int percentWidth = display.getStrWidth(percentStr);
    display.drawStr((128 - percentWidth) / 2, 14, percentStr);
    
    // Progress bar for battery level
    drawProgressBar(4, 40, 120, 10, (int)percent);
    
    // Voltage and current on bottom line
    display.setFont(u8g2_font_5x8_tf);
    char voltStr[16], currStr[16];
    snprintf(voltStr, sizeof(voltStr), "%.1fV", voltage);
    snprintf(currStr, sizeof(currStr), "%.1fA", current);
    display.drawStr(4, 52, voltStr);
    
    // Right-align current
    int currWidth = display.getStrWidth(currStr);
    display.drawStr(124 - currWidth, 52, currStr);
}

// ============================================================================
// Solar Production Page
// ============================================================================

void drawSolarPage(float power1, float power2) {
    float totalPower = power1 + power2;
    
    // Title
    display.setFont(u8g2_font_7x13B_tf);
    display.drawStr(0, 0, "SOLAR");
    
    // Total power (large)
    display.setFont(u8g2_font_logisoso18_tn);
    char totalStr[12];
    snprintf(totalStr, sizeof(totalStr), "%.0fW", totalPower);
    int totalWidth = display.getStrWidth(totalStr);
    display.drawStr((128 - totalWidth) / 2, 14, totalStr);
    
    // Individual MPPT readings
    display.setFont(u8g2_font_6x10_tf);
    
    // MPPT1
    char mppt1Str[20];
    snprintf(mppt1Str, sizeof(mppt1Str), "MPPT1: %.0fW", power1);
    display.drawStr(4, 38, mppt1Str);
    
    // MPPT2
    char mppt2Str[20];
    snprintf(mppt2Str, sizeof(mppt2Str), "MPPT2: %.0fW", power2);
    display.drawStr(4, 50, mppt2Str);
}

// ============================================================================
// System Status Page
// ============================================================================

void drawSystemPage(bool wifiConnected, const char* ipAddress, unsigned long uptimeMs) {
    // Title
    display.setFont(u8g2_font_7x13B_tf);
    display.drawStr(0, 0, "SYSTEM");
    
    display.setFont(u8g2_font_6x10_tf);
    
    // WiFi status
    display.drawStr(4, 16, "WiFi:");
    if (wifiConnected) {
        display.drawStr(40, 16, "Connected");
    } else {
        display.drawStr(40, 16, "Disconnected");
    }
    
    // IP Address
    display.drawStr(4, 28, "IP:");
    if (ipAddress && strlen(ipAddress) > 0) {
        display.drawStr(24, 28, ipAddress);
    } else {
        display.drawStr(24, 28, "---");
    }
    
    // Uptime
    unsigned long seconds = uptimeMs / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    char uptimeStr[24];
    if (days > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02luh %02lum", days, hours % 24, minutes % 60);
    } else if (hours > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%luh %02lum %02lus", hours, minutes % 60, seconds % 60);
    } else {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lum %02lus", minutes, seconds % 60);
    }
    display.drawStr(4, 40, "Uptime:");
    display.drawStr(4, 50, uptimeStr);
}

// ============================================================================
// Progress Bar Drawing
// ============================================================================

void drawProgressBar(int x, int y, int width, int height, int percent) {
    // Clamp percent to 0-100
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    // Draw border
    display.drawFrame(x, y, width, height);
    
    // Calculate fill width
    int fillWidth = ((width - 4) * percent) / 100;
    
    // Draw fill
    if (fillWidth > 0) {
        display.drawBox(x + 2, y + 2, fillWidth, height - 4);
    }
}

// ============================================================================
// Page Navigation
// ============================================================================

void nextDisplayPage() {
    currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
}

#else // OLED_ENABLED == 0

// ============================================================================
// Stub functions when OLED is disabled
// ============================================================================

void initDisplay() {
    Serial.println("[OLED] Display disabled (OLED_ENABLED=0)");
}

void updateDisplay(float batteryPercent, float batteryVoltage, float batteryCurrent,
                   float solarPower1, float solarPower2, bool wifiConnected, const char* ipAddress) {
    // Do nothing - display disabled
}

void drawBatteryPage(float percent, float voltage, float current) {}
void drawSolarPage(float power1, float power2) {}
void drawSystemPage(bool wifiConnected, const char* ipAddress, unsigned long uptimeMs) {}
void drawProgressBar(int x, int y, int width, int height, int percent) {}
void nextDisplayPage() {}

#endif // OLED_ENABLED
