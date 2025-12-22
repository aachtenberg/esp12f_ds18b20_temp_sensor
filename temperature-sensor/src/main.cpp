/*********
  Temperature Sensor with WiFiManager
  Based on Rui Santos project from RandomNerdTutorials.com

  Uses standard WebServer for compatibility with WiFiManager
*********/

#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <WiFiClient.h>
#endif

#include <WiFiManager.h>
#include <ESP_DoubleResetDetector.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#ifdef ESP32
  #include <SPIFFS.h>
#else
  #include <LittleFS.h>
#endif
#include "secrets.h"
#include "device_config.h"
#include "trace.h"
#include "display.h"
#include "version.h"

// Double Reset Detector configuration
#define DRD_TIMEOUT 3           // Seconds to wait for second reset
#define DRD_ADDRESS 0           // RTC memory address (ESP8266) or EEPROM address (ESP32)

// Create Double Reset Detector instance
DoubleResetDetector* drd;

// Device name storage
char deviceName[40] = "Temp Sensor";  // Default name
const char* DEVICE_NAME_FILE = "/device_name.txt";

// Device metrics structure for monitoring
struct DeviceMetrics {
    unsigned long bootTime;
    unsigned int wifiReconnects;
    unsigned int sensorReadFailures;
    unsigned int mqttPublishFailures;
    float minTempC;
    float maxTempC;
    unsigned long lastSuccessfulMqttPublish;
  float batteryVoltage;
  int batteryPercent;

    DeviceMetrics() : bootTime(0), wifiReconnects(0), sensorReadFailures(0),
                      mqttPublishFailures(0),
                      minTempC(999.0f), maxTempC(-999.0f),
            lastSuccessfulMqttPublish(0),
            batteryVoltage(0.0f), batteryPercent(-1) {}

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

// Create WebServer object on port 80
#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif

// MQTT for remote logging (disabled by default)
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// MQTT settings and timers
String chipId;
String topicBase;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastMqttConnectionCheck = 0;  // Track when we last checked MQTT status
unsigned long lastPublishTime = 0;
unsigned long lastSuccessfulMqttCheck = 0;
unsigned long mqttReconnectInterval = 5000;  // Dynamic backoff interval
unsigned long mqttBackoffResetTime = 0;      // Track when backoff started for periodic reset
unsigned int mqttConsecutiveFailures = 0;    // Count consecutive connection failures
int lastMqttState = MQTT_DISCONNECTED;       // Track last known MQTT state for change detection
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long MQTT_RECONNECT_INTERVAL_MAX_MS = 30000;  // Max 30s backoff (reduced from 60s)
const unsigned long MQTT_CONNECTION_CHECK_INTERVAL_MS = 30000;  // Check connection health every 30s
const unsigned long MQTT_PUBLISH_INTERVAL_MS = 30000;
const unsigned long MQTT_STALE_CONNECTION_TIMEOUT_MS = 120000;  // Force reconnect if no activity for 2 mins
const unsigned long MQTT_BACKOFF_RESET_INTERVAL_MS = 300000;   // Reset backoff to minimum every 5 mins
const unsigned int MQTT_MAX_CONSECUTIVE_FAILURES = 10;         // Reset backoff after this many failures

// WiFi reconnection tracking
unsigned long wifiDisconnectedSince = 0;
const unsigned long WIFI_STALE_CONNECTION_TIMEOUT_MS = 90000;   // Full WiFi restart after 90s disconnected
const int WIFI_MIN_RSSI = -85;  // Defer operations if signal weaker than this

// Timer variables
const unsigned long publishIntervalMs = MQTT_PUBLISH_INTERVAL_MS;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = WIFI_CHECK_INTERVAL_MS;
unsigned long lastDisplayUpdate = 0;
// Periodic status logging
unsigned long lastStatusLog = 0;

// Load device name from filesystem
void loadDeviceName() {
#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    Serial.println("[FS] Failed to mount SPIFFS");
    return;
  }
  if (SPIFFS.exists(DEVICE_NAME_FILE)) {
    File file = SPIFFS.open(DEVICE_NAME_FILE, "r");
#else
  if (!LittleFS.begin()) {
    Serial.println("[FS] Failed to mount LittleFS");
    return;
  }
  if (LittleFS.exists(DEVICE_NAME_FILE)) {
    File file = LittleFS.open(DEVICE_NAME_FILE, "r");
#endif
    if (file) {
      String name = file.readStringUntil('\n');
      name.trim();
      if (name.length() > 0 && name.length() < sizeof(deviceName)) {
        strcpy(deviceName, name.c_str());
        Serial.print("[Config] Loaded device name: ");
        Serial.println(deviceName);
      }
      file.close();
    }
  } else {
    Serial.println("[Config] No saved device name, using default");
  }
}

// Save device name to filesystem
void saveDeviceName(const char* name) {
#ifdef ESP32
  File file = SPIFFS.open(DEVICE_NAME_FILE, "w");
#else
  File file = LittleFS.open(DEVICE_NAME_FILE, "w");
#endif
  if (file) {
    file.println(name);
    file.close();
    Serial.print("[Config] Saved device name: ");
    Serial.println(name);
  } else {
    Serial.println("[Config] Failed to save device name");
  }
}

// Forward declarations
String generateChipId();
String sanitizeDeviceName(const char* name);
void updateTopicBase();
bool ensureMqttConnected();
void publishEvent(const String& eventType, const String& message, const String& severity = "info");
void publishTemperature();
void publishStatus();

// Validate that temperature reading is valid
bool isValidTemperature(const String& temp) {
  return temp.length() > 0 && temp != "--";
}

// Read battery voltage and percentage (optional)
void readBattery() {
#ifdef BATTERY_MONITOR_ENABLED
  int raw = analogRead(BATTERY_PIN);
  float voltage = (raw / ADC_MAX) * REF_VOLTAGE * VOLTAGE_DIVIDER * CALIBRATION;
  
  // Convert to percentage (3.0V = 0%, 4.2V = 100%)
  float percent = ((voltage - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V)) * 100.0;
  percent = constrain(percent, 0, 100);
  
  metrics.batteryVoltage = voltage;
  metrics.batteryPercent = (int)percent;
  
  Serial.printf("[Battery] %.2fV, %d%%\n", voltage, (int)percent);
#endif
}

// Update both temperature globals in one call
void updateTemperatures() {
  sensors.requestTemperatures();
  float tC = sensors.getTempCByIndex(0);
  if (tC == DEVICE_DISCONNECTED_C) {
    temperatureC = "--";
    temperatureF = "--";
    metrics.sensorReadFailures++;
    publishEvent("sensor_error", "DS18B20 read failed", "error");
  } else {
    char buf[16];
    dtostrf(tC, 0, 2, buf);
    temperatureC = String(buf);
    float tF = sensors.toFahrenheit(tC);
    dtostrf(tF, 0, 2, buf);
    temperatureF = String(buf);
    metrics.updateTemperature(tC);
  }
}

String generateChipId() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toUpperCase();
  return mac;
}

// Setup OTA updates
void setupOTA() {
  // Set hostname for OTA
  ArduinoOTA.setHostname(deviceName);
  
  // Set password from secrets.h
  ArduinoOTA.setPassword(OTA_PASSWORD);
  // Removed verbose password confirmation logging
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("[OTA] Update started: " + type);
    publishEvent("ota_start", "OTA update starting (" + type + ")", "warning");
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] Update complete");
    publishEvent("ota_complete", "OTA update completed successfully", "info");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned int lastPercent = 0;
    unsigned int percent = (progress / (total / 100));
    if (percent != lastPercent && percent % 25 == 0) {  // Changed from 10% to 25% to reduce logging
      Serial.printf("[OTA] Progress: %u%%\n", percent);
      lastPercent = percent;
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    String errorMsg;
    if (error == OTA_AUTH_ERROR) errorMsg = "Auth Failed";
    else if (error == OTA_BEGIN_ERROR) errorMsg = "Begin Failed";
    else if (error == OTA_CONNECT_ERROR) errorMsg = "Connect Failed";
    else if (error == OTA_RECEIVE_ERROR) errorMsg = "Receive Failed";
    else if (error == OTA_END_ERROR) errorMsg = "End Failed";
    Serial.println(errorMsg);
    publishEvent("ota_error", "OTA update failed: " + errorMsg, "error");
  });
  
  ArduinoOTA.begin();
  Serial.println("[OTA] Ready");
  // Removed verbose hostname logging
}

String sanitizeDeviceName(const char* name) {
  String sanitized = String(name);
  sanitized.replace(" ", "-");
  return sanitized;
}

void updateTopicBase() {
  topicBase = String("esp-sensor-hub/") + sanitizeDeviceName(deviceName);
}

String getTopicTemperature() {
  return topicBase + "/temperature";
}

String getTopicStatus() {
  return topicBase + "/status";
}

String getTopicEvents() {
  return topicBase + "/events";
}

// Helper to get MQTT state description for logging
const char* getMqttStateString(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
    case MQTT_CONNECTION_LOST: return "CONNECTION_LOST";
    case MQTT_CONNECT_FAILED: return "CONNECT_FAILED";
    case MQTT_DISCONNECTED: return "DISCONNECTED";
    case MQTT_CONNECTED: return "CONNECTED";
    case MQTT_CONNECT_BAD_PROTOCOL: return "BAD_PROTOCOL";
    case MQTT_CONNECT_BAD_CLIENT_ID: return "BAD_CLIENT_ID";
    case MQTT_CONNECT_UNAVAILABLE: return "UNAVAILABLE";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "BAD_CREDENTIALS";
    case MQTT_CONNECT_UNAUTHORIZED: return "UNAUTHORIZED";
    default: return "UNKNOWN";
  }
}

bool ensureMqttConnected() {
  unsigned long now = millis();

  // Log state changes for debugging
  int currentState = mqttClient.state();
  if (currentState != lastMqttState) {
    Serial.printf("[MQTT] State changed: %s -> %s\n",
                  getMqttStateString(lastMqttState),
                  getMqttStateString(currentState));
    lastMqttState = currentState;
  }

  // Check for stale connection: if connected but no successful publish in 2 minutes, force reconnect
  if (mqttClient.connected()) {
    if (metrics.lastSuccessfulMqttPublish > 0 &&
        (now - metrics.lastSuccessfulMqttPublish) > MQTT_STALE_CONNECTION_TIMEOUT_MS) {
      Serial.println("[MQTT] Stale connection detected - forcing reconnect");
      Serial.printf("[MQTT] Last successful publish was %lu seconds ago\n",
                    (now - metrics.lastSuccessfulMqttPublish) / 1000);
      mqttClient.disconnect();
      delay(100);
      // Fall through to reconnection logic below
    } else {
      lastSuccessfulMqttCheck = now;
      mqttConsecutiveFailures = 0;  // Reset failure count on successful connection
      mqttBackoffResetTime = 0;     // Reset backoff timer
      return true;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[MQTT] WiFi not connected, cannot connect to broker");
    return false;
  }

  // Periodic backoff reset: if we've been failing for too long, reset to try more aggressively
  if (mqttBackoffResetTime > 0 && (now - mqttBackoffResetTime) > MQTT_BACKOFF_RESET_INTERVAL_MS) {
    Serial.println("[MQTT] Backoff reset - trying more aggressively");
    mqttReconnectInterval = MQTT_RECONNECT_INTERVAL_MS;
    mqttBackoffResetTime = now;
    mqttConsecutiveFailures = 0;
  }

  // Also reset backoff after too many consecutive failures (try fresh)
  if (mqttConsecutiveFailures >= MQTT_MAX_CONSECUTIVE_FAILURES) {
    Serial.printf("[MQTT] %u consecutive failures - resetting backoff\n", mqttConsecutiveFailures);
    mqttReconnectInterval = MQTT_RECONNECT_INTERVAL_MS;
    mqttConsecutiveFailures = 0;
  }

  // Rate limit reconnection attempts using dynamic backoff interval
  if (lastMqttReconnectAttempt > 0 && (now - lastMqttReconnectAttempt) < mqttReconnectInterval) {
    return false;
  }

  lastMqttReconnectAttempt = now;

  // Start tracking backoff time on first failure
  if (mqttBackoffResetTime == 0) {
    mqttBackoffResetTime = now;
  }

  String clientId = String(deviceName) + "-" + chipId;
  Serial.printf("[MQTT] Attempting connection to %s:%d as %s\n",
                MQTT_BROKER, MQTT_PORT, clientId.c_str());

  // Connect anonymously if no credentials provided, otherwise use authentication
  bool connected;
  if (strlen(MQTT_USER) == 0) {
    connected = mqttClient.connect(clientId.c_str());
  } else {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
  }

  if (connected) {
    Serial.println("[MQTT] Connected to broker");
    lastSuccessfulMqttCheck = now;
    mqttReconnectInterval = MQTT_RECONNECT_INTERVAL_MS;  // Reset backoff on success
    mqttConsecutiveFailures = 0;
    mqttBackoffResetTime = 0;
    lastMqttState = MQTT_CONNECTED;
  } else {
    mqttConsecutiveFailures++;
    int state = mqttClient.state();
    Serial.printf("[MQTT] Connection failed: %s (state: %d), failures: %u, retry in %lu sec\n",
                  getMqttStateString(state), state, mqttConsecutiveFailures,
                  mqttReconnectInterval / 1000);
    metrics.mqttPublishFailures++;
    // Exponential backoff: double interval up to max
    mqttReconnectInterval = min(mqttReconnectInterval * 2, MQTT_RECONNECT_INTERVAL_MAX_MS);
  }
  return connected;
}

bool publishJson(const String& topic, JsonDocument& doc, bool retain = false) {
  if (!ensureMqttConnected()) {
    return false;
  }

  String payload;
  serializeJson(doc, payload);
  
  bool ok = mqttClient.publish(topic.c_str(), payload.c_str(), retain);
  if (!ok) {
    metrics.mqttPublishFailures++;
  } else {
    metrics.lastSuccessfulMqttPublish = millis();
  }
  return ok;
}

void publishEvent(const String& eventType, const String& message, const String& severity) {
  StaticJsonDocument<256> doc;
  doc["device"] = deviceName;
  doc["chip_id"] = chipId;
  doc["firmware_version"] = getFirmwareVersion();
  doc["trace_id"] = Trace::getTraceId();
  doc["traceparent"] = Trace::getTraceparent();
  doc["seq_num"] = Trace::getSequenceNumber();
  doc["schema_version"] = 1;
  doc["event"] = eventType;
  doc["severity"] = severity;
  doc["timestamp"] = millis() / 1000;
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  if (message.length() > 0) {
    doc["message"] = message;
  }
  publishJson(getTopicEvents(), doc, false);
}

void publishTemperature() {
  if (!isValidTemperature(temperatureC)) {
    return;
  }
  
  StaticJsonDocument<256> doc;
  doc["device"] = deviceName;
  doc["chip_id"] = chipId;
  doc["trace_id"] = Trace::getTraceId();
  doc["traceparent"] = Trace::getTraceparent();
  #ifdef BATTERY_MONITOR_ENABLED
    if (metrics.batteryPercent >= 0) {
      doc["battery_voltage"] = metrics.batteryVoltage;
      doc["battery_percent"] = metrics.batteryPercent;
    }
  #endif
  doc["seq_num"] = Trace::getSequenceNumber();
  doc["schema_version"] = 1;
  doc["timestamp"] = millis() / 1000;
  doc["celsius"] = temperatureC.toFloat();
  doc["fahrenheit"] = temperatureF.toFloat();
  publishJson(getTopicTemperature(), doc, false);
}

void publishStatus() {
  StaticJsonDocument<256> doc;
  doc["device"] = deviceName;
  doc["chip_id"] = chipId;
  doc["firmware_version"] = getFirmwareVersion();
  doc["trace_id"] = Trace::getTraceId();
  doc["traceparent"] = Trace::getTraceparent();
  doc["seq_num"] = Trace::getSequenceNumber();
  doc["schema_version"] = 1;
  doc["timestamp"] = millis() / 1000;
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifi_rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -999;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["sensor_healthy"] = isValidTemperature(temperatureC);
  doc["wifi_reconnects"] = metrics.wifiReconnects;
  doc["sensor_read_failures"] = metrics.sensorReadFailures;
  #ifdef BATTERY_MONITOR_ENABLED
    if (metrics.batteryPercent >= 0) {
      doc["battery_voltage"] = metrics.batteryVoltage;
      doc["battery_percent"] = metrics.batteryPercent;
    }
  #endif
  publishJson(getTopicStatus(), doc, true);
}

// HTML page with template placeholders
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE HTML><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>%PAGE_TITLE%</title><style>body{margin:0;padding:8px;background:#0f172a;font-family:system-ui;color:#e2e8f0;text-align:center}.c{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:16px;max-width:350px;margin:0 auto}.h{margin-bottom:12px}.dn{font-size:1.3rem;font-weight:600;color:#94a3b8;margin-bottom:4px}.st{font-size:0.8rem;color:#94a3b8}.si{display:inline-block;width:8px;height:8px;background:#10b981;border-radius:50%;margin-right:4px;animation:p 2s infinite}@keyframes p{0%,100%{opacity:1}50%{opacity:0.5}}.td{background:linear-gradient(135deg,#1e3a5f,#0f172a);border:1px solid #334155;border-radius:10px;padding:16px;margin-bottom:12px}.tdc{display:flex;justify-content:center;align-items:baseline;gap:4px}.tv{font-size:3rem;font-weight:700;color:#38bdf8}.tu{font-size:0.9rem;color:#94a3b8}.f{background:#0f172a;border:1px solid #334155;border-radius:8px;padding:10px;display:flex;justify-content:center;align-items:center;gap:6px}.tl{font-size:0.85rem;color:#94a3b8}.tr{font-size:1.5rem;font-weight:700;color:#38bdf8}.ft{margin-top:12px;padding-top:8px;border-top:1px solid #334155;font-size:0.7rem;color:#64748b}</style></head><body><div class="c"><div class="h"><div class="dn">%PAGE_TITLE%</div><div><span class="si"></span><span class="st">Live</span></div></div><div class="td"><div class="tdc"><div class="tv" id="tc">%TEMPERATUREC%</div><div class="tu">C</div></div></div><div class="f"><span class="tr" id="tf">%TEMPERATUREF%</span><span class="tl">F</span></div><div class="ft">Updates every 15s</div></div><script>function u(){fetch('/temperaturec').then(r=>r.text()).then(d=>{document.getElementById('tc').textContent=d}).catch(e=>{});fetch('/temperaturef').then(r=>r.text()).then(d=>{document.getElementById('tf').textContent=d}).catch(e=>{});}u();setInterval(u,15000);</script></body></html>)rawliteral";

// Process template placeholders
String processTemplate(const String& html) {
  String result = html;
  result.replace("%PAGE_TITLE%", deviceName);
  result.replace("%TEMPERATUREC%", temperatureC);
  result.replace("%TEMPERATUREF%", temperatureF);
  return result;
}

// Health check endpoint response
String getHealthStatus() {
  JsonDocument doc;

  doc["status"] = "ok";
  doc["device"] = deviceName;
  doc["board"] = DEVICE_BOARD;
  doc["firmware_version"] = getFirmwareVersion();
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["temperature_valid"] = isValidTemperature(temperatureC);
  doc["current_temp_c"] = temperatureC;
  doc["current_temp_f"] = temperatureF;

#ifdef BATTERY_MONITOR_ENABLED
  if (metrics.batteryPercent >= 0) {
    doc["battery_voltage"] = metrics.batteryVoltage;
    doc["battery_percent"] = metrics.batteryPercent;
  }
#endif

  doc["metrics"]["wifi_reconnects"] = metrics.wifiReconnects;
  doc["metrics"]["sensor_read_failures"] = metrics.sensorReadFailures;
  doc["metrics"]["mqtt_publish_failures"] = metrics.mqttPublishFailures;

  if (metrics.minTempC < 900.0f) {
    doc["metrics"]["min_temp_c"] = metrics.minTempC;
  }
  if (metrics.maxTempC > -900.0f) {
    doc["metrics"]["max_temp_c"] = metrics.maxTempC;
  }
  if (metrics.lastSuccessfulMqttPublish > 0) {
    doc["last_success"]["mqtt_seconds_ago"] = (millis() - metrics.lastSuccessfulMqttPublish) / 1000;
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
#ifndef API_ENDPOINTS_ONLY
  server.on("/", HTTP_GET, handleRoot);
#endif
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
  String apName = String(deviceName);
  apName.replace(" ", "-");
  apName = "Temp-" + apName + "-Setup";

  // Create custom parameter for device name (must persist throughout function)
  WiFiManagerParameter custom_device_name("device_name", "Device Name", deviceName, 40);
  wm.addParameter(&custom_device_name);

  // Don't use timeout - keep retrying forever in weak WiFi zones
  wm.setConnectTimeout(0);
  
  // Enable WiFi power save mode (modem sleep) for low power operation
  #ifdef ESP32
    WiFi.setSleep(true);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  #else
    WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
  #endif
  Serial.println("[POWER] WiFi modem sleep enabled");
  
  WiFi.mode(WIFI_STA);
  // Enable auto-reconnect and persist credentials (low-power friendly)
  #ifdef ESP8266
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  #else
    // ESP32 also supports auto reconnect in Arduino core
    WiFi.setAutoReconnect(true);
  #endif
  
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

    // Set save config callback to save device name
    bool shouldSaveConfig = false;
    wm.setSaveConfigCallback([&shouldSaveConfig](){
      Serial.println("[Config] Configuration saved, will update device name...");
      shouldSaveConfig = true;
    });

    if (!wm.startConfigPortal(apName.c_str())) {
      Serial.println("[WiFi] Failed to connect after config portal");
      Serial.println("[WiFi] Restarting...");
      delay(3000);
      ESP.restart();
    } else {
      // Save the device name if config was saved
      if (shouldSaveConfig) {
        const char* newName = custom_device_name.getValue();
        Serial.print("[Config] New device name: ");
        Serial.println(newName);
        String oldName = String(deviceName);
        strcpy(deviceName, newName);
        updateTopicBase();
        saveDeviceName(deviceName);
        
        // Log configuration change with details
        if (oldName != String(newName)) {
          publishEvent("device_configured", "Name: '" + oldName + "' -> '" + String(newName) + "', SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString(), "info");
        } else {
          publishEvent("device_configured", "WiFi reconfigured - SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString() + ", Name unchanged: " + oldName, "info");
        }
      }
    }
  } else {
    Serial.println("[WiFi] Normal boot - attempting connection...");
    // Removed verbose double-reset message to reduce logging

    // Set save config callback for autoConnect mode
    bool shouldSaveConfig = false;
    wm.setSaveConfigCallback([&shouldSaveConfig](){
      Serial.println("[Config] Configuration saved, will update device name...");
      shouldSaveConfig = true;
    });

    if (!wm.autoConnect(apName.c_str())) {
      Serial.println("[WiFi] Failed to connect - running in offline mode");
      // Removed verbose double-reset message to reduce logging
    } else if (shouldSaveConfig) {
      // Save the device name if config was saved
      const char* newName = custom_device_name.getValue();
      Serial.print("[Config] New device name: ");
      Serial.println(newName);
      String oldName = String(deviceName);
      strcpy(deviceName, newName);
      updateTopicBase();
      saveDeviceName(deviceName);
      
      // Log configuration change with details
      if (oldName != String(newName)) {
        publishEvent("device_configured", "Name: '" + oldName + "' -> '" + String(newName) + "', SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString(), "info");
      } else {
        publishEvent("device_configured", "WiFi reconfigured - SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString() + ", Name unchanged: " + oldName, "info");
      }
    }
  }

  // Print connection status - simplified
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected to %s, IP: %s, RSSI: %d dBm\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    
    // Log WiFi connection event
    publishEvent("wifi_connected", "Connected to " + WiFi.SSID() + " with IP " + WiFi.localIP().toString(), "info");
  } else {
    Serial.println("[WiFi] Not connected - running in offline mode");
  }
}

void setup() {
  // Initialize metrics
  metrics.bootTime = millis();

  // Initialize trace instrumentation
  Trace::init();

  // Serial port for debugging
  Serial.begin(115200);
  delay(1000);

  // Set CPU frequency to low-power mode
  #ifdef ESP8266
    system_update_cpu_freq(CPU_FREQ_MHZ);
  #else
    setCpuFrequencyMhz(CPU_FREQ_MHZ);
  #endif
  Serial.print("[POWER] CPU frequency set to ");
  Serial.print(CPU_FREQ_MHZ);
  Serial.println(" MHz");

  // Print reset reason for diagnostics
  #ifdef ESP8266
    Serial.print("[DEBUG] Reset reason: ");
    Serial.println(ESP.getResetReason());
    Serial.print("[DEBUG] Free heap: ");
    Serial.println(ESP.getFreeHeap());
  #endif

  Serial.println();
  Serial.println("========================================");
  Serial.println("     Temperature Sensor");
  Serial.println("========================================");
  Serial.println();

  // Load device name from filesystem
  loadDeviceName();

  chipId = generateChipId();
  updateTopicBase();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
#ifdef ESP32
  mqttClient.setBufferSize(2048);  // Larger buffer for ESP32 with battery monitoring
#else
  mqttClient.setBufferSize(1024);  // Standard buffer for ESP8266
#endif
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(5);  // Reduced from 15s to minimize blocking during connection issues

  // Start up the DS18B20 library
  sensors.begin();
  updateTemperatures();

  #ifdef BATTERY_MONITOR_ENABLED
    // Configure ADC for battery monitoring
    analogReadResolution(12);
    readBattery();
  #endif

  // Initialize OLED display
  initDisplay();

  // Connect to WiFi
  setupWiFi();

  // Setup OTA updates (after WiFi is connected)
  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
  }

  // Setup web server (only if enabled)
  #if HTTP_SERVER_ENABLED
    setupWebServer();
  #else
    Serial.println("[HTTP] Web server disabled (battery mode)");
  #endif

      // Log device boot/reset event
      String resetReason;
      #ifdef ESP8266
        resetReason = ESP.getResetReason();
      #else
        resetReason = String(esp_reset_reason());
      #endif
      publishEvent("device_boot", "Device started - Reset reason: " + resetReason + ", Uptime: 0s, Free heap: " + String(ESP.getFreeHeap()) + " bytes", "info");

      // Publish initial readings/status immediately
      publishTemperature();
      publishStatus();
      lastPublishTime = millis();

  Serial.println();
  Serial.println("========================================");
  Serial.println("     Setup Complete");
  Serial.println("========================================");
  Serial.println();
}

void loop() {
  // Must call drd->loop() to keep double reset detection working
  drd->loop();

  // Handle web requests first for responsiveness
  server.handleClient();

  // Process MQTT messages - detect connection loss early
  if (!mqttClient.loop()) {
    // loop() returns false if disconnected - log state change immediately
    int currentState = mqttClient.state();
    if (currentState != lastMqttState && lastMqttState == MQTT_CONNECTED) {
      Serial.printf("[MQTT] Connection lost! State: %s (%d)\n",
                    getMqttStateString(currentState), currentState);
      lastMqttState = currentState;
    }
  }

  unsigned long now = millis();

  // MQTT connection management - check health periodically (catches stale connections)
  if ((now - lastMqttConnectionCheck) > MQTT_CONNECTION_CHECK_INTERVAL_MS) {
    lastMqttConnectionCheck = now;
    ensureMqttConnected();  // This checks for stale connections even if "connected"
  }

  // Reconnect if disconnected, respecting backoff interval
  if (!mqttClient.connected() && (now - lastMqttReconnectAttempt) >= mqttReconnectInterval) {
    ensureMqttConnected();
  }

  if (now - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = now;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting reconnection...");
      // Check if credentials exist before attempting reconnect
      if (WiFi.SSID().length() > 0) {
        WiFi.reconnect();
      } else {
        Serial.println("[WiFi] No stored credentials - skipping reconnect");
      }
      metrics.wifiReconnects++;
      // Track how long we've been offline
      if (wifiDisconnectedSince == 0) {
        wifiDisconnectedSince = now;
      }
      // If WiFi has been disconnected for a long time, do a clean restart of WiFi
      if ((now - wifiDisconnectedSince) > WIFI_STALE_CONNECTION_TIMEOUT_MS) {
        Serial.println("[WiFi] Stale disconnect detected (>90s). Restarting WiFi...");
        publishEvent("wifi_reset", "WiFi stale disconnect - restarting interface", "warning");
        WiFi.disconnect();
        // Don't use blocking delay - just continue and let next loop handle it
        WiFi.mode(WIFI_STA);
        #ifdef ESP8266
          WiFi.persistent(true);
          WiFi.setAutoReconnect(true);
        #else
          WiFi.setAutoReconnect(true);
        #endif
        WiFi.reconnect();
        wifiDisconnectedSince = now;  // reset the timer after forced restart
      }
      // Only log every 5 reconnects to avoid spam
      if (metrics.wifiReconnects % 5 == 1) {
        publishEvent("wifi_reconnect", "WiFi disconnected, reconnect attempt #" + String(metrics.wifiReconnects), "warning");
      }
    } else if (wifiDisconnectedSince != 0) {
      // We were disconnected previously and now back online
      publishEvent("wifi_reconnected", "WiFi reconnected - SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString(), "info");
      wifiDisconnectedSince = 0;
    }
  }

  // Periodic temperature reading and MQTT publish
  if ((now - lastPublishTime) > publishIntervalMs) {
    #ifdef ESP8266
      // Check heap before operations
      uint32_t freeHeap = ESP.getFreeHeap();
      if (freeHeap < 8000) {
        Serial.print("[WARNING] Low heap: ");
        Serial.println(freeHeap);
      }
    #endif
    
    // Check signal strength - defer MQTT if too weak
    int rssi = WiFi.RSSI();
    if (rssi < WIFI_MIN_RSSI) {
      Serial.printf("[WARNING] Signal too weak (%d dBm), deferring MQTT publish\n", rssi);
      lastPublishTime = now;  // Reset timer to retry later
      // Still call OTA.handle() before returning
      ArduinoOTA.handle();
      return;  // Skip this cycle
    }
    
    updateTemperatures();
    yield(); // Feed watchdog between operations
    
    #ifdef BATTERY_MONITOR_ENABLED
      readBattery();
    #endif
    
    publishTemperature();
    publishStatus();
    lastPublishTime = now;
  }

  // Periodic status heartbeat (helps diagnose connectivity)
  if (now - lastStatusLog >= 30000) {
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    int rssi = wifiConnected ? WiFi.RSSI() : -999;
    IPAddress ip = wifiConnected ? WiFi.localIP() : IPAddress(0,0,0,0);
    bool mqttConnected = mqttClient.connected();
    int mqttState = mqttClient.state();

    Serial.printf("[Status] WiFi:%s RSSI:%d IP:%s | MQTT:%s(%s) failures:%u backoff:%lus\n",
                  wifiConnected ? "OK" : "DOWN",
                  rssi,
                  wifiConnected ? ip.toString().c_str() : "0.0.0.0",
                  mqttConnected ? "OK" : "DOWN",
                  getMqttStateString(mqttState),
                  mqttConsecutiveFailures,
                  mqttReconnectInterval / 1000);

    // Log last successful publish age if having issues
    if (!mqttConnected && metrics.lastSuccessfulMqttPublish > 0) {
      unsigned long publishAge = (now - metrics.lastSuccessfulMqttPublish) / 1000;
      Serial.printf("[Status] Last MQTT publish: %lu sec ago | Total failures: %u\n",
                    publishAge, metrics.mqttPublishFailures);
    }
    lastStatusLog = now;
  }

  // Update OLED display
  if (millis() - lastDisplayUpdate >= 1000) {
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    String ipStr = wifiConnected ? WiFi.localIP().toString() : "";
    updateDisplay(temperatureC.c_str(), temperatureF.c_str(), wifiConnected, ipStr.c_str());
    lastDisplayUpdate = millis();
  }
  
  // Handle OTA updates (called frequently for responsive OTA)
  ArduinoOTA.handle();
  
  yield(); // Feed watchdog at end of loop
}
