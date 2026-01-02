/*********
  BME280 Environmental Sensor with WiFiManager
  Measures temperature, humidity, and atmospheric pressure
  
  Reports metrics to MQTT broker for monitoring
  Supports WiFi configuration portal and OTA updates
*********/

#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
#endif

#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <ArduinoOTA.h>
#ifdef ESP32
  #include <SPIFFS.h>
#else
  #include <LittleFS.h>
#endif

#include "secrets.h"
#include "device_config.h"
#include "version.h"

// =============================================================================
// DEVICE CONFIGURATION
// =============================================================================

// Device name storage
char deviceName[40] = "BME280 Sensor";
const char* DEVICE_NAME_FILE = "/device_name.txt";

// Sensor readings
float temperature_c = 0.0;
float humidity_rh = 0.0;
float pressure_pa = 0.0;
float altitude_m = 0.0;

// Device metrics
struct DeviceMetrics {
    unsigned long bootTime;
    unsigned int wifiReconnects;
    unsigned int sensorReadFailures;
    unsigned int mqttPublishFailures;
    float batteryVoltage;
    int batteryPercent;
    unsigned long lastSuccessfulMqttPublish;
    
    DeviceMetrics() : bootTime(0), wifiReconnects(0), sensorReadFailures(0),
                      mqttPublishFailures(0), batteryVoltage(0.0f), batteryPercent(-1),
                      lastSuccessfulMqttPublish(0) {}
};

DeviceMetrics metrics;

// Deep sleep configuration
int deepSleepSeconds = 0;
volatile bool otaInProgress = false;
const char* DEEP_SLEEP_FILE = "/deep_sleep_seconds.txt";

// Global instances
Adafruit_BME280 bme280;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
String chipId;
String topicBase;

#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif

// MQTT timers
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastPublishTime = 0;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long MQTT_PUBLISH_INTERVAL_MS = 30000;

// WiFi timers
unsigned long wifiDisconnectedSince = 0;
const unsigned long WIFI_STALE_CONNECTION_TIMEOUT_MS = 90000;
const int WIFI_MIN_RSSI = -85;

// =============================================================================
// DEVICE NAME MANAGEMENT
// =============================================================================

void loadDeviceName() {
#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    Serial.println("[FS] Failed to mount SPIFFS");
    return;
  }
  File file = SPIFFS.open(DEVICE_NAME_FILE, "r");
#else
  if (!LittleFS.begin()) {
    Serial.println("[FS] Failed to mount LittleFS");
    return;
  }
  File file = LittleFS.open(DEVICE_NAME_FILE, "r");
#endif
  
  if (file) {
    size_t size = file.size();
    if (size > 0 && size < sizeof(deviceName)) {
      file.readBytes(deviceName, size);
      deviceName[size] = '\0';
      Serial.printf("[CONFIG] Loaded device name: %s\n", deviceName);
    }
    file.close();
  } else {
    Serial.println("[CONFIG] No device name file found, using default");
  }
}

void saveDeviceName(const char* name) {
#ifdef ESP32
  if (!SPIFFS.begin(true)) return;
  File file = SPIFFS.open(DEVICE_NAME_FILE, "w");
#else
  if (!LittleFS.begin()) return;
  File file = LittleFS.open(DEVICE_NAME_FILE, "w");
#endif
  
  if (file) {
    strncpy(deviceName, name, sizeof(deviceName) - 1);
    deviceName[sizeof(deviceName) - 1] = '\0';
    file.write((uint8_t*)deviceName, strlen(deviceName));
    file.close();
    Serial.printf("[CONFIG] Saved device name: %s\n", deviceName);
  }
}

// =============================================================================
// DEEP SLEEP MANAGEMENT
// =============================================================================

void loadDeepSleepConfig() {
#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    deepSleepSeconds = 0;
    return;
  }
  File file = SPIFFS.open(DEEP_SLEEP_FILE, "r");
#else
  if (!LittleFS.begin()) {
    deepSleepSeconds = 0;
    return;
  }
  File file = LittleFS.open(DEEP_SLEEP_FILE, "r");
#endif
  
  if (file) {
    deepSleepSeconds = file.readStringUntil('\n').toInt();
    file.close();
    Serial.printf("[DEEP SLEEP] Loaded config: %d seconds\n", deepSleepSeconds);
  } else {
    deepSleepSeconds = 0;
    Serial.println("[DEEP SLEEP] No config file, defaulting to 0 (disabled)");
  }
}

void saveDeepSleepConfig() {
#ifdef ESP32
  if (!SPIFFS.begin(true)) return;
  File file = SPIFFS.open(DEEP_SLEEP_FILE, "w");
#else
  if (!LittleFS.begin()) return;
  File file = LittleFS.open(DEEP_SLEEP_FILE, "w");
#endif
  
  if (file) {
    file.println(deepSleepSeconds);
    file.close();
    Serial.printf("[DEEP SLEEP] Saved config: %d seconds\n", deepSleepSeconds);
  }
}

// =============================================================================
// SENSOR OPERATIONS
// =============================================================================

bool initializeSensor() {
  if (!bme280.begin(BME280_I2C_ADDR)) {
    Serial.println("[SENSOR] BME280 initialization failed!");
    return false;
  }
  
  // Configure sensor for weather monitoring
  bme280.setSampling(
    Adafruit_BME280::MODE_NORMAL,        // Operating Mode
    Adafruit_BME280::SAMPLING_X2,        // Temp. oversampling
    Adafruit_BME280::SAMPLING_X16,       // Pressure oversampling
    Adafruit_BME280::SAMPLING_X2,        // Humidity oversampling
    Adafruit_BME280::FILTER_OFF,         // Filtering
    Adafruit_BME280::STANDBY_MS_0_5      // Standby time
  );
  
  Serial.println("[SENSOR] BME280 initialized successfully");
  return true;
}

void readSensorData() {
  sensors_event_t temp_event, pressure_event, humidity_event;
  
  // Create Adafruit Unified Sensor objects for the BME280 environmental sensor
  Adafruit_Sensor *bme_temp = bme280.getTemperatureSensor();
  Adafruit_Sensor *bme_pressure = bme280.getPressureSensor();
  Adafruit_Sensor *bme_humidity = bme280.getHumiditySensor();
  
  bme_temp->getEvent(&temp_event);
  bme_pressure->getEvent(&pressure_event);
  bme_humidity->getEvent(&humidity_event);
  
  temperature_c = temp_event.temperature + TEMP_OFFSET;
  pressure_pa = pressure_event.pressure * 100.0;  // Convert hPa to Pa
  humidity_rh = humidity_event.relative_humidity + HUMIDITY_OFFSET;
  
  // Calculate altitude from pressure
  altitude_m = 44330.0 * (1.0 - pow(pressure_pa / (PRESSURE_SEA_LEVEL * 100.0), 1.0/5.255));
  
  Serial.printf("[SENSOR] Temp: %.2f°C, Humidity: %.1f%%, Pressure: %.2f hPa, Altitude: %.1f m\n",
                temperature_c, humidity_rh, pressure_pa / 100.0, altitude_m);
}

// =============================================================================
// MQTT OPERATIONS
// =============================================================================

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

String getTopicReadings() {
  return topicBase + "/readings";
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

bool publishJson(const String& topic, JsonDocument& doc, bool retain = false) {
  if (!mqttClient.connected()) {
    return false;
  }
  
  String payload;
  serializeJson(doc, payload);
  
  if (!mqttClient.publish(topic.c_str(), payload.c_str(), retain)) {
    metrics.mqttPublishFailures++;
    return false;
  }
  
  metrics.lastSuccessfulMqttPublish = millis();
  return true;
}

void publishEvent(const String& eventType, const String& message, const String& severity = "info") {
  StaticJsonDocument<256> doc;
  doc["device"] = deviceName;
  doc["chip_id"] = chipId;
  doc["firmware_version"] = getFirmwareVersion();
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

bool publishReadings() {
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Not connected - skipping readings publish");
    return false;
  }
  
  StaticJsonDocument<512> doc;
  doc["device"] = deviceName;
  doc["chip_id"] = chipId;
  doc["firmware_version"] = getFirmwareVersion();
  doc["schema_version"] = 1;
  doc["timestamp"] = millis() / 1000;
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["temperature_c"] = temperature_c;
  doc["humidity_rh"] = humidity_rh;
  doc["pressure_pa"] = pressure_pa;
  doc["pressure_hpa"] = pressure_pa / 100.0;
  doc["altitude_m"] = altitude_m;
  
#ifdef BATTERY_MONITOR_ENABLED
  if (metrics.batteryPercent >= 0) {
    doc["battery_voltage"] = metrics.batteryVoltage;
    doc["battery_percent"] = metrics.batteryPercent;
  }
#endif
  
  bool success = publishJson(getTopicReadings(), doc, false);
  if (success) {
    Serial.printf("[MQTT] ✓ Published readings to %s\n", getTopicReadings().c_str());
  } else {
    Serial.printf("[MQTT] ✗ Failed to publish readings (state=%d)\n", mqttClient.state());
    metrics.mqttPublishFailures++;
  }
  return success;
}

void publishStatus() {
  if (!mqttClient.connected()) {
    return;  // Silent - published by periodic health check
  }
  
  StaticJsonDocument<512> doc;
  doc["device"] = deviceName;
  doc["chip_id"] = chipId;
  doc["firmware_version"] = getFirmwareVersion();
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["rssi"] = WiFi.RSSI();
  doc["mqtt_connected"] = mqttClient.connected();
  doc["mqtt_publish_failures"] = metrics.mqttPublishFailures;
  doc["sensor_read_failures"] = metrics.sensorReadFailures;
  doc["wifi_reconnects"] = metrics.wifiReconnects;
  doc["deep_sleep_seconds"] = deepSleepSeconds;
  
  publishJson(getTopicStatus(), doc, false);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  String topicStr = String(topic);
  Serial.printf("[MQTT] Received on %s: %s\n", topic, message.c_str());
  
  if (topicStr.endsWith("/command")) {
    // Handle device commands here
    if (message == "restart") {
      publishEvent("device_restart", "Restarting device via MQTT command", "warning");
      delay(500);
      ESP.restart();
    }
  }
}

bool ensureMqttConnected() {
  if (mqttClient.connected()) {
    return true;
  }
  
  unsigned long now = millis();
  if (now - lastMqttReconnectAttempt < MQTT_RECONNECT_INTERVAL_MS) {
    return false;
  }
  
  lastMqttReconnectAttempt = now;
  Serial.printf("[MQTT] Attempting connection to %s:%d\n", MQTT_BROKER, MQTT_PORT);
  
  if (mqttClient.connect(chipId.c_str(), MQTT_USER, MQTT_PASS)) {
    Serial.println("[MQTT] Connected!");
    mqttClient.subscribe(getTopicCommand().c_str());
    publishEvent("mqtt_connected", "Connected to MQTT broker", "info");
    return true;
  } else {
    Serial.printf("[MQTT] Connection failed, rc=%d\n", mqttClient.state());
    return false;
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(2048);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(5);
  mqttClient.setCallback(mqttCallback);
}

// =============================================================================
// WiFi AND OTA
// =============================================================================

void setupWiFi() {
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(300);
  
  if (!wifiManager.autoConnect(deviceName)) {
    Serial.println("[WiFi] Configuration failed, restarting...");
    delay(3000);
    ESP.restart();
  }
  
  Serial.printf("[WiFi] Connected to %s\n", WiFi.SSID().c_str());
  Serial.printf("[WiFi] IP address: %s\n", WiFi.localIP().toString().c_str());
  metrics.bootTime = millis();
}

void setupOTA() {
  ArduinoOTA.setHostname(deviceName);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    publishEvent("ota_start", "OTA update starting", "warning");
    Serial.println("[OTA] Update started");
  });
  
  ArduinoOTA.onEnd([]() {
    otaInProgress = false;
    Serial.println("[OTA] Update complete");
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    otaInProgress = false;
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
}

// =============================================================================
// WEB SERVER
// =============================================================================

void handleRoot(AsyncWebServerRequest* request) {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>BME280 Sensor</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; }
        .data { background: #f0f0f0; padding: 15px; border-radius: 5px; margin: 10px 0; }
        .value { font-size: 24px; font-weight: bold; color: #0066cc; }
    </style>
</head>
<body>
    <h1>BME280 Environmental Sensor</h1>
    <div class="data">
        <p>Temperature: <span class="value" id="temp">--</span> °C</p>
        <p>Humidity: <span class="value" id="humidity">--</span> %</p>
        <p>Pressure: <span class="value" id="pressure">--</span> hPa</p>
        <p>Altitude: <span class="value" id="altitude">--</span> m</p>
    </div>
    <script>
        setInterval(() => {
            fetch('/api/readings').then(r => r.json()).then(d => {
                document.getElementById('temp').textContent = d.temperature_c.toFixed(1);
                document.getElementById('humidity').textContent = d.humidity_rh.toFixed(1);
                document.getElementById('pressure').textContent = (d.pressure_pa / 100).toFixed(1);
                document.getElementById('altitude').textContent = d.altitude_m.toFixed(1);
            });
        }, 1000);
    </script>
</body>
</html>
  )";
  request->send(200, "text/html", html);
}

void setupWebServer() {
  // Web interface
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleRoot(request);
  });
  
  // JSON API
  server.on("/api/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    doc["temperature_c"] = temperature_c;
    doc["humidity_rh"] = humidity_rh;
    doc["pressure_pa"] = pressure_pa;
    doc["altitude_m"] = altitude_m;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  server.begin();
}

// =============================================================================
// MAIN SETUP AND LOOP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n================================");
  Serial.println("  BME280 Environmental Sensor");
  Serial.println("================================\n");
  
  // Load configuration
  loadDeviceName();
  loadDeepSleepConfig();
  
  // Initialize sensor
  if (!initializeSensor()) {
    Serial.println("[FATAL] BME280 sensor failed to initialize!");
    while (1) {
      delay(1000);
      Serial.println("[FATAL] Halted - check BME280 I2C connection");
    }
  }
  
  // Setup identifiers
  chipId = generateChipId();
  updateTopicBase();
  
  // Setup MQTT
  setupMQTT();
  
  // Connect to WiFi
  setupWiFi();
  
  // Setup OTA
  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
  }
  
  // Setup web server
  // setupWebServer();
  
  Serial.println("\n[SETUP] Device ready!\n");
}

void loop() {
  // Handle OTA
  if (deepSleepSeconds == 0) {
    ArduinoOTA.handle();
  }
  
  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    ensureMqttConnected();
  } else {
    mqttClient.loop();
  }
  
  // Periodic sensor reading and publish
  static unsigned long lastReadTime = 0;
  static unsigned long lastStatusLog = 0;
  
  unsigned long now = millis();
  
  if (now - lastReadTime > MQTT_PUBLISH_INTERVAL_MS) {
    lastReadTime = now;
    
    // Read sensor
    readSensorData();
    
    // Publish data only if MQTT connected
    if (mqttClient.connected()) {
      bool readingsPublished = publishReadings();
      publishStatus();
      
      if (!readingsPublished) {
        metrics.sensorReadFailures++;
      }
    } else {
      Serial.println("[MQTT] Skipping publish - not connected to broker");
      metrics.mqttPublishFailures++;
    }
  }
  
  // Periodic status logging to serial (every 60 seconds)
  if (now - lastStatusLog > 60000) {
    lastStatusLog = now;
    
    Serial.printf("\n[STATUS] ====== Periodic Health Check ======\n");
    Serial.printf("[STATUS] Uptime: %lus | Free Heap: %u bytes\n",
                  (now - metrics.bootTime) / 1000, ESP.getFreeHeap());
    Serial.printf("[STATUS] WiFi: %s (RSSI: %d dBm) | MQTT: %s\n",
                  WiFi.isConnected() ? "✓" : "✗",
                  WiFi.RSSI(),
                  mqttClient.connected() ? "✓" : "✗");
    Serial.printf("[STATUS] Sensor: Temp=%.1f°C Humidity=%.1f%% Pressure=%.0f hPa\n",
                  temperature_c, humidity_rh, pressure_pa / 100.0);
    Serial.printf("[STATUS] Failures: MQTT=%u | Sensor=%u | WiFi Reconnects=%u\n",
                  metrics.mqttPublishFailures, metrics.sensorReadFailures, metrics.wifiReconnects);
    Serial.printf("[STATUS] ======================================\n\n");
  }
  
  delay(10);
}
