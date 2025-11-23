/*********
  Temperature Sensor with WiFiManager
  Based on Rui Santos project from RandomNerdTutorials.com

  Uses standard WebServer for compatibility with WiFiManager
*********/

#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  #include <HTTPClient.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClient.h>
#endif

#include <WiFiManager.h>
#include <ESP_DoubleResetDetector.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "device_config.h"

// Double Reset Detector configuration
#define DRD_TIMEOUT 3           // Seconds to wait for second reset
#define DRD_ADDRESS 0           // RTC memory address (ESP8266) or EEPROM address (ESP32)

// Create Double Reset Detector instance
DoubleResetDetector* drd;

// Device metrics structure for monitoring
struct DeviceMetrics {
    unsigned long bootTime;
    unsigned int wifiReconnects;
    unsigned int sensorReadFailures;
    unsigned int influxSendFailures;
    float minTempC;
    float maxTempC;
    unsigned long lastSuccessfulInfluxSend;

    DeviceMetrics() : bootTime(0), wifiReconnects(0), sensorReadFailures(0),
                     influxSendFailures(0),
                     minTempC(999.0f), maxTempC(-999.0f),
                     lastSuccessfulInfluxSend(0) {}

    void updateTemperature(float tempC) {
        if (tempC > -100.0f && tempC < 100.0f) {
            minTempC = min(minTempC, tempC);
            maxTempC = max(maxTempC, tempC);
        }
    }
};

// Global instances
DeviceMetrics metrics;

// Data wire is connected to GPIO 4
OneWire oneWire(ONE_WIRE_PIN);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// Variables to store temperature values
String temperatureF = "--";
String temperatureC = "--";

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = TEMPERATURE_READ_INTERVAL_MS;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = WIFI_CHECK_INTERVAL_MS;

// Create WebServer object on port 80
#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif

// MQTT for remote logging (disabled by default)
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Validate that temperature reading is valid
bool isValidTemperature(const String& temp) {
  return temp.length() > 0 && temp != "--";
}

// Update both temperature globals in one call
void updateTemperatures() {
  sensors.requestTemperatures();
  float tC = sensors.getTempCByIndex(0);
  if (tC == DEVICE_DISCONNECTED_C) {
    temperatureC = "--";
    temperatureF = "--";
    Serial.println("DS18B20 read failed");
    metrics.sensorReadFailures++;
  } else {
    char buf[16];
    dtostrf(tC, 0, 2, buf);
    temperatureC = String(buf);
    float tF = sensors.toFahrenheit(tC);
    dtostrf(tF, 0, 2, buf);
    temperatureF = String(buf);
    Serial.println("Temperature C: " + temperatureC);
    Serial.println("Temperature F: " + temperatureF);
    metrics.updateTemperature(tC);
  }
}

void sendToInfluxDB() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping InfluxDB");
    return;
  }

  if (!isValidTemperature(temperatureC)) {
    Serial.println("Invalid temperature, skipping InfluxDB");
    return;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + String(INFLUXDB_ORG) + "&bucket=" + String(INFLUXDB_BUCKET) + "&precision=s";
  Serial.println("InfluxDB: " + url);

  http.begin(client, url);
  http.addHeader("Authorization", "Token " + String(INFLUXDB_TOKEN));
  http.addHeader("Content-Type", "text/plain");

  String deviceTag = String(DEVICE_LOCATION);
  deviceTag.replace(" ", "_");
  String payload = "temperature,sensor=ds18b20,location=esp12f,device=" + deviceTag + " tempC=" + temperatureC + ",tempF=" + temperatureF;

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    if (httpCode == 204) {
      Serial.println("InfluxDB: 204 OK");
      metrics.lastSuccessfulInfluxSend = millis();
    } else {
      String response = http.getString();
      Serial.println("InfluxDB Code " + String(httpCode) + ": " + response);
      metrics.influxSendFailures++;
    }
  } else {
    Serial.println("InfluxDB POST failed: " + String(http.errorToString(httpCode)));
    metrics.influxSendFailures++;
  }
  http.end();
}

// HTML page with template placeholders
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE HTML><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>%PAGE_TITLE%</title><style>body{margin:0;padding:8px;background:#0f172a;font-family:system-ui;color:#e2e8f0;text-align:center}.c{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:16px;max-width:350px;margin:0 auto}.h{margin-bottom:12px}.dn{font-size:1.3rem;font-weight:600;color:#94a3b8;margin-bottom:4px}.st{font-size:0.8rem;color:#94a3b8}.si{display:inline-block;width:8px;height:8px;background:#10b981;border-radius:50%;margin-right:4px;animation:p 2s infinite}@keyframes p{0%,100%{opacity:1}50%{opacity:0.5}}.td{background:linear-gradient(135deg,#1e3a5f,#0f172a);border:1px solid #334155;border-radius:10px;padding:16px;margin-bottom:12px}.tdc{display:flex;justify-content:center;align-items:baseline;gap:4px}.tv{font-size:3rem;font-weight:700;color:#38bdf8}.tu{font-size:0.9rem;color:#94a3b8}.f{background:#0f172a;border:1px solid #334155;border-radius:8px;padding:10px;display:flex;justify-content:center;align-items:center;gap:6px}.tl{font-size:0.85rem;color:#94a3b8}.tr{font-size:1.5rem;font-weight:700;color:#38bdf8}.ft{margin-top:12px;padding-top:8px;border-top:1px solid #334155;font-size:0.7rem;color:#64748b}</style></head><body><div class="c"><div class="h"><div class="dn">%PAGE_TITLE%</div><div><span class="si"></span><span class="st">Live</span></div></div><div class="td"><div class="tdc"><div class="tv" id="tc">%TEMPERATUREC%</div><div class="tu">C</div></div></div><div class="f"><span class="tr" id="tf">%TEMPERATUREF%</span><span class="tl">F</span></div><div class="ft">Updates every 15s</div></div><script>function u(){fetch('/temperaturec').then(r=>r.text()).then(d=>{document.getElementById('tc').textContent=d}).catch(e=>{});fetch('/temperaturef').then(r=>r.text()).then(d=>{document.getElementById('tf').textContent=d}).catch(e=>{});}u();setInterval(u,15000);</script></body></html>)rawliteral";

// Process template placeholders
String processTemplate(const String& html) {
  String result = html;
  result.replace("%PAGE_TITLE%", DEVICE_LOCATION);
  result.replace("%TEMPERATUREC%", temperatureC);
  result.replace("%TEMPERATUREF%", temperatureF);
  return result;
}

// Health check endpoint response
String getHealthStatus() {
  JsonDocument doc;

  doc["status"] = "ok";
  doc["device"] = DEVICE_LOCATION;
  doc["board"] = DEVICE_BOARD;
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["temperature_valid"] = isValidTemperature(temperatureC);
  doc["current_temp_c"] = temperatureC;
  doc["current_temp_f"] = temperatureF;

  doc["metrics"]["wifi_reconnects"] = metrics.wifiReconnects;
  doc["metrics"]["sensor_read_failures"] = metrics.sensorReadFailures;
  doc["metrics"]["influx_send_failures"] = metrics.influxSendFailures;

  if (metrics.minTempC < 900.0f) {
    doc["metrics"]["min_temp_c"] = metrics.minTempC;
  }
  if (metrics.maxTempC > -900.0f) {
    doc["metrics"]["max_temp_c"] = metrics.maxTempC;
  }
  if (metrics.lastSuccessfulInfluxSend > 0) {
    doc["last_success"]["influx_seconds_ago"] = (millis() - metrics.lastSuccessfulInfluxSend) / 1000;
  }

  String response;
  serializeJson(doc, response);
  return response;
}

// Web request handlers
void handleRoot() {
  String html = FPSTR(index_html);
  html = processTemplate(html);
  server.send(200, "text/html", html);
}

void handleTemperatureC() {
  server.send(200, "text/plain", temperatureC);
}

void handleTemperatureF() {
  server.send(200, "text/plain", temperatureF);
}

void handleHealth() {
  String response = getHealthStatus();
  server.send(200, "application/json", response);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/temperaturec", HTTP_GET, handleTemperatureC);
  server.on("/temperaturef", HTTP_GET, handleTemperatureF);
  server.on("/health", HTTP_GET, handleHealth);
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");
}

void setupWiFi() {
  // Initialize Double Reset Detector
  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);

  // Create WiFiManager instance
  WiFiManager wm;

  // Set custom AP name based on device location
  String apName = String(DEVICE_LOCATION);
  apName.replace(" ", "-");
  apName = "Temp-" + apName + "-Setup";

  // Don't use timeout - keep retrying forever in weak WiFi zones
  wm.setConnectTimeout(0);

  #ifdef ESP32
    WiFi.setSleep(false);
  #else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
  #endif
  WiFi.mode(WIFI_STA);

  // Check for double reset - enter config portal if detected
  if (drd->detectDoubleReset()) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("  DOUBLE RESET DETECTED");
    Serial.println("  Starting WiFi Configuration Portal");
    Serial.println("========================================");
    Serial.println();
    Serial.println("[WiFi] Connect to AP: " + apName);
    Serial.println("[WiFi] Then open http://192.168.4.1 in browser");
    Serial.println();

    if (!wm.startConfigPortal(apName.c_str())) {
      Serial.println("[WiFi] Failed to connect after config portal");
      Serial.println("[WiFi] Restarting...");
      delay(3000);
      ESP.restart();
    }
  } else {
    Serial.println("[WiFi] Normal boot - attempting connection...");
    Serial.println("[WiFi] (Double-reset within 3 seconds to enter config mode)");
    Serial.println();

    if (!wm.autoConnect(apName.c_str())) {
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

void setup() {
  // Initialize metrics
  metrics.bootTime = millis();

  // Serial port for debugging
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("     Temperature Sensor");
  Serial.println("========================================");
  Serial.println();

  // Start up the DS18B20 library
  sensors.begin();
  updateTemperatures();

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

void loop() {
  // Must call drd->loop() to keep double reset detection working
  drd->loop();

  // Handle web requests
  server.handleClient();

  // Periodic WiFi check
  unsigned long now = millis();
  if (now - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = now;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting reconnection...");
      WiFi.reconnect();
      metrics.wifiReconnects++;
    }
  }

  // Periodic temperature reading and InfluxDB send
  if ((millis() - lastTime) > timerDelay) {
    updateTemperatures();
    if (temperatureC != "--") {
      sendToInfluxDB();
    }
    lastTime = millis();
  }
}
