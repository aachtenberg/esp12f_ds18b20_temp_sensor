/**
 * ESP32 Solar Monitor
 *
 * WiFi-enabled monitoring for Victron solar equipment:
 * - SmartShunt SHU050150050 (battery monitor)
 * - SmartSolar MPPT SCC110050210 (charge controller)
 *
 * Hardware:
 * - ESP32-WROOM-32
 * - GPIO 16 (UART2 RX) <- SmartShunt TX
 * - GPIO 19 (UART1 RX) <- MPPT TX
 * - VE.Direct: 19200 baud, 3.3V TTL
 *
 * API Endpoints:
 * - GET /           - HTML dashboard
 * - GET /api/battery - SmartShunt data (JSON)
 * - GET /api/solar   - MPPT data (JSON)
 * - GET /api/system  - Combined system data (JSON)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "VictronSmartShunt.h"
#include "VictronMPPT.h"
#include "secrets.h"

// ============================================================================
// Configuration
// ============================================================================

// UART Pin assignments
#define SMARTSHUNT_RX_PIN 16  // GPIO 16 - UART2 RX
#define MPPT_RX_PIN 19        // GPIO 19 - UART1 RX

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
HardwareSerial mpptSerial(1);   // UART1 for MPPT

// Victron device instances
VictronSmartShunt smartShunt(&shuntSerial);
VictronMPPT mppt(&mpptSerial);

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

    Serial.println("[UART] Initializing MPPT on GPIO 19...");
    mpptSerial.begin(VEDIRECT_BAUD, SERIAL_8N1, MPPT_RX_PIN, -1);

    // Initialize device drivers
    smartShunt.begin();
    mppt.begin();

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
    // Update device data (non-blocking)
    smartShunt.update();
    mppt.update();

    // Handle web requests
    server.handleClient();

    // Periodic status output
    if (millis() - lastStatusPrint >= STATUS_INTERVAL) {
        printStatus();
        lastStatusPrint = millis();
    }

    // Small delay to prevent watchdog issues
    delay(1);
}

// ============================================================================
// WiFi Setup
// ============================================================================

void setupWiFi() {
    Serial.print("[WiFi] Connecting to ");
    Serial.print(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" Connected!");
        Serial.print("[WiFi] IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WiFi] Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        Serial.println(" FAILED!");
        Serial.println("[WiFi] Running in offline mode - web interface unavailable");
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
        .stat { display: inline-block; margin: 10px 20px 10px 0; }
        .stat-value { font-size: 24px; font-weight: bold; color: #333; }
        .stat-label { font-size: 12px; color: #666; text-transform: uppercase; }
        .status-ok { color: #34C759; }
        .status-warn { color: #FF9500; }
        .status-error { color: #FF3B30; }
        a { color: #007AFF; }
        .api-links { margin-top: 20px; }
        .api-links a { display: inline-block; margin-right: 15px; }
    </style>
</head>
<body>
    <h1>ESP32 Solar Monitor</h1>

    <div class="card">
        <h2>Battery (SmartShunt)</h2>
        <div id="battery-data">Loading...</div>
    </div>

    <div class="card">
        <h2>Solar (MPPT)</h2>
        <div id="solar-data">Loading...</div>
    </div>

    <div class="api-links">
        <strong>API Endpoints:</strong>
        <a href="/api/battery">/api/battery</a>
        <a href="/api/solar">/api/solar</a>
        <a href="/api/system">/api/system</a>
    </div>

    <script>
        function updateData() {
            fetch('/api/system')
                .then(r => r.json())
                .then(data => {
                    // Battery data
                    let battHtml = '';
                    if (data.battery) {
                        battHtml = `
                            <div class="stat"><div class="stat-value">${data.battery.voltage.toFixed(2)} V</div><div class="stat-label">Voltage</div></div>
                            <div class="stat"><div class="stat-value">${data.battery.current.toFixed(2)} A</div><div class="stat-label">Current</div></div>
                            <div class="stat"><div class="stat-value">${data.battery.soc.toFixed(1)}%</div><div class="stat-label">State of Charge</div></div>
                            <div class="stat"><div class="stat-value">${data.battery.time_remaining} min</div><div class="stat-label">Time Remaining</div></div>
                        `;
                    }
                    document.getElementById('battery-data').innerHTML = battHtml || 'No data';

                    // Solar data
                    let solarHtml = '';
                    if (data.solar) {
                        solarHtml = `
                            <div class="stat"><div class="stat-value">${data.solar.pv_voltage.toFixed(1)} V</div><div class="stat-label">Panel Voltage</div></div>
                            <div class="stat"><div class="stat-value">${data.solar.pv_power.toFixed(0)} W</div><div class="stat-label">Panel Power</div></div>
                            <div class="stat"><div class="stat-value">${data.solar.charge_current.toFixed(2)} A</div><div class="stat-label">Charge Current</div></div>
                            <div class="stat"><div class="stat-value">${data.solar.charge_state}</div><div class="stat-label">State</div></div>
                            <div class="stat"><div class="stat-value">${data.solar.yield_today.toFixed(2)} kWh</div><div class="stat-label">Yield Today</div></div>
                        `;
                    }
                    document.getElementById('solar-data').innerHTML = solarHtml || 'No data';
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
    StaticJsonDocument<512> doc;

    doc["pv_voltage"] = mppt.getPanelVoltage();
    doc["pv_power"] = mppt.getPanelPower();
    doc["battery_voltage"] = mppt.getBatteryVoltage();
    doc["charge_current"] = mppt.getChargeCurrent();
    doc["charge_state"] = mppt.getChargeState();
    doc["error_code"] = mppt.getErrorCode();
    doc["error_string"] = mppt.getErrorString();
    doc["yield_today"] = mppt.getYieldToday();
    doc["yield_yesterday"] = mppt.getYieldYesterday();
    doc["yield_total"] = mppt.getYieldTotal();
    doc["max_power_today"] = mppt.getMaxPowerToday();
    doc["max_power_yesterday"] = mppt.getMaxPowerYesterday();
    doc["last_update"] = mppt.getLastUpdate();
    doc["valid"] = mppt.isDataValid();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSystemData() {
    StaticJsonDocument<1024> doc;

    // Battery subsystem
    JsonObject battery = doc.createNestedObject("battery");
    battery["voltage"] = smartShunt.getBatteryVoltage();
    battery["current"] = smartShunt.getBatteryCurrent();
    battery["soc"] = smartShunt.getStateOfCharge();
    battery["time_remaining"] = smartShunt.getTimeRemaining();
    battery["consumed_ah"] = smartShunt.getConsumedAh();
    battery["valid"] = smartShunt.isDataValid();

    // Solar subsystem
    JsonObject solar = doc.createNestedObject("solar");
    solar["pv_voltage"] = mppt.getPanelVoltage();
    solar["pv_power"] = mppt.getPanelPower();
    solar["charge_current"] = mppt.getChargeCurrent();
    solar["charge_state"] = mppt.getChargeState();
    solar["yield_today"] = mppt.getYieldToday();
    solar["yield_yesterday"] = mppt.getYieldYesterday();
    solar["error_code"] = mppt.getErrorCode();
    solar["valid"] = mppt.isDataValid();

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

    // MPPT data
    if (mppt.isDataValid()) {
        Serial.printf("Solar:   PV %.1fV | %.0fW | Charge %.2fA | %s\n",
            mppt.getPanelVoltage(),
            mppt.getPanelPower(),
            mppt.getChargeCurrent(),
            mppt.getChargeState().c_str());
        Serial.printf("         Yield: %.2f kWh today | %.2f kWh yesterday\n",
            mppt.getYieldToday(),
            mppt.getYieldYesterday());
    } else {
        Serial.println("Solar:   No data from MPPT");
    }

    // System info
    Serial.printf("System:  Uptime %lu sec | WiFi %d dBm | Heap %u bytes\n",
        (millis() - bootTime) / 1000,
        WiFi.RSSI(),
        ESP.getFreeHeap());

    Serial.println("---------------------");
}
