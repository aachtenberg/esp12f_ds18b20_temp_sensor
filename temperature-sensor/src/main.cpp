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

// Create Double Reset Detector instance (stack allocation, no memory leak)
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

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

// Deep sleep configuration
int deepSleepSeconds = 0;  // Default disabled
volatile bool otaInProgress = false;  // Tracks active OTA upload to prevent sleep during transfer
const char* DEEP_SLEEP_FILE = "/deep_sleep_seconds.txt";

// Track if we just woke from deep sleep to prevent immediate re-sleep
bool justWokeFromSleep = false;

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
unsigned long lastMqttConnectionCheck = 0;
unsigned long lastPublishTime = 0;
unsigned long lastSuccessfulMqttCheck = 0;
int lastMqttState = MQTT_DISCONNECTED;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long MQTT_CONNECTION_CHECK_INTERVAL_MS = 30000;
const unsigned long MQTT_PUBLISH_INTERVAL_MS = 30000;
const unsigned long MQTT_STALE_CONNECTION_TIMEOUT_MS = 120000;  // Force reconnect if no activity for 2 mins

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

// Load deep sleep config from filesystem
void loadDeepSleepConfig() {
#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    Serial.println("[FS] Failed to mount SPIFFS");
    deepSleepSeconds = 0;
    return;
  }
  // Load deepSleepSeconds from SPIFFS file
  File file = SPIFFS.open(DEEP_SLEEP_FILE, "r");
  if (file) {
    deepSleepSeconds = file.readStringUntil('\n').toInt();
    file.close();
    Serial.printf("[DEEP SLEEP] Loaded config: %d seconds\n", deepSleepSeconds);
  } else {
    deepSleepSeconds = 0; // Default: no deep sleep
    Serial.println("[DEEP SLEEP] No config file found, defaulting to 0 (no deep sleep)");
  }
#else // ESP8266
  if (!LittleFS.begin()) {
    Serial.println("[FS] Failed to mount LittleFS");
    deepSleepSeconds = 0;
    return;
  }
  // Load deepSleepSeconds from LittleFS file
  File file = LittleFS.open(DEEP_SLEEP_FILE, "r");
  if (file) {
    deepSleepSeconds = file.readStringUntil('\n').toInt();
    file.close();
    Serial.printf("[DEEP SLEEP] Loaded config: %d seconds\n", deepSleepSeconds);
  } else {
    deepSleepSeconds = 0; // Default: no deep sleep
    Serial.println("[DEEP SLEEP] No config file found, defaulting to 0 (no deep sleep)");
  }
#endif
}

// Save deep sleep config to filesystem
void saveDeepSleepConfig() {
#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    Serial.println("[FS] Failed to mount SPIFFS for save");
    return;
  }
  File file = SPIFFS.open(DEEP_SLEEP_FILE, "w");
#else // ESP8266
  if (!LittleFS.begin()) {
    Serial.println("[FS] Failed to mount LittleFS for save");
    return;
  }
  File file = LittleFS.open(DEEP_SLEEP_FILE, "w");
#endif
  if (file) {
    file.println(deepSleepSeconds);
    file.close();
    Serial.printf("[DEEP SLEEP] Saved config: %d seconds\n", deepSleepSeconds);
  } else {
    Serial.println("[DEEP SLEEP] Failed to save config file");
  }
}

// Forward declarations
String generateChipId();
String sanitizeDeviceName(const char* name);
void updateTopicBase();
bool ensureMqttConnected();
void publishEvent(const String& eventType, const String& message, const String& severity = "info");
bool publishTemperature();
void publishStatus();
void mqttCallback(char* topic, byte* payload, unsigned int length);

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

String getTopicCommand() {
  return topicBase + "/command";
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

// Gracefully disconnect from MQTT broker with proper cleanup
void gracefulMqttDisconnect() {
  if (!mqttClient.connected()) {
    return;  // Already disconnected
  }
  
  Serial.println("[MQTT] Initiating graceful disconnect...");
  mqttClient.disconnect();
  
  // Wait up to 500ms for graceful disconnect to complete
  // PubSubClient needs time to properly close the TCP connection
  unsigned long disconnectStart = millis();
  while (mqttClient.connected() && (millis() - disconnectStart) < 500) {
    delay(10);
  }
  
  if (mqttClient.connected()) {
    Serial.println("[MQTT] Timeout waiting for graceful disconnect");
  } else {
    Serial.println("[MQTT] Gracefully disconnected from broker");
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
      gracefulMqttDisconnect();
      delay(100);
      // Fall through to reconnection logic below
    } else {
      lastSuccessfulMqttCheck = now;
      return true;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[MQTT] WiFi not connected, cannot connect to broker");
    return false;
  }

  // Simple rate limit: don't try reconnecting too frequently
  if (lastMqttReconnectAttempt > 0 && (now - lastMqttReconnectAttempt) < MQTT_RECONNECT_INTERVAL_MS) {
    return false;
  }

  lastMqttReconnectAttempt = now;

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
    // Subscribe to command topic
    mqttClient.subscribe(getTopicCommand().c_str());
    Serial.printf("[MQTT] Subscribed to command topic: %s\n", getTopicCommand().c_str());
    lastSuccessfulMqttCheck = now;
    lastMqttState = MQTT_CONNECTED;
  } else {
    int state = mqttClient.state();
    Serial.printf("[MQTT] Connection failed: %s (state: %d), retry in %lu sec\n",
                  getMqttStateString(state), state, MQTT_RECONNECT_INTERVAL_MS / 1000);
    metrics.mqttPublishFailures++;
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

bool publishTemperature() {
  if (!isValidTemperature(temperatureC)) {
    return false;
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
  return publishJson(getTopicTemperature(), doc, false);
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
  doc["deep_sleep_enabled"] = (deepSleepSeconds > 0);
  doc["deep_sleep_seconds"] = deepSleepSeconds;
  #ifdef BATTERY_MONITOR_ENABLED
    if (metrics.batteryPercent >= 0) {
      doc["battery_voltage"] = metrics.batteryVoltage;
      doc["battery_percent"] = metrics.batteryPercent;
    }
  #endif
  publishJson(getTopicStatus(), doc, true);
}

// Enter deep sleep if configured
void enterDeepSleepIfEnabled() {
  // Check if deep sleep is disabled at compile time (e.g., for ESP8266 without GPIO16→RST wiring)
  #ifdef DISABLE_DEEP_SLEEP
    if (deepSleepSeconds > 0) {
      Serial.println("[DEEP SLEEP] Deep sleep is disabled on this device (DISABLE_DEEP_SLEEP flag set)");
    }
    return;
  #endif

  if (deepSleepSeconds > 0) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("  DEEP SLEEP ACTIVATED");
    Serial.println("========================================");
    Serial.printf("[DEEP SLEEP] Entering deep sleep for %d seconds...\n", deepSleepSeconds);
    #ifdef ESP8266
      Serial.println();
      Serial.println("*** CRITICAL HARDWARE REQUIREMENT ***");
      Serial.println("GPIO 16 (D0) MUST be connected to RST pin for wake-up!");
      Serial.println("Without this connection, device will sleep FOREVER!");
      Serial.println("Circuit: RST ──► 10KΩ ──► GPIO 16, with 0.1µF cap GPIO16─►GND");
      Serial.println("*** END HARDWARE REQUIREMENT ***");
      Serial.println();
    #endif
    #ifdef ESP32
      Serial.println("[DEEP SLEEP] ESP32 RTC timer configured - no hardware mods needed");

      // Gracefully disconnect MQTT and WiFi before deep sleep
      Serial.println("[DEEP SLEEP] Disconnecting MQTT and WiFi...");
      gracefulMqttDisconnect();
      WiFi.disconnect(true);  // true = turn off WiFi radio
      delay(100);  // Give time for WiFi to power down
    #endif

    // Flush serial before sleeping
    Serial.flush();
    delay(50);

#ifdef ESP8266
    // ESP8266 deep sleep with timer requires GPIO 16 (D0) connected to RST pin
    // For proper wake-up: RST -> 10K resistor -> GPIO 16, with 0.1µF capacitor RST->GND
    ESP.deepSleep(deepSleepSeconds * 1000000ULL); // microseconds
#else // ESP32
    // ESP32 has built-in RTC - no hardware modifications needed
    uint64_t sleepTime = deepSleepSeconds * 1000000ULL;
    Serial.printf("[DEEP SLEEP] Configuring RTC timer for %llu microseconds\n", sleepTime);
    esp_sleep_enable_timer_wakeup(sleepTime);
    Serial.println("[DEEP SLEEP] Starting deep sleep NOW...");
    Serial.flush();
    esp_deep_sleep_start();
#endif
  }
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

void handleDeepSleepGet() {
  JsonDocument doc;
  doc["deep_sleep_seconds"] = deepSleepSeconds;
  doc["device"] = deviceName;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleDeepSleepPost() {
  if (server.hasArg("seconds")) {
    int newSeconds = server.arg("seconds").toInt();
    if (newSeconds >= 0 && newSeconds <= 3600) { // Max 1 hour
      deepSleepSeconds = newSeconds;
      saveDeepSleepConfig();

      char msg[64];
      snprintf(msg, sizeof(msg), "Deep sleep set to %d seconds", deepSleepSeconds);
      publishEvent("deep_sleep_config", msg, "info");

      char response[64];
      snprintf(response, sizeof(response), "{\"status\":\"ok\",\"deep_sleep_seconds\":%d}", deepSleepSeconds);
      server.send(200, "application/json", response);
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid seconds value (0-3600)\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Missing 'seconds' parameter\"}");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Use char arrays instead of String to avoid heap fragmentation
  char payloadStr[64];
  unsigned int copyLen = min(length, sizeof(payloadStr) - 1);
  memcpy(payloadStr, payload, copyLen);
  payloadStr[copyLen] = '\0';

  Serial.printf("[MQTT] Received command: %s = %s\n", topic, payloadStr);

  // Handle deep sleep configuration commands (exact topic match to avoid false positives)
  if (strcmp(topic, getTopicCommand().c_str()) == 0) {
    if (strncmp(payloadStr, "deepsleep ", 10) == 0) {
      int newSeconds = atoi(payloadStr + 10); // Parse number after "deepsleep "
      if (newSeconds >= 0 && newSeconds <= 3600) { // Max 1 hour
        // Warn if OTA is active
        if (otaInProgress) {
          Serial.println("[WARNING] OTA upload in progress - ignoring deep sleep change");
          publishEvent("ota_warning", "Ignored deep sleep change during active OTA upload", "warning");
          return;
        }
        deepSleepSeconds = newSeconds;
        saveDeepSleepConfig();

        char msg[64];
        snprintf(msg, sizeof(msg), "Deep sleep set to %d seconds via MQTT", deepSleepSeconds);
        publishEvent("deep_sleep_config", msg, "info");
        publishStatus(); // Publish updated status
      } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Invalid deep sleep seconds: %d", newSeconds);
        publishEvent("command_error", msg, "error");
      }
    } else if (strcmp(payloadStr, "status") == 0) {
      publishStatus();
    } else if (strcmp(payloadStr, "restart") == 0) {
      publishEvent("device_restart", "Restarting device via MQTT command", "warning");
      delay(500);
      ESP.restart();
    }
  }
}

void setupWebServer() {
#ifndef API_ENDPOINTS_ONLY
  server.on("/", HTTP_GET, handleRoot);
#endif
  server.on("/temperaturec", HTTP_GET, handleTemperatureC);
  server.on("/temperaturef", HTTP_GET, handleTemperatureF);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/deepsleep", HTTP_GET, handleDeepSleepGet);
  server.on("/deepsleep", HTTP_POST, handleDeepSleepPost);
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");
}

void setupWiFi() {
  // Double Reset Detector already initialized globally (no memory leak)

  // Disable WiFi power save on all devices for reliable OTA/MQTT
  // Deep sleep (when enabled) handles power saving for battery devices
  #ifdef ESP32
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
  #else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);  // Disable power save on ESP8266
  #endif
  Serial.println("[POWER] WiFi power save disabled (full radio power for OTA/MQTT reliability)");

  WiFi.mode(WIFI_STA);
  // Enable auto-reconnect and persist credentials (low-power friendly)
  #ifdef ESP8266
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  #else
    // ESP32 also supports auto reconnect in Arduino core
    WiFi.setAutoReconnect(true);
  #endif

  // Check for double reset - always enter config portal if detected
  if (drd.detectDoubleReset()) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("  DOUBLE RESET DETECTED");
    Serial.println("  Starting WiFi Configuration Portal");
    Serial.println("========================================");
    Serial.println();

    WiFiManager wm;
    String apName = String(deviceName);
    apName.replace(" ", "-");
    apName = "Temp-" + apName + "-Setup";

    Serial.println("[WiFi] Connect to AP: " + apName);
    Serial.println("[WiFi] Then open http://192.168.4.1 in browser");
    Serial.println();

    WiFiManagerParameter custom_device_name("device_name", "Device Name", deviceName, 40);
    wm.addParameter(&custom_device_name);
    wm.setConnectTimeout(0);

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
      if (shouldSaveConfig) {
        const char* newName = custom_device_name.getValue();
        Serial.print("[Config] New device name: ");
        Serial.println(newName);
        String oldName = String(deviceName);
        strcpy(deviceName, newName);
        updateTopicBase();
        saveDeviceName(deviceName);

        if (oldName != String(newName)) {
          publishEvent("device_configured", "Name: '" + oldName + "' -> '" + String(newName) + "', SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString(), "info");
        } else {
          publishEvent("device_configured", "WiFi reconfigured - SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString() + ", Name unchanged: " + oldName, "info");
        }
      }
    }
    return;  // Exit early after portal configuration
  }

  // Normal boot: attempt WiFi connection with retry logic
  Serial.println("[WiFi] Normal boot - attempting connection...");

  // Check if we have saved credentials
  if (WiFi.SSID().length() == 0) {
    Serial.println("[WiFi] No saved credentials found");

    // For deep sleep devices, skip portal and go to sleep - will retry on next wake
    if (deepSleepSeconds > 0) {
      Serial.println("[WiFi] Deep sleep enabled - will retry on next wake cycle");
      Serial.println("[WiFi] Tip: Double-tap reset button to configure WiFi");
      return;
    }

    // For non-deep-sleep devices, start config portal
    Serial.println("[WiFi] Starting configuration portal...");
    WiFiManager wm;
    String apName = String(deviceName);
    apName.replace(" ", "-");
    apName = "Temp-" + apName + "-Setup";

    WiFiManagerParameter custom_device_name("device_name", "Device Name", deviceName, 40);
    wm.addParameter(&custom_device_name);
    wm.setConnectTimeout(0);

    bool shouldSaveConfig = false;
    wm.setSaveConfigCallback([&shouldSaveConfig](){
      shouldSaveConfig = true;
    });

    if (wm.autoConnect(apName.c_str()) && shouldSaveConfig) {
      const char* newName = custom_device_name.getValue();
      strcpy(deviceName, newName);
      updateTopicBase();
      saveDeviceName(deviceName);
    }
    return;
  }

  // We have saved credentials - attempt connection with retries
  Serial.printf("[WiFi] Connecting to saved network: %s\n", WiFi.SSID().c_str());

  // For deep sleep devices: retry without starting portal on failure
  if (deepSleepSeconds > 0) {
    const int MAX_RETRIES = 3;
    const unsigned long RETRY_TIMEOUT_MS = 10000;  // 10 seconds per attempt

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
      Serial.printf("[WiFi] Connection attempt %d/%d...\n", attempt, MAX_RETRIES);

      WiFi.begin();  // Use saved credentials

      unsigned long startAttempt = millis();
      while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < RETRY_TIMEOUT_MS) {
        delay(100);
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        publishEvent("wifi_connected", "Connected to " + WiFi.SSID() + " with IP " + WiFi.localIP().toString(), "info");
        return;
      }

      Serial.printf("[WiFi] Attempt %d failed (status: %d)\n", attempt, WiFi.status());

      if (attempt < MAX_RETRIES) {
        Serial.println("[WiFi] Retrying...");
        WiFi.disconnect();
        delay(2000);  // Wait before retry
      }
    }

    // All retries failed - don't start portal, will retry on next wake
    Serial.println("[WiFi] All connection attempts failed");
    Serial.println("[WiFi] Battery-powered device - skipping portal to conserve power");
    Serial.println("[WiFi] Will retry on next wake cycle");
    Serial.println("[WiFi] Tip: Double-tap reset button if you need to reconfigure WiFi");
    return;
  }

  // For non-deep-sleep devices: use WiFiManager with portal fallback
  WiFiManager wm;
  String apName = String(deviceName);
  apName.replace(" ", "-");
  apName = "Temp-" + apName + "-Setup";

  WiFiManagerParameter custom_device_name("device_name", "Device Name", deviceName, 40);
  wm.addParameter(&custom_device_name);
  wm.setConnectTimeout(0);

  bool shouldSaveConfig = false;
  wm.setSaveConfigCallback([&shouldSaveConfig](){
    shouldSaveConfig = true;
  });

  if (!wm.autoConnect(apName.c_str())) {
    Serial.println("[WiFi] Failed to connect - running in offline mode");
  } else {
    Serial.printf("[WiFi] Connected to %s, IP: %s, RSSI: %d dBm\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    publishEvent("wifi_connected", "Connected to " + WiFi.SSID() + " with IP " + WiFi.localIP().toString(), "info");

    if (shouldSaveConfig) {
      const char* newName = custom_device_name.getValue();
      String oldName = String(deviceName);
      strcpy(deviceName, newName);
      updateTopicBase();
      saveDeviceName(deviceName);

      if (oldName != String(newName)) {
        publishEvent("device_configured", "Name: '" + oldName + "' -> '" + String(newName) + "', SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString(), "info");
      } else {
        publishEvent("device_configured", "WiFi reconfigured - SSID: " + WiFi.SSID() + ", IP: " + WiFi.localIP().toString() + ", Name unchanged: " + oldName, "info");
      }
    }
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
  #else
    Serial.printf("[DEBUG] Reset reason code: 0x%02x\n", esp_reset_reason());
    Serial.printf("[DEBUG] Free heap: %u bytes\n", ESP.getFreeHeap());
  #endif

  Serial.println();
  Serial.println("========================================");
  Serial.println("     Temperature Sensor");
  Serial.println("========================================");
  Serial.println();

  // Detect wake from deep sleep
  #ifdef ESP32
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    switch (wakeupCause) {
      case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println();
        Serial.println("  *** WOKE FROM DEEP SLEEP (TIMER) ***");
        Serial.println();
        justWokeFromSleep = true;
        break;
      case ESP_SLEEP_WAKEUP_GPIO:
        Serial.println();
        Serial.println("  *** WOKE FROM DEEP SLEEP (GPIO) ***");
        Serial.println();
        justWokeFromSleep = true;
        break;
      case ESP_SLEEP_WAKEUP_UART:
        Serial.println();
        Serial.println("  *** WOKE FROM DEEP SLEEP (UART) ***");
        Serial.println();
        justWokeFromSleep = true;
        break;
      case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println();
        Serial.println("  *** WOKE FROM DEEP SLEEP (TOUCHPAD) ***");
        Serial.println();
        justWokeFromSleep = true;
        break;
      case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println();
        Serial.println("  *** WOKE FROM DEEP SLEEP (EXT0) ***");
        Serial.println();
        justWokeFromSleep = true;
        break;
      case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println();
        Serial.println("  *** WOKE FROM DEEP SLEEP (EXT1) ***");
        Serial.println();
        justWokeFromSleep = true;
        break;
      case ESP_SLEEP_WAKEUP_COCPU:
        Serial.println();
        Serial.println("  *** WOKE FROM DEEP SLEEP (COCPU) ***");
        Serial.println();
        justWokeFromSleep = true;
        break;
      default:
        // Normal boot, not a wake-up
        justWokeFromSleep = false;
        break;
    }
  #endif

  // Load device name from filesystem
  loadDeviceName();

  // Load deep sleep configuration
  loadDeepSleepConfig();

  chipId = generateChipId();
  updateTopicBase();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
#ifdef ESP32
  mqttClient.setBufferSize(2048);  // Larger buffer for ESP32 (recommended when battery monitoring enabled)
#else
  mqttClient.setBufferSize(512);   // Standard buffer for ESP8266 (512 bytes)
#endif
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(5);  // Reduced from 15s to minimize blocking during connection issues
  mqttClient.setCallback(mqttCallback);

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

  // Setup OTA updates (after WiFi is connected) - always available regardless of deep sleep config
  // OTA will only listen when deepSleepSeconds == 0 (see loop())
  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
  }

  // If deep sleep is enabled, publish immediately and wait for MQTT commands
  if (deepSleepSeconds > 0) {
    Serial.println("[DEEP SLEEP] Deep sleep mode enabled - publishing and waiting for commands");

    // Ensure MQTT is connected before publishing
    if (!ensureMqttConnected()) {
      Serial.println("[DEEP SLEEP] MQTT connection failed - staying awake to retry");
      lastPublishTime = millis();
      return;
    }

    // Read temperature before publishing
    updateTemperatures();

    // Publish immediately
    bool publishSuccess = publishTemperature();
    publishStatus();

    Serial.println();
    Serial.println("========================================");
    Serial.println("     Setup Complete (Deep Sleep Mode)");
    Serial.println("========================================");
    Serial.println();

    // Wait briefly to process any incoming MQTT commands (e.g., to disable deep sleep or OTA)
    Serial.println("[DEEP SLEEP] Waiting 5 seconds for MQTT commands...");
    unsigned long commandWaitStart = millis();
    while (millis() - commandWaitStart < 5000) {
      // Check if MQTT connection is still active before processing
      if (!mqttClient.connected()) {
        Serial.println("[DEEP SLEEP] MQTT disconnected during command wait window");
        break;
      }
      mqttClient.loop();  // Process incoming MQTT messages
      if (deepSleepSeconds == 0) {
        ArduinoOTA.handle();
      }
      delay(10);
    }

    // Enter deep sleep immediately if publish succeeded
    if (publishSuccess) {
      enterDeepSleepIfEnabled();
    } else {
      Serial.println("[DEEP SLEEP] Initial publish failed - staying awake to retry");
    }
    // If we reach here, publish failed and we'll retry in loop()
    lastPublishTime = millis();
    return;
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
  drd.loop();

  // Handle web requests first for responsiveness
  server.handleClient();

  // Process MQTT messages - detect connection loss early
  bool mqttLoopResult = mqttClient.loop();
  if (!mqttLoopResult) {
    // loop() returns false if disconnected - log state change immediately
    int currentState = mqttClient.state();
    if (currentState != lastMqttState && lastMqttState == MQTT_CONNECTED) {
      Serial.printf("[MQTT] Connection lost! State: %s (%d)\n",
                    getMqttStateString(currentState), currentState);
      lastMqttState = currentState;
    }
  }

  unsigned long now = millis();

  // MQTT connection management
  if ((now - lastMqttConnectionCheck) > MQTT_CONNECTION_CHECK_INTERVAL_MS) {
    lastMqttConnectionCheck = now;
    ensureMqttConnected();
  }

  // Reconnect if disconnected
  if (!mqttClient.connected() && (now - lastMqttReconnectAttempt) >= MQTT_RECONNECT_INTERVAL_MS) {
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
        Serial.printf("[WARNING] Low heap: %lu bytes\n", freeHeap);
        // Force MQTT reconnection on critically low memory
        if (freeHeap < 6000) {
          Serial.println("[WARNING] Critical heap - reconnecting MQTT");
          gracefulMqttDisconnect();
          lastMqttReconnectAttempt = 0;  // Force immediate reconnect
        }
      }
    #endif
    
    // Check signal strength - defer MQTT if too weak
    int rssi = WiFi.RSSI();
    if (rssi < WIFI_MIN_RSSI) {
      Serial.printf("[WARNING] Signal too weak (%d dBm), deferring MQTT publish\n", rssi);
      lastPublishTime = now;  // Reset timer to retry later
      // Still call OTA.handle() if deep sleep disabled
      if (deepSleepSeconds == 0) {
        ArduinoOTA.handle();
      }
      return;  // Skip this cycle
    }
    
    updateTemperatures();
    yield(); // Feed watchdog between operations

    #ifdef BATTERY_MONITOR_ENABLED
      readBattery();
    #endif

    bool publishSuccess = publishTemperature();
    publishStatus();
    lastPublishTime = now;

    // Only enter deep sleep if temperature was published successfully
    if (publishSuccess) {
      enterDeepSleepIfEnabled();
    } else {
      Serial.println("[DEEP SLEEP] Skipping deep sleep - publish failed, will retry");
    }
  }

  // Periodic status heartbeat (helps diagnose connectivity)
  if (now - lastStatusLog >= 30000) {
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    int rssi = wifiConnected ? WiFi.RSSI() : -999;
    IPAddress ip = wifiConnected ? WiFi.localIP() : IPAddress(0,0,0,0);
    bool mqttConnected = mqttClient.connected();
    int mqttState = mqttClient.state();

    Serial.printf("[Status] WiFi:%s RSSI:%d IP:%s | MQTT:%s(%s) failures:%u\n",
                  wifiConnected ? "OK" : "DOWN",
                  rssi,
                  wifiConnected ? ip.toString().c_str() : "0.0.0.0",
                  mqttConnected ? "OK" : "DOWN",
                  getMqttStateString(mqttState),
                  metrics.mqttPublishFailures);

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
  
  // Handle OTA updates (only when deep sleep is disabled)
  // When deep sleep is enabled, device sleeps and OTA is unavailable
  if (deepSleepSeconds == 0) {
    ArduinoOTA.handle();
  }
  
  yield(); // Feed watchdog at end of loop
}
