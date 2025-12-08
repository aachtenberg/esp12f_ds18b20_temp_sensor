#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <base64.h>
#include <ESP_DoubleResetDetector.h>
#include <LittleFS.h>
#include "camera_config.h"
#include "device_config.h"
#include "secrets.h"

// Double Reset Detector configuration
#define DRD_TIMEOUT 3
#define DRD_ADDRESS 0

// Create Double Reset Detector instance
DoubleResetDetector* drd;

// Device name storage
char deviceName[40] = "Surveillance Cam";
const char* DEVICE_NAME_FILE = "/device_name.txt";

// Motion detection config
const char* MOTION_CONFIG_FILE = "/motion_config.txt";
bool motionEnabled = true;  // Default enabled
unsigned long motionDetectCount = 0;
volatile bool motionDetected = false;
unsigned long lastMotionTime = 0;

// Global objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(WEB_SERVER_PORT);
WiFiManager wifiManager;
InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// Data points
Point sensorData("camera_metrics");
Point eventData("camera_events");

// Timing variables
unsigned long lastCaptureTime = 0;
unsigned long lastMqttReconnect = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastInfluxWrite = 0;
unsigned long lastMqttStatus = 0;

// Device state
bool cameraReady = false;
bool mqttConnected = false;

// Metrics
unsigned long captureCount = 0;
unsigned long cameraErrors = 0;
unsigned long mqttPublishCount = 0;

// Function declarations
void loadDeviceName();
void saveDeviceName(const char* name);
void loadMotionConfig();
void saveMotionConfig(bool enabled);
void IRAM_ATTR motionISR();
void setupWiFi();
void setupCamera();
void setupMQTT();
void setupWebServer();
void setupInfluxDB();
void reconnectMQTT();
void publishStatus();
void captureAndPublish();
void captureAndPublishWithImage();
void publishMetricsToInflux();
void logEventToInflux(const char* event, const char* severity);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleRoot(AsyncWebServerRequest *request);
void handleCapture(AsyncWebServerRequest *request);
void handleStream(AsyncWebServerRequest *request);
void handleControl(AsyncWebServerRequest *request);
void handleMotionControl(AsyncWebServerRequest *request);
void handleMotionDetection();

void setup() {
    Serial.begin(115200);
    delay(2000);  // Extra delay to stabilize serial

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.printf("%s v%s\n", DEVICE_NAME, FIRMWARE_VERSION);
    Serial.println("========================================");
    Serial.println("[SETUP] Starting initialization...");

    // Load device name from filesystem
    Serial.println("[SETUP] Loading device name...");
    loadDeviceName();

    // Load motion config
    Serial.println("[SETUP] Loading motion config...");
    loadMotionConfig();

    // Setup PIR motion sensor
    pinMode(PIR_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIR_PIN), motionISR, RISING);
    Serial.printf("[SETUP] PIR sensor initialized on GPIO%d (motion detection %s)\n", 
                  PIR_PIN, motionEnabled ? "enabled" : "disabled");

    // Disable WiFi power saving for consistent streaming performance
    WiFi.setSleep(false);

    // Initialize status LED
    #ifdef STATUS_LED_PIN
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    #endif

    // Setup WiFi
    setupWiFi();

    // Setup InfluxDB
    setupInfluxDB();

    // Setup Camera
    setupCamera();

    // Setup MQTT
    setupMQTT();

    // Setup Web Server
    setupWebServer();

    Serial.println("Setup complete!");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());

    // Log boot event
    logEventToInflux("device_boot", "info");

    #ifdef STATUS_LED_PIN
    digitalWrite(STATUS_LED_PIN, HIGH);
    #endif
}

void loop() {
    unsigned long currentMillis = millis();

    // Handle WiFi reconnection
    if (WiFi.status() != WL_CONNECTED) {
        if (currentMillis - lastWiFiCheck >= WIFI_RECONNECT_INTERVAL) {
            Serial.println("WiFi disconnected, attempting reconnection...");
            WiFi.reconnect();
            lastWiFiCheck = currentMillis;
        }
    }

    // Handle MQTT
    if (!mqttClient.connected()) {
        if (currentMillis - lastMqttReconnect >= MQTT_RECONNECT_INTERVAL) {
            reconnectMQTT();
            lastMqttReconnect = currentMillis;
        }
    } else {
        mqttClient.loop();
    }

    // Periodic MQTT status publish (shows increasing uptime)
    if (mqttConnected) {
        const unsigned long MQTT_STATUS_INTERVAL = 30000; // 30s
        if (currentMillis - lastMqttStatus >= MQTT_STATUS_INTERVAL) {
            publishStatus();
            lastMqttStatus = currentMillis;
        }
    }

    // Handle motion detection
    if (motionDetected && motionEnabled) {
        handleMotionDetection();
    }

    // Periodic image capture
    if (cameraReady && mqttConnected) {
        if (currentMillis - lastCaptureTime >= CAPTURE_INTERVAL) {
            captureAndPublish();
            lastCaptureTime = currentMillis;
        }
    }

    // Publish metrics to InfluxDB every 60 seconds (reduced frequency to avoid blocking)
    if (currentMillis - lastInfluxWrite >= 60000) {
        publishMetricsToInflux();
        lastInfluxWrite = currentMillis;
    }

    // Minimal delay - yield to system tasks
    yield();
}

void loadDeviceName() {
    // LittleFS.begin(true) is idempotent - safe to call multiple times, true = format if needed
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Warning: LittleFS mount issue, using default device name");
        return;
    }

    if (LittleFS.exists(DEVICE_NAME_FILE)) {
        File file = LittleFS.open(DEVICE_NAME_FILE, "r");
        if (file) {
            String name = file.readStringUntil('\n');
            name.trim();
            if (name.length() > 0 && name.length() < sizeof(deviceName)) {
                strncpy(deviceName, name.c_str(), sizeof(deviceName) - 1);
                deviceName[sizeof(deviceName) - 1] = '\0';
                Serial.print("[Config] Loaded device name: ");
                Serial.println(deviceName);
            }
            file.close();
        }
    } else {
        Serial.println("[Config] No saved device name, using default");
    }
}

void saveDeviceName(const char* name) {
    // LittleFS.begin(true) is idempotent - safe to call multiple times, true = format if needed
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Warning: Cannot save device name due to filesystem issue");
        return;
    }

    File file = LittleFS.open(DEVICE_NAME_FILE, "w");
    if (file) {
        file.println(name);
        file.close();
        Serial.print("[FS] Saved device name: ");
        Serial.println(name);
    } else {
        Serial.println("[FS] Failed to save device name");
    }
}

void loadMotionConfig() {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Warning: LittleFS mount issue, using default motion config");
        return;
    }

    if (LittleFS.exists(MOTION_CONFIG_FILE)) {
        File file = LittleFS.open(MOTION_CONFIG_FILE, "r");
        if (file) {
            String config = file.readStringUntil('\n');
            config.trim();
            motionEnabled = (config == "1" || config.equalsIgnoreCase("true"));
            Serial.printf("[Config] Loaded motion config: %s\n", motionEnabled ? "enabled" : "disabled");
            file.close();
        }
    } else {
        Serial.println("[Config] No saved motion config, using default (enabled)");
    }
}

void saveMotionConfig(bool enabled) {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Warning: Cannot save motion config due to filesystem issue");
        return;
    }

    File file = LittleFS.open(MOTION_CONFIG_FILE, "w");
    if (file) {
        file.println(enabled ? "1" : "0");
        file.close();
        Serial.printf("[FS] Saved motion config: %s\n", enabled ? "enabled" : "disabled");
    } else {
        Serial.println("[FS] Failed to save motion config");
    }
}

void IRAM_ATTR motionISR() {
    motionDetected = true;
}

void handleMotionDetection() {
    unsigned long currentMillis = millis();
    
    // Clear flag
    motionDetected = false;
    
    // Debounce check
    if (currentMillis - lastMotionTime < PIR_DEBOUNCE_MS) {
        return;
    }
    
    lastMotionTime = currentMillis;
    motionDetectCount++;
    
    Serial.printf("[MOTION] Detected! Count: %lu\n", motionDetectCount);
    
    // Log to InfluxDB
    logEventToInflux("pir_motion", "info");
    
    // Trigger capture if camera is ready
    if (cameraReady) {
        captureAndPublish();
    }
}

void setupWiFi() {
    Serial.println("Setting up WiFi...");

    // Initialize Double Reset Detector
    drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);

    // Set WiFi mode
    WiFi.mode(WIFI_STA);

    // Create custom AP name based on device name
    String apName = String(deviceName);
    apName.replace(" ", "-");
    apName = "Cam-" + apName + "-Setup";

    // Add custom parameter for device name
    WiFiManagerParameter customDeviceName("device_name", "Device Name", deviceName, sizeof(deviceName));
    wifiManager.addParameter(&customDeviceName);

    // Track if we need to save config
    bool shouldSaveConfig = false;
    String oldDeviceName = String(deviceName);

    // Set save config callback
    wifiManager.setSaveConfigCallback([&shouldSaveConfig]() {
        shouldSaveConfig = true;
    });

    // WiFiManager configuration
    wifiManager.setConfigPortalTimeout(180);
    wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT / 1000);

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

        // Start config portal (blocking)
        if (!wifiManager.startConfigPortal(apName.c_str())) {
            Serial.println("[WiFi] Failed to connect after config portal");
            Serial.println("[WiFi] Restarting...");
            delay(3000);
            ESP.restart();
        }

        // Save device name if config was saved
        if (shouldSaveConfig) {
            String newName = customDeviceName.getValue();
            if (newName.length() > 0 && newName != oldDeviceName) {
                strncpy(deviceName, newName.c_str(), sizeof(deviceName) - 1);
                deviceName[sizeof(deviceName) - 1] = '\0';
                saveDeviceName(deviceName);
                Serial.print("[Config] Device name updated to: ");
                Serial.println(deviceName);
            }
        }
    } else {
        // Normal boot - try to auto-connect
        Serial.println("[WiFi] Normal boot - attempting connection...");
        Serial.println("[WiFi] (Double-reset within 3 seconds to enter config mode)");

        // Try to connect
        if (!wifiManager.autoConnect(apName.c_str())) {
            Serial.println("Failed to connect to WiFi");
            ESP.restart();
        }

        // Save device name if config was updated during autoConnect
        if (shouldSaveConfig) {
            String newName = customDeviceName.getValue();
            if (newName.length() > 0 && newName != oldDeviceName) {
                strncpy(deviceName, newName.c_str(), sizeof(deviceName) - 1);
                deviceName[sizeof(deviceName) - 1] = '\0';
                saveDeviceName(deviceName);
                Serial.print("[Config] Device name updated to: ");
                Serial.println(deviceName);
            }
        }
    }

    // Clear DRD flag
    drd->stop();

    Serial.println("WiFi connected!");
    Serial.print("Device name: ");
    Serial.println(deviceName);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}

void setupCamera() {
    Serial.println("Initializing camera...");

    cameraReady = initCamera();

    if (!cameraReady) {
        Serial.println("Camera initialization failed!");
    } else {
        Serial.println("Camera ready!");
    }
}

void setupMQTT() {
    Serial.println("Setting up MQTT...");
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024); // Increase buffer for JSON messages

    reconnectMQTT();
}

void setupWebServer() {
    Serial.println("Setting up web server...");

    // Root page
    server.on("/", HTTP_GET, handleRoot);

    // Capture endpoint
    server.on("/capture", HTTP_GET, handleCapture);

    // Stream endpoint (basic MJPEG)
    server.on("/stream", HTTP_GET, handleStream);

    // Camera control endpoint
    server.on("/control", HTTP_GET, handleControl);

    // Status endpoint
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device"] = deviceName;
        doc["version"] = FIRMWARE_VERSION;
        doc["uptime"] = millis() / 1000;
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["free_heap"] = ESP.getFreeHeap();
        doc["psram_free"] = ESP.getFreePsram();
        doc["camera_ready"] = cameraReady;
        doc["mqtt_connected"] = mqttConnected;

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // Motion control endpoint
    server.on("/motion-control", HTTP_GET, handleMotionControl);

    // OTA update endpoint (manual - upload firmware.bin)
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
    });

    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        bool shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain",
            shouldReboot ? "OK - Rebooting..." : "FAIL - " + String(Update.errorString()));
        response->addHeader("Connection", "close");
        request->send(response);

        if (shouldReboot) {
            delay(100);
            ESP.restart();
        }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(!index){
            Serial.printf("OTA Update Start: %s\n", filename.c_str());

            // ESP32-S3: Use UPDATE_SIZE_UNKNOWN for automatic size detection
            if(!Update.begin(UPDATE_SIZE_UNKNOWN)){
                Update.printError(Serial);
                Serial.println("OTA Update failed to begin");
            } else {
                Serial.println("OTA Update begun successfully");
            }
        }

        if(!Update.hasError()){
            if(Update.write(data, len) != len){
                Update.printError(Serial);
                Serial.printf("OTA Write failed at index %u\n", index);
            } else {
                Serial.printf("OTA Written: %u bytes (total: %u)\n", len, index + len);
            }
        }

        if(final){
            if(Update.end(true)){
                Serial.printf("OTA Update Success! Total size: %u bytes\n", index + len);
            } else {
                Update.printError(Serial);
                Serial.printf("OTA Update failed at final step. Error: %s\n", Update.errorString());
            }
        }
    });

    server.begin();
    Serial.printf("Web server started on http://%s\n", WiFi.localIP().toString().c_str());
}

void reconnectMQTT() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    Serial.print("Attempting MQTT connection...");

    String clientId = String(deviceName) + "-" + String(WiFi.macAddress());

    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("connected!");
        mqttConnected = true;

        // Subscribe to command topic
        mqttClient.subscribe(MQTT_TOPIC_COMMAND);

        // Publish status
        publishStatus();
    } else {
        Serial.print("failed, rc=");
        Serial.println(mqttClient.state());
        mqttConnected = false;
    }
}

void publishStatus() {
    if (!mqttConnected) return;

    unsigned long uptimeSeconds = millis() / 1000;
    unsigned long days = uptimeSeconds / 86400;
    unsigned long hours = (uptimeSeconds % 86400) / 3600;
    unsigned long minutes = (uptimeSeconds % 3600) / 60;
    unsigned long seconds = uptimeSeconds % 60;

    JsonDocument doc;
    doc["device"] = deviceName;
    doc["version"] = FIRMWARE_VERSION;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime_seconds"] = uptimeSeconds;
    doc["uptime"] = String(days) + "d " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
    doc["camera_ready"] = cameraReady;
    doc["motion_enabled"] = motionEnabled;
    doc["motion_count"] = motionDetectCount;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_psram"] = ESP.getFreePsram();
    doc["capture_count"] = captureCount;
    doc["camera_errors"] = cameraErrors;

    String output;
    serializeJson(doc, output);

    mqttClient.publish(MQTT_TOPIC_STATUS, output.c_str(), true);
    Serial.println("Status published to MQTT");
}

void captureAndPublish() {
    Serial.println("Capturing image...");

    camera_fb_t * fb = capturePhoto();
    if (!fb) {
        Serial.println("Capture failed");
        cameraErrors++;
        logEventToInflux("capture_failed", "error");
        return;
    }

    captureCount++;
    Serial.printf("Image captured: %d bytes\n", fb->len);

    // Publish image to MQTT (in chunks if needed)
    // Note: Large images may need to be published in chunks or base64 encoded
    // For now, just publish metadata
    JsonDocument doc;
    doc["timestamp"] = millis();
    doc["size"] = fb->len;
    doc["width"] = fb->width;
    doc["height"] = fb->height;
    doc["format"] = "JPEG";

    String output;
    serializeJson(doc, output);

    if (mqttClient.publish(MQTT_TOPIC_IMAGE, output.c_str())) {
        mqttPublishCount++;
    }

    // TODO: Implement actual image transfer (HTTP POST, FTP, or chunked MQTT)

    returnFrameBuffer(fb);
}

void captureAndPublishWithImage() {
    Serial.println("Capturing image with base64 encoding...");

    camera_fb_t * fb = capturePhoto();
    if (!fb) {
        Serial.println("Capture failed");
        cameraErrors++;
        logEventToInflux("capture_failed", "error");
        return;
    }

    captureCount++;
    Serial.printf("Image captured: %d bytes\n", fb->len);

    // Encode to base64
    String base64Image = base64::encode(fb->buf, fb->len);
    
    // Publish metadata + base64 image
    // Note: Large images may exceed MQTT packet size limits
    // Consider using smaller resolutions or HTTP POST for full images
    JsonDocument doc;
    doc["timestamp"] = millis();
    doc["size"] = fb->len;
    doc["width"] = fb->width;
    doc["height"] = fb->height;
    doc["format"] = "JPEG";
    doc["image"] = base64Image;

    String output;
    serializeJson(doc, output);

    Serial.printf("Publishing image with base64 (%d bytes JSON)\n", output.length());
    
    // Publish to separate topic for images with data
    if (mqttClient.publish("surveillance/image/full", output.c_str())) {
        mqttPublishCount++;
        Serial.println("Full image published to MQTT");
    } else {
        Serial.println("Failed to publish full image (likely too large)");
    }

    returnFrameBuffer(fb);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("MQTT message received on topic: %s\n", topic);

    // Parse JSON command
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.println("Failed to parse MQTT message");
        return;
    }

    // Handle commands
    if (doc["command"].is<String>()) {
        String cmd = doc["command"].as<String>();

        if (cmd == "capture") {
            captureAndPublish();
        } else if (cmd == "status") {
            publishStatus();
        } else if (cmd == "restart" || cmd == "reboot") {
            Serial.println("Restart command received");
            mqttClient.publish(MQTT_TOPIC_STATUS, "{\"status\":\"rebooting\"}", false);
            delay(100);
            ESP.restart();
        } else if (cmd == "capture_with_image") {
            captureAndPublishWithImage();
        } else {
            Serial.printf("Unknown command: %s\n", cmd.c_str());
        }
    }
}

void handleRoot(AsyncWebServerRequest *request) {
    String html = R"=====(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 Surveillance</title>
<style>
:root{--bg:#0b0f14;--panel:#121821;--panel-alt:#0f141c;--border:#1f2a38;--text:#e6edf3;--muted:#9aa7b2;--accent:#00bcd4;--accent-contrast:#07343b;--danger:#ff5252;}
body{font-family:Inter,Arial,Helvetica,sans-serif;background:var(--bg);color:var(--text);font-size:16px}
a{color:var(--text)}
section.main{display:flex;flex-direction:column}
#menu{display:none;flex-direction:column;flex-wrap:nowrap;color:var(--text);width:380px;background:var(--panel);padding:10px;border-radius:8px;margin-top:-10px;margin-right:10px;border:1px solid var(--border)}
figure{padding:0;margin:0}
figure img{display:block;max-width:100%;width:auto;height:auto;border-radius:8px;margin-top:8px}
#nav-toggle{cursor:pointer;display:block}
#nav-toggle-cb{outline:0;opacity:0;width:0;height:0}
#nav-toggle-cb:checked+#menu{display:flex}
.input-group{display:flex;flex-wrap:nowrap;line-height:22px;margin:8px 0;padding:8px;border:1px solid var(--border);border-radius:8px;background:var(--panel-alt)}
.input-group>label{display:inline-block;padding-right:10px;min-width:47%}
.input-group input,.input-group select{flex-grow:1}
.range-max,.range-min{display:inline-block;padding:0 5px;color:var(--muted)}
button{display:block;margin:3px;padding:0 10px;border:0;line-height:28px;cursor:pointer;color:#001318;background:var(--accent);border-radius:8px;font-size:16px;outline:0;box-shadow:0 0 0 1px var(--accent-contrast) inset}
button:hover{filter:brightness(1.08)}
button:active{filter:brightness(0.95)}
button.disabled{cursor:default;opacity:.6}
input[type=range]{-webkit-appearance:none;width:100%;height:22px;background:var(--panel-alt);cursor:pointer;margin:0;border-radius:6px}
input[type=range]:focus{outline:0}
input[type=range]::-webkit-slider-runnable-track{width:100%;height:2px;cursor:pointer;background:var(--text);border-radius:0;border:0}
input[type=range]::-webkit-slider-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:var(--accent);cursor:pointer;-webkit-appearance:none;margin-top:-11.5px}
.switch{display:block;position:relative;line-height:22px;font-size:16px;height:22px}
.switch input{outline:0;opacity:0;width:0;height:0}
.slider{width:50px;height:22px;border-radius:22px;cursor:pointer;background-color:#3a4b61;display:inline-block;transition:.4s}
.slider:before{position:relative;content:"";border-radius:50%;height:16px;width:16px;left:4px;top:3px;background-color:#fff;display:inline-block;transition:.4s}
input:checked+.slider{background-color:var(--accent)}
input:checked+.slider:before{-webkit-transform:translateX(26px);transform:translateX(26px)}
select{border:1px solid var(--border);font-size:14px;height:22px;outline:0;border-radius:8px;background:var(--panel-alt);color:var(--text)}
.image-container{position:relative;min-height:240px}
.close{position:absolute;right:5px;top:5px;background:var(--danger);width:30px;height:30px;border-radius:100%;color:#fff;text-align:center;line-height:30px;cursor:pointer;box-shadow:0 0 0 1px #7a1e1e inset}
.hidden{display:none}
@media (min-width:800px) and (orientation:landscape){#content{display:flex;flex-wrap:nowrap;align-items:stretch}}
</style>
</head>
<body>
<section class="main">
<div id="logo">
<label for="nav-toggle-cb" id="nav-toggle">&#9776;&nbsp;&nbsp;Settings&nbsp;&nbsp;&nbsp;&nbsp;</label>
<button id="get-still" style="float:left;">Get Still</button>
<button id="toggle-stream" style="float:left;">Start Stream</button>
</div>
<div id="content">
<div class="hidden" id="sidebar">
<input type="checkbox" id="nav-toggle-cb" checked="checked">
<nav id="menu">
<div class="input-group" id="motion-group">
<label for="motion_enabled">Motion Detection</label>
<div class="switch">
<input id="motion_enabled" type="checkbox" checked="checked">
<label class="slider" for="motion_enabled"></label>
</div>
</div>
<div class="input-group" id="framesize-group">
<label for="framesize">Resolution</label>
<select id="framesize" class="default-action">
<option value="13">UXGA(1600x1200)</option>
<option value="12">SXGA(1280x1024)</option>
<option value="11">HD(1280x720)</option>
<option value="10">XGA(1024x768)</option>
<option value="9">SVGA(800x600)</option>
<option value="8" selected="selected">VGA(640x480)</option>
<option value="7">HVGA(480x320)</option>
<option value="6">CIF(400x296)</option>
<option value="5">QVGA(320x240)</option>
<option value="3">HQVGA(240x176)</option>
<option value="1">QQVGA(160x120)</option>
</select>
</div>
<div class="input-group" id="quality-group">
<label for="quality">Quality</label>
<div class="range-min">High</div>
<input type="range" id="quality" min="10" max="63" value="10" class="default-action">
<div class="range-max">Low</div>
</div>
<div class="input-group" id="brightness-group">
<label for="brightness">Brightness</label>
<div class="range-min">-2</div>
<input type="range" id="brightness" min="-2" max="2" value="0" class="default-action">
<div class="range-max">2</div>
</div>
<div class="input-group" id="contrast-group">
<label for="contrast">Contrast</label>
<div class="range-min">-2</div>
<input type="range" id="contrast" min="-2" max="2" value="0" class="default-action">
<div class="range-max">2</div>
</div>
<div class="input-group" id="saturation-group">
<label for="saturation">Saturation</label>
<div class="range-min">-2</div>
<input type="range" id="saturation" min="-2" max="2" value="0" class="default-action">
<div class="range-max">2</div>
</div>
<div class="input-group" id="special_effect-group">
<label for="special_effect">Special Effect</label>
<select id="special_effect" class="default-action">
<option value="0" selected="selected">No Effect</option>
<option value="1">Negative</option>
<option value="2">Grayscale</option>
<option value="3">Red Tint</option>
<option value="4">Green Tint</option>
<option value="5">Blue Tint</option>
<option value="6">Sepia</option>
</select>
</div>
<div class="input-group" id="awb-group">
<label for="awb">AWB</label>
<div class="switch">
<input id="awb" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="awb"></label>
</div>
</div>
<div class="input-group" id="awb_gain-group">
<label for="awb_gain">AWB Gain</label>
<div class="switch">
<input id="awb_gain" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="awb_gain"></label>
</div>
</div>
<div class="input-group" id="wb_mode-group">
<label for="wb_mode">WB Mode</label>
<select id="wb_mode" class="default-action">
<option value="0" selected="selected">Auto</option>
<option value="1">Sunny</option>
<option value="2">Cloudy</option>
<option value="3">Office</option>
<option value="4">Home</option>
</select>
</div>
<div class="input-group" id="aec-group">
<label for="aec">AEC Sensor</label>
<div class="switch">
<input id="aec" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="aec"></label>
</div>
</div>
<div class="input-group" id="aec2-group">
<label for="aec2">AEC DSP</label>
<div class="switch">
<input id="aec2" type="checkbox" class="default-action">
<label class="slider" for="aec2"></label>
</div>
</div>
<div class="input-group" id="ae_level-group">
<label for="ae_level">AE Level</label>
<div class="range-min">-2</div>
<input type="range" id="ae_level" min="-2" max="2" value="0" class="default-action">
<div class="range-max">2</div>
</div>
<div class="input-group" id="aec_value-group">
<label for="aec_value">Exposure</label>
<div class="range-min">0</div>
<input type="range" id="aec_value" min="0" max="1200" value="300" class="default-action">
<div class="range-max">1200</div>
</div>
<div class="input-group" id="agc-group">
<label for="agc">AGC</label>
<div class="switch">
<input id="agc" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="agc"></label>
</div>
</div>
<div class="input-group hidden" id="agc_gain-group">
<label for="agc_gain">Gain</label>
<div class="range-min">1x</div>
<input type="range" id="agc_gain" min="0" max="30" value="5" class="default-action">
<div class="range-max">31x</div>
</div>
<div class="input-group" id="gainceiling-group">
<label for="gainceiling">Gain Ceiling</label>
<div class="range-min">2x</div>
<input type="range" id="gainceiling" min="0" max="6" value="0" class="default-action">
<div class="range-max">128x</div>
</div>
<div class="input-group" id="bpc-group">
<label for="bpc">BPC</label>
<div class="switch">
<input id="bpc" type="checkbox" class="default-action">
<label class="slider" for="bpc"></label>
</div>
</div>
<div class="input-group" id="wpc-group">
<label for="wpc">WPC</label>
<div class="switch">
<input id="wpc" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="wpc"></label>
</div>
</div>
<div class="input-group" id="raw_gma-group">
<label for="raw_gma">Raw GMA</label>
<div class="switch">
<input id="raw_gma" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="raw_gma"></label>
</div>
</div>
<div class="input-group" id="lenc-group">
<label for="lenc">Lens Correction</label>
<div class="switch">
<input id="lenc" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="lenc"></label>
</div>
</div>
<div class="input-group" id="hmirror-group">
<label for="hmirror">H-Mirror</label>
<div class="switch">
<input id="hmirror" type="checkbox" class="default-action">
<label class="slider" for="hmirror"></label>
</div>
</div>
<div class="input-group" id="vflip-group">
<label for="vflip">V-Flip</label>
<div class="switch">
<input id="vflip" type="checkbox" class="default-action">
<label class="slider" for="vflip"></label>
</div>
</div>
<div class="input-group" id="dcw-group">
<label for="dcw">DCW (Downsize EN)</label>
<div class="switch">
<input id="dcw" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="dcw"></label>
</div>
</div>
<div class="input-group" id="colorbar-group">
<label for="colorbar">Test Pattern</label>
<div class="switch">
<input id="colorbar" type="checkbox" class="default-action">
<label class="slider" for="colorbar"></label>
</div>
</div>
<div class="input-group" id="presets-group">
<label>Presets</label>
<div>
    <button id="preset-smooth" class="btn secondary">Smooth</button>
    <button id="preset-balanced" class="btn secondary">Balanced</button>
    <button id="preset-detail" class="btn secondary">Detail</button>
</div>
</div>
</nav>
</div>
<figure>
<div id="stream-container" class="image-container hidden">
<div class="close" id="close-stream">×</div>
<img id="stream" src="">
</div>
</figure>
</div>
</section>
<script>
document.addEventListener('DOMContentLoaded',function(){
const baseHost=document.location.origin;
const settings=document.getElementById('sidebar');
const view=document.getElementById('stream');
const viewContainer=document.getElementById('stream-container');
const stillButton=document.getElementById('get-still');
const streamButton=document.getElementById('toggle-stream');
const closeButton=document.getElementById('close-stream');
let streamInterval=null;
let isLoading=false;
const hide=el=>el.classList.add('hidden');
const show=el=>el.classList.remove('hidden');
const disable=el=>{el.classList.add('disabled');el.disabled=true};
const enable=el=>{el.classList.remove('disabled');el.disabled=false};
const updateValue=(el,value,updateRemote)=>{
updateRemote=updateRemote==null?true:updateRemote;
let initialValue;
if(el.type==='checkbox'){
initialValue=el.checked;
value=!!value;
el.checked=value;
}else{
initialValue=el.value;
el.value=value;
}
if(updateRemote&&initialValue!==value){
updateConfig(el);
}else if(!updateRemote){
if(el.id==="aec"){
value?hide(document.getElementById('aec_value-group')):show(document.getElementById('aec_value-group'));
}else if(el.id==="agc"){
if(value){
show(document.getElementById('gainceiling-group'));
hide(document.getElementById('agc_gain-group'));
}else{
hide(document.getElementById('gainceiling-group'));
show(document.getElementById('agc_gain-group'));
}
}else if(el.id==="awb_gain"){
value?show(document.getElementById('wb_mode-group')):hide(document.getElementById('wb_mode-group'));
}
}
};
function updateConfig(el){
let value;
switch(el.type){
case 'checkbox':value=el.checked?1:0;break;
case 'range':
case 'select-one':value=el.value;break;
default:return;
}
const query=`${baseHost}/control?var=${el.id}&val=${value}`;
fetch(query).then(response=>{
console.log(`request to ${query} finished, status: ${response.status}`);
});
}
document.querySelectorAll('.close').forEach(el=>{
el.onclick=()=>hide(el.parentNode);
});
fetch(`${baseHost}/status`).then(function(response){
return response.json();
}).then(function(state){
document.querySelectorAll('.default-action').forEach(el=>{
updateValue(el,state[el.id],false);
});
if(state.motion_enabled!==undefined){
document.getElementById('motion_enabled').checked=state.motion_enabled;
}
show(settings);
});
const stopStream=()=>{
view.src='';
streamButton.innerHTML='Start Stream';
hide(viewContainer);
};
const startStream=()=>{
view.src=`${baseHost}/stream`;
show(viewContainer);
streamButton.innerHTML='Stop Stream';
};
stillButton.onclick=()=>{
stopStream();
view.src=`${baseHost}/capture?_cb=${Date.now()}`;
show(viewContainer);
};
closeButton.onclick=()=>{
stopStream();
hide(viewContainer);
};
streamButton.onclick=()=>{
const streamEnabled=streamButton.innerHTML==='Stop Stream';
if(streamEnabled){
stopStream();
}else{
startStream();
}
};
document.querySelectorAll('.default-action').forEach(el=>{
el.onchange=()=>updateConfig(el);
});
const agc=document.getElementById('agc');
const agcGain=document.getElementById('agc_gain-group');
const gainCeiling=document.getElementById('gainceiling-group');
agc.onchange=()=>{
updateConfig(agc);
if(agc.checked){
show(gainCeiling);
hide(agcGain);
}else{
hide(gainCeiling);
show(agcGain);
}
};
const aec=document.getElementById('aec');
const exposure=document.getElementById('aec_value-group');
aec.onchange=()=>{
updateConfig(aec);
aec.checked?hide(exposure):show(exposure);
};
const awb=document.getElementById('awb_gain');
const wb=document.getElementById('wb_mode-group');
awb.onchange=()=>{
updateConfig(awb);
awb.checked?show(wb):hide(wb);
};

// Motion detection toggle
const motionToggle=document.getElementById('motion_enabled');
motionToggle.onchange=()=>{
const enabled=motionToggle.checked?1:0;
fetch(`${baseHost}/motion-control?enabled=${enabled}`).then(response=>response.json()).then(data=>{
console.log('Motion detection:',data.motion_enabled?'enabled':'disabled');
}).catch(err=>console.error('Motion control failed:',err));
};

// Preset helpers
const setAndPush=(id,val)=>{
    const el=document.getElementById(id);
    if(!el) return;
    updateValue(el,val,true);
};
document.getElementById('preset-smooth').onclick=()=>{
    setAndPush('framesize','5'); // QVGA
    setAndPush('quality','12');
    setAndPush('aec','1');
    setAndPush('aec_value','200');
    setAndPush('gainceiling','1');
};
document.getElementById('preset-balanced').onclick=()=>{
    setAndPush('framesize','7'); // HVGA
    setAndPush('quality','12');
    setAndPush('aec','1');
    setAndPush('aec_value','250');
    setAndPush('gainceiling','2');
};
document.getElementById('preset-detail').onclick=()=>{
    setAndPush('framesize','9'); // SVGA
    setAndPush('quality','15');
    setAndPush('aec','1');
    setAndPush('aec_value','250');
    setAndPush('gainceiling','2');
};
});
</script>
</body>
</html>
)=====";
    request->send(200, "text/html", html);
}

void handleCapture(AsyncWebServerRequest *request) {
    camera_fb_t * fb = capturePhoto();
    if (!fb) {
        cameraErrors++;
        request->send(500, "text/plain", "Camera capture failed");
        return;
    }

    captureCount++;

    // Send the image directly from the buffer
    // The response will take ownership of a copy of the data
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/jpeg", fb->buf, fb->len);
    response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);

    // Now it's safe to return the frame buffer
    returnFrameBuffer(fb);
}

// MJPEG streaming using chunked response
// Note: AsyncWebServer has limitations with continuous streaming
// For best results, use JavaScript-based polling or consider switching to ESP-IDF httpd
// Optimized MJPEG streaming response
// Key optimization: Send larger chunks to reduce TCP overhead
class AsyncJpegStreamResponse: public AsyncAbstractResponse {
private:
    camera_fb_t *_fb;
    size_t _index;
    bool _boundary_sent;
    static const size_t CHUNK_SIZE = 4096;  // Larger chunks for efficiency

public:
    AsyncJpegStreamResponse() {
        _code = 200;
        _contentType = "multipart/x-mixed-replace; boundary=frame";
        _sendContentLength = false;
        _chunked = true;
        _fb = NULL;
        _index = 0;
        _boundary_sent = false;
    }

    ~AsyncJpegStreamResponse() {
        if (_fb) {
            returnFrameBuffer(_fb);
        }
    }

    bool _sourceValid() const override { return true; }

    virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
        size_t ret = 0;

        // Get a new frame if we don't have one
        if (!_fb) {
            _fb = capturePhoto();
            if (!_fb) {
                return RESPONSE_TRY_AGAIN;
            }
            _index = 0;
            _boundary_sent = false;
        }

        // Send boundary header first
        if (!_boundary_sent) {
            String boundary = "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(_fb->len) + "\r\n\r\n";
            size_t blen = boundary.length();
            if (blen > maxLen) {
                return RESPONSE_TRY_AGAIN;
            }
            memcpy(buf, boundary.c_str(), blen);
            _boundary_sent = true;
            return blen;
        }

        // Send frame data in larger chunks for efficiency
        if (_index < _fb->len) {
            // Use min of: maxLen, remaining data, or CHUNK_SIZE
            ret = min(maxLen, min(_fb->len - _index, CHUNK_SIZE));
            memcpy(buf, _fb->buf + _index, ret);
            _index += ret;

            // If we finished this frame, prepare for next one
            if (_index >= _fb->len) {
                returnFrameBuffer(_fb);
                _fb = NULL;
                _index = 0;
                _boundary_sent = false;
            }

            return ret;
        }

        return RESPONSE_TRY_AGAIN;
    }
};

void handleStream(AsyncWebServerRequest *request) {
    if (!cameraReady) {
        request->send(503, "text/plain", "Camera not ready");
        return;
    }

    Serial.println("Starting MJPEG stream");
    AsyncJpegStreamResponse *response = new AsyncJpegStreamResponse();
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void handleControl(AsyncWebServerRequest *request) {
    if (!cameraReady) {
        request->send(500, "text/plain", "Camera not ready");
        return;
    }

    sensor_t * s = esp_camera_sensor_get();
    if (s == NULL) {
        request->send(500, "text/plain", "Failed to get camera sensor");
        return;
    }

    String var = request->getParam("var")->value();
    int val = request->getParam("val")->value().toInt();

    int res = 0;

    if (var == "framesize") {
        res = s->set_framesize(s, (framesize_t)val);
    } else if (var == "quality") {
        res = s->set_quality(s, val);
    } else if (var == "brightness") {
        res = s->set_brightness(s, val);
    } else if (var == "contrast") {
        res = s->set_contrast(s, val);
    } else if (var == "saturation") {
        res = s->set_saturation(s, val);
    } else if (var == "special_effect") {
        res = s->set_special_effect(s, val);
    } else if (var == "hmirror") {
        res = s->set_hmirror(s, val);
    } else if (var == "vflip") {
        res = s->set_vflip(s, val);
    } else if (var == "awb") {
        res = s->set_whitebal(s, val);
    } else if (var == "aec") {
        res = s->set_exposure_ctrl(s, val);
    } else if (var == "agc") {
        res = s->set_gain_ctrl(s, val);
    } else if (var == "awb_gain") {
        res = s->set_awb_gain(s, val);
    } else if (var == "aec2") {
        res = s->set_aec2(s, val);
    } else if (var == "ae_level") {
        res = s->set_ae_level(s, val);
    } else if (var == "aec_value") {
        res = s->set_aec_value(s, val);
    } else if (var == "agc_gain") {
        res = s->set_agc_gain(s, val);
    } else if (var == "gainceiling") {
        res = s->set_gainceiling(s, (gainceiling_t)val);
    } else if (var == "wb_mode") {
        res = s->set_wb_mode(s, val);
    } else if (var == "bpc") {
        res = s->set_bpc(s, val);
    } else if (var == "wpc") {
        res = s->set_wpc(s, val);
    } else if (var == "raw_gma") {
        res = s->set_raw_gma(s, val);
    } else if (var == "lenc") {
        res = s->set_lenc(s, val);
    } else if (var == "dcw") {
        res = s->set_dcw(s, val);
    } else if (var == "colorbar") {
        res = s->set_colorbar(s, val);
    } else {
        request->send(400, "text/plain", "Unknown control parameter");
        return;
    }

    if (res == 0) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Failed to set control");
    }
}

void setupInfluxDB() {
    Serial.println("Setting up InfluxDB...");

    // Add tags to data points
    sensorData.addTag("device", deviceName);
    sensorData.addTag("location", "surveillance");

    eventData.addTag("device", deviceName);
    eventData.addTag("location", "surveillance");

    // Note: Skip validateConnection() as it's blocking and can delay startup
    // InfluxDB will be validated on first write attempt
    Serial.println("InfluxDB configured (will validate on first write)");
}

void publishMetricsToInflux() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    // Clear previous data
    sensorData.clearFields();

    // Add metrics
    sensorData.addField("uptime", millis() / 1000);
    sensorData.addField("wifi_rssi", WiFi.RSSI());
    sensorData.addField("free_heap", ESP.getFreeHeap());
    sensorData.addField("free_psram", ESP.getFreePsram());
    sensorData.addField("camera_ready", cameraReady ? 1 : 0);
    sensorData.addField("mqtt_connected", mqttConnected ? 1 : 0);
    sensorData.addField("capture_count", captureCount);
    sensorData.addField("camera_errors", cameraErrors);
    sensorData.addField("mqtt_publishes", mqttPublishCount);

    // Write to InfluxDB (non-blocking, ignore errors to prevent spam)
    static unsigned long lastErrorLog = 0;
    if (!influxClient.writePoint(sensorData)) {
        // Only log errors every 5 minutes to avoid spam
        if (millis() - lastErrorLog > 300000) {
            Serial.print("InfluxDB write failed: ");
            Serial.println(influxClient.getLastErrorMessage());
            lastErrorLog = millis();
        }
    }
}

void logEventToInflux(const char* event, const char* severity) {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    // Clear previous data
    eventData.clearFields();

    // Add event data
    eventData.addField("event", event);
    eventData.addField("severity", severity);
    eventData.addField("uptime", millis() / 1000);
    eventData.addField("free_heap", ESP.getFreeHeap());

    // Write to InfluxDB (silently fail to avoid blocking/spam)
    influxClient.writePoint(eventData);
}

void handleMotionControl(AsyncWebServerRequest *request) {
    if (!request->hasParam("enabled")) {
        request->send(400, "text/plain", "Missing 'enabled' parameter");
        return;
    }

    String enabledParam = request->getParam("enabled")->value();
    bool newState = (enabledParam == "1" || enabledParam.equalsIgnoreCase("true"));
    
    motionEnabled = newState;
    saveMotionConfig(newState);
    
    Serial.printf("[Motion] Detection %s via web control\n", newState ? "enabled" : "disabled");
    
    JsonDocument doc;
    doc["motion_enabled"] = motionEnabled;
    doc["motion_count"] = motionDetectCount;
    doc["status"] = "ok";
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}