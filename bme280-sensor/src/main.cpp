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

// DRD storage configuration - MUST be defined BEFORE including ESP_DoubleResetDetector.h
#ifdef ESP32
  #define ESP_DRD_USE_SPIFFS    true
  #define ESP_DRD_USE_EEPROM    false
  #define ESP_DRD_USE_LITTLEFS  false
#else
  #define ESP_DRD_USE_LITTLEFS  true
  #define ESP_DRD_USE_EEPROM    false
  #define ESP_DRD_USE_SPIFFS    false
  #define ESP8266_DRD_USE_RTC   false
#endif

#define DOUBLERESETDETECTOR_DEBUG  true  // Enable debug output

#include <ESP_DoubleResetDetector.h>
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

// Double Reset Detector configuration
#define DRD_TIMEOUT 10          // Seconds to wait for second reset
#define DRD_ADDRESS 0           // RTC memory address (ESP8266) or EEPROM address (ESP32)

// Create Double Reset Detector instance (initialized in setup after filesystem mount)
static DoubleResetDetector* drd = nullptr;

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

// Pressure baseline tracking (barometer-style)
float pressureBaseline = PRESSURE_BASELINE_DEFAULT;

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
void savePressureBaseline(float baseline) {
#ifdef ESP32
  File file = SPIFFS.open(PRESSURE_BASELINE_FILE, "w");
#else
  File file = LittleFS.open(PRESSURE_BASELINE_FILE, "w");
#endif
  if (file) {
    file.println(baseline, 2);  // Save with 2 decimal places
    file.close();
    Serial.printf("[CONFIG] Saved pressure baseline: %.2f Pa (%.2f hPa)\n", baseline, baseline / 100.0);
  } else {
    Serial.println("[CONFIG] Failed to save pressure baseline");
  }
}

float loadPressureBaseline() {
#ifdef ESP32
  if (!SPIFFS.exists(PRESSURE_BASELINE_FILE)) {
    return PRESSURE_BASELINE_DEFAULT;
  }
  File file = SPIFFS.open(PRESSURE_BASELINE_FILE, "r");
#else
  if (!LittleFS.exists(PRESSURE_BASELINE_FILE)) {
    return PRESSURE_BASELINE_DEFAULT;
  }
  File file = LittleFS.open(PRESSURE_BASELINE_FILE, "r");
#endif
  
  if (!file) {
    return PRESSURE_BASELINE_DEFAULT;
  }
  
  float baseline = file.parseFloat();
  file.close();
  
  if (baseline > 0) {
    Serial.printf("[BASELINE] Loaded: %.2f Pa (%.2f hPa)\n", baseline, baseline / 100.0);
  } else {
    Serial.println("[BASELINE] Tracking disabled (0.0)");
  }
  return baseline;
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
  // Initialize I2C with correct GPIO pins for ESP32-S3
  Wire.begin(BME280_I2C_SDA, BME280_I2C_SCL);
  
  // Initialize BME280 with confirmed I2C address
  if (!bme280.begin(BME280_I2C_ADDR)) {
    Serial.printf("[SENSOR] BME280 initialization failed at address 0x%02X!\n", BME280_I2C_ADDR);
    Serial.printf("[SENSOR] Check wiring: SDA=GPIO%d, SCL=GPIO%d\n", BME280_I2C_SDA, BME280_I2C_SCL);
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
  
  Serial.printf("[SENSOR] BME280 initialized successfully at address 0x%02X\n", BME280_I2C_ADDR);
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
  altitude_m = 44330.0 * (1.0 - pow(pressure_pa / (PRESSURE_SEA_LEVEL), 1.0/5.255));
  
  Serial.printf("[SENSOR] Temp: %.2f°C, Humidity: %.1f%%, Pressure: %.2f hPa, Altitude: %.1f m\n",
                temperature_c, humidity_rh, pressure_pa / 100.0, altitude_m);
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
  
  Serial.printf("[BATTERY] Voltage: %.2fV, Percentage: %d%%\n", voltage, (int)percent);
#endif
}

// =============================================================================
// DEEP SLEEP MANAGEMENT
// =============================================================================

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
      if (mqttClient.connected()) {
        mqttClient.disconnect();
      }
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
  
  // Add pressure baseline tracking (barometer-style)
  if (pressureBaseline > 0) {
    float pressureChange = pressure_pa - pressureBaseline;
    doc["pressure_change_pa"] = pressureChange;
    doc["pressure_change_hpa"] = pressureChange / 100.0;
    doc["pressure_trend"] = (pressureChange > 50) ? "rising" : (pressureChange < -50) ? "falling" : "steady";
    doc["baseline_hpa"] = pressureBaseline / 100.0;
  }
  
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
  doc["schema_version"] = 1;
  doc["timestamp"] = millis() / 1000;
  doc["uptime_seconds"] = (millis() - metrics.bootTime) / 1000;
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  doc["wifi_rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -999;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["sensor_healthy"] = (metrics.sensorReadFailures == 0);
  doc["wifi_reconnects"] = metrics.wifiReconnects;
  doc["sensor_read_failures"] = metrics.sensorReadFailures;
  doc["deep_sleep_enabled"] = (deepSleepSeconds > 0);
  doc["deep_sleep_seconds"] = deepSleepSeconds;
  if (pressureBaseline > 0) {
    doc["pressure_baseline_hpa"] = pressureBaseline / 100.0;
  }
  
  publishJson(getTopicStatus(), doc, true);
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
    if (message == "calibrate" || message == "set_baseline") {
      // Set current pressure as baseline
      pressureBaseline = pressure_pa;
      savePressureBaseline(pressureBaseline);
      String msg = String("Pressure baseline set to ") + String(pressureBaseline / 100.0, 2) + " hPa (current reading)";
      publishEvent("pressure_calibrated", msg, "info");
      publishStatus();
    } else if (message.startsWith("baseline ")) {
      // Set specific baseline value (in hPa)
      float baselineHpa = message.substring(9).toFloat();
      if (baselineHpa > 900 && baselineHpa < 1100) {  // Sanity check
        pressureBaseline = baselineHpa * 100.0;  // Convert hPa to Pa
        savePressureBaseline(pressureBaseline);
        String msg = String("Pressure baseline set to ") + String(baselineHpa, 2) + " hPa";
        publishEvent("pressure_calibrated", msg, "info");
        publishStatus();
      } else {
        publishEvent("command_error", "Invalid baseline value (must be 900-1100 hPa)", "error");
      }
    } else if (message == "clear_baseline") {
      pressureBaseline = 0.0;
      savePressureBaseline(0.0);
      publishEvent("pressure_calibrated", "Pressure baseline cleared (tracking disabled)", "info");
      publishStatus();
    } else if (message == "restart") {
      publishEvent("device_restart", "Restarting device via MQTT command", "warning");
      delay(500);
      ESP.restart();
    } else if (message == "status") {
      Serial.println("[MQTT] Received status request");
      publishStatus();
    } else if (message.startsWith("deepsleep ")) {
      // Format: "deepsleep 300" for 300 seconds
      String secondsStr = message.substring(10);
      int seconds = secondsStr.toInt();
      
      if (seconds >= 0 && seconds <= 3600) {  // Max 1 hour
        deepSleepSeconds = seconds;
        saveDeepSleepConfig();
        
        if (seconds > 0) {
          String msg = "Deep sleep set to " + String(seconds) + " seconds via MQTT";
          publishEvent("deep_sleep_config", msg, "info");
          Serial.printf("[DEEP SLEEP] Configuration updated: %d seconds\n", seconds);
          Serial.println("[DEEP SLEEP] Device will restart to apply configuration");
          delay(1000);
          ESP.restart();
        } else {
          publishEvent("deep_sleep_config", "Deep sleep disabled via MQTT", "info");
          Serial.println("[DEEP SLEEP] Deep sleep disabled");
        }
      } else {
        Serial.printf("[MQTT] Invalid deep sleep value: %d (must be 0-3600)\n", seconds);
      }
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
  Serial.printf("[MQTT] Attempting connection to %s:%d\n", MQTT_SERVER, MQTT_PORT);
  
  if (mqttClient.connect(chipId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
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
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
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
  
  // Disable WiFi power save for stable MQTT connectivity
  #ifdef ESP32
    WiFi.setSleep(false);
    Serial.println("[WiFi] Power save disabled (ESP32)");
  #else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    Serial.println("[WiFi] Power save disabled (ESP8266)");
  #endif
  
  // Add custom parameter for device name
  WiFiManagerParameter custom_device_name("device_name", "Device Name", deviceName, 40);
  wifiManager.addParameter(&custom_device_name);
  
  // Set save config callback to capture custom parameters
  wifiManager.setSaveConfigCallback([]() {
    Serial.println("[WiFi] Configuration saved via portal");
  });
  
  if (!wifiManager.autoConnect(deviceName)) {
    Serial.println("[WiFi] Configuration failed, restarting...");
    delay(3000);
    ESP.restart();
  }
  
  // Save device name if it was changed in the portal
  const char* newDeviceName = custom_device_name.getValue();
  if (strlen(newDeviceName) > 0 && strcmp(newDeviceName, deviceName) != 0) {
    saveDeviceName(newDeviceName);
    updateTopicBase();
    Serial.printf("[CONFIG] Device name updated to: %s\n", deviceName);
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
// Web server disabled to reduce memory footprint and power consumption
// All data access via MQTT topics:
//   - Readings: esp-sensor-hub/{device-name}/readings
//   - Status: esp-sensor-hub/{device-name}/status
//   - Events: esp-sensor-hub/{device-name}/events
//   - Commands: esp-sensor-hub/{device-name}/command

// =============================================================================
// MAIN SETUP AND LOOP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Mount filesystem FIRST (required for DRD and config storage)
#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    Serial.println("[FS] SPIFFS mount failed");
  } else {
    Serial.println("[FS] SPIFFS mounted successfully");
  }
#else
  if (!LittleFS.begin()) {
    Serial.println("[FS] LittleFS mount failed");
  } else {
    Serial.println("[FS] LittleFS mounted successfully");
  }
#endif

  // Initialize Double Reset Detector (after filesystem mount)
  static DoubleResetDetector drdInstance(DRD_TIMEOUT, DRD_ADDRESS);
  drd = &drdInstance;
  Serial.println("[DRD] Double Reset Detector initialized");

  // Check for double reset IMMEDIATELY (before slow WiFi/MQTT setup)
  if (drd->detectDoubleReset()) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("  DOUBLE RESET DETECTED");
    Serial.println("  Starting WiFi Configuration Portal");
    Serial.println("========================================");
    Serial.println();

    // Load device name first (needed for AP name)
    loadDeviceName();

    WiFiManager wm;
    String apName = String(deviceName);
    apName.replace(" ", "-");
    apName = "BME280-" + apName + "-Setup";

    Serial.println("[WiFi] Connect to AP: " + apName);
    Serial.println("[WiFi] Then open http://192.168.4.1 in browser");
    Serial.println();

    WiFiManagerParameter custom_device_name("device_name", "Device Name", deviceName, 40);
    wm.addParameter(&custom_device_name);
    wm.setConfigPortalTimeout(300); // 5 minute timeout

    bool shouldSaveConfig = false;
    wm.setSaveConfigCallback([&shouldSaveConfig](){
      shouldSaveConfig = true;
    });

    if (wm.startConfigPortal(apName.c_str())) {
      drd->stop();  // Clear double-reset flag after successful config
      if (shouldSaveConfig) {
        const char* newName = custom_device_name.getValue();
        strcpy(deviceName, newName);
        saveDeviceName(deviceName);
        Serial.print("[Config] Device name updated: ");
        Serial.println(newName);
      }
      Serial.println("[WiFi] Configuration portal completed successfully");
      Serial.println("[WiFi] Restarting to apply new configuration...");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("[WiFi] Configuration portal timeout or cancelled");
      Serial.println("[WiFi] Continuing with existing configuration...");
      drd->stop();  // Clear flag even if portal was cancelled
    }
  }

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
  
  // Load configuration
  loadDeviceName();
  loadDeepSleepConfig();
  pressureBaseline = loadPressureBaseline();
  Serial.printf("[CONFIG] Device: %s\n", deviceName);
  Serial.printf("[DEEP SLEEP] Config: %d seconds\n", deepSleepSeconds);
  
  // Setup MQTT
  setupMQTT();
  
  // Connect to WiFi
  setupWiFi();
  
  // Setup OTA
  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
  }
  
  // Deep sleep mode: skip web server, publish immediately, enter sleep
  if (deepSleepSeconds > 0) {
    Serial.println("\n[DEEP SLEEP] Device configured for deep sleep mode");
    Serial.printf("[DEEP SLEEP] Will sleep for %d seconds after publishing\n\n", deepSleepSeconds);
    
    // Read battery and sensor immediately
    readBattery();
    readSensorData();
    
    // Publish data
    bool publishSuccess = publishReadings();
    publishStatus();
    
    if (publishSuccess) {
      Serial.println("[DEEP SLEEP] Initial publish successful");
    } else {
      Serial.println("[DEEP SLEEP] Initial publish failed - will retry");
    }
    
    // Give MQTT time to complete
    delay(100);
    mqttClient.loop();
    delay(100);

    Serial.println();
    Serial.println("========================================");
    Serial.println("     Setup Complete (Deep Sleep Mode)");
    Serial.println("========================================");
    Serial.println();

    // Wait briefly to process any incoming MQTT commands (e.g., to disable deep sleep)
    Serial.println("[DEEP SLEEP] Waiting 5 seconds for MQTT commands...");
    unsigned long commandWaitStart = millis();
    while (millis() - commandWaitStart < 5000) {
      // Check if MQTT connection is still active before processing
      if (!mqttClient.connected()) {
        Serial.println("[DEEP SLEEP] MQTT disconnected during command wait window");
        break;
      }
      mqttClient.loop();  // Process incoming MQTT messages
      delay(10);
    }

    // Enter deep sleep only if it's still enabled (user may have disabled it via MQTT)
    if (deepSleepSeconds > 0 && publishSuccess) {
      enterDeepSleepIfEnabled();
      // If we reach here, deep sleep failed or was aborted
    } else if (deepSleepSeconds == 0) {
      Serial.println("[DEEP SLEEP] Disabled via MQTT - continuing normal operation");
    } else {
      Serial.println("[DEEP SLEEP] Initial publish failed - staying awake to retry");
    }
    // Fall through to normal operation
  }
  
  // Web server disabled - use MQTT for all data access
  
  Serial.println("\n[SETUP] Device ready!\n");
}

void loop() {
  // Stop DRD once we're in main loop (device successfully started)
  if (drd) {
    drd->loop();
  }
  
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
    
    // Read battery voltage (if enabled)
    readBattery();
    
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
    
    // Enter deep sleep if configured (after publishing)
    enterDeepSleepIfEnabled();
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
