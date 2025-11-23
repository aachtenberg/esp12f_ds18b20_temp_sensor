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

#include "VictronSmartShunt.h"
#include "VictronMPPT.h"
#include "secrets.h"

// Double Reset Detector configuration
#define DRD_TIMEOUT 3           // Seconds to wait for second reset
#define DRD_ADDRESS 0           // EEPROM address for ESP32

// Create Double Reset Detector instance
DoubleResetDetector* drd;

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
unsigned long bootTime = 0;

// ============================================================================
// Function Declarations
// ============================================================================

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

// Status tracking
unsigned long lastInfluxDBSend = 0;

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

    // Connect to WiFi
    setupWiFi();

    // Setup web server
    setupWebServer();

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

    // Update device data (non-blocking)
    smartShunt.update();
    mppt1.update();
    mppt2.update();

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
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Solar Monitor</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; margin: 20px; background: #f5f5f5; }
        h1 { color: #333; }
        .card { background: white; border-radius: 8px; padding: 20px; margin: 15px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .card h2 { margin-top: 0; color: #007AFF; }
        .card h3 { margin: 15px 0 10px 0; color: #666; font-size: 14px; border-bottom: 1px solid #eee; padding-bottom: 5px; }
        .stat { display: inline-block; margin: 10px 20px 10px 0; }
        .stat-value { font-size: 24px; font-weight: bold; color: #333; }
        .stat-value.total { color: #007AFF; }
        .stat-label { font-size: 12px; color: #666; text-transform: uppercase; }
        .status-ok { color: #34C759; }
        .status-warn { color: #FF9500; }
        .status-error { color: #FF3B30; }
        .no-data { color: #999; font-style: italic; }
        a { color: #007AFF; }
        .api-links { margin-top: 20px; }
        .api-links a { display: inline-block; margin-right: 15px; }
        .mppt-row { display: flex; flex-wrap: wrap; gap: 20px; }
        .mppt-section { flex: 1; min-width: 280px; }
    </style>
</head>
<body>
    <h1>ESP32 Solar Monitor</h1>

    <div class="card">
        <h2>Battery (SmartShunt)</h2>
        <div id="battery-data">Loading...</div>
    </div>

    <div class="card">
        <h2>Solar (Combined)</h2>
        <div id="solar-total">Loading...</div>
        <div class="mppt-row">
            <div class="mppt-section">
                <h3>MPPT 1</h3>
                <div id="mppt1-data">Loading...</div>
            </div>
            <div class="mppt-section">
                <h3>MPPT 2</h3>
                <div id="mppt2-data">Loading...</div>
            </div>
        </div>
    </div>

    <div class="api-links">
        <strong>API Endpoints:</strong>
        <a href="/api/battery">/api/battery</a>
        <a href="/api/solar">/api/solar</a>
        <a href="/api/system">/api/system</a>
    </div>

    <script>
        function renderMppt(mppt) {
            if (!mppt || !mppt.valid) return '<span class="no-data">No data</span>';
            return `
                <div class="stat"><div class="stat-value">${mppt.pv_voltage.toFixed(1)} V</div><div class="stat-label">Panel V</div></div>
                <div class="stat"><div class="stat-value">${mppt.pv_power.toFixed(0)} W</div><div class="stat-label">Power</div></div>
                <div class="stat"><div class="stat-value">${mppt.charge_current.toFixed(2)} A</div><div class="stat-label">Current</div></div>
                <div class="stat"><div class="stat-value">${mppt.charge_state}</div><div class="stat-label">State</div></div>
                <div class="stat"><div class="stat-value">${mppt.yield_today.toFixed(2)} kWh</div><div class="stat-label">Today</div></div>
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
                            <div class="stat"><div class="stat-value">${data.battery.voltage.toFixed(2)} V</div><div class="stat-label">Voltage</div></div>
                            <div class="stat"><div class="stat-value">${data.battery.current.toFixed(2)} A</div><div class="stat-label">Current</div></div>
                            <div class="stat"><div class="stat-value">${data.battery.soc.toFixed(1)}%</div><div class="stat-label">State of Charge</div></div>
                            <div class="stat"><div class="stat-value">${data.battery.time_remaining} min</div><div class="stat-label">Time Remaining</div></div>
                        `;
                    } else {
                        battHtml = '<span class="no-data">No data from SmartShunt</span>';
                    }
                    document.getElementById('battery-data').innerHTML = battHtml;

                    // Combined solar totals
                    let totalHtml = '';
                    if (data.solar && data.solar.valid) {
                        totalHtml = `
                            <div class="stat"><div class="stat-value total">${data.solar.pv_power.toFixed(0)} W</div><div class="stat-label">Total Power</div></div>
                            <div class="stat"><div class="stat-value total">${data.solar.charge_current.toFixed(2)} A</div><div class="stat-label">Total Current</div></div>
                            <div class="stat"><div class="stat-value total">${data.solar.yield_today.toFixed(2)} kWh</div><div class="stat-label">Total Today</div></div>
                        `;
                    } else {
                        totalHtml = '<span class="no-data">No solar data</span>';
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
    mppt1Data["pv_voltage"] = mppt1.getPanelVoltage();
    mppt1Data["pv_power"] = mppt1.getPanelPower();
    mppt1Data["battery_voltage"] = mppt1.getBatteryVoltage();
    mppt1Data["charge_current"] = mppt1.getChargeCurrent();
    mppt1Data["charge_state"] = mppt1.getChargeState();
    mppt1Data["error_code"] = mppt1.getErrorCode();
    mppt1Data["error_string"] = mppt1.getErrorString();
    mppt1Data["yield_today"] = mppt1.getYieldToday();
    mppt1Data["yield_yesterday"] = mppt1.getYieldYesterday();
    mppt1Data["yield_total"] = mppt1.getYieldTotal();
    mppt1Data["max_power_today"] = mppt1.getMaxPowerToday();
    mppt1Data["max_power_yesterday"] = mppt1.getMaxPowerYesterday();
    mppt1Data["last_update"] = mppt1.getLastUpdate();
    mppt1Data["valid"] = mppt1.isDataValid();

    // MPPT2 data
    JsonObject mppt2Data = doc.createNestedObject("mppt2");
    mppt2Data["pv_voltage"] = mppt2.getPanelVoltage();
    mppt2Data["pv_power"] = mppt2.getPanelPower();
    mppt2Data["battery_voltage"] = mppt2.getBatteryVoltage();
    mppt2Data["charge_current"] = mppt2.getChargeCurrent();
    mppt2Data["charge_state"] = mppt2.getChargeState();
    mppt2Data["error_code"] = mppt2.getErrorCode();
    mppt2Data["error_string"] = mppt2.getErrorString();
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
        data += "battery,device=solar-monitor,location=garage ";
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
        data += "solar,device=solar-monitor,location=garage,mppt=1 ";
        data += "pv_voltage=" + String(mppt1.getPanelVoltage(), 3) + ",";
        data += "pv_power=" + String(mppt1.getPanelPower(), 1) + ",";
        data += "battery_voltage=" + String(mppt1.getBatteryVoltage(), 3) + ",";
        data += "charge_current=" + String(mppt1.getChargeCurrent(), 3) + ",";
        data += "charge_state=\"" + mppt1.getChargeState() + "\",";
        data += "error_code=" + String(mppt1.getErrorCode()) + ",";
        data += "yield_today=" + String(mppt1.getYieldToday(), 3) + ",";
        data += "yield_yesterday=" + String(mppt1.getYieldYesterday(), 3) + ",";
        data += "yield_total=" + String(mppt1.getYieldTotal(), 3) + ",";
        data += "max_power_today=" + String(mppt1.getMaxPowerToday()) + ",";
        data += "max_power_yesterday=" + String(mppt1.getMaxPowerYesterday());
        data += "\n";
    }

    // Solar data (MPPT2)
    if (mppt2.isDataValid()) {
        data += "solar,device=solar-monitor,location=garage,mppt=2 ";
        data += "pv_voltage=" + String(mppt2.getPanelVoltage(), 3) + ",";
        data += "pv_power=" + String(mppt2.getPanelPower(), 1) + ",";
        data += "battery_voltage=" + String(mppt2.getBatteryVoltage(), 3) + ",";
        data += "charge_current=" + String(mppt2.getChargeCurrent(), 3) + ",";
        data += "charge_state=\"" + mppt2.getChargeState() + "\",";
        data += "error_code=" + String(mppt2.getErrorCode()) + ",";
        data += "yield_today=" + String(mppt2.getYieldToday(), 3) + ",";
        data += "yield_yesterday=" + String(mppt2.getYieldYesterday(), 3) + ",";
        data += "yield_total=" + String(mppt2.getYieldTotal(), 3) + ",";
        data += "max_power_today=" + String(mppt2.getMaxPowerToday()) + ",";
        data += "max_power_yesterday=" + String(mppt2.getMaxPowerYesterday());
        data += "\n";
    }

    // System data
    data += "system,device=solar-monitor,location=garage ";
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
    } else {
        Serial.printf("[InfluxDB] Failed to send data, error: %d\n", httpResponseCode);
        Serial.println("[InfluxDB] Response: " + http.getString());
    }

    http.end();
}
