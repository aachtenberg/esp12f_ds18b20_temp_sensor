/**
 * ESP32 Solar Monitor
 *
 * WiFi-enabled monitoring for Victron solar equipment:
 * - SmartShunt SHU050150050 (battery monitor)
 * - SmartSolar MPPT x2 (charge controllers)
 *
 * Hardware:
 * - ESP32-WROOM-32
 * - GPIO 16 (UART2 RX) <- SmartShunt TX
 * - GPIO 19 (UART1 RX) <- MPPT1 TX
 * - GPIO 18 (SoftwareSerial RX) <- MPPT2 TX
 * - VE.Direct: 19200 baud, 3.3V TTL
 *
 * API Endpoints:
 * - GET /           - HTML dashboard
 * - GET /api/battery - SmartShunt data (JSON)
 * - GET /api/solar   - Both MPPTs data (JSON)
 * - GET /api/system  - Combined system data (JSON)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>
#include <ESP_DoubleResetDetector.h>

// Filesystem for device name storage
#ifdef ESP32
  #include <SPIFFS.h>
  #define FILESYSTEM SPIFFS
#else
  #include <LittleFS.h>
  #define FILESYSTEM LittleFS
#endif

#include "VictronSmartShunt.h"
#include "VictronMPPT.h"
#include "secrets.h"
#include "display.h"

// Double Reset Detector configuration
#define DRD_TIMEOUT 3           // Seconds to wait for second reset
#define DRD_ADDRESS 0           // EEPROM address for ESP32

// Create Double Reset Detector instance
DoubleResetDetector* drd;

// Device name storage
char deviceName[40] = "Solar Monitor";
const char* DEVICE_NAME_FILE = "/device_name.txt";

// ============================================================================
// Configuration
// ============================================================================

// UART Pin assignments
#define SMARTSHUNT_RX_PIN 16  // GPIO 16 - UART2 RX
#define MPPT1_RX_PIN 19       // GPIO 19 - UART1 RX
#define MPPT2_RX_PIN 18       // GPIO 18 - SoftwareSerial RX

// VE.Direct baud rate
#define VEDIRECT_BAUD 19200

// Web server port
#define HTTP_PORT 80

// Status update interval (ms)
#define STATUS_INTERVAL 10000

// ============================================================================
// Global Objects
// ============================================================================

// Hardware serial ports for VE.Direct
HardwareSerial shuntSerial(2);  // UART2 for SmartShunt
HardwareSerial mppt1Serial(1);  // UART1 for MPPT1

// SoftwareSerial for MPPT2 (RX only, TX pin -1)
SoftwareSerial mppt2Serial;

// Victron device instances
VictronSmartShunt smartShunt(&shuntSerial);
VictronMPPT mppt1(&mppt1Serial);
VictronMPPT mppt2(&mppt2Serial);

// Web server
WebServer server(HTTP_PORT);

// Status tracking
unsigned long lastStatusPrint = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long bootTime = 0;

// ============================================================================
// Function Declarations
// ============================================================================

void loadDeviceName();
void saveDeviceName(const char* name);
void sendEventToInfluxDB(const String& eventType, const String& message, const String& severity = "info");
void setupWiFi();
void setupWebServer();
void handleRoot();
void handleBatteryData();
void handleSolarData();
void handleSystemData();
void printStatus();
void sendDataToInfluxDB();

// ============================================================================
// InfluxDB Configuration
// ============================================================================

// Data sending interval (ms)
#define INFLUXDB_SEND_INTERVAL 30000  // Send data every 30 seconds
#define HTTP_TIMEOUT_MS 5000          // HTTP timeout for InfluxDB requests

// Status tracking
unsigned long lastInfluxDBSend = 0;
int influxDBFailureCount = 0;

// ============================================================================
// Device Name Management
// ============================================================================

void loadDeviceName() {
    if (!FILESYSTEM.begin()) {
        Serial.println("[FS] Failed to mount filesystem");
        return;
    }

    if (FILESYSTEM.exists(DEVICE_NAME_FILE)) {
        File file = FILESYSTEM.open(DEVICE_NAME_FILE, "r");
        if (file) {
            String name = file.readStringUntil('\n');
            name.trim();
            if (name.length() > 0 && name.length() < sizeof(deviceName)) {
                strncpy(deviceName, name.c_str(), sizeof(deviceName) - 1);
                deviceName[sizeof(deviceName) - 1] = '\0';
                Serial.print("[FS] Loaded device name: ");
                Serial.println(deviceName);
            }
            file.close();
        }
    } else {
        Serial.println("[FS] No saved device name, using default");
    }
}

void saveDeviceName(const char* name) {
    if (!FILESYSTEM.begin()) {
        Serial.println("[FS] Failed to mount filesystem");
        return;
    }

    File file = FILESYSTEM.open(DEVICE_NAME_FILE, "w");
    if (file) {
        file.println(name);
        file.close();
        Serial.print("[FS] Saved device name: ");
        Serial.println(name);
    } else {
        Serial.println("[FS] Failed to save device name");
    }
}

// ============================================================================
// Event Logging to InfluxDB
// ============================================================================

void sendEventToInfluxDB(const String& eventType, const String& message, const String& severity) {
    if (WiFi.status() != WL_CONNECTED) {
        return;  // Skip if WiFi not connected
    }

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + INFLUXDB_ORG + "&bucket=" + INFLUXDB_BUCKET;
    http.begin(url);
    http.addHeader("Authorization", "Token " + String(INFLUXDB_TOKEN));
    http.addHeader("Content-Type", "text/plain; charset=utf-8");

    // Replace spaces with underscores in device name for tag value
    String deviceTag = String(deviceName);
    deviceTag.replace(" ", "_");

    // Build line protocol: measurement,tags fields
    String data = "device_events,";
    data += "device=" + deviceTag + ",";
    data += "board=esp32,";
    data += "event_type=" + eventType + ",";
    data += "severity=" + severity + " ";
    data += "message=\"" + message + "\",";
    data += "value=1";

    int httpCode = http.POST(data);
    
    if (httpCode == 204 || httpCode == 200) {
        Serial.println("[Event] Logged: " + eventType + " - " + message);
    } else {
        Serial.printf("[Event] Failed to log: %d\n", httpCode);
    }

    http.end();
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    // Initialize debug serial
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("     ESP32 Solar Monitor");
    Serial.println("========================================");
    Serial.println();

    bootTime = millis();

    // Load device name from filesystem
    loadDeviceName();

    // Initialize VE.Direct serial ports (RX only, TX pin = -1)
    Serial.println("[UART] Initializing SmartShunt on GPIO 16...");
    shuntSerial.begin(VEDIRECT_BAUD, SERIAL_8N1, SMARTSHUNT_RX_PIN, -1);

    Serial.println("[UART] Initializing MPPT1 on GPIO 19...");
    mppt1Serial.begin(VEDIRECT_BAUD, SERIAL_8N1, MPPT1_RX_PIN, -1);

    Serial.println("[UART] Initializing MPPT2 on GPIO 18 (SoftwareSerial)...");
    mppt2Serial.begin(VEDIRECT_BAUD, SWSERIAL_8N1, MPPT2_RX_PIN, -1, false);

    // Initialize device drivers
    smartShunt.begin();
    mppt1.begin();
    mppt2.begin();

    // Initialize OLED display
    initDisplay();

    // Connect to WiFi
    setupWiFi();

    // Setup web server
    setupWebServer();

    // Log device boot event
    String resetReason = "Unknown";
    #ifdef ESP32
        esp_reset_reason_t reason = esp_reset_reason();
        switch(reason) {
            case ESP_RST_POWERON: resetReason = "Power On"; break;
            case ESP_RST_EXT: resetReason = "External Reset"; break;
            case ESP_RST_SW: resetReason = "Software Reset"; break;
            case ESP_RST_PANIC: resetReason = "Panic/Exception"; break;
            case ESP_RST_INT_WDT: resetReason = "Watchdog"; break;
            case ESP_RST_TASK_WDT: resetReason = "Task Watchdog"; break;
            case ESP_RST_WDT: resetReason = "Other Watchdog"; break;
            case ESP_RST_DEEPSLEEP: resetReason = "Deep Sleep"; break;
            case ESP_RST_BROWNOUT: resetReason = "Brownout"; break;
            case ESP_RST_SDIO: resetReason = "SDIO"; break;
            default: resetReason = "Unknown"; break;
        }
    #endif
    String bootMsg = "Device started - Reset reason: " + resetReason + 
                     ", Uptime: 0s, Free heap: " + String(ESP.getFreeHeap()) + " bytes";
    sendEventToInfluxDB("device_boot", bootMsg, "info");

    Serial.println();
    Serial.println("========================================");
    Serial.println("     Setup Complete");
    Serial.println("========================================");
    Serial.println();
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // Must call drd->loop() to keep double reset detection working
    drd->loop();

    // Check WiFi connection and log reconnects
    static int reconnectCount = 0;
    static bool wasConnected = false;
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    
    if (!isConnected && wasConnected) {
        // WiFi disconnected
        reconnectCount++;
        // Log every 5th reconnect attempt to avoid spam
        if (reconnectCount % 5 == 1) {
            String msg = "WiFi disconnected, reconnect attempt #" + String(reconnectCount);
            sendEventToInfluxDB("wifi_reconnect", msg, "warning");
        }
    } else if (isConnected && !wasConnected) {
        // WiFi reconnected
        reconnectCount = 0;
    }
    wasConnected = isConnected;

    // Update device data (non-blocking) and log errors
    static unsigned long lastSmartShuntData = 0;
    static unsigned long lastMppt1Data = 0;
    static unsigned long lastMppt2Data = 0;
    static bool smartShuntErrorLogged = false;
    static bool mppt1ErrorLogged = false;
    static bool mppt2ErrorLogged = false;
    
    smartShunt.update();
    mppt1.update();
    mppt2.update();
    
    // Log sensor errors after 60 seconds of no data (log once until recovered)
    unsigned long now = millis();
    if (smartShunt.isDataValid()) {
        lastSmartShuntData = now;
        smartShuntErrorLogged = false;
    } else if (!smartShuntErrorLogged && now - lastSmartShuntData > 60000) {
        sendEventToInfluxDB("sensor_error", "SmartShunt no data for 60+ seconds", "error");
        smartShuntErrorLogged = true;
    }
    
    if (mppt1.isDataValid()) {
        lastMppt1Data = now;
        mppt1ErrorLogged = false;
    } else if (!mppt1ErrorLogged && now - lastMppt1Data > 60000) {
        sendEventToInfluxDB("sensor_error", "MPPT1 no data for 60+ seconds", "error");
        mppt1ErrorLogged = true;
    }
    
    if (mppt2.isDataValid()) {
        lastMppt2Data = now;
        mppt2ErrorLogged = false;
    } else if (!mppt2ErrorLogged && now - lastMppt2Data > 60000) {
        sendEventToInfluxDB("sensor_error", "MPPT2 no data for 60+ seconds", "error");
        mppt2ErrorLogged = true;
    }

    // Handle web requests
    server.handleClient();

    // Periodic status output
    if (millis() - lastStatusPrint >= STATUS_INTERVAL) {
        printStatus();
        lastStatusPrint = millis();
    }

    // Periodic InfluxDB data sending
    if (millis() - lastInfluxDBSend >= INFLUXDB_SEND_INTERVAL) {
        sendDataToInfluxDB();
        lastInfluxDBSend = millis();
    }

    // Update OLED display periodically
    if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        // Get current data from SmartShunt
        float batteryPercent = smartShunt.getStateOfCharge();
        float batteryVoltage = smartShunt.getBatteryVoltage();
        float batteryCurrent = smartShunt.getBatteryCurrent();
        
        // Get solar power from both MPPTs
        float solarPower1 = mppt1.getPanelPower();
        float solarPower2 = mppt2.getPanelPower();
        
        // Get WiFi status
        bool wifiConnected = (WiFi.status() == WL_CONNECTED);
        String ipStr = wifiConnected ? WiFi.localIP().toString() : "";
        
        // Build daily stats from MPPTs
        SolarDailyStats dailyStats;
        dailyStats.yieldToday1 = mppt1.getYieldToday();
        dailyStats.yieldToday2 = mppt2.getYieldToday();
        dailyStats.yieldYesterday1 = mppt1.getYieldYesterday();
        dailyStats.yieldYesterday2 = mppt2.getYieldYesterday();
        dailyStats.maxPowerToday1 = mppt1.getMaxPowerToday();
        dailyStats.maxPowerToday2 = mppt2.getMaxPowerToday();
        dailyStats.maxPowerYesterday1 = mppt1.getMaxPowerYesterday();
        dailyStats.maxPowerYesterday2 = mppt2.getMaxPowerYesterday();
        
        // Update display
        updateDisplay(batteryPercent, batteryVoltage, batteryCurrent,
                     solarPower1, solarPower2, wifiConnected, ipStr.c_str(), &dailyStats);
        
        lastDisplayUpdate = millis();
    }

    // Small delay to prevent watchdog issues
    delay(1);
}

// ============================================================================
// WiFi Setup with WiFiManager and Double Reset Detection
// ============================================================================

void setupWiFi() {
    // Initialize Double Reset Detector
    drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);

    // Create WiFiManager instance
    WiFiManager wm;

    // Set custom AP name for config portal
    const char* apName = "SolarMonitor-Setup";
    
    // Add device name parameter
    WiFiManagerParameter customDeviceName("device_name", "Device Name", deviceName, sizeof(deviceName));
    wm.addParameter(&customDeviceName);
    
    // Track if we need to save config
    bool shouldSaveConfig = false;
    String oldDeviceName = String(deviceName);
    
    // Set save config callback for both modes
    wm.setSaveConfigCallback([&shouldSaveConfig]() {
        shouldSaveConfig = true;
    });
    wm.setSaveParamsCallback([&shouldSaveConfig]() {
        shouldSaveConfig = true;
    });

    // Don't use timeout - we want to keep retrying forever in weak WiFi zones
    // User must double-reset to enter config mode
    wm.setConnectTimeout(0);

    WiFi.mode(WIFI_STA);

    // Check for double reset - enter config portal if detected
    if (drd->detectDoubleReset()) {
        Serial.println();
        Serial.println("========================================");
        Serial.println("  DOUBLE RESET DETECTED");
        Serial.println("  Starting WiFi Configuration Portal");
        Serial.println("========================================");
        Serial.println();
        Serial.print("[WiFi] Connect to AP: ");
        Serial.println(apName);
        Serial.println("[WiFi] Then open http://192.168.4.1 in browser");
        Serial.println();

        // Start config portal (blocking - waits until configured)
        if (!wm.startConfigPortal(apName)) {
            Serial.println("[WiFi] Failed to connect after config portal");
            Serial.println("[WiFi] Restarting...");
            delay(3000);
            ESP.restart();
        }
        
        // Save device name and log configuration event
        if (shouldSaveConfig) {
            String newName = customDeviceName.getValue();
            if (newName.length() > 0 && newName != oldDeviceName) {
                strncpy(deviceName, newName.c_str(), sizeof(deviceName) - 1);
                deviceName[sizeof(deviceName) - 1] = '\0';
                saveDeviceName(deviceName);
                
                String msg = "Name: '" + oldDeviceName + "' -> '" + String(deviceName) + "', SSID: " + 
                            WiFi.SSID() + ", IP: " + WiFi.localIP().toString();
                sendEventToInfluxDB("device_configured", msg, "info");
            } else {
                String msg = "WiFi reconfigured - SSID: " + WiFi.SSID() + ", IP: " + 
                            WiFi.localIP().toString() + ", Name unchanged: " + String(deviceName);
                sendEventToInfluxDB("device_configured", msg, "info");
            }
        }
    } else {
        // Normal boot - try to connect with saved credentials
        Serial.println("[WiFi] Normal boot - attempting connection...");
        Serial.println("[WiFi] (Double-reset within 3 seconds to enter config mode)");
        Serial.println();

        // Try to auto-connect with saved credentials
        // If no saved credentials, will start config portal
        if (!wm.autoConnect(apName)) {
            Serial.println("[WiFi] Failed to connect");
            Serial.println("[WiFi] Running in offline mode - double-reset to configure");
        }
        
        // Save device name and log configuration event if config was saved
        if (shouldSaveConfig) {
            String newName = customDeviceName.getValue();
            if (newName.length() > 0 && newName != oldDeviceName) {
                strncpy(deviceName, newName.c_str(), sizeof(deviceName) - 1);
                deviceName[sizeof(deviceName) - 1] = '\0';
                saveDeviceName(deviceName);
                
                String msg = "Name: '" + oldDeviceName + "' -> '" + String(deviceName) + "', SSID: " + 
                            WiFi.SSID() + ", IP: " + WiFi.localIP().toString();
                sendEventToInfluxDB("device_configured", msg, "info");
            } else if (WiFi.status() == WL_CONNECTED) {
                String msg = "WiFi reconfigured - SSID: " + WiFi.SSID() + ", IP: " + 
                            WiFi.localIP().toString() + ", Name unchanged: " + String(deviceName);
                sendEventToInfluxDB("device_configured", msg, "info");
            }
        }
    }

    // Print connection status
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("========================================");
        Serial.println("  WiFi Connected!");
        Serial.println("========================================");
        Serial.print("[WiFi] SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("[WiFi] IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WiFi] Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        Serial.println();
        
        // Log WiFi connection event
        String msg = "Connected to " + WiFi.SSID() + " with IP " + WiFi.localIP().toString();
        sendEventToInfluxDB("wifi_connected", msg, "info");
    } else {
        Serial.println("[WiFi] Not connected - running in offline mode");
    }
}

// ============================================================================
// Web Server Setup
// ============================================================================

void setupWebServer() {
    // Register endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/battery", HTTP_GET, handleBatteryData);
    server.on("/api/solar", HTTP_GET, handleSolarData);
    server.on("/api/system", HTTP_GET, handleSystemData);

    // Start server
    server.begin();
    Serial.println("[HTTP] Web server started on port 80");
}

// ============================================================================
// Web Request Handlers
// ============================================================================

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>";
    html += String(deviceName);
    html += R"rawliteral(</title>
    <style>
        body { margin: 0; padding: 8px; background: #0f172a; font-family: system-ui; color: #e2e8f0; }
        .container { max-width: 800px; margin: 0 auto; }
        .header { text-align: center; margin-bottom: 16px; }
        .title { font-size: 1.3rem; font-weight: 600; color: #94a3b8; margin-bottom: 4px; }
        .status { font-size: 0.8rem; color: #94a3b8; }
        .status-indicator { display: inline-block; width: 8px; height: 8px; background: #10b981; border-radius: 50%; margin-right: 4px; animation: pulse 2s infinite; }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }

        .card { background: #1e293b; border: 1px solid #334155; border-radius: 12px; padding: 16px; margin-bottom: 12px; }
        .card-title { font-size: 1.1rem; font-weight: 600; color: #38bdf8; margin-bottom: 12px; text-align: center; }

        .main-display { background: linear-gradient(135deg, #1e3a5f, #0f172a); border: 1px solid #334155; border-radius: 10px; padding: 16px; margin-bottom: 12px; text-align: center; }
        .main-value { font-size: 3rem; font-weight: 700; color: #38bdf8; }
        .main-unit { font-size: 0.9rem; color: #94a3b8; margin-left: 4px; }

        .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 10px; }
        .stat-box { background: #0f172a; border: 1px solid #334155; border-radius: 8px; padding: 10px; text-align: center; }
        .stat-value { font-size: 1.5rem; font-weight: 700; color: #38bdf8; }
        .stat-label { font-size: 0.75rem; color: #94a3b8; margin-top: 4px; text-transform: uppercase; }

        .section-title { font-size: 0.85rem; color: #94a3b8; margin: 12px 0 8px 0; padding-bottom: 4px; border-bottom: 1px solid #334155; }
        .no-data { color: #64748b; font-style: italic; text-align: center; }

        .footer { margin-top: 12px; padding-top: 8px; border-top: 1px solid #334155; font-size: 0.7rem; color: #64748b; text-align: center; }
        .footer a { color: #38bdf8; text-decoration: none; margin: 0 6px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="title">)rawliteral";
    html += String(deviceName);
    html += R"rawliteral(</div>
            <div class="status"><span class="status-indicator"></span>Live</div>
        </div>

        <div class="card">
            <div class="card-title">Battery (SmartShunt)</div>
            <div id="battery-data">Loading...</div>
        </div>

        <div class="card">
            <div class="card-title">Solar Production</div>
            <div id="solar-total">Loading...</div>
            <div class="section-title">MPPT 1</div>
            <div id="mppt1-data">Loading...</div>
            <div class="section-title">MPPT 2</div>
            <div id="mppt2-data">Loading...</div>
        </div>

        <div class="footer">
            <a href="/api/battery">Battery API</a>
            <a href="/api/solar">Solar API</a>
            <a href="/api/system">System API</a>
        </div>
    </div>

    <script>
        function renderMppt(mppt) {
            if (!mppt || !mppt.valid) return '<div class="no-data">No data available</div>';
            return `
                <div class="stats-grid">
                    <div class="stat-box"><div class="stat-value">${mppt.pv_power.toFixed(0)}</div><div class="stat-label">Watts</div></div>
                    <div class="stat-box"><div class="stat-value">${mppt.pv_voltage.toFixed(1)}</div><div class="stat-label">Panel V</div></div>
                    <div class="stat-box"><div class="stat-value">${mppt.charge_current.toFixed(2)}</div><div class="stat-label">Current A</div></div>
                    <div class="stat-box"><div class="stat-value">${mppt.yield_today.toFixed(2)}</div><div class="stat-label">Today kWh</div></div>
                </div>
            `;
        }

        function updateData() {
            fetch('/api/system')
                .then(r => r.json())
                .then(data => {
                    // Battery data
                    let battHtml = '';
                    if (data.battery && data.battery.valid) {
                        battHtml = `
                            <div class="main-display">
                                <div><span class="main-value">${data.battery.soc.toFixed(1)}</span><span class="main-unit">%</span></div>
                            </div>
                            <div class="stats-grid">
                                <div class="stat-box"><div class="stat-value">${data.battery.voltage.toFixed(1)}</div><div class="stat-label">Voltage</div></div>
                                <div class="stat-box"><div class="stat-value">${data.battery.current.toFixed(1)}</div><div class="stat-label">Current A</div></div>
                                <div class="stat-box"><div class="stat-value">${data.battery.time_remaining}</div><div class="stat-label">Time min</div></div>
                            </div>
                        `;
                    } else {
                        battHtml = '<div class="no-data">No data from SmartShunt</div>';
                    }
                    document.getElementById('battery-data').innerHTML = battHtml;

                    // Combined solar totals
                    let totalHtml = '';
                    if (data.solar && data.solar.valid) {
                        totalHtml = `
                            <div class="main-display">
                                <div><span class="main-value">${data.solar.pv_power.toFixed(0)}</span><span class="main-unit">W</span></div>
                            </div>
                            <div class="stats-grid">
                                <div class="stat-box"><div class="stat-value">${data.solar.charge_current.toFixed(2)}</div><div class="stat-label">Current A</div></div>
                                <div class="stat-box"><div class="stat-value">${data.solar.yield_today.toFixed(2)}</div><div class="stat-label">Total Today kWh</div></div>
                            </div>
                        `;
                    } else {
                        totalHtml = '<div class="no-data">No solar data</div>';
                    }
                    document.getElementById('solar-total').innerHTML = totalHtml;

                    // Individual MPPT data
                    document.getElementById('mppt1-data').innerHTML = renderMppt(data.mppt1);
                    document.getElementById('mppt2-data').innerHTML = renderMppt(data.mppt2);
                })
                .catch(e => console.error('Update failed:', e));
        }

        // Update every 2 seconds
        updateData();
        setInterval(updateData, 2000);
    </script>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}

void handleBatteryData() {
    StaticJsonDocument<512> doc;

    doc["voltage"] = smartShunt.getBatteryVoltage();
    doc["current"] = smartShunt.getBatteryCurrent();
    doc["soc"] = smartShunt.getStateOfCharge();
    doc["time_remaining"] = smartShunt.getTimeRemaining();
    doc["consumed_ah"] = smartShunt.getConsumedAh();
    doc["alarm"] = smartShunt.getAlarmState();
    doc["relay"] = smartShunt.getRelayState();
    doc["min_voltage"] = smartShunt.getMinVoltage();
    doc["max_voltage"] = smartShunt.getMaxVoltage();
    doc["charge_cycles"] = smartShunt.getChargeCycles();
    doc["last_update"] = smartShunt.getLastUpdate();
    doc["valid"] = smartShunt.isDataValid();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSolarData() {
    StaticJsonDocument<1024> doc;

    // MPPT1 data
    JsonObject mppt1Data = doc.createNestedObject("mppt1");
    mppt1Data["product_id"] = mppt1.getProductID();
    mppt1Data["serial_number"] = mppt1.getSerialNumber();
    mppt1Data["pv_voltage"] = mppt1.getPanelVoltage();
    mppt1Data["pv_power"] = mppt1.getPanelPower();
    mppt1Data["battery_voltage"] = mppt1.getBatteryVoltage();
    mppt1Data["charge_current"] = mppt1.getChargeCurrent();
    mppt1Data["charge_state"] = mppt1.getChargeState();
    mppt1Data["error_code"] = mppt1.getErrorCode();
    mppt1Data["error_string"] = mppt1.getErrorString();
    mppt1Data["load_state"] = mppt1.getLoadState();
    mppt1Data["load_current"] = mppt1.getLoadCurrent();
    mppt1Data["yield_today"] = mppt1.getYieldToday();
    mppt1Data["yield_yesterday"] = mppt1.getYieldYesterday();
    mppt1Data["yield_total"] = mppt1.getYieldTotal();
    mppt1Data["max_power_today"] = mppt1.getMaxPowerToday();
    mppt1Data["max_power_yesterday"] = mppt1.getMaxPowerYesterday();
    mppt1Data["last_update"] = mppt1.getLastUpdate();
    mppt1Data["valid"] = mppt1.isDataValid();

    // MPPT2 data
    JsonObject mppt2Data = doc.createNestedObject("mppt2");
    mppt2Data["product_id"] = mppt2.getProductID();
    mppt2Data["serial_number"] = mppt2.getSerialNumber();
    mppt2Data["pv_voltage"] = mppt2.getPanelVoltage();
    mppt2Data["pv_power"] = mppt2.getPanelPower();
    mppt2Data["battery_voltage"] = mppt2.getBatteryVoltage();
    mppt2Data["charge_current"] = mppt2.getChargeCurrent();
    mppt2Data["charge_state"] = mppt2.getChargeState();
    mppt2Data["error_code"] = mppt2.getErrorCode();
    mppt2Data["error_string"] = mppt2.getErrorString();
    mppt2Data["load_state"] = mppt2.getLoadState();
    mppt2Data["load_current"] = mppt2.getLoadCurrent();
    mppt2Data["yield_today"] = mppt2.getYieldToday();
    mppt2Data["yield_yesterday"] = mppt2.getYieldYesterday();
    mppt2Data["yield_total"] = mppt2.getYieldTotal();
    mppt2Data["max_power_today"] = mppt2.getMaxPowerToday();
    mppt2Data["max_power_yesterday"] = mppt2.getMaxPowerYesterday();
    mppt2Data["last_update"] = mppt2.getLastUpdate();
    mppt2Data["valid"] = mppt2.isDataValid();

    // Combined totals
    JsonObject totals = doc.createNestedObject("totals");
    totals["pv_power"] = mppt1.getPanelPower() + mppt2.getPanelPower();
    totals["charge_current"] = mppt1.getChargeCurrent() + mppt2.getChargeCurrent();
    totals["load_current"] = mppt1.getLoadCurrent() + mppt2.getLoadCurrent();
    totals["yield_today"] = mppt1.getYieldToday() + mppt2.getYieldToday();
    totals["yield_yesterday"] = mppt1.getYieldYesterday() + mppt2.getYieldYesterday();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSystemData() {
    StaticJsonDocument<2048> doc;

    // Battery subsystem
    JsonObject battery = doc.createNestedObject("battery");
    battery["voltage"] = smartShunt.getBatteryVoltage();
    battery["current"] = smartShunt.getBatteryCurrent();
    battery["soc"] = smartShunt.getStateOfCharge();
    battery["time_remaining"] = smartShunt.getTimeRemaining();
    battery["consumed_ah"] = smartShunt.getConsumedAh();
    battery["valid"] = smartShunt.isDataValid();

    // Solar subsystem - combined totals for backward compatibility
    JsonObject solar = doc.createNestedObject("solar");
    solar["pv_voltage"] = max(mppt1.getPanelVoltage(), mppt2.getPanelVoltage());
    solar["pv_power"] = mppt1.getPanelPower() + mppt2.getPanelPower();
    solar["charge_current"] = mppt1.getChargeCurrent() + mppt2.getChargeCurrent();
    solar["charge_state"] = mppt1.getChargeState();  // Use MPPT1 state as primary
    solar["yield_today"] = mppt1.getYieldToday() + mppt2.getYieldToday();
    solar["yield_yesterday"] = mppt1.getYieldYesterday() + mppt2.getYieldYesterday();
    solar["error_code"] = max(mppt1.getErrorCode(), mppt2.getErrorCode());
    solar["valid"] = mppt1.isDataValid() || mppt2.isDataValid();

    // MPPT1 details
    JsonObject mppt1Data = doc.createNestedObject("mppt1");
    mppt1Data["pv_voltage"] = mppt1.getPanelVoltage();
    mppt1Data["pv_power"] = mppt1.getPanelPower();
    mppt1Data["charge_current"] = mppt1.getChargeCurrent();
    mppt1Data["charge_state"] = mppt1.getChargeState();
    mppt1Data["yield_today"] = mppt1.getYieldToday();
    mppt1Data["error_code"] = mppt1.getErrorCode();
    mppt1Data["valid"] = mppt1.isDataValid();

    // MPPT2 details
    JsonObject mppt2Data = doc.createNestedObject("mppt2");
    mppt2Data["pv_voltage"] = mppt2.getPanelVoltage();
    mppt2Data["pv_power"] = mppt2.getPanelPower();
    mppt2Data["charge_current"] = mppt2.getChargeCurrent();
    mppt2Data["charge_state"] = mppt2.getChargeState();
    mppt2Data["yield_today"] = mppt2.getYieldToday();
    mppt2Data["error_code"] = mppt2.getErrorCode();
    mppt2Data["valid"] = mppt2.isDataValid();

    // System info
    JsonObject system = doc.createNestedObject("system");
    system["uptime"] = (millis() - bootTime) / 1000;
    system["wifi_rssi"] = WiFi.RSSI();
    system["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    system["ip_address"] = WiFi.localIP().toString();
    system["free_heap"] = ESP.getFreeHeap();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// ============================================================================
// Status Output
// ============================================================================

void printStatus() {
    Serial.println();
    Serial.println("--- Status Update ---");

    // SmartShunt data
    if (smartShunt.isDataValid()) {
        Serial.printf("Battery: %.2fV | %.2fA | %.1f%% SOC | TTG: %d min\n",
            smartShunt.getBatteryVoltage(),
            smartShunt.getBatteryCurrent(),
            smartShunt.getStateOfCharge(),
            smartShunt.getTimeRemaining());
    } else {
        Serial.println("Battery: No data from SmartShunt");
    }

    // MPPT1 data
    if (mppt1.isDataValid()) {
        Serial.printf("MPPT1:   PV %.1fV | %.0fW | Charge %.2fA | %s\n",
            mppt1.getPanelVoltage(),
            mppt1.getPanelPower(),
            mppt1.getChargeCurrent(),
            mppt1.getChargeState().c_str());
        Serial.printf("         Yield: %.2f kWh today | %.2f kWh yesterday\n",
            mppt1.getYieldToday(),
            mppt1.getYieldYesterday());
    } else {
        Serial.println("MPPT1:   No data");
    }

    // MPPT2 data
    if (mppt2.isDataValid()) {
        Serial.printf("MPPT2:   PV %.1fV | %.0fW | Charge %.2fA | %s\n",
            mppt2.getPanelVoltage(),
            mppt2.getPanelPower(),
            mppt2.getChargeCurrent(),
            mppt2.getChargeState().c_str());
        Serial.printf("         Yield: %.2f kWh today | %.2f kWh yesterday\n",
            mppt2.getYieldToday(),
            mppt2.getYieldYesterday());
    } else {
        Serial.println("MPPT2:   No data");
    }

    // Combined solar totals
    float totalPvPower = 0;
    float totalYieldToday = 0;
    if (mppt1.isDataValid()) {
        totalPvPower += mppt1.getPanelPower();
        totalYieldToday += mppt1.getYieldToday();
    }
    if (mppt2.isDataValid()) {
        totalPvPower += mppt2.getPanelPower();
        totalYieldToday += mppt2.getYieldToday();
    }
    if (mppt1.isDataValid() || mppt2.isDataValid()) {
        Serial.printf("Total:   %.0fW solar | %.2f kWh today\n", totalPvPower, totalYieldToday);
    }

    // System info
    Serial.printf("System:  Uptime %lu sec | WiFi %d dBm | Heap %u bytes\n",
        (millis() - bootTime) / 1000,
        WiFi.RSSI(),
        ESP.getFreeHeap());

    Serial.println("---------------------");
}

// ============================================================================
// InfluxDB Data Sending
// ============================================================================

void sendDataToInfluxDB() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[InfluxDB] WiFi not connected, skipping data send");
        return;
    }

    HTTPClient http;
    http.setTimeout(5000);  // 5 second timeout

    // Build InfluxDB URL with authentication
    String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + INFLUXDB_ORG + "&bucket=" + INFLUXDB_BUCKET;
    http.begin(url);
    http.addHeader("Authorization", "Token " + String(INFLUXDB_TOKEN));
    http.addHeader("Content-Type", "text/plain; charset=utf-8");

    // Build line protocol data
    String data = "";

    // Battery data (SmartShunt)
    if (smartShunt.isDataValid()) {
        // Replace spaces with underscores in device name for tag value
        String deviceTag = String(deviceName);
        deviceTag.replace(" ", "_");
        
        data += "battery,device=" + deviceTag + ",location=garage ";
        data += "voltage=" + String(smartShunt.getBatteryVoltage(), 3) + ",";
        data += "current=" + String(smartShunt.getBatteryCurrent(), 3) + ",";
        data += "soc=" + String(smartShunt.getStateOfCharge(), 1) + ",";
        data += "time_remaining=" + String(smartShunt.getTimeRemaining()) + ",";
        data += "consumed_ah=" + String(smartShunt.getConsumedAh(), 3) + ",";
        data += "alarm=" + String(smartShunt.getAlarmState() ? 1 : 0) + ",";
        data += "relay=" + String(smartShunt.getRelayState() ? 1 : 0) + ",";
        data += "min_voltage=" + String(smartShunt.getMinVoltage(), 3) + ",";
        data += "max_voltage=" + String(smartShunt.getMaxVoltage(), 3) + ",";
        data += "charge_cycles=" + String(smartShunt.getChargeCycles()) + ",";
        data += "deepest_discharge=" + String(smartShunt.getDeepestDischarge(), 3) + ",";
        data += "last_discharge=" + String(smartShunt.getLastDischarge(), 3);
        data += "\n";
    }

    // Solar data (MPPT1)
    if (mppt1.isDataValid()) {
        String deviceTag = String(deviceName);
        deviceTag.replace(" ", "_");

        // Add product_id and serial as tags if available
        String tags = "solar,device=" + deviceTag + ",location=garage,mppt=1";
        if (mppt1.getProductID().length() > 0) {
            tags += ",product_id=" + mppt1.getProductID();
        }
        if (mppt1.getSerialNumber().length() > 0) {
            String serial = mppt1.getSerialNumber();
            serial.replace(" ", "_");
            tags += ",serial=" + serial;
        }

        data += tags + " ";
        data += "pv_voltage=" + String(mppt1.getPanelVoltage(), 3) + ",";
        data += "pv_power=" + String(mppt1.getPanelPower(), 1) + ",";
        data += "battery_voltage=" + String(mppt1.getBatteryVoltage(), 3) + ",";
        data += "charge_current=" + String(mppt1.getChargeCurrent(), 3) + ",";
        data += "charge_state=\"" + mppt1.getChargeState() + "\",";
        data += "error_code=" + String(mppt1.getErrorCode()) + ",";
        data += "load_state=\"" + mppt1.getLoadState() + "\",";
        data += "load_current=" + String(mppt1.getLoadCurrent(), 3) + ",";
        data += "yield_today=" + String(mppt1.getYieldToday(), 3) + ",";
        data += "yield_yesterday=" + String(mppt1.getYieldYesterday(), 3) + ",";
        data += "yield_total=" + String(mppt1.getYieldTotal(), 3) + ",";
        data += "max_power_today=" + String(mppt1.getMaxPowerToday()) + ",";
        data += "max_power_yesterday=" + String(mppt1.getMaxPowerYesterday());
        data += "\n";
    }

    // Solar data (MPPT2)
    if (mppt2.isDataValid()) {
        String deviceTag = String(deviceName);
        deviceTag.replace(" ", "_");

        // Add product_id and serial as tags if available
        String tags = "solar,device=" + deviceTag + ",location=garage,mppt=2";
        if (mppt2.getProductID().length() > 0) {
            tags += ",product_id=" + mppt2.getProductID();
        }
        if (mppt2.getSerialNumber().length() > 0) {
            String serial = mppt2.getSerialNumber();
            serial.replace(" ", "_");
            tags += ",serial=" + serial;
        }

        data += tags + " ";
        data += "pv_voltage=" + String(mppt2.getPanelVoltage(), 3) + ",";
        data += "pv_power=" + String(mppt2.getPanelPower(), 1) + ",";
        data += "battery_voltage=" + String(mppt2.getBatteryVoltage(), 3) + ",";
        data += "charge_current=" + String(mppt2.getChargeCurrent(), 3) + ",";
        data += "charge_state=\"" + mppt2.getChargeState() + "\",";
        data += "error_code=" + String(mppt2.getErrorCode()) + ",";
        data += "load_state=\"" + mppt2.getLoadState() + "\",";
        data += "load_current=" + String(mppt2.getLoadCurrent(), 3) + ",";
        data += "yield_today=" + String(mppt2.getYieldToday(), 3) + ",";
        data += "yield_yesterday=" + String(mppt2.getYieldYesterday(), 3) + ",";
        data += "yield_total=" + String(mppt2.getYieldTotal(), 3) + ",";
        data += "max_power_today=" + String(mppt2.getMaxPowerToday()) + ",";
        data += "max_power_yesterday=" + String(mppt2.getMaxPowerYesterday());
        data += "\n";
    }

    // System data
    String deviceTag = String(deviceName);
    deviceTag.replace(" ", "_");
    
    data += "system,device=" + deviceTag + ",location=garage ";
    data += "uptime=" + String((millis() - bootTime) / 1000) + ",";
    data += "wifi_rssi=" + String(WiFi.RSSI()) + ",";
    data += "free_heap=" + String(ESP.getFreeHeap()) + ",";
    data += "wifi_connected=" + String(WiFi.status() == WL_CONNECTED ? 1 : 0);
    data += "\n";

    // Send the data
    Serial.println("[InfluxDB] Sending data...");
    int httpResponseCode = http.POST(data);

    if (httpResponseCode > 0) {
        Serial.printf("[InfluxDB] Data sent successfully, response: %d\n", httpResponseCode);
        influxDBFailureCount = 0;  // Reset failure counter on success
    } else {
        Serial.printf("[InfluxDB] Failed to send data, error: %d\n", httpResponseCode);
        Serial.println("[InfluxDB] Response: " + http.getString());
        
        influxDBFailureCount++;
        // Log error every 10th failure to avoid spam
        if (influxDBFailureCount % 10 == 1) {
            String errorMsg = "POST failed: " + String(httpResponseCode < 0 ? "connection failed" : "HTTP " + String(httpResponseCode));
            sendEventToInfluxDB("influxdb_error", errorMsg, "error");
        }
    }

    http.end();
}
