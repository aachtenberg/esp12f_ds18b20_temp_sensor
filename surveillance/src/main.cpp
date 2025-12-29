#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <base64.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <SD.h> // For SD card read/write utilities
#include <Preferences.h>
#include <esp_system.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <img_converters.h>
#include <vector>
#include <algorithm>
#include "camera_config.h"
#include "device_config.h"
#include "secrets.h"
#include "trace.h"

// RTC memory for reset detection and crash loop recovery
// These survive software resets but are cleared on power loss
RTC_NOINIT_ATTR uint32_t rtcResetCount;      // Counts rapid resets
RTC_NOINIT_ATTR uint32_t rtcResetTimestamp;  // Timestamp of last reset
RTC_NOINIT_ATTR uint32_t rtcCrashLoopFlag;   // Magic number if boot incomplete
RTC_NOINIT_ATTR uint32_t rtcCrashCount;      // Consecutive crash count

// SD_MMC pin definitions for ESP32-S3 only (not for ESP32-CAM)
#ifdef ARDUINO_FREENOVE_ESP32_S3_WROOM
#define SD_MMC_CMD 38 //Please do not modify it.
#define SD_MMC_CLK 39 //Please do not modify it. 
#define SD_MMC_D0  40 //Please do not modify it.
#endif

// Boot/recovery state
const char* configPortalReason = "none";     // Why portal was triggered

// Device name storage
char deviceName[40] = "Surveillance Cam";
const char* DEVICE_NAME_FILE = "/device_name.txt";

// Device hardware identifiers
char deviceChipId[17];  // 16 hex chars + null terminator
char deviceMac[18];     // XX:XX:XX:XX:XX:XX format

// Motion detection config
const char* MOTION_CONFIG_FILE = "/motion_config.txt";
bool motionEnabled = true;  // Default ENABLED - now uses proper JPEG decoding
unsigned long motionDetectCount = 0;
volatile bool motionDetected = false;
unsigned long lastMotionTime = 0;
unsigned long flashOffTime = 0;  // Track when to turn off flash LED

// Flash LED config
const char* FLASH_CONFIG_FILE = "/flash_config.txt";
bool flashEnabled = true;   // Flash during captures (always ON, not configurable)
bool flashMotionEnabled = false;  // Flash for motion indicator (default OFF - too bright for continuous use)
bool flashManualOn = false;  // Manual flashlight mode (default OFF)

// Filesystem status
bool littleFsReady = false;

// SD card capture storage
bool sdReady = false;
const char* SD_CAPTURE_DIR = "/captures";

// Global objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(WEB_SERVER_PORT);
WiFiManager wifiManager;
Preferences resetPrefs;

// Timing variables
unsigned long lastCaptureTime = 0;
unsigned long lastMqttReconnect = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastMetricsPublish = 0;
unsigned long lastMqttStatus = 0;
unsigned long lastMotionCheck = 0;

// Device state
bool cameraReady = false;
bool mqttConnected = false;
volatile bool otaInProgress = false;  // Track active OTA transfers

// Metrics
unsigned long captureCount = 0;
unsigned long cameraErrors = 0;
unsigned long mqttPublishCount = 0;

// Camera motion detection
uint8_t* previousFrame = NULL;
size_t previousFrameSize = 0;

// Function declarations
void loadDeviceName();
void saveDeviceName(const char* name);
void loadMotionConfig();
void saveMotionConfig(bool enabled);
void loadFlashConfig();
void saveFlashConfig(bool illumination, bool motion);
void getDeviceChipId();
void getDeviceMacAddress();
void IRAM_ATTR motionISR();
bool checkCameraMotion();
void setupWiFi();
void setupSD();
bool deleteOldestCaptures(int count);
bool deleteAllCaptures();
void setupCamera();
void setupMQTT();
void setupOTA();
void setupWebServer();
void reconnectMQTT();
void gracefulMqttDisconnect();
void publishStatus();
void captureAndPublish();
void captureAndPublishWithImage();
void publishMetricsToMQTT();
void logEventToMQTT(const char* event, const char* severity);
bool saveImageToSD(camera_fb_t* fb, const char* reason);

// Dynamic topic builders (device-specific for multiple camera support)
String getTopicStatus() { return String(MQTT_TOPIC_BASE) + "/" + deviceName + MQTT_TOPIC_STATUS_SUFFIX; }
String getTopicImage() { return String(MQTT_TOPIC_BASE) + "/" + deviceName + MQTT_TOPIC_IMAGE_SUFFIX; }
String getTopicMotion() { return String(MQTT_TOPIC_BASE) + "/" + deviceName + MQTT_TOPIC_MOTION_SUFFIX; }
String getTopicCommand() { return String(MQTT_TOPIC_BASE) + "/" + deviceName + MQTT_TOPIC_COMMAND_SUFFIX; }
String getTopicMetrics() { return String(MQTT_TOPIC_BASE) + "/" + deviceName + MQTT_TOPIC_METRICS_SUFFIX; }
String getTopicEvents() { return String(MQTT_TOPIC_BASE) + "/" + deviceName + MQTT_TOPIC_EVENTS_SUFFIX; }
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleRoot(AsyncWebServerRequest *request);
void handleCapture(AsyncWebServerRequest *request);
void handleStream(AsyncWebServerRequest *request);
void handleControl(AsyncWebServerRequest *request);
void handleMotionControl(AsyncWebServerRequest *request);
void handleFlashControl(AsyncWebServerRequest *request);
void handleWiFiReset(AsyncWebServerRequest *request);
void handleMotionDetection();
void checkResetCounter();
void clearCrashLoop();

void setup() {
    Serial.begin(115200);

    // Check reset counter and crash loop FIRST (before anything else and any delays)
    checkResetCounter();

    // Configure SD_MMC pins (ESP32-S3 only)
    #ifdef ARDUINO_FREENOVE_ESP32_S3_WROOM
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    #endif
    
    // Initialize trace instrumentation (keep early, but after reset check)
    Trace::init();

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.printf("%s v%s\n", DEVICE_NAME, FIRMWARE_VERSION);
    Serial.println("========================================");
    Serial.println("[SETUP] Starting initialization...");

    // Removed: reset check now runs at the very start to ensure window timing works

    // Initialize LittleFS once (used for config storage)
    Serial.println("[SETUP] Mounting LittleFS...");
    littleFsReady = LittleFS.begin(true);  // true = format if needed
    if (!littleFsReady) {
        Serial.println("[SETUP] WARNING: LittleFS mount failed!");
    } else {
        Serial.println("[SETUP] LittleFS mounted successfully");
    }

    // Load device name from filesystem
    Serial.println("[SETUP] Loading device name...");
    loadDeviceName();

    // Get device chip ID and MAC address
    Serial.println("[SETUP] Getting device identifiers...");
    getDeviceChipId();
    getDeviceMacAddress();

    // Load motion config
    Serial.println("[SETUP] Loading motion config...");
    loadMotionConfig();

    // PIR sensor disabled for testing
    // pinMode(PIR_PIN, INPUT_PULLDOWN);
    // attachInterrupt(digitalPinToInterrupt(PIR_PIN), motionISR, RISING);
    Serial.println("[SETUP] PIR sensor disabled");

    // Load flash config before GPIO init
    Serial.println("[SETUP] Loading flash config...");
    loadFlashConfig();

    // Flash LED for capture illumination (controlled during capture only)
    if (FLASH_PIN >= 0) {
        pinMode(FLASH_PIN, OUTPUT);
        digitalWrite(FLASH_PIN, LOW);
        Serial.printf("[SETUP] Flash LED initialized on GPIO%d (capture flash=%s)\n", FLASH_PIN, flashEnabled ? "ON" : "OFF");
    } else {
        Serial.println("[SETUP] No flash LED available on this board");
    }

    // Mount SD card (1-bit mode for ESP32-CAM) for capture storage
    setupSD();

    // Disable WiFi power saving for consistent streaming performance
    WiFi.setSleep(false);

    // Initialize status LED
    #ifdef STATUS_LED_PIN
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    #endif

    // Setup WiFi
    setupWiFi();

    // Initialize camera in background (non-blocking)
    // Camera will be initialized asynchronously, web server will respond with camera_ready=false until done
    xTaskCreate(
        [](void* param) {
            Serial.println("[Camera] Initializing in background task...");
            cameraReady = initCamera();
            if (!cameraReady) {
                Serial.println("[Camera] Initialization FAILED!");
                Serial.printf("[Camera] cameraReady = %d\n", cameraReady);
            } else {
                Serial.println("[Camera] Initialization complete!");
                Serial.printf("[Camera] cameraReady = %d\n", cameraReady);
            }
            vTaskDelete(NULL);
        },
        "CameraInit",
        4096,
        NULL,
        1,
        NULL
    );

    // Setup MQTT
    setupMQTT();

    // Setup OTA updates (disabled)
    // setupOTA();

    // Setup Web Server
    setupWebServer();

    Serial.println("Setup complete!");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM free: %d bytes\n", ESP.getFreePsram());

    // Clear crash loop flag - we made it through setup successfully
    clearCrashLoop();

    // Log boot event
    logEventToMQTT("device_boot", "info");

    #ifdef STATUS_LED_PIN
    digitalWrite(STATUS_LED_PIN, HIGH);
    #endif
}

void loop() {
    unsigned long currentMillis = millis();

    // Handle OTA updates (disabled)
    // ArduinoOTA.handle();

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

    // Camera-based motion detection (throttled to every 3 seconds)
    if (motionEnabled && cameraReady) {
        if (currentMillis - lastMotionCheck >= MOTION_CHECK_INTERVAL) {
            if (checkCameraMotion()) {
                // Motion detected - publish to MQTT
                if (mqttConnected) {
                    JsonDocument doc;
                    doc["device"] = deviceName;
                    doc["motion"] = true;
                    doc["timestamp"] = currentMillis / 1000;
                    doc["count"] = motionDetectCount;

                    String output;
                    serializeJson(doc, output);

                    String topic = String(MQTT_TOPIC_BASE) + "/" + deviceName + "/motion";
                    mqttClient.publish(topic.c_str(), output.c_str(), false);
                    Serial.println("[Motion] Published to MQTT");
                }
                
                // Flash LED on motion if enabled (respects both motion flash setting and manual override)
                if (flashMotionEnabled && !flashManualOn && FLASH_PIN >= 0) {
                    digitalWrite(FLASH_PIN, HIGH);
                    flashOffTime = currentMillis + FLASH_PULSE_MS;
                    Serial.printf("[FLASH] Motion indicator triggered for %d ms\n", FLASH_PULSE_MS);
                }
            }
            lastMotionCheck = currentMillis;
        }
    }

    // Turn off flash LED after motion pulse duration (only if not in manual mode)
    if (!flashManualOn && flashOffTime > 0 && currentMillis >= flashOffTime && FLASH_PIN >= 0) {
        digitalWrite(FLASH_PIN, LOW);
        flashOffTime = 0;
    }

    // Periodic image capture - DISABLED (motion-only mode)
    // if (cameraReady && mqttConnected) {
    //     if (currentMillis - lastCaptureTime >= CAPTURE_INTERVAL) {
    //         captureAndPublish();
    //         lastCaptureTime = currentMillis;
    //     }
    // }

    // Publish metrics to MQTT every 60 seconds
    if (currentMillis - lastMetricsPublish >= 60000) {
        publishMetricsToMQTT();
        lastMetricsPublish = currentMillis;
    }

    // Minimal delay - yield to system tasks
    yield();
}

bool checkCameraMotion() {
    // Proper motion detection: decode JPEG to RGB565, then compare pixels
    // Based on MJPEG2SD algorithm
    
    if (!cameraReady) return false;

    camera_fb_t* fb = capturePhoto();
    if (!fb) {
        Serial.println("[Motion] Failed to capture frame");
        return false;
    }

    bool motionDetected = false;

    // Downsample dimensions (96x96 at 8x scale from typical SVGA 800x600)
    const int DOWNSAMPLE_WIDTH = 96;
    const int DOWNSAMPLE_HEIGHT = 96;
    const int DOWNSAMPLE_SIZE = DOWNSAMPLE_WIDTH * DOWNSAMPLE_HEIGHT;
    
    // Allocate RGB565 buffer for decoded JPEG (first time only)
    static uint8_t* rgb565Buffer = NULL;
    if (rgb565Buffer == NULL) {
        rgb565Buffer = (uint8_t*)ps_malloc(DOWNSAMPLE_SIZE * 2); // RGB565 = 2 bytes per pixel
    }
    
    if (previousFrame == NULL) {
        // First frame - allocate grayscale buffer
        previousFrame = (uint8_t*)malloc(DOWNSAMPLE_SIZE);
        if (previousFrame && rgb565Buffer) {
            // Decode JPEG to RGB565 at downsampled resolution
            if (jpg2rgb565(fb->buf, fb->len, rgb565Buffer, JPG_SCALE_8X)) {
                // Convert RGB565 to grayscale
                for (int i = 0; i < DOWNSAMPLE_SIZE; i++) {
                    uint16_t pixel = ((uint16_t*)rgb565Buffer)[i];
                    // Extract RGB from RGB565: RRRRRGGGGGGBBBBB
                    uint8_t r = (pixel >> 11) & 0x1F;  // 5 bits
                    uint8_t g = (pixel >> 5) & 0x3F;   // 6 bits
                    uint8_t b = pixel & 0x1F;          // 5 bits
                    // Scale to 8-bit and convert to grayscale
                    previousFrame[i] = (r * 8 + g * 4 + b * 8) / 3;
                }
                previousFrameSize = DOWNSAMPLE_SIZE;
                Serial.printf("[Motion] First frame decoded - %dx%d grayscale\n", DOWNSAMPLE_WIDTH, DOWNSAMPLE_HEIGHT);
            } else {
                Serial.println("[Motion] JPEG decode failed");
            }
        }
        returnFrameBuffer(fb);
        return false;
    }

    // Decode current frame to RGB565
    if (!jpg2rgb565(fb->buf, fb->len, rgb565Buffer, JPG_SCALE_8X)) {
        Serial.println("[Motion] JPEG decode failed");
        returnFrameBuffer(fb);
        return false;
    }

    // Convert to grayscale and compare
    int changedPixels = 0;
    int totalPixels = DOWNSAMPLE_SIZE;
    
    for (int i = 0; i < DOWNSAMPLE_SIZE; i++) {
        // Convert RGB565 pixel to grayscale
        uint16_t pixel = ((uint16_t*)rgb565Buffer)[i];
        uint8_t r = (pixel >> 11) & 0x1F;
        uint8_t g = (pixel >> 5) & 0x3F;
        uint8_t b = pixel & 0x1F;
        uint8_t currentGray = (r * 8 + g * 4 + b * 8) / 3;
        
        // Compare with previous frame
        int diff = abs((int)currentGray - (int)previousFrame[i]);
        if (diff > MOTION_THRESHOLD) {
            changedPixels++;
        }
        
        // Update previous frame buffer
        previousFrame[i] = currentGray;
    }

    // Determine if motion detected
    if (changedPixels >= MOTION_CHANGED_BLOCKS) {
        motionDetected = true;
        float changePercent = (float)changedPixels / totalPixels * 100.0;
        Serial.printf("[Motion] *** DETECTED *** %d/%d pixels changed (%.1f%%) - Count: %lu\n",
                      changedPixels, totalPixels, changePercent, motionDetectCount + 1);
        motionDetectCount++;
    }

    returnFrameBuffer(fb);
    return motionDetected;
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

void getDeviceChipId() {
    uint64_t chipId = ESP.getEfuseMac();
    sprintf(deviceChipId, "%012llX", chipId);
    Serial.print("[Config] Device Chip ID: ");
    Serial.println(deviceChipId);
}

void getDeviceMacAddress() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    sprintf(deviceMac, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.print("[Config] Device MAC: ");
    Serial.println(deviceMac);
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

void loadFlashConfig() {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Warning: LittleFS mount issue, using default flash config");
        return;
    }

    if (LittleFS.exists(FLASH_CONFIG_FILE)) {
        File file = LittleFS.open(FLASH_CONFIG_FILE, "r");
        if (file) {
            String line1 = file.readStringUntil('\n');
            String line2 = file.readStringUntil('\n');
            line1.trim();
            line2.trim();
            flashEnabled = (line1 == "1" || line1.equalsIgnoreCase("true"));
            flashMotionEnabled = (line2 == "1" || line2.equalsIgnoreCase("true"));
            Serial.printf("[Config] Loaded flash config: illumination=%s, motion=%s\n",
                          flashEnabled ? "enabled" : "disabled",
                          flashMotionEnabled ? "enabled" : "disabled");
            file.close();
        }
    } else {
        Serial.println("[Config] No saved flash config, using defaults (both disabled)");
    }
}

void saveFlashConfig(bool illumination, bool motion) {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Warning: Cannot save flash config due to filesystem issue");
        return;
    }

    File file = LittleFS.open(FLASH_CONFIG_FILE, "w");
    if (file) {
        file.println(illumination ? "1" : "0");
        file.println(motion ? "1" : "0");
        file.close();
        Serial.printf("[FS] Saved flash config: illumination=%s, motion=%s\n",
                      illumination ? "enabled" : "disabled",
                      motion ? "enabled" : "disabled");
    } else {
        Serial.println("[FS] Failed to save flash config");
    }
}

// SD Card recovery: graceful unmount before reboot to prevent corruption on power loss
// Set to 0 to disable if it causes issues
#define SD_GRACEFUL_UNMOUNT 1
#define SD_UNMOUNT_DELAY_MS 500

void gracefulSDShutdown() {
    #if SD_GRACEFUL_UNMOUNT
    if (sdReady) {
        Serial.println("[SD] Gracefully unmounting before shutdown...");
        SD_MMC.end();
        vTaskDelay(pdMS_TO_TICKS(SD_UNMOUNT_DELAY_MS));  // Give card time to finish writes
        Serial.println("[SD] Unmount complete");
    }
    #endif
}

void setupSD() {
    Serial.println("[SD] Mounting SD card...");
    
    // ESP32-CAM: Reduce CPU frequency for stable SD_MMC operations
    #ifndef ARDUINO_FREENOVE_ESP32_S3_WROOM
    setCpuFrequencyMhz(160);  // Reduce from 240MHz to 160MHz for SD stability
    Serial.println("[SD] CPU frequency reduced to 160MHz for SD_MMC stability");
    #endif
    
    // Configure SD_MMC pins FIRST (critical for ESP32-S3)
    #ifdef ARDUINO_FREENOVE_ESP32_S3_WROOM
    Serial.println("[SD] Configuring pins for ESP32-S3...");
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    Serial.printf("[SD] Pins set: CLK=%d CMD=%d D0=%d\n", SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    #endif
    
    // Use different SD_MMC initialization based on board
    #ifdef ARDUINO_FREENOVE_ESP32_S3_WROOM
    // ESP32-S3: use 1-bit mode with proper parameters
    Serial.println("[SD] Calling SD_MMC.begin(\"/sdcard\", true, false, 40000000)...");
    if (!SD_MMC.begin("/sdcard", true, false, 40000000)) {
      Serial.println("[SD] Card Mount Failed");
      return;
    }
    #else
    // ESP32-CAM: use defaults (no parameters) - matches working SDMMC_Test.ino
    if (!SD_MMC.begin()) {
      Serial.println("[SD] Card Mount Failed");
      return;
    }
    #endif
    Serial.println("[SD] SD_MMC.begin() returned success");
    
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] No SD card attached");
        sdReady = false;
        return;
    }
    Serial.print("SD_MMC Card Type: ");
    
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    sdReady = true;
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card mounted successfully: %llu MB, Type: %d\n", cardSize, cardType);
    
    // Ensure capture directory exists
    if (!SD_MMC.exists(SD_CAPTURE_DIR)) {
        if (SD_MMC.mkdir(SD_CAPTURE_DIR)) {
            Serial.printf("[SD] Created capture directory: %s\n", SD_CAPTURE_DIR);
        } else {
            Serial.println("[SD] Failed to create capture directory");
        }
    }
}

bool saveImageToSD(camera_fb_t* fb, const char* reason) {
    if (!sdReady || !fb) {
        return false;
    }
    
    char path[80];
    snprintf(path, sizeof(path), "%s/%lu_%s.jpg", SD_CAPTURE_DIR, millis(), reason);
    
    File file = SD_MMC.open(path, "w");
    if (!file) {
        Serial.println("[SD] Failed to open file for writing");
        return false;
    }
    
    size_t written = file.write(fb->buf, fb->len);
    file.close();
    
    if (written == fb->len) {
        Serial.printf("[SD] Saved %s: %s (%u bytes)\n", reason, path, (unsigned int)fb->len);
        return true;
    } else {
        Serial.printf("[SD] Write failed: expected %u, wrote %u\n", (unsigned int)fb->len, (unsigned int)written);
        return false;
    }
}

bool deleteOldestCaptures(int count) {
    if (!sdReady) {
        Serial.println("[SD] Cannot delete captures - SD not ready");
        return false;
    }

    // Build list of files with timestamps
    struct FileInfo {
        String path;
        time_t timestamp;
    };
    std::vector<FileInfo> files;

    File dir = SD_MMC.open(SD_CAPTURE_DIR);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[SD] Capture directory missing");
        return false;
    }

    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            FileInfo info;
            info.path = entry.path();
            info.timestamp = entry.getLastWrite();
            files.push_back(info);
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    if (files.empty()) {
        Serial.println("[SD] No captures to delete");
        return true;
    }

    // Sort by timestamp (oldest first)
    std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
        return a.timestamp < b.timestamp;
    });

    // Delete oldest N files
    int toDelete = min(count, (int)files.size());
    int deleted = 0;
    for (int i = 0; i < toDelete; i++) {
        if (SD_MMC.remove(files[i].path)) {
            deleted++;
            Serial.printf("[SD] Deleted old capture: %s\n", files[i].path.c_str());
        }
    }

    Serial.printf("[SD] Deleted %d oldest captures\n", deleted);
    return deleted > 0;
}

bool deleteAllCaptures() {
    if (!sdReady) {
        Serial.println("[SD] Cannot delete captures - SD not ready");
        return false;
    }

    File dir = SD_MMC.open(SD_CAPTURE_DIR);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[SD] Capture directory missing");
        return false;
    }

    size_t deleted = 0;
    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String path = entry.path();
            entry.close();  // Close file before deleting
            if (SD_MMC.remove(path)) {
                deleted++;
                Serial.printf("[SD] Deleted: %s\n", path.c_str());
            } else {
                Serial.printf("[SD] Failed to delete: %s\n", path.c_str());
            }
        } else {
            entry.close();
        }
        entry = dir.openNextFile();
    }
    dir.close();

    Serial.printf("[SD] Deleted %u captures\n", (unsigned int)deleted);
    return deleted > 0 || deleted == 0;  // Return true even if nothing to delete
}

// ==================== Reset Detection & Recovery ====================

void checkResetCounter() {
    // Check for triple-reset and crash loop conditions (ESP32-S3 reliable via NVS)
    // This must be called early in setup() before other initialization

    // Open preferences namespace for reset tracking
    resetPrefs.begin("reset", false);

    // ===== Crash loop detection =====
    uint32_t crashFlag = resetPrefs.getUInt("crash_flag", 0);
    uint32_t crashCnt = resetPrefs.getUInt("crash_cnt", 0);

    if (crashFlag == CRASH_LOOP_MAGIC) {
        crashCnt++;
        resetPrefs.putUInt("crash_cnt", crashCnt);
        Serial.printf("[RESET] Incomplete boot detected, crash count: %lu\n", crashCnt);

        if (crashCnt >= CRASH_LOOP_THRESHOLD) {
            Serial.println("[RESET] CRASH LOOP RECOVERY - entering config portal");
            configPortalReason = "crash_recovery";
            resetPrefs.putUInt("crash_cnt", 0); // Reset for next time
        }
    } else {
        // Fresh power-on (or flag cleared), reset crash count only.
        // Do NOT clear triple-reset counters here; they must persist across boots.
        resetPrefs.putUInt("crash_cnt", 0);
    }

    // Mark boot in progress (cleared in clearCrashLoop after successful boot)
    resetPrefs.putUInt("crash_flag", CRASH_LOOP_MAGIC);

    // ===== Triple-reset detection =====
    if (strcmp(configPortalReason, "crash_recovery") != 0) {
        uint32_t cnt = resetPrefs.getUInt("reset_cnt", 0);
        uint32_t window = resetPrefs.getUInt("window", 0);

        // Sanity check
        if (cnt > 10) {
            Serial.printf("[RESET] Reset counter corrupted (%lu), clearing\n", cnt);
            cnt = 0;
            window = 0;
            resetPrefs.putUInt("reset_cnt", cnt);
            resetPrefs.putUInt("window", window);
        }

        if (window == 1 && cnt > 0 && cnt < 10) {
            // Previous boot started a detection window
            cnt++;
            resetPrefs.putUInt("reset_cnt", cnt);
            Serial.printf("[RESET] Reset count: %lu (within window)\n", cnt);

            if (cnt >= RESET_COUNT_THRESHOLD) {
                Serial.println("[RESET] TRIPLE RESET DETECTED - entering config portal");
                configPortalReason = "triple_reset";
                // Clear for next time
                resetPrefs.putUInt("reset_cnt", 0);
                resetPrefs.putUInt("window", 0);
            }
        } else {
            // First reset or outside window
            cnt = 1;
            window = 1;
            resetPrefs.putUInt("reset_cnt", cnt);
            resetPrefs.putUInt("window", window);
            Serial.println("[RESET] First reset, starting detection window");
        }

        // Wait for window; if no reset occurs, clear markers
        delay(RESET_DETECT_TIMEOUT * 1000);

        if (strcmp(configPortalReason, "none") == 0) {
            Serial.println("[RESET] Reset window expired, normal boot");
            resetPrefs.putUInt("reset_cnt", 0);
            resetPrefs.putUInt("window", 0);
        }
    }

    Serial.printf("[RESET] Boot reason: %s\n", configPortalReason);
}

void clearCrashLoop() {
    // Called after successful boot (WiFi + camera + web server initialized)
    resetPrefs.putUInt("crash_flag", 0);
    resetPrefs.putUInt("crash_cnt", 0);
    Serial.println("[RESET] Crash loop flag cleared - boot successful");
}

// ==================== End Reset Detection & Recovery ====================

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
    
    // Always flash on motion (save current manual state)
    bool wasManualOn = flashManualOn;
    if (!wasManualOn && FLASH_PIN >= 0) {
        digitalWrite(FLASH_PIN, HIGH);
        Serial.println("[FLASH] Motion flash triggered");
    }
    
    // Publish motion event to dedicated topic
    JsonDocument motionDoc;
    motionDoc["device"] = deviceName;
    motionDoc["chip_id"] = deviceChipId;
    motionDoc["trace_id"] = Trace::getTraceId();
    motionDoc["traceparent"] = Trace::getTraceparent();
    motionDoc["seq_num"] = Trace::getSequenceNumber();
    motionDoc["timestamp"] = millis() / 1000;
    motionDoc["motion_count"] = motionDetectCount;
    motionDoc["event"] = "motion_detected";
    
    String motionOutput;
    serializeJson(motionDoc, motionOutput);
    mqttClient.publish(getTopicMotion().c_str(), motionOutput.c_str(), false);
    
    // Log to MQTT events topic
    logEventToMQTT("pir_motion", "info");
    
    // Trigger capture if camera is ready
    if (cameraReady) {
        captureAndPublish();
    }
    
    // Restore flash state after capture
    if (!wasManualOn) {
        digitalWrite(FLASH_PIN, LOW);
    }
}

void setupWiFi() {
    Serial.println("Setting up WiFi...");

    // Set WiFi mode to station (STA) only
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
    wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
    wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT / 1000);

    // Determine if we should enter config portal
    bool enterConfigPortal = false;
    
    // Check 1: Triple-reset detection (already checked in checkResetCounter, sets configPortalReason)
    if (strcmp(configPortalReason, "triple_reset") == 0) {
        enterConfigPortal = true;
    }
    
    // Check 2: Crash loop recovery (already checked in checkResetCounter, sets configPortalReason)
    if (strcmp(configPortalReason, "crash_recovery") == 0) {
        enterConfigPortal = true;
    }

    if (enterConfigPortal) {
        Serial.println();
        Serial.println("========================================");
        Serial.printf("  CONFIG PORTAL TRIGGERED: %s\n", configPortalReason);
        Serial.println("  Starting WiFi Configuration Portal");
        Serial.println("========================================");
        Serial.println();
        Serial.print("[WiFi] Connect to AP: ");
        Serial.println(apName);
        Serial.println("[WiFi] Then open http://192.168.4.1 in browser");
        Serial.printf("[WiFi] Portal timeout: %d seconds\n", CONFIG_PORTAL_TIMEOUT);
        Serial.println();

        // Start config portal (blocking)
        bool portalResult = wifiManager.startConfigPortal(apName.c_str());
        
        if (!portalResult) {
            // Portal timed out or failed - check if we have saved credentials
            if (wifiManager.getWiFiIsSaved()) {
                Serial.println("[WiFi] Portal timed out, but saved credentials exist");
                Serial.println("[WiFi] Attempting to connect with saved credentials...");
                
                // Try to connect with saved credentials
                WiFi.mode(WIFI_STA);
                WiFi.begin();
                
                unsigned long startAttempt = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
                    delay(500);
                    Serial.print(".");
                }
                Serial.println();
                
                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("[WiFi] Failed to connect with saved credentials");
                    Serial.println("[WiFi] Restarting...");
                    delay(3000);
                    ESP.restart();
                }
            } else {
                Serial.println("[WiFi] Portal timed out, no saved credentials");
                Serial.println("[WiFi] Restarting...");
                delay(3000);
                ESP.restart();
            }
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
        Serial.printf("[WiFi] (Triple-reset within %d seconds to enter config mode)\n", RESET_DETECT_TIMEOUT);

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

    Serial.println("WiFi connected!");
    Serial.print("Device name: ");
    Serial.println(deviceName);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}

void setupMQTT() {
    Serial.println("Setting up MQTT...");
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(60);        // Keep-alive ping every 60s
    mqttClient.setSocketTimeout(30);    // Socket timeout 30s
    mqttClient.setBufferSize(1024);     // Increase buffer for JSON messages

    reconnectMQTT();
}

void setupOTA() {
    // Set OTA hostname to match device name for easy identification
    String hostname = String(OTA_HOSTNAME_PREFIX) + "-" + deviceChipId;
    ArduinoOTA.setHostname(hostname.c_str());
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        Serial.println("[OTA] Starting update: " + type);

        // Set OTA in progress flag
        otaInProgress = true;

        // Log OTA start event to MQTT (before disconnecting)
        logEventToMQTT("ota_start", "info");

        // CRITICAL: Stop all services before OTA to prevent crashes
        Serial.println("[OTA] Stopping web server...");
        server.end();

        Serial.println("[OTA] Disconnecting MQTT gracefully...");
        gracefulMqttDisconnect();

        // Stop camera to free memory
        Serial.println("[OTA] Deinitializing camera...");
        if (cameraReady) {
            esp_camera_deinit();
            cameraReady = false;
        }

        Serial.println("[OTA] Services stopped, ready for update");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
        
        // Log OTA completion to MQTT
        logEventToMQTT("ota_complete", "info");
        
        // Give time for MQTT message to send
        delay(100);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned long lastLog = 0;
        unsigned long now = millis();
        // Log progress every 2 seconds to avoid flooding
        if (now - lastLog > 2000) {
            Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
            lastLog = now;
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

        // Clear OTA in progress flag on error
        otaInProgress = false;

        // Log OTA error to MQTT
        logEventToMQTT("ota_error", "error");
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready on %s.local\n", hostname.c_str());
}

void setupWebServer() {
    Serial.println("Setting up web server...");

    // Root page
    server.on("/", HTTP_GET, handleRoot);

    // Capture endpoint
    server.on("/capture", HTTP_GET, handleCapture);

    // Delete all captures on SD card
    server.on("/captures/clear", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!sdReady) {
            request->send(500, "application/json", "{\"status\":\"sd_not_ready\"}");
            return;
        }
        bool ok = deleteAllCaptures();
        if (ok) {
            request->send(200, "application/json", "{\"status\":\"cleared\"}");
        } else {
            request->send(500, "application/json", "{\"status\":\"error\"}");
        }
    });

    // Format SD card - sets flag and reboots
    server.on("/sd/format", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Check for confirmation parameter
        if (!request->hasParam("confirm", true)) {
            request->send(400, "application/json", "{\"status\":\"missing_confirmation\"}");
            return;
        }
        
        String confirm = request->getParam("confirm", true)->value();
        if (confirm != "yes") {
            request->send(400, "application/json", "{\"status\":\"not_confirmed\"}");
            return;
        }
        
        Serial.println("[SD] Format requested - rebooting...");
        
        request->send(200, "application/json", "{\"status\":\"rebooting_to_format\"}");
        
        delay(100);  // Allow response to be sent
        gracefulSDShutdown();  // Clean up SD card before reboot
        
        Serial.println("[SD] Rebooting device...");
        ESP.restart();
    });

    // SD card info endpoint
    server.on("/sd/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!sdReady) {
            request->send(503, "application/json", "{\"status\":\"sd_not_ready\"}");
            return;
        }

        JsonDocument doc;
        doc["ready"] = sdReady;
        doc["card_size_mb"] = SD_MMC.cardSize() / (1024 * 1024);
        doc["total_bytes"] = SD_MMC.totalBytes();
        doc["used_bytes"] = SD_MMC.usedBytes();
        doc["free_bytes"] = SD_MMC.totalBytes() - SD_MMC.usedBytes();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // SD card cleanup endpoint - delete oldest captures
    server.on("/sd/cleanup", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!sdReady) {
            request->send(503, "application/json", "{\"status\":\"sd_not_ready\"}");
            return;
        }

        // Default to deleting 10 oldest captures
        int count = 10;
        if (request->hasParam("count", true)) {
            count = request->getParam("count", true)->value().toInt();
        }

        bool success = deleteOldestCaptures(count);

        JsonDocument doc;
        doc["status"] = success ? "success" : "error";
        doc["deleted"] = count;

        String response;
        serializeJson(doc, response);
        request->send(success ? 200 : 500, "application/json", response);
    });

    // Device name endpoint
    server.on("/device-name", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device_name"] = deviceName;
        doc["chip_id"] = deviceChipId;
        doc["mac_address"] = deviceMac;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Stream endpoint (basic MJPEG)
    server.on("/stream", HTTP_GET, handleStream);

    // Camera control endpoint
    server.on("/control", HTTP_GET, handleControl);

    // Status endpoint
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device"] = deviceName;
        doc["device_name"] = deviceName;
        doc["chip_id"] = deviceChipId;
        doc["mac_address"] = deviceMac;
        doc["version"] = FIRMWARE_VERSION;
        doc["uptime"] = millis() / 1000;
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["free_heap"] = ESP.getFreeHeap();
        doc["psram_free"] = ESP.getFreePsram();
        doc["camera_ready"] = cameraReady;
        doc["mqtt_connected"] = mqttConnected;
        doc["motion_enabled"] = motionEnabled;
        doc["sd_ready"] = sdReady;
        
        // Board capabilities
        #if defined(CAMERA_MODEL_AI_THINKER)
            doc["has_flash_led"] = true;
        #else
            doc["has_flash_led"] = false;
        #endif
        
        if (sdReady) {
            doc["sd_size_mb"] = SD_MMC.cardSize() / (1024 * 1024);
            doc["sd_used_mb"] = SD_MMC.usedBytes() / (1024 * 1024);
        }
        
        // Reset/recovery status
        doc["boot_reason"] = configPortalReason;
        doc["crash_count"] = rtcCrashCount;

        if (cameraReady) {
            sensor_t *s = esp_camera_sensor_get();
            if (s) {
                doc["framesize"] = s->status.framesize;
                doc["quality"] = s->status.quality;
                doc["brightness"] = s->status.brightness;
                doc["contrast"] = s->status.contrast;
                doc["saturation"] = s->status.saturation;
                doc["special_effect"] = s->status.special_effect;
                doc["hmirror"] = s->status.hmirror;
                doc["vflip"] = s->status.vflip;
                doc["awb"] = s->status.awb;
                doc["aec"] = s->status.aec;
                doc["aec2"] = s->status.aec2;
                doc["aec_value"] = s->status.aec_value;
                doc["agc"] = s->status.agc;
                doc["agc_gain"] = s->status.agc_gain;
                doc["gainceiling"] = s->status.gainceiling;
                doc["awb_gain"] = s->status.awb_gain;
                doc["wb_mode"] = s->status.wb_mode;
            }
        }

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // Motion control endpoint
    server.on("/motion-control", HTTP_GET, handleMotionControl);
    server.on("/flash-control", HTTP_GET, handleFlashControl);
    
    // WiFi reset endpoint (requires secret token)
    server.on("/wifi-reset", HTTP_GET, handleWiFiReset);

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
    // Log reachable addresses depending on current mode
    wifi_mode_t mode = WiFi.getMode();
    if (mode & WIFI_MODE_STA) {
        Serial.printf("Web server (STA) on http://%s\n", WiFi.localIP().toString().c_str());
    }
    if (mode & WIFI_MODE_AP) {
        Serial.printf("Web server (AP)  on http://%s\n", WiFi.softAPIP().toString().c_str());
    }
}

void reconnectMQTT() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    Serial.printf("[MQTT] Connecting to %s:%d...", MQTT_SERVER, MQTT_PORT);

    // Shorten client ID to avoid broker limits
    String clientId = String(deviceName) + "-" + deviceChipId;
    clientId.replace(" ", "-");
    if (clientId.length() > 23) {
        clientId = clientId.substring(0, 23);
    }

    // Anonymous connect if credentials are empty, otherwise authenticate
    bool connected;
    if (strlen(MQTT_USER) == 0) {
        connected = mqttClient.connect(clientId.c_str());
    } else {
        connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
    }

    if (connected) {
        Serial.println("connected!");
        mqttConnected = true;

        // Subscribe to command topic
        mqttClient.subscribe(getTopicCommand().c_str());

        // Publish status
        publishStatus();
    } else {
        Serial.printf("failed, rc=%d\n", mqttClient.state());
        mqttConnected = false;
    }
}

void gracefulMqttDisconnect() {
    if (!mqttConnected || !mqttClient.connected()) {
        return;
    }

    Serial.println("[MQTT] Gracefully disconnecting...");

    // Attempt clean disconnect with timeout
    mqttClient.disconnect();

    unsigned long disconnectStart = millis();
    while (mqttClient.connected() && (millis() - disconnectStart) < 500) {
        yield();  // Allow TCP stack to process
    }

    mqttConnected = false;

    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Disconnected cleanly");
    } else {
        Serial.println("[MQTT] Disconnect timeout, forcing");
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
    doc["chip_id"] = deviceChipId;
    doc["trace_id"] = Trace::getTraceId();
    doc["traceparent"] = Trace::getTraceparent();
    doc["seq_num"] = Trace::getSequenceNumber();
    doc["version"] = FIRMWARE_VERSION;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime_seconds"] = uptimeSeconds;
    doc["uptime"] = String(days) + "d " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
    doc["camera_ready"] = cameraReady;
    doc["motion_enabled"] = motionEnabled;
    doc["motion_count"] = motionDetectCount;
    doc["flash_illumination"] = flashEnabled;
    doc["flash_motion"] = flashMotionEnabled;
    doc["flash_manual"] = flashManualOn;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_psram"] = ESP.getFreePsram();
    doc["capture_count"] = captureCount;
    doc["camera_errors"] = cameraErrors;
    
    // Reset/recovery status
    doc["boot_reason"] = configPortalReason;
    doc["crash_count"] = rtcCrashCount;

    String output;
    serializeJson(doc, output);

    mqttClient.publish(getTopicStatus().c_str(), output.c_str(), true);
    Serial.println("Status published to MQTT");
}

void captureAndPublish() {
    Serial.printf("[CAPTURE] Starting capture (manual=%s)...\n", 
                  flashManualOn ? "ON" : "OFF");

    // Always flash on capture (unless in manual mode)
    if (!flashManualOn && FLASH_PIN >= 0) {
        digitalWrite(FLASH_PIN, HIGH);
        Serial.println("[FLASH] LED ON for capture");
        delay(100);  // Give flash time to reach full brightness
    }

    camera_fb_t * fb = capturePhoto();
    
    // Turn off flash after capture (only if we turned it on)
    if (!flashManualOn && FLASH_PIN >= 0) {
        digitalWrite(FLASH_PIN, LOW);
        Serial.println("[FLASH] LED OFF after capture");
    }
    if (!fb) {
        Serial.println("Capture failed");
        cameraErrors++;
        logEventToMQTT("capture_failed", "error");
        return;
    }

    captureCount++;
    Serial.printf("Image captured: %d bytes\n", fb->len);
    
    // Save to SD card if available
    saveImageToSD(fb, "capture");

    // Publish image to MQTT (in chunks if needed)
    // Note: Large images may need to be published in chunks or base64 encoded
    // For now, just publish metadata
    JsonDocument doc;
    doc["device"] = deviceName;
    doc["chip_id"] = deviceChipId;
    doc["trace_id"] = Trace::getTraceId();
    doc["traceparent"] = Trace::getTraceparent();
    doc["seq_num"] = Trace::getSequenceNumber();
    doc["timestamp"] = millis();
    doc["size"] = fb->len;
    doc["width"] = fb->width;
    doc["height"] = fb->height;
    doc["format"] = "JPEG";

    String output;
    serializeJson(doc, output);

    if (mqttClient.publish(getTopicImage().c_str(), output.c_str())) {
        mqttPublishCount++;
    }

    // TODO: Implement actual image transfer (HTTP POST, FTP, or chunked MQTT)

    returnFrameBuffer(fb);
}

void captureAndPublishWithImage() {
    Serial.printf("[CAPTURE] Starting image capture with base64 (manual=%s)...\n",
                  flashManualOn ? "ON" : "OFF");

    // Always flash on capture (unless in manual mode)
    if (!flashManualOn && FLASH_PIN >= 0) {
        digitalWrite(FLASH_PIN, HIGH);
        Serial.println("[FLASH] LED ON for image capture");
        delay(100);  // Give flash time to reach full brightness
    }

    camera_fb_t * fb = capturePhoto();
    
    // Turn off flash after capture (only if we turned it on)
    if (!flashManualOn && FLASH_PIN >= 0) {
        digitalWrite(FLASH_PIN, LOW);
        Serial.println("[FLASH] LED OFF after image capture");
    }
    if (!fb) {
        Serial.println("Capture failed");
        cameraErrors++;
        logEventToMQTT("capture_failed", "error");
        return;
    }

    captureCount++;
    Serial.printf("Image captured: %d bytes\n", fb->len);
    
    // Save to SD card if available
    saveImageToSD(fb, "full");

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
    // Ignore MQTT commands during OTA to prevent conflicts
    if (otaInProgress) {
        Serial.println("[MQTT] Ignoring command during OTA");
        return;
    }

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
            mqttClient.publish(getTopicStatus().c_str(), "{\"status\":\"rebooting\"}", false);
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
    // Try to serve gzipped HTML from LittleFS (6.7KB vs 27KB uncompressed)
    if (littleFsReady && LittleFS.exists("/index.html.gz")) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html.gz", "text/html");
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
        return;
    }

    // Fallback: embedded HTML if LittleFS file missing
    String html = R"=====(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="cache-control" content="no-cache, no-store, must-revalidate">
<meta http-equiv="pragma" content="no-cache">
<meta http-equiv="expires" content="0">
<title>ESP32-CAM Surveillance</title>
<style>
:root{
  --primary-bg:#000000;
  --secondary-bg:#1a1a1a;
  --glass-bg:rgba(26,26,26,0.8);
  --glass-border:rgba(255,255,255,0.1);
  --accent-cyan:#00d9ff;
  --accent-blue:#007aff;
  --text-primary:#ffffff;
  --text-secondary:#a0a0a0;
  --text-muted:#666666;
  --success:#34c759;
  --warning:#ff9500;
  --error:#ff3b30;
  --radius-lg:16px;
  --radius-md:12px;
  --radius-sm:8px;
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:var(--primary-bg);color:var(--text-primary);font-size:14px;height:100vh;overflow:hidden;display:flex;flex-direction:column}
.app-container{display:flex;flex-direction:column;height:100vh;overflow:hidden;position:relative;width:100%}

/* ===== HEADER ===== */
.header{display:flex;align-items:center;justify-content:space-between;padding:12px 24px;background:var(--primary-bg);z-index:10;border-bottom:1px solid var(--glass-border)}
.header-left{display:flex;flex-direction:column;gap:2px}
.camera-name{font-size:18px;font-weight:600;display:flex;align-items:center;gap:8px}
.camera-name::before{content:'●';color:var(--success);font-size:10px;margin-right:4px}
.bitrate{font-size:12px;color:var(--text-secondary);font-family:monospace;opacity:0.8}
.header-right{display:flex;gap:20px;align-items:center}
.header-icon{width:24px;height:24px;fill:var(--text-primary);cursor:pointer;opacity:0.7;transition:all 0.2s}
.header-icon:hover{opacity:1;transform:scale(1.1)}

/* ===== VIDEO SECTION ===== */
.video-main{flex:1;display:flex;align-items:center;justify-content:center;background:#080808;position:relative;overflow:hidden;padding:20px}
.video-container{width:100%;max-width:1000px;aspect-ratio:16/9;background:#000;position:relative;overflow:hidden;display:flex;align-items:center;justify-content:center;border-radius:var(--radius-lg);box-shadow:0 20px 50px rgba(0,0,0,0.5);border:1px solid var(--glass-border)}
.video-container img{width:100%;height:100%;object-fit:contain;display:none}
.video-container img.active{display:block}
.video-placeholder{position:absolute;display:flex;flex-direction:column;align-items:center;gap:16px;color:var(--text-muted)}
.video-placeholder svg{width:64px;height:64px;fill:var(--text-muted);opacity:0.5}
.video-overlay-top{position:absolute;top:12px;left:16px;font-family:monospace;font-size:14px;color:#fff;text-shadow:1px 1px 2px #000;pointer-events:none;background:rgba(0,0,0,0.3);padding:4px 8px;border-radius:4px}

/* ===== VIDEO CONTROLS ===== */
.video-controls{display:flex;align-items:center;justify-content:center;gap:32px;padding:20px;background:var(--primary-bg);border-top:1px solid var(--glass-border)}
.control-btn{width:48px;height:48px;display:flex;align-items:center;justify-content:center;cursor:pointer;border-radius:50%;transition:all 0.2s;background:var(--secondary-bg)}
.control-btn:hover{background:#333;transform:translateY(-2px)}
.control-btn:active{transform:translateY(0)}
.control-btn svg{width:24px;height:24px;fill:var(--text-primary)}
.control-btn.active svg{fill:var(--accent-cyan)}
.quality-badge{font-size:11px;font-weight:bold;border:1.5px solid var(--text-primary);padding:2px 6px;border-radius:6px;text-transform:uppercase;letter-spacing:0.5px}

/* ===== SETTINGS PANEL (DRAWER) ===== */
.settings-drawer{position:fixed;top:0;right:-100%;width:100%;max-width:500px;height:100%;background:var(--primary-bg);z-index:100;transition:right 0.3s cubic-bezier(0.4, 0, 0.2, 1);display:flex;flex-direction:column;box-shadow:-8px 0 32px rgba(0,0,0,0.7)}
.settings-drawer.active{right:0}
.settings-header{display:flex;align-items:center;padding:20px 24px;border-bottom:1px solid var(--glass-border)}
.settings-title{flex:1;text-align:center;font-weight:600;font-size:18px}
.close-settings{cursor:pointer;padding:8px;border-radius:50%;transition:background 0.2s}
.close-settings:hover{background:var(--secondary-bg)}

.settings-content{flex:1;overflow-y:auto;padding:24px}
.settings-section{margin-bottom:32px}
.settings-label{font-size:12px;font-weight:700;color:var(--text-muted);text-transform:uppercase;margin-bottom:16px;display:block;letter-spacing:1px}
.settings-row{display:flex;align-items:center;justify-content:space-between;padding:16px 0;border-bottom:1px solid rgba(255,255,255,0.05)}
.settings-row:last-child{border-bottom:none}

/* ===== UI ELEMENTS ===== */
select, input[type=text]{background:var(--secondary-bg);color:#fff;border:1px solid var(--glass-border);padding:8px 12px;border-radius:var(--radius-sm);font-size:14px;width:100%}
.switch{position:relative;width:44px;height:24px}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#333;transition:.3s;border-radius:24px}
.slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background-color:white;transition:.3s;border-radius:50%}
input:checked + .slider{background-color:var(--accent-blue)}
input:checked + .slider:before{transform:translateX(20px)}

.btn-primary{background:var(--accent-blue);color:#fff;border:none;padding:12px;border-radius:var(--radius-md);font-weight:600;cursor:pointer;width:100%;margin-top:12px}
.btn-danger{background:var(--error);color:#fff;border:none;padding:12px;border-radius:var(--radius-md);font-weight:600;cursor:pointer;width:100%;margin-top:12px}

/* ===== UTILS ===== */
.hidden{display:none !important}
.backdrop{position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:90;display:none}
.backdrop.active{display:block}

@media (min-width: 768px) {
  .app-container { max-width: 100%; }
  .video-main { padding: 40px; }
}
</style>
</head>
<body>
<div class="app-container">
  <!-- HEADER -->
  <header class="header">
    <div class="header-left">
      <div class="camera-name" id="camera-name-display">ESP32-CAM</div>
      <div class="bitrate">
        <span id="ip-display" style="color:var(--accent-cyan);margin-right:12px">--</span>
        <span id="motion-status" style="margin-right:12px">●</span>
        <span id="sd-status" style="margin-right:12px;color:var(--text-secondary)">💾 --</span>
        <span id="sftp-status" style="margin-right:12px;color:var(--text-secondary)">📤 --</span>
        <span id="bitrate-display">Ready</span>
      </div>
    </div>
    <div class="header-right">
      <svg class="header-icon" id="open-settings" viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
    </div>
  </header>

  <!-- VIDEO SECTION -->
  <div class="video-main">
    <div class="video-container" id="video-container">
      <div class="video-placeholder" id="video-placeholder">
        <svg viewBox="0 0 24 24"><path d="M9 2L7.17 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V6c0-1.1-.9-2-2-2h-3.17L15 2H9zm3 15c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5z"/><circle cx="12" cy="12" r="3.2"/></svg>
        <span>Click Play to start streaming</span>
      </div>
      <div class="video-overlay-top hidden" id="timestamp-overlay"></div>
      <img id="stream" src="" alt="Live Stream">
    </div>
  </div>

  <!-- VIDEO CONTROLS -->
  <div class="video-controls">
    <div class="control-btn" id="toggle-stream" title="Play/Pause Stream">
      <svg viewBox="0 0 24 24" id="play-icon"><path d="M8 5v14l11-7z"/></svg>
      <svg viewBox="0 0 24 24" id="pause-icon" class="hidden"><path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/></svg>
    </div>
    <div class="control-btn" id="get-still" title="Capture Still Image">
      <svg viewBox="0 0 24 24"><path d="M9 2L7.17 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V6c0-1.1-.9-2-2-2h-3.17L15 2H9zm3 15c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5z"/><circle cx="12" cy="12" r="3.2"/></svg>
    </div>
    <div class="control-btn" title="Quality">
      <span class="quality-badge" id="quality-badge">VGA</span>
    </div>
    <div class="control-btn" id="fullscreen-btn" title="Fullscreen">
      <svg viewBox="0 0 24 24"><path d="M7 14H5v5h5v-2H7v-3zm-2-4h2V7h3V5H5v5zm12 7h-3v2h5v-5h-2v3zM14 5v2h3v3h2V5h-5z"/></svg>
    </div>
  </div>

  <!-- SETTINGS DRAWER -->
  <div class="settings-drawer" id="settings-drawer">
    <div class="settings-header">
      <div class="close-settings" id="close-settings">
        <svg viewBox="0 0 24 24" width="24" height="24" fill="white"><path d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/></svg>
      </div>
      <div class="settings-title">Settings</div>
      <div style="width:24px"></div>
    </div>
    <div class="settings-content">
      <div class="settings-section">
        <span class="settings-label">Device</span>
        <div class="settings-row">
          <span>Name</span>
          <input type="text" id="device-name-input" style="width:150px">
        </div>
        <button class="btn-primary" id="save-device-name">Save Name</button>
      </div>

      <div class="settings-section">
        <span class="settings-label">Camera</span>
        <div class="settings-row">
          <span>Resolution</span>
          <select id="framesize" class="default-action" style="width:150px">
            <option value="8">VGA (640x480)</option>
            <option value="9">SVGA (800x600)</option>
            <option value="10">XGA (1024x768)</option>
            <option value="11">HD (1280x720)</option>
          </select>
        </div>
        <div class="settings-row">
          <span>Quality</span>
          <input type="range" id="quality" min="10" max="63" value="12" class="default-action" style="width:150px">
        </div>
        <div class="settings-row">
          <span>Motion Detection</span>
          <label class="switch">
            <input type="checkbox" id="motion_enabled" class="default-action">
            <span class="slider"></span>
          </label>
        </div>
        <div class="settings-row">
          <span>SFTP Upload</span>
          <label class="switch">
            <input type="checkbox" id="sftp_enabled">
            <span class="slider"></span>
          </label>
        </div>
        <div class="settings-row">
          <span>Flashlight</span>
          <label class="switch">
            <input type="checkbox" id="flash_manual">
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <div class="settings-section">
        <span class="settings-label">Image</span>
        <div class="settings-row">
          <span>Brightness</span>
          <input type="range" id="brightness" min="-2" max="2" value="0" class="default-action" style="width:150px">
        </div>
        <div class="settings-row">
          <span>V-Flip</span>
          <label class="switch">
            <input type="checkbox" id="vflip" class="default-action">
            <span class="slider"></span>
          </label>
        </div>
        <div class="settings-row">
          <span>H-Mirror</span>
          <label class="switch">
            <input type="checkbox" id="hmirror" class="default-action">
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <div class="settings-section">
        <span class="settings-label">System</span>
        <div class="settings-row">
          <span>IP Address</span>
          <span id="ip-value" style="color:var(--accent-cyan);font-family:monospace">--</span>
        </div>
        <div class="settings-row">
          <span>SD Card</span>
          <span id="sd-info">--</span>
        </div>
        <button class="btn-primary" id="reboot-btn" style="background:var(--accent-blue);margin-bottom:12px">Reboot Device</button>
        <button class="btn-danger" id="reset-sensor">Reset Camera Settings</button>
      </div>
    </div>
  </div>

  <div class="backdrop" id="backdrop"></div>
</div>
<script>
document.addEventListener('DOMContentLoaded', function() {
  const baseHost = document.location.origin;
  const streamUrl = baseHost;
  const view = document.getElementById('stream');
  const toggleStreamBtn = document.getElementById('toggle-stream');
  const playIcon = document.getElementById('play-icon');
  const pauseIcon = document.getElementById('pause-icon');
  const getStillBtn = document.getElementById('get-still');
  const fullscreenBtn = document.getElementById('fullscreen-btn');
  const videoContainer = document.getElementById('video-container');
  const openSettingsBtn = document.getElementById('open-settings');
  const closeSettingsBtn = document.getElementById('close-settings');
  const settingsDrawer = document.getElementById('settings-drawer');
  const backdrop = document.getElementById('backdrop');
  const bitrateDisplay = document.getElementById('bitrate-display');
  const qualityBadge = document.getElementById('quality-badge');
  const timestampOverlay = document.getElementById('timestamp-overlay');
  const videoPlaceholder = document.getElementById('video-placeholder');
  const cameraNameDisplay = document.getElementById('camera-name-display');
  const ipDisplay = document.getElementById('ip-display');
  const motionStatus = document.getElementById('motion-status');
  const sdStatus = document.getElementById('sd-status');
  const sftpStatus = document.getElementById('sftp-status');
  const deviceNameInput = document.getElementById('device-name-input');
  const saveDeviceNameBtn = document.getElementById('save-device-name');
  const ipValue = document.getElementById('ip-value');
  const sdInfo = document.getElementById('sd-info');
  const rebootBtn = document.getElementById('reboot-btn');
  const resetBtn = document.getElementById('reset-sensor');

  let isStreaming = false;
  let lastFrameTime = Date.now();
  let bitrateInterval;

  const toggleSettings = (show) => {
    if (show) {
      settingsDrawer.classList.add('active');
      backdrop.classList.add('active');
    } else {
      settingsDrawer.classList.remove('active');
      backdrop.classList.remove('active');
    }
  };

  const updateBitrate = () => {
    const now = Date.now();
    const delta = (now - lastFrameTime) / 1000;
    if (delta >= 1 && bitrateDisplay) {
      const mockBitrate = (Math.random() * 500 + 1200).toFixed(2);
      bitrateDisplay.textContent = `${mockBitrate} kbps`;
      lastFrameTime = now;
    }
  };

  const updateTimestamp = () => {
    if (!isStreaming || !timestampOverlay) return;
    const now = new Date();
    const year = now.getFullYear();
    const month = String(now.getMonth() + 1).padStart(2, '0');
    const day = String(now.getDate()).padStart(2, '0');
    const hours = String(now.getHours()).padStart(2, '0');
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');
    const ampm = now.getHours() >= 12 ? 'pm' : 'am';
    const days = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'];
    const dayName = days[now.getDay()];
    timestampOverlay.textContent = `${year}/${month}/${day} ${hours}:${minutes}:${seconds} ${ampm} ${dayName}`;
  };

  setInterval(updateTimestamp, 1000);

  view.addEventListener('load', () => {
    if (isStreaming) {
      view.classList.add('active');
      videoPlaceholder.classList.add('hidden');
      timestampOverlay.classList.remove('hidden');
    }
  });

  view.addEventListener('error', () => {
    view.classList.remove('active');
    if (isStreaming) {
      videoPlaceholder.classList.remove('hidden');
    }
  });

  const stopStream = () => {
    view.src = '';
    if (view) view.classList.remove('active');
    isStreaming = false;
    if (playIcon) playIcon.classList.remove('hidden');
    if (pauseIcon) pauseIcon.classList.add('hidden');
    clearInterval(bitrateInterval);
    if (bitrateDisplay) bitrateDisplay.textContent = 'Ready';
    if (videoPlaceholder) videoPlaceholder.classList.remove('hidden');
    if (timestampOverlay) timestampOverlay.classList.add('hidden');
  };

  const startStream = () => {
    view.src = `${streamUrl}/stream`;
    isStreaming = true;
    if (playIcon) playIcon.classList.add('hidden');
    if (pauseIcon) pauseIcon.classList.remove('hidden');
    lastFrameTime = Date.now();
    bitrateInterval = setInterval(updateBitrate, 1000);
  };

  if (toggleStreamBtn) {
    toggleStreamBtn.onclick = () => {
      if (isStreaming) stopStream();
      else startStream();
    };
  }

  if (getStillBtn) {
    getStillBtn.onclick = () => {
      isStreaming = false;
      if (playIcon) playIcon.classList.remove('hidden');
      if (pauseIcon) pauseIcon.classList.add('hidden');
      clearInterval(bitrateInterval);
      if (bitrateDisplay) bitrateDisplay.textContent = 'Ready';
      if (timestampOverlay) timestampOverlay.classList.add('hidden');
      
      view.src = `${baseHost}/capture?_cb=${Date.now()}`;
      view.classList.add('active');
      if (videoPlaceholder) videoPlaceholder.classList.add('hidden');
    };
  }

  fullscreenBtn.onclick = () => {
    if (videoContainer.requestFullscreen) {
      videoContainer.requestFullscreen();
    } else if (videoContainer.webkitRequestFullscreen) {
      videoContainer.webkitRequestFullscreen();
    } else if (videoContainer.msRequestFullscreen) {
      videoContainer.msRequestFullscreen();
    }
  };

  openSettingsBtn.onclick = () => toggleSettings(true);
  closeSettingsBtn.onclick = () => toggleSettings(false);
  backdrop.onclick = () => toggleSettings(false);

  document.querySelectorAll('.default-action').forEach(el => {
    el.onchange = () => {
      let value = el.type === 'checkbox' ? (el.checked ? 1 : 0) : el.value;
      let url = `${baseHost}/control?var=${el.id}&val=${value}`;
      if (el.id === 'motion_enabled') {
        url = `${baseHost}/motion-control?enabled=${value}`;
      }
      fetch(url).then(() => {
        if (el.id === 'framesize') {
          const sizes = { '8': 'VGA', '9': 'SVGA', '10': 'XGA', '11': 'HD' };
          qualityBadge.textContent = sizes[value] || 'Custom';
        }
      });
    };
  });

  document.getElementById('flash_manual').onchange = (e) => {
    fetch(`${baseHost}/flash-control?manual=${e.target.checked ? 1 : 0}`);
  };

  document.getElementById('sftp_enabled').onchange = (e) => {
    fetch(`${baseHost}/sftp-control?enabled=${e.target.checked ? 1 : 0}`)
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          console.log('SFTP ' + (e.target.checked ? 'enabled' : 'disabled'));
          loadStatus(); // Refresh status display
        }
      });
  };

  saveDeviceNameBtn.onclick = () => {
    const name = deviceNameInput.value.trim();
    if (!name) return;
    fetch(`${baseHost}/device-name?name=${encodeURIComponent(name)}`)
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          cameraNameDisplay.textContent = name;
          alert('Name saved!');
        }
      });
  };

  if (rebootBtn) {
    rebootBtn.onclick = () => {
      if (confirm('Reboot the device? You will need to reconnect.')) {
        fetch(`${baseHost}/control?var=reboot&val=1`).then(() => {
          alert('Rebooting device...');
          setTimeout(() => location.reload(), 5000);
        });
      }
    };
  }

  resetBtn.onclick = () => {
    if (confirm('Reset camera settings to defaults?')) {
      fetch(`${baseHost}/control?var=reset&val=1`).then(() => {
        alert('Camera settings reset. Reloading...');
        setTimeout(() => location.reload(), 1000);
      });
    }
  };

  const loadStatus = () => {
    fetch(`${baseHost}/status`)
      .then(r => r.json())
      .then(state => {
        document.querySelectorAll('.default-action, #flash_manual').forEach(el => {
          if (state.hasOwnProperty(el.id)) {
            if (el.type === 'checkbox') el.checked = !!state[el.id];
            else el.value = state[el.id];
          }
        });
        if (state.device_name) {
          cameraNameDisplay.textContent = state.device_name;
          deviceNameInput.value = state.device_name;
        }
        const ip = state.ip || location.hostname;
        ipValue.textContent = ip;
        ipDisplay.textContent = ip;
        const motionEnabled = !!state.motion_enabled;
        motionStatus.textContent = motionEnabled ? '● Motion' : '○ Motion';
        motionStatus.style.color = motionEnabled ? 'var(--success)' : 'var(--text-muted)';
        
        if (state.sd_ready) {
          const used = (state.sd_used_mb || 0).toFixed(1);
          const total = (state.sd_size_mb || 0).toFixed(1);
          sdStatus.textContent = `💾 ${used}/${total}MB`;
          sdStatus.style.color = 'var(--accent-cyan)';
        } else {
          sdStatus.textContent = '💾 None';
          sdStatus.style.color = 'var(--text-muted)';
        }
        
        // Update SFTP status
        if (state.sftp_enabled) {
          const success = state.sftp_success_count || 0;
          const fail = state.sftp_fail_count || 0;
          const fallback = state.sftp_fallback_count || 0;
          sftpStatus.textContent = `📤 ✓${success}/✗${fail}/⇓${fallback}`;
          sftpStatus.style.color = fail > 0 ? 'var(--error)' : 'var(--success)';
        } else {
          sftpStatus.textContent = '📤 Off';
          sftpStatus.style.color = 'var(--text-muted)';
        }
        
        const sizes = { '8': 'VGA', '9': 'SVGA', '10': 'XGA', '11': 'HD' };
        qualityBadge.textContent = sizes[state.framesize] || 'Custom';
        if (state.sd_ready) {
          const used = (state.sd_used_mb || 0).toFixed(1);
          const total = (state.sd_size_mb || 0).toFixed(1);
          sdInfo.textContent = `${used}MB / ${total}MB`;
        } else {
          sdInfo.textContent = 'No SD Card';
        }
      });
  };

  loadStatus();
  setInterval(loadStatus, 10000);
});
</script>
</body>
</html>
)=====";
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
}

void handleCapture(AsyncWebServerRequest *request) {
    Serial.printf("[Capture] Starting capture, flashlight=%s\n", 
                  flashManualOn ? "ON" : "OFF");
    
    // Always flash on capture (save current manual state)
    bool wasManualOn = flashManualOn;
    if (!wasManualOn && FLASH_PIN >= 0) {
        digitalWrite(FLASH_PIN, HIGH);
        delay(100);  // Allow flash to reach brightness
    }

    camera_fb_t * fb = capturePhoto();
    
    // Restore flash state after capture
    if (!wasManualOn && FLASH_PIN >= 0) {
        digitalWrite(FLASH_PIN, LOW);
    }
    
    if (!fb) {
        cameraErrors++;
        request->send(500, "text/plain", "Camera capture failed");
        return;
    }

    captureCount++;

    // Copy the frame so we can safely return the original buffer immediately
    uint8_t *copyBuf = (uint8_t*)malloc(fb->len);
    size_t copyLen = fb->len;
    if (!copyBuf) {
        cameraErrors++;
        request->send(500, "text/plain", "Memory allocation failed");
        returnFrameBuffer(fb);
        return;
    }
    memcpy(copyBuf, fb->buf, copyLen);

    // Return the frame buffer to the camera driver quickly to avoid corruption
    returnFrameBuffer(fb);

    // Save to SD card if available
    bool sdSaved = false;
    if (sdReady) {
        char path[64];
        snprintf(path, sizeof(path), "%s/%lu_%lu.jpg", SD_CAPTURE_DIR, millis(), captureCount);
        File file = SD_MMC.open(path, "w");
        if (file) {
            file.write(copyBuf, copyLen);
            file.close();
            Serial.printf("[SD] Saved capture: %s (%u bytes)\n", path, (unsigned int)copyLen);
            sdSaved = true;
        } else {
            Serial.println("[SD] Failed to write capture");
        }
    }

    // LED remains in manual state (not controlled by capture)

    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/jpeg", copyBuf, copyLen);
    response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("X-SD-Saved", sdSaved ? "true" : "false");
    request->onDisconnect([copyBuf]() {
        free(copyBuf);
    });
    request->send(response);
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
    } else if (var == "reset") {
        resetCameraSettings();
        res = 0;
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

void publishMetricsToMQTT() {
    if (WiFi.status() != WL_CONNECTED || !mqttConnected) {
        return;
    }

    JsonDocument doc;
    doc["device"] = deviceName;
    doc["chip_id"] = deviceChipId;
    doc["trace_id"] = Trace::getTraceId();
    doc["traceparent"] = Trace::getTraceparent();
    doc["seq_num"] = Trace::getSequenceNumber();
    doc["schema_version"] = 1;
    doc["location"] = "surveillance";
    doc["timestamp"] = millis() / 1000;
    doc["uptime"] = millis() / 1000;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_psram"] = ESP.getFreePsram();
    doc["camera_ready"] = cameraReady ? 1 : 0;
    doc["mqtt_connected"] = mqttConnected ? 1 : 0;
    doc["capture_count"] = captureCount;
    doc["camera_errors"] = cameraErrors;
    doc["mqtt_publishes"] = mqttPublishCount;

    String output;
    serializeJson(doc, output);

    if (!mqttClient.publish(getTopicMetrics().c_str(), output.c_str(), true)) {
        Serial.println("Failed to publish metrics to MQTT");
    }
}

void logEventToMQTT(const char* event, const char* severity) {
    if (WiFi.status() != WL_CONNECTED || !mqttConnected) {
        return;
    }

    JsonDocument doc;
    doc["device"] = deviceName;
    doc["chip_id"] = deviceChipId;
    doc["trace_id"] = Trace::getTraceId();
    doc["traceparent"] = Trace::getTraceparent();
    doc["seq_num"] = Trace::getSequenceNumber();
    doc["schema_version"] = 1;
    doc["location"] = "surveillance";
    doc["timestamp"] = millis() / 1000;
    doc["event"] = event;
    doc["severity"] = severity;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();

    String output;
    serializeJson(doc, output);

    if (!mqttClient.publish(getTopicEvents().c_str(), output.c_str(), false)) {
        Serial.println("Failed to publish event to MQTT");
    }
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

void handleFlashControl(AsyncWebServerRequest *request) {
    if (!request->hasParam("manual")) {
        request->send(400, "text/plain", "Missing 'manual' parameter");
        return;
    }

    String manualParam = request->getParam("manual")->value();
    
    bool newManual = (manualParam == "1" || manualParam.equalsIgnoreCase("true"));
    
    flashManualOn = newManual;
    
    // Re-initialize GPIO to ensure it's in OUTPUT mode
    if (FLASH_PIN >= 0) {
        pinMode(FLASH_PIN, OUTPUT);
        digitalWrite(FLASH_PIN, flashManualOn ? HIGH : LOW);
    }
    
    Serial.printf("[FLASH] Manual flashlight=%s (GPIO%d)\n",
                  flashManualOn ? "ON" : "OFF", FLASH_PIN);
    
    JsonDocument doc;
    doc["flash_manual"] = flashManualOn;
    doc["flash_pin"] = FLASH_PIN;
    doc["status"] = "ok";
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void handleWiFiReset(AsyncWebServerRequest *request) {
    // Requires secret token for security
    if (!request->hasParam("token")) {
        request->send(401, "application/json", "{\"error\":\"Missing token parameter\"}");
        return;
    }

    String token = request->getParam("token")->value();
    
    #ifdef WIFI_RESET_TOKEN
    if (token != WIFI_RESET_TOKEN) {
        Serial.println("[WiFi Reset] Invalid token attempt");
        request->send(403, "application/json", "{\"error\":\"Invalid token\"}");
        return;
    }
    #else
    // No token defined - reject all requests
    request->send(503, "application/json", "{\"error\":\"WiFi reset not configured - add WIFI_RESET_TOKEN to secrets.h\"}");
    return;
    #endif

    Serial.println("[WiFi Reset] Valid token received, clearing credentials...");
    
    // Log event before reset
    logEventToMQTT("wifi_reset_requested", "warning");
    
    // Clear saved WiFi credentials
    wifiManager.resetSettings();
    
    // Set flag to enter config portal on next boot
    configPortalReason = "wifi_reset";
    
    JsonDocument doc;
    doc["status"] = "ok";
    doc["message"] = "WiFi credentials cleared. Device will restart into config portal.";
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
    
    // Restart after brief delay
    delay(1000);
    ESP.restart();
}