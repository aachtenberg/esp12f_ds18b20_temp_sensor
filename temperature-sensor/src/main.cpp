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
#include "display.h"

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

    DeviceMetrics() : bootTime(0), wifiReconnects(0), sensorReadFailures(0),
                      mqttPublishFailures(0),
                      minTempC(999.0f), maxTempC(-999.0f),
                      lastSuccessfulMqttPublish(0) {}

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
unsigned long lastPublishTime = 0;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long MQTT_PUBLISH_INTERVAL_MS = 30000;

// Timer variables
const unsigned long publishIntervalMs = MQTT_PUBLISH_INTERVAL_MS;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = WIFI_CHECK_INTERVAL_MS;
unsigned long lastDisplayUpdate = 0;

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

// Update both temperature globals in one call
void updateTemperatures() {
  sensors.requestTemperatures();
  float tC = sensors.getTempCByIndex(0);
  if (tC == DEVICE_DISCONNECTED_C) {
    temperatureC = "--";
    temperatureF = "--";
    Serial.println("DS18B20 read failed");
    metrics.sensorReadFailures++;
    publishEvent("sensor_error", "DS18B20 read failed", "error");
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

String generateChipId() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toUpperCase();
  return mac;
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

bool ensureMqttConnected() {
  if (mqttClient.connected()) {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  String clientId = String(deviceName) + "-" + chipId;
  bool connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
  if (!connected) {
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
  if (ok) {
    metrics.lastSuccessfulMqttPublish = millis();
  } else {
    metrics.mqttPublishFailures++;
  }
  return ok;
}

void publishEvent(const String& eventType, const String& message, const String& severity) {
  StaticJsonDocument<256> doc;
  doc["device"] = deviceName;
  doc["chip_id"] = chipId;
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
  doc["timestamp"] = millis() / 1000;
  doc["celsius"] = temperatureC.toFloat();
  doc["fahrenheit"] = temperatureF.toFloat();
  publishJson(getTopicTemperature(), doc, false);
}

void publishStatus() {
  StaticJsonDocument<256> doc;
  doc["device"] = deviceName;
  doc["chip_id"] = chipId;
  doc["timestamp"] = millis() / 1000;
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifi_rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -999;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["sensor_healthy"] = isValidTemperature(temperatureC);
  doc["wifi_reconnects"] = metrics.wifiReconnects;
  doc["sensor_read_failures"] = metrics.sensorReadFailures;
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
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["temperature_valid"] = isValidTemperature(temperatureC);
  doc["current_temp_c"] = temperatureC;
  doc["current_temp_f"] = temperatureF;

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
  String apName = String(deviceName);
  apName.replace(" ", "-");
  apName = "Temp-" + apName + "-Setup";

  // Create custom parameter for device name (must persist throughout function)
  WiFiManagerParameter custom_device_name("device_name", "Device Name", deviceName, 40);
  wm.addParameter(&custom_device_name);

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
    Serial.println("[WiFi] (Double-reset within 3 seconds to enter config mode)");
    Serial.println();

    // Set save config callback for autoConnect mode
    bool shouldSaveConfig = false;
    wm.setSaveConfigCallback([&shouldSaveConfig](){
      Serial.println("[Config] Configuration saved, will update device name...");
      shouldSaveConfig = true;
    });

    if (!wm.autoConnect(apName.c_str())) {
      Serial.println("[WiFi] Failed to connect");
      Serial.println("[WiFi] Running in offline mode - double-reset to configure");
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
    publishEvent("wifi_connected", "Connected to " + WiFi.SSID() + " with IP " + WiFi.localIP().toString(), "info");
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
  mqttClient.setBufferSize(1024);

  // Start up the DS18B20 library
  sensors.begin();
  updateTemperatures();

  // Initialize OLED display
  initDisplay();

  // Connect to WiFi
  setupWiFi();

  // Setup web server
  setupWebServer();

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

  // Handle web requests
  server.handleClient();

  mqttClient.loop();

  // Periodic WiFi check
  unsigned long now = millis();

  if (!mqttClient.connected() && (now - lastMqttReconnectAttempt) > MQTT_RECONNECT_INTERVAL_MS) {
    lastMqttReconnectAttempt = now;
    ensureMqttConnected();
  }

  if (now - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = now;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting reconnection...");
      WiFi.reconnect();
      metrics.wifiReconnects++;
      // Only log every 5 reconnects to avoid spam
      if (metrics.wifiReconnects % 5 == 1) {
        publishEvent("wifi_reconnect", "WiFi disconnected, reconnect attempt #" + String(metrics.wifiReconnects), "warning");
      }
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
    
    updateTemperatures();
    yield(); // Feed watchdog between operations
    
    publishTemperature();
    publishStatus();
    lastPublishTime = now;
  }

  // Update OLED display
  if (millis() - lastDisplayUpdate >= 1000) {
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    String ipStr = wifiConnected ? WiFi.localIP().toString() : "";
    updateDisplay(temperatureC.c_str(), temperatureF.c_str(), wifiConnected, ipStr.c_str());
    lastDisplayUpdate = millis();
  }
  
  yield(); // Feed watchdog at end of loop
}
