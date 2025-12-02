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

// Store daily stats for use across function calls
static SolarDailyStats cachedDailyStats = {0};

void updateDisplay(float batteryPercent, float batteryVoltage, float batteryCurrent,
                   float solarPower1, float solarPower2, bool wifiConnected, const char* ipAddress,
                   const SolarDailyStats* dailyStats) {
    
    // Cache daily stats if provided
    if (dailyStats != nullptr) {
        cachedDailyStats = *dailyStats;
    }
    
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
        case PAGE_DAILY_STATS:
            drawDailyStatsPage(&cachedDailyStats);
            break;
        case PAGE_SYSTEM:
            drawSystemPage(wifiConnected, ipAddress, millis());
            break;
        default:
            drawBatteryPage(batteryPercent, batteryVoltage, batteryCurrent);
            break;
    }
    
    // Draw page indicator dots at bottom
    int dotY = 62;
    int dotSpacing = 6;
    int startX = 64 - (PAGE_COUNT * dotSpacing / 2);
    for (int i = 0; i < PAGE_COUNT; i++) {
        int x = startX + (i * dotSpacing);
        if (i == currentPage) {
            display.drawDisc(x, dotY, 1);  // Filled circle for current page
        } else {
            display.drawCircle(x, dotY, 1);  // Empty circle for other pages
        }
    }
    
    display.sendBuffer();
}

// ============================================================================
// Battery Status Page
// ============================================================================

void drawBatteryPage(float percent, float voltage, float current) {
    // Title
    display.setFont(u8g2_font_9x15B_tf);
    display.drawStr(0, 0, "BATTERY");

    // Large battery percentage
    display.setFont(u8g2_font_logisoso32_tn);
    char percentStr[8];
    snprintf(percentStr, sizeof(percentStr), "%.0f", percent);

    // Center the percentage
    int percentWidth = display.getStrWidth(percentStr);
    display.drawStr((128 - percentWidth - 20) / 2, 18, percentStr);

    // Add % symbol separately in smaller font
    display.setFont(u8g2_font_10x20_tf);
    display.drawStr((128 + percentWidth - 20) / 2 + 2, 28, "%");

    // Voltage and current on bottom line (larger)
    display.setFont(u8g2_font_8x13_tf);
    char voltStr[16], currStr[16];
    snprintf(voltStr, sizeof(voltStr), "%.1fV", voltage);
    snprintf(currStr, sizeof(currStr), "%.1fA", current);
    display.drawStr(2, 52, voltStr);

    // Right-align current
    int currWidth = display.getStrWidth(currStr);
    display.drawStr(126 - currWidth, 52, currStr);
}

// ============================================================================
// Solar Production Page
// ============================================================================

void drawSolarPage(float power1, float power2) {
    float totalPower = power1 + power2;

    // Title
    display.setFont(u8g2_font_9x15B_tf);
    display.drawStr(0, 0, "SOLAR");

    // Total power (large)
    display.setFont(u8g2_font_logisoso26_tn);
    char totalStr[12];
    snprintf(totalStr, sizeof(totalStr), "%.0f", totalPower);
    int totalWidth = display.getStrWidth(totalStr);
    display.drawStr((128 - totalWidth - 16) / 2, 18, totalStr);

    // Add W separately
    display.setFont(u8g2_font_9x15_tf);
    display.drawStr((128 + totalWidth - 16) / 2 + 2, 30, "W");

    // Individual MPPT readings (larger)
    display.setFont(u8g2_font_8x13_tf);

    // MPPT1
    char mppt1Str[20];
    snprintf(mppt1Str, sizeof(mppt1Str), "M1: %.0fW", power1);
    display.drawStr(2, 50, mppt1Str);

    // MPPT2
    char mppt2Str[20];
    snprintf(mppt2Str, sizeof(mppt2Str), "M2: %.0fW", power2);
    int mppt2Width = display.getStrWidth(mppt2Str);
    display.drawStr(126 - mppt2Width, 50, mppt2Str);
}

// ============================================================================
// Daily Stats Page
// ============================================================================

void drawDailyStatsPage(const SolarDailyStats* stats) {
    // Title
    display.setFont(u8g2_font_9x15B_tf);
    display.drawStr(0, 0, "TODAY");

    if (stats == nullptr) {
        display.setFont(u8g2_font_8x13_tf);
        display.drawStr(30, 30, "No data");
        return;
    }

    // Today's total yield
    float totalToday = stats->yieldToday1 + stats->yieldToday2;
    float totalYesterday = stats->yieldYesterday1 + stats->yieldYesterday2;
    int maxPowerToday = stats->maxPowerToday1 + stats->maxPowerToday2;

    // Today yield (large)
    display.setFont(u8g2_font_logisoso24_tn);
    char yieldStr[16];
    snprintf(yieldStr, sizeof(yieldStr), "%.1f", totalToday);
    int yieldWidth = display.getStrWidth(yieldStr);
    display.drawStr((128 - yieldWidth - 30) / 2, 18, yieldStr);
    display.setFont(u8g2_font_8x13_tf);
    display.drawStr((128 + yieldWidth - 30) / 2 + 4, 28, "kWh");

    // Yesterday and peak (larger)
    display.setFont(u8g2_font_7x13_tf);
    char yesterdayStr[20];
    snprintf(yesterdayStr, sizeof(yesterdayStr), "Yday: %.1f", totalYesterday);
    display.drawStr(2, 48, yesterdayStr);

    // Peak power
    char maxStr[20];
    snprintf(maxStr, sizeof(maxStr), "Pk:%dW", maxPowerToday);
    int maxWidth = display.getStrWidth(maxStr);
    display.drawStr(126 - maxWidth, 48, maxStr);
}

// ============================================================================
// System Status Page
// ============================================================================

void drawSystemPage(bool wifiConnected, const char* ipAddress, unsigned long uptimeMs) {
    // Title
    display.setFont(u8g2_font_9x15B_tf);
    display.drawStr(0, 0, "SYSTEM");

    display.setFont(u8g2_font_8x13_tf);

    // WiFi status
    display.drawStr(2, 18, "WiFi:");
    if (wifiConnected) {
        display.drawStr(48, 18, "OK");
    } else {
        display.drawStr(48, 18, "OFF");
    }

    // IP Address (smaller font for long IPs)
    display.setFont(u8g2_font_7x13_tf);
    display.drawStr(2, 32, "IP:");
    if (ipAddress && strlen(ipAddress) > 0) {
        display.drawStr(24, 32, ipAddress);
    } else {
        display.drawStr(24, 32, "---");
    }

    // Uptime
    unsigned long seconds = uptimeMs / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    display.setFont(u8g2_font_8x13_tf);
    char uptimeStr[24];
    if (days > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02luh", days, hours % 24);
    } else if (hours > 0) {
        snprintf(uptimeStr, sizeof(uptimeStr), "%luh %02lum", hours, minutes % 60);
    } else {
        snprintf(uptimeStr, sizeof(uptimeStr), "%lum %02lus", minutes, seconds % 60);
    }
    display.drawStr(2, 50, uptimeStr);
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
                   float solarPower1, float solarPower2, bool wifiConnected, const char* ipAddress,
                   const SolarDailyStats* dailyStats) {
    // Do nothing - display disabled
}

void drawBatteryPage(float percent, float voltage, float current) {}
void drawSolarPage(float power1, float power2) {}
void drawDailyStatsPage(const SolarDailyStats* stats) {}
void drawSystemPage(bool wifiConnected, const char* ipAddress, unsigned long uptimeMs) {}
void drawProgressBar(int x, int y, int width, int height, int percent) {}
void nextDisplayPage() {}

#endif // OLED_ENABLED
