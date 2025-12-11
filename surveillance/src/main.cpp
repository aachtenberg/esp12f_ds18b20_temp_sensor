#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <base64.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <esp_system.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <img_converters.h>
#include "camera_config.h"
#include "device_config.h"
#include "secrets.h"

// RTC memory for reset detection and crash loop recovery
// These survive software resets but are cleared on power loss
RTC_NOINIT_ATTR uint32_t rtcResetCount;      // Counts rapid resets
RTC_NOINIT_ATTR uint32_t rtcResetTimestamp;  // Timestamp of last reset
RTC_NOINIT_ATTR uint32_t rtcCrashLoopFlag;   // Magic number if boot incomplete
RTC_NOINIT_ATTR uint32_t rtcCrashCount;      // Consecutive crash count

// Boot/recovery state
const char* configPortalReason = "none";     // Why portal was triggered
bool fallbackAPActive = false;                // Fallback AP is running
unsigned long wifiDisconnectTime = 0;         // When WiFi first disconnected

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

// SD card capture storage
bool sdReady = false;
const char* SD_CAPTURE_DIR = "/captures";

// Global objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(WEB_SERVER_PORT);
WiFiManager wifiManager;

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
bool deleteAllCaptures();
void setupCamera();
void setupMQTT();
void setupWebServer();
void reconnectMQTT();
void publishStatus();
void captureAndPublish();
void captureAndPublishWithImage();
void publishMetricsToMQTT();
void logEventToMQTT(const char* event, const char* severity);

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
void checkWiFiFallback();

void setup() {
    Serial.begin(115200);
    delay(2000);  // Extra delay to stabilize serial

    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.printf("%s v%s\n", DEVICE_NAME, FIRMWARE_VERSION);
    Serial.println("========================================");
    Serial.println("[SETUP] Starting initialization...");

    // Check reset counter and crash loop FIRST (before anything else)
    checkResetCounter();

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
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);
    Serial.printf("[SETUP] Flash LED initialized on GPIO%d (capture flash=%s)\n", FLASH_PIN, flashEnabled ? "ON" : "OFF");

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

    // Check for WiFi fallback AP mode
    checkWiFiFallback();

    // Handle WiFi reconnection
    if (WiFi.status() != WL_CONNECTED) {
        if (currentMillis - lastWiFiCheck >= WIFI_RECONNECT_INTERVAL) {
            Serial.println("WiFi disconnected, attempting reconnection...");
            WiFi.reconnect();
            lastWiFiCheck = currentMillis;
        }
    } else {
        // WiFi reconnected - stop fallback AP if running
        if (fallbackAPActive) {
            Serial.println("[WiFi] STA reconnected, stopping fallback AP");
            WiFi.softAPdisconnect(true);
            fallbackAPActive = false;
            wifiDisconnectTime = 0;
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
                if (flashMotionEnabled && !flashManualOn) {
                    digitalWrite(FLASH_PIN, HIGH);
                    flashOffTime = currentMillis + FLASH_PULSE_MS;
                    Serial.printf("[FLASH] Motion indicator triggered for %d ms\n", FLASH_PULSE_MS);
                }
            }
            lastMotionCheck = currentMillis;
        }
    }

    // Turn off flash LED after motion pulse duration (only if not in manual mode)
    if (!flashManualOn && flashOffTime > 0 && currentMillis >= flashOffTime) {
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

    // Debug: Always log the change analysis
    float changePercent = (float)changedPixels / totalPixels * 100.0;
    Serial.printf("[Motion] Analysis: %d/%d pixels changed (%.1f%%) - threshold: %d pixels\n",
                  changedPixels, totalPixels, changePercent, MOTION_CHANGED_BLOCKS);

    // Determine if motion detected
    if (changedPixels >= MOTION_CHANGED_BLOCKS) {
        motionDetected = true;
        Serial.printf("[Motion] *** MOTION DETECTED *** Count: %lu\n", motionDetectCount + 1);
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
    Serial.println("[SD] Mounting SD card with low-level sdmmc API...");
    
    #if SD_GRACEFUL_UNMOUNT
    // Always unmount any previous mount first (ignore errors)
    SD_MMC.end();
    vTaskDelay(pdMS_TO_TICKS(100));
    #endif
    
    // Low-level sdmmc configuration (fixes 98% of SD card issues)
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = 19000;              // Stay under 20 MHz for compatibility
    host.flags = SDMMC_HOST_FLAG_1BIT;      // 1-bit mode (CLK, CMD, D0 only)
    host.command_timeout_ms = 1000;         // Critical: prevents timeouts
    
    // Initialize host
    esp_err_t ret = sdmmc_host_init();
    if (ret != ESP_OK) {
        Serial.printf("[SD] Host init failed: 0x%x\n", ret);
        sdReady = false;
        return;
    }
    
    // Force 400 kHz clock during card initialization
    sdmmc_host_set_card_clk(host.slot, 400);
    
    // Initialize card
    sdmmc_card_t card;
    ret = sdmmc_card_init(&host, &card);
    if (ret != ESP_OK) {
        Serial.printf("[SD] Card init failed: 0x%x\n", ret);
        sdmmc_host_deinit();
        sdReady = false;
        return;
    }
    
    // Switch back to full speed (19 MHz)
    sdmmc_host_set_card_clk(host.slot, 19000);
    
    // Print card info
    Serial.printf("[SD] Card initialized: %d MB, speed=%d kHz\n",
        card.csd.capacity / (1024 * 2),
        host.max_freq_khz);
    
    // Now mount filesystem on top of initialized card
    if (SD_MMC.begin("/sdcard", true, false, 19000000)) {
        sdReady = true;
        Serial.println("[SD] Filesystem mounted successfully");
    } else {
        Serial.println("[SD] Filesystem mount failed");
        sdmmc_host_deinit();
        sdReady = false;
    }
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
    // Check for triple-reset and crash loop conditions
    // This must be called early in setup() before other initialization
    
    uint32_t currentTime = millis();
    
    // Check for crash loop first (5 consecutive incomplete boots)
    if (rtcCrashLoopFlag == CRASH_LOOP_MAGIC) {
        rtcCrashCount++;
        Serial.printf("[RESET] Incomplete boot detected, crash count: %lu\n", rtcCrashCount);
        
        if (rtcCrashCount >= CRASH_LOOP_THRESHOLD) {
            Serial.println("[RESET] CRASH LOOP RECOVERY - entering config portal");
            configPortalReason = "crash_recovery";
            rtcCrashCount = 0;  // Reset for next time
        }
    } else {
        // Fresh power-on (or RTC cleared), reset crash counter
        rtcCrashCount = 0;
        rtcResetCount = 0;      // Also clear reset count on power-on
        rtcResetTimestamp = 0;
    }
    
    // Set crash loop flag (cleared in clearCrashLoop after successful boot)
    rtcCrashLoopFlag = CRASH_LOOP_MAGIC;
    
    // Check for triple-reset (3 resets within 2 seconds)
    // Only if not already in crash recovery mode
    if (strcmp(configPortalReason, "crash_recovery") != 0) {
        // Sanity check: reset counter should be reasonable (0-10)
        if (rtcResetCount > 10) {
            Serial.printf("[RESET] Reset counter corrupted (%lu), clearing\n", rtcResetCount);
            rtcResetCount = 0;
            rtcResetTimestamp = 0;
        }
        
        // Check if this reset is within the timeout window of the last one
        if (rtcResetTimestamp != 0 && rtcResetCount > 0 && rtcResetCount < 10) {
            // RTC timestamp was set and counter is valid
            rtcResetCount++;
            Serial.printf("[RESET] Reset count: %lu (within window)\n", rtcResetCount);
            
            if (rtcResetCount >= RESET_COUNT_THRESHOLD) {
                Serial.println("[RESET] TRIPLE RESET DETECTED - entering config portal");
                configPortalReason = "triple_reset";
                rtcResetCount = 0;  // Reset for next time
                rtcResetTimestamp = 0;
            }
        } else {
            // First reset or outside window
            rtcResetCount = 1;
            Serial.println("[RESET] First reset, starting detection window");
        }
        
        // Store timestamp for next boot
        rtcResetTimestamp = 1;  // Mark that a reset occurred
        
        // Wait for reset window, then clear counter if no more resets
        // This is done asynchronously - if device doesn't reset within window,
        // the counter stays but timestamp check will fail on next boot
        delay(RESET_DETECT_TIMEOUT * 1000);
        
        // If we get here, no more resets happened within window
        if (strcmp(configPortalReason, "none") == 0) {
            Serial.println("[RESET] Reset window expired, normal boot");
            rtcResetCount = 0;
            rtcResetTimestamp = 0;
        }
    }
    
    Serial.printf("[RESET] Boot reason: %s\n", configPortalReason);
}

void clearCrashLoop() {
    // Called after successful boot (WiFi + camera + web server initialized)
    rtcCrashLoopFlag = 0;
    rtcCrashCount = 0;
    Serial.println("[RESET] Crash loop flag cleared - boot successful");
}

void checkWiFiFallback() {
    // Check if WiFi has been disconnected long enough to start fallback AP
    unsigned long currentMillis = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
        // Track when WiFi first disconnected
        if (wifiDisconnectTime == 0) {
            wifiDisconnectTime = currentMillis;
        }
        
        // Check if we've been disconnected long enough
        if (!fallbackAPActive && (currentMillis - wifiDisconnectTime >= WIFI_FALLBACK_TIMEOUT)) {
            Serial.println("[WiFi] Starting fallback AP mode");
            
            // Create AP name
            String apName = String(deviceName);
            apName.replace(" ", "-");
            apName = "Cam-" + apName + "-Fallback";
            
            // Start AP alongside STA (device remains discoverable)
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP(apName.c_str());
            
            fallbackAPActive = true;
            Serial.printf("[WiFi] Fallback AP started: %s\n", apName.c_str());
            Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
            
            logEventToMQTT("wifi_fallback_ap", "warning");
        }
    } else {
        // WiFi is connected, reset disconnect timer
        wifiDisconnectTime = 0;
    }
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
    if (!wasManualOn) {
        digitalWrite(FLASH_PIN, HIGH);
        Serial.println("[FLASH] Motion flash triggered");
    }
    
    // Publish motion event to dedicated topic
    JsonDocument motionDoc;
    motionDoc["device"] = deviceName;
    motionDoc["chip_id"] = deviceChipId;
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

    // Set WiFi mode - use AP_STA to allow fallback AP later
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
    mqttClient.setBufferSize(1024); // Increase buffer for JSON messages

    reconnectMQTT();
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
        if (sdReady) {
            doc["sd_size_mb"] = SD_MMC.cardSize() / (1024 * 1024);
            doc["sd_used_mb"] = SD_MMC.usedBytes() / (1024 * 1024);
        }
        
        // Reset/recovery status
        doc["boot_reason"] = configPortalReason;
        doc["crash_count"] = rtcCrashCount;
        doc["fallback_ap_active"] = fallbackAPActive;
        if (fallbackAPActive) {
            doc["fallback_ap_ip"] = WiFi.softAPIP().toString();
        }
        doc["wifi_disconnect_time"] = wifiDisconnectTime > 0 ? (millis() - wifiDisconnectTime) / 1000 : 0;

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
        mqttClient.subscribe(getTopicCommand().c_str());

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
    doc["chip_id"] = deviceChipId;
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
    doc["fallback_ap_active"] = fallbackAPActive;

    String output;
    serializeJson(doc, output);

    mqttClient.publish(getTopicStatus().c_str(), output.c_str(), true);
    Serial.println("Status published to MQTT");
}

void captureAndPublish() {
    Serial.printf("[CAPTURE] Starting capture (manual=%s)...\n", 
                  flashManualOn ? "ON" : "OFF");

    // Always flash on capture (unless in manual mode)
    if (!flashManualOn) {
        digitalWrite(FLASH_PIN, HIGH);
        Serial.println("[FLASH] LED ON for capture");
        delay(100);  // Give flash time to reach full brightness
    }

    camera_fb_t * fb = capturePhoto();
    
    // Turn off flash after capture (only if we turned it on)
    if (!flashManualOn) {
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
    if (!flashManualOn) {
        digitalWrite(FLASH_PIN, HIGH);
        Serial.println("[FLASH] LED ON for image capture");
        delay(100);  // Give flash time to reach full brightness
    }

    camera_fb_t * fb = capturePhoto();
    
    // Turn off flash after capture (only if we turned it on)
    if (!flashManualOn) {
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
    String html = R"=====(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 Surveillance</title>
<style>
:root{--bg:#0b0f14;--panel:#121821;--panel-alt:#0f141c;--border:#1f2a38;--text:#e6edf3;--muted:#9aa7b2;--accent:#00bcd4;--accent-contrast:#07343b;--danger:#ff5252;}
*{box-sizing:border-box}
body{font-family:Inter,Arial,Helvetica,sans-serif;background:var(--bg);color:var(--text);font-size:14px;margin:0;padding:0;height:100vh;overflow:hidden}
a{color:var(--text)}
.container{display:flex;flex-direction:column;height:100vh}
#top-bar{background:var(--panel);border-bottom:1px solid var(--border);padding:12px 16px;display:flex;gap:12px;align-items:center;flex-wrap:wrap}
#controls-group{display:flex;gap:12px;align-items:center;flex:1;flex-wrap:wrap;min-width:0}
#stream-area{display:flex;flex:1;min-height:0;gap:12px;padding:12px}
#stream-container{flex:1;display:flex;flex-direction:column;min-width:0;background:var(--panel-alt);border:1px solid var(--border);border-radius:8px;overflow:hidden}
#stream-container img{max-width:100%;max-height:100%;object-fit:contain}
#right-panel{width:300px;background:var(--panel);border:1px solid var(--border);border-radius:8px;display:flex;flex-direction:column;transition:width 0.3s ease,opacity 0.3s ease;max-height:100%}
#right-panel.collapsed{width:0;opacity:0;overflow:hidden;padding:0;border:0}
#right-toggle{width:30px;height:30px;padding:0;margin:0;background:var(--accent)}
.panel-header{padding:12px;border-bottom:1px solid var(--border);display:flex;justify-content:space-between;align-items:center;font-weight:600;flex-shrink:0}
.panel-content{padding:12px;overflow-y:auto;overflow-x:hidden;flex:1;min-height:0}
.panel-content::-webkit-scrollbar{width:8px}
.panel-content::-webkit-scrollbar-track{background:var(--panel-alt);border-radius:4px}
.panel-content::-webkit-scrollbar-thumb{background:var(--border);border-radius:4px}
.panel-content::-webkit-scrollbar-thumb:hover{background:var(--muted)}
.input-group{display:flex;flex-wrap:nowrap;line-height:22px;margin:8px 0;padding:8px;border:1px solid var(--border);border-radius:8px;background:var(--panel-alt);gap:8px;align-items:center}
.input-group>label{display:inline-block;min-width:85px;white-space:nowrap;font-size:13px}
.input-group input,.input-group select{flex:1;min-width:0}
.range-max,.range-min{display:inline-block;padding:0 2px;color:var(--muted);font-size:11px;min-width:15px;text-align:center}
.slider-container{display:flex;gap:2px;align-items:center;flex:1;max-width:100%}
.control-row{display:flex;gap:6px;align-items:center}
.control-row.tight{gap:4px}
.control-row label{min-width:60px;white-space:nowrap}
.control-row.tight label{min-width:auto}
.control-row select,.control-row input[type=range]{flex:1}
button{display:inline-block;margin:3px;padding:0 10px;border:0;line-height:28px;cursor:pointer;color:#001318;background:var(--accent);border-radius:8px;font-size:14px;outline:0;box-shadow:0 0 0 1px var(--accent-contrast) inset;white-space:nowrap}
button:hover{filter:brightness(1.08)}
button:active{filter:brightness(0.95)}
button.disabled{cursor:default;opacity:.6}
button.small{padding:0 8px;line-height:24px;font-size:12px}
button.preset{flex:1;min-width:80px}
.button-group{display:flex;gap:6px;flex-wrap:wrap}
.status-pill{display:inline-flex;align-items:center;gap:6px;padding:0 10px;height:28px;border-radius:999px;font-size:12px;font-weight:600;line-height:1;background:var(--panel-alt);color:var(--muted);border:1px solid var(--border)}
.status-pill .dot{width:10px;height:10px;border-radius:50%;background:var(--border);display:inline-block}
.status-ok{color:#0ecb81;border-color:#0ecb81;background:rgba(14,203,129,0.08)}
.status-ok .dot{background:#0ecb81}
.status-warn{color:#f5a524;border-color:#f5a524;background:rgba(245,165,36,0.08)}
.status-warn .dot{background:#f5a524}
.status-bad{color:#ff5c5c;border-color:#ff5c5c;background:rgba(255,92,92,0.08)}
.status-bad .dot{background:#ff5c5c}
input[type=range]{-webkit-appearance:none;height:22px;background:var(--panel-alt);cursor:pointer;margin:0;border-radius:6px}
input[type=range]:focus{outline:0}
input[type=range]::-webkit-slider-runnable-track{width:100%;height:2px;cursor:pointer;background:var(--text);border-radius:0;border:0}
input[type=range]::-webkit-slider-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:var(--accent);cursor:pointer;-webkit-appearance:none;margin-top:-11.5px}
.switch{display:inline-block;position:relative;height:22px}
.switch input{outline:0;opacity:0;width:0;height:0}
.slider{width:50px;height:22px;border-radius:22px;cursor:pointer;background-color:#3a4b61;display:inline-block;transition:.4s}
.slider:before{position:absolute;content:"";border-radius:50%;height:16px;width:16px;left:4px;top:3px;background-color:#fff;display:inline-block;transition:.4s}
input:checked+.slider{background-color:var(--accent)}
input:checked+.slider:before{-webkit-transform:translateX(26px);transform:translateX(26px)}
select{border:1px solid var(--border);font-size:14px;height:22px;outline:0;border-radius:8px;background:var(--panel-alt);color:var(--text);padding:2px 6px}
.close{position:absolute;right:5px;top:5px;background:var(--danger);width:30px;height:30px;border-radius:100%;color:#fff;text-align:center;line-height:30px;cursor:pointer;box-shadow:0 0 0 1px #7a1e1e inset}
.hidden{display:none}
.section-title{font-size:12px;font-weight:600;color:var(--muted);margin-top:12px;margin-bottom:6px;text-transform:uppercase;letter-spacing:.5px}
#device-name-bar{display:none}
@media (max-width:1200px){
#device-name-bar{display:inline-block}
#right-panel{width:250px}
.input-group>label{min-width:90px}
}
@media (max-width:768px){
#top-bar{flex-direction:column;align-items:stretch;padding:8px}
#stream-area{flex-direction:column;gap:8px;padding:8px}
#right-panel{display:none;width:100%}
#right-toggle{display:block}
#right-panel.mobile-visible{display:flex;position:fixed;right:0;top:0;bottom:0;z-index:1000;height:100vh;width:90%;max-width:320px;box-shadow:-4px 0 12px rgba(0,0,0,0.3)}
.panel-content{padding:8px}
.input-group{margin:6px 0;padding:6px}
button{font-size:13px;padding:0 8px;line-height:26px}
.section-title{margin-top:8px;margin-bottom:4px}
}
@media (max-width:480px){
#right-panel.mobile-visible{width:100%;max-width:100%}
.input-group{flex-direction:column;align-items:stretch}
.input-group>label{min-width:auto;margin-bottom:4px}
.slider-container{width:100%}
}
</style>
</head>
<body>
<div class="container">
<div id="top-bar">
<button id="get-still">Capture</button>
<button id="toggle-stream">Start Stream</button>
<span id="status-pill" class="status-pill status-warn"><span class="dot"></span><span id="status-text">Initializing...</span></span>
<span id="device-name-bar" style="font-size:14px;font-weight:600;color:var(--text);white-space:nowrap">Loading...</span>
<div id="controls-group">
<div class="control-row">
<label for="framesize">Resolution:</label>
<select id="framesize" class="default-action">
<option value="13">UXGA</option>
<option value="12">SXGA</option>
<option value="11">HD</option>
<option value="10">XGA</option>
<option value="9">SVGA</option>
<option value="8" selected="selected">VGA</option>
<option value="7">HVGA</option>
<option value="6">CIF</option>
<option value="5">QVGA</option>
<option value="3">HQVGA</option>
<option value="1">QQVGA</option>
</select>
</div>
<div class="control-row">
<label for="quality">Quality:</label>
<input type="range" id="quality" min="10" max="63" value="8" class="default-action" style="width:100px">
</div>
<div class="control-row tight">
<label for="motion_enabled">Motion:</label>
<div class="switch">
<input id="motion_enabled" type="checkbox" checked="checked">
<label class="slider" for="motion_enabled"></label>
</div>
</div>
<div class="button-group">
<button id="preset-smooth" class="preset small">Fast</button>
<button id="preset-balanced" class="preset small">Default</button>
<button id="preset-detail" class="preset small">High-Quality</button>
<button id="clear_sd" class="preset small" style="margin-left:8px">Clear SD</button>
<button id="format_sd" class="preset small">Format SD</button>
<div id="sd_status" style="font-size:10px;font-weight:bold;padding:0 4px;"></div>
</div>
</div>
<button id="right-toggle">⚙</button>
</div>
<div id="stream-area">
<div id="stream-container">
<img id="stream" src="">
</div>
<div id="right-panel">
<div class="panel-header">
<div style="display:flex;flex-direction:column;gap:4px;flex:1">
<span style="font-weight:600">Camera: <span id="device-name">Loading...</span></span>
<span style="font-size:12px;color:var(--muted)">ID: <span id="device-id">--</span></span>
<div style="display:flex;gap:12px;font-size:12px;color:var(--muted)">
<span id="wifi-status">WiFi: --</span>
<span id="mqtt-status">MQTT: --</span>
</div>
</div>
</div>
<div class="panel-content">
<nav id="menu">
<div class="section-title">Image Settings</div>
<div class="input-group" id="brightness-group">
<label for="brightness">Brightness</label>
<div class="slider-container">
<span class="range-min">-2</span>
<input type="range" id="brightness" min="-2" max="2" value="0" class="default-action" style="flex:1">
<span class="range-max">2</span>
</div>
</div>
<div class="input-group" id="contrast-group">
<label for="contrast">Contrast</label>
<div class="slider-container">
<span class="range-min">-2</span>
<input type="range" id="contrast" min="-2" max="2" value="0" class="default-action" style="flex:1">
<span class="range-max">2</span>
</div>
</div>
<div class="input-group" id="saturation-group">
<label for="saturation">Saturation</label>
<div class="slider-container">
<span class="range-min">-2</span>
<input type="range" id="saturation" min="-2" max="2" value="0" class="default-action" style="flex:1">
<span class="range-max">2</span>
</div>
</div>
<div class="section-title">Effects</div>
<div class="input-group" id="special_effect-group">
<label for="special_effect">Effect</label>
<select id="special_effect" class="default-action" style="flex:1">
<option value="0" selected="selected">None</option>
<option value="1">Negative</option>
<option value="2">Grayscale</option>
<option value="3">Red Tint</option>
<option value="4">Green Tint</option>
<option value="5">Blue Tint</option>
<option value="6">Sepia</option>
</select>
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
<div class="section-title">Exposure & Gain</div>
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
<select id="wb_mode" class="default-action" style="flex:1">
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
<div class="input-group" id="aec_value-group">
<label for="aec_value">Exposure</label>
<div class="slider-container">
<span class="range-min">0</span>
<input type="range" id="aec_value" min="0" max="1200" value="300" class="default-action">
<span class="range-max">1200</span>
</div>
</div>
<div class="input-group" id="agc-group">
<label for="agc">AGC</label>
<div class="switch">
<input id="agc" type="checkbox" class="default-action" checked="checked">
<label class="slider" for="agc"></label>
</div>
</div>
<div class="input-group" id="gainceiling-group">
<label for="gainceiling">Gain Ceiling</label>
<div class="slider-container">
<span class="range-min">2x</span>
<input type="range" id="gainceiling" min="0" max="6" value="0" class="default-action" style="flex:1">
<span class="range-max">128x</span>
</div>
</div>
<div class="section-title">Processing</div>
<div class="input-group" id="aec2-group">
<label for="aec2">AEC DSP</label>
<div class="switch">
<input id="aec2" type="checkbox" class="default-action">
<label class="slider" for="aec2"></label>
</div>
</div>
<div class="input-group" id="ae_level-group">
<label for="ae_level">AE Level</label>
<div class="slider-container">
<span class="range-min">-2</span>
<input type="range" id="ae_level" min="-2" max="2" value="0" class="default-action">
<span class="range-max">2</span>
</div>
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
<div class="input-group" id="dcw-group">
<label for="dcw">DCW</label>
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
<div class="section-title">Flash LED</div>
<div class="input-group" id="flash_master-group">
<label for="flash_master">Manual Flashlight</label>
<div class="switch">
<input id="flash_master" type="checkbox">
<label class="slider" for="flash_master"></label>
</div>
</div>
</nav>
</div>
</div>
</div>
</div>
</div>
<script>
document.addEventListener('DOMContentLoaded',function(){
const baseHost=document.location.origin;
let flashMotionState=true; // preserve motion indicator state
const view=document.getElementById('stream');
const streamContainer=document.getElementById('stream-container');
const stillButton=document.getElementById('get-still');
const streamButton=document.getElementById('toggle-stream');
const rightPanel=document.getElementById('right-panel');
const rightToggle=document.getElementById('right-toggle');
let isStreaming=false;
const hide=el=>el.classList.add('hidden');
const show=el=>el.classList.remove('hidden');
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
console.log(`Control updated: ${el.id}=${value}`);
});
}
// Status helpers
const statusPill=document.getElementById('status-pill');
const statusText=document.getElementById('status-text');
const wifiStatus=document.getElementById('wifi-status');
const mqttStatus=document.getElementById('mqtt-status');
const setPill=(mode,text)=>{
 if(!statusPill||!statusText)return;
 statusPill.classList.remove('status-ok','status-warn','status-bad');
 statusPill.classList.add(mode);
 statusText.textContent=text;
};
const setConnectivity=(rssi,mqtt)=>{
 if(wifiStatus){
 let wifiLabel='--';
 let wifiClass='status-warn';
 if(typeof rssi==='number'){
 wifiLabel=`WiFi: ${rssi} dBm`;
 wifiClass=rssi>-60?'status-ok':(rssi>-75?'status-warn':'status-bad');
 }
 wifiStatus.textContent=wifiLabel;
 wifiStatus.className=wifiClass;
 }
 if(mqttStatus){
 mqttStatus.textContent=`MQTT: ${mqtt?"Connected":"Disconnected"}`;
 mqttStatus.className=mqtt?'status-ok':'status-bad';
 }
};
// Panel toggle (works for both desktop collapsed and mobile slide-in)
rightToggle.onclick=()=>{
 if(window.innerWidth<=768){
 rightPanel.classList.toggle('mobile-visible');
 } else {
 rightPanel.classList.toggle('collapsed');
 }
};
// Load initial settings and device info
fetch(`${baseHost}/status`).then(response=>response.json()).then(state=>{
document.querySelectorAll('.default-action').forEach(el=>{
updateValue(el,state[el.id],false);
});
if(state.motion_enabled!==undefined){
document.getElementById('motion_enabled').checked=state.motion_enabled;
}
if(state.flash_manual!==undefined){
document.getElementById('flash_master').checked=state.flash_manual;
}
if(state.flash_motion!==undefined){
flashMotionState=!!state.flash_motion;
}
// Display device name and chip ID
if(state.device_name){
document.getElementById('device-name').textContent=state.device_name;
const topBarName=document.getElementById('device-name-bar');
if(topBarName) topBarName.textContent=state.device_name;
}
if(state.chip_id){
const shortId=state.chip_id.substring(0,8).toUpperCase();
document.getElementById('device-id').textContent=shortId;
}
if(state.framesize!==undefined){
updateValue(document.getElementById('framesize'),state.framesize,false);
}
if(state.quality!==undefined){
updateValue(document.getElementById('quality'),state.quality,false);
}
if(state.camera_ready){
setPill('status-ok','Ready');
} else {
// Show "Initializing..." for first 10 seconds, then "Camera not ready"
const uptime = state.uptime || 0;
if (uptime < 10) {
setPill('status-warn','Initializing...');
} else {
setPill('status-warn','Camera not ready');
}
}
setConnectivity(state.wifi_rssi,state.mqtt_connected);
}).catch(err=>{
console.error('Status load failed',err);
});
// Poll status every 2 seconds to update camera ready status and connectivity
setInterval(()=>{
fetch(`${baseHost}/status`).then(response=>response.json()).then(state=>{
if(state.camera_ready){
setPill('status-ok','Ready');
} else {
// Show "Initializing..." for first 10 seconds, then "Camera not ready"
const uptime = state.uptime || 0;
if (uptime < 10) {
setPill('status-warn','Initializing...');
} else {
setPill('status-warn','Camera not ready');
}
}
setConnectivity(state.wifi_rssi,state.mqtt_connected);
}).catch(err=>{
console.error('Status poll failed',err);
});
},2000);
// Stream controls
const stopStream=()=>{
view.src='';
streamButton.textContent='Start Stream';
isStreaming=false;
 setPill('status-ok','Ready');
};
const startStream=()=>{
view.src=`${baseHost}/stream`;
streamButton.textContent='Stop Stream';
isStreaming=true;
 setPill('status-ok','Streaming');
};
stillButton.onclick=()=>{
stopStream();
view.src=`${baseHost}/capture?_cb=${Date.now()}`;
};
streamButton.onclick=()=>{
isStreaming?stopStream():startStream();
};
// Control listeners
document.querySelectorAll('.default-action').forEach(el=>{
el.onchange=()=>updateConfig(el);
});
// Conditional visibility
const agc=document.getElementById('agc');
const gainCeiling=document.getElementById('gainceiling-group');
agc.onchange=()=>{
updateConfig(agc);
agc.checked?show(gainCeiling):hide(gainCeiling);
};
const aec=document.getElementById('aec');
const exposure=document.getElementById('aec_value-group');
aec.onchange=()=>{
updateConfig(aec);
aec.checked?hide(exposure):show(exposure);
};
const awbGain=document.getElementById('awb_gain');
const wbMode=document.getElementById('wb_mode-group');
awbGain.onchange=()=>{
updateConfig(awbGain);
awbGain.checked?show(wbMode):hide(wbMode);
};
// Motion toggle
const motionToggle=document.getElementById('motion_enabled');
motionToggle.onchange=()=>{
const enabled=motionToggle.checked?1:0;
fetch(`${baseHost}/motion-control?enabled=${enabled}`).then(response=>response.json()).catch(err=>console.error('Motion control failed:',err));
};
// Flash toggle (manual flashlight on/off)
const flashMasterToggle=document.getElementById('flash_master');
flashMasterToggle.onchange=()=>{
const manual=flashMasterToggle.checked?1:0;
console.log(`[FLASH] Manual toggle changed: manual=${manual}`);
fetch(`${baseHost}/flash-control?manual=${manual}`).then(response=>response.json()).then(data=>{console.log('[FLASH] Server response:',data);}).catch(err=>console.error('[FLASH] Control failed:',err));
};
// Presets
const setAndPush=(id,val)=>{
const el=document.getElementById(id);
if(!el) return;
updateValue(el,val,true);
};

// SD clear button
const clearSdBtn=document.getElementById('clear_sd');
const formatSdBtn=document.getElementById('format_sd');
const sdStatus=document.getElementById('sd_status');
if(clearSdBtn){
clearSdBtn.onclick=()=>{
if(!confirm('Delete all captures on SD card?')) return;
clearSdBtn.disabled=true;
sdStatus.textContent='Clearing...';
sdStatus.style.color='#ffa500'; // Orange
fetch(`${baseHost}/captures/clear`).then(r=>r.json()).then(res=>{
if(res.status==='cleared'){
sdStatus.textContent='✓ Cleared successfully';
sdStatus.style.color='#4CAF50'; // Green
setTimeout(()=>{sdStatus.textContent='';},3000);
} else if(res.status==='sd_not_ready'){
sdStatus.textContent='✗ SD card not ready';
sdStatus.style.color='#f44336'; // Red
setTimeout(()=>{sdStatus.textContent='';},5000);
} else {
sdStatus.textContent='✗ Error: '+res.status;
sdStatus.style.color='#f44336'; // Red
setTimeout(()=>{sdStatus.textContent='';},5000);
}
}).catch(err=>{
console.error('SD clear failed:',err);
sdStatus.textContent='✗ Network error';
sdStatus.style.color='#f44336'; // Red
setTimeout(()=>{sdStatus.textContent='';},5000);
}).finally(()=>{
clearSdBtn.disabled=false;
});
};
}
if(formatSdBtn){
formatSdBtn.onclick=async()=>{
if(!confirm('⚠️ WARNING: FORMAT WILL REBOOT THE DEVICE!\n\nThis will:\n• REBOOT the camera immediately\n• ERASE ALL DATA on the SD card\n• Take about 10 seconds to complete\n\nContinue with format?')) return;
formatSdBtn.disabled=true;
clearSdBtn.disabled=true;
sdStatus.textContent='⟳ Rebooting to format...';
sdStatus.style.color='#ffa500';
try{
const formData=new FormData();
formData.append('confirm','yes');
const res=await(await fetch(`${baseHost}/sd/format`,{method:'POST',body:formData})).json();
if(res.status==='rebooting_to_format'){
sdStatus.textContent='⟳ Device rebooting - please wait...';
setTimeout(()=>{location.reload();},10000);
} else {
sdStatus.textContent='✗ Format failed: '+res.status;
sdStatus.style.color='#f44336';
formatSdBtn.disabled=false;
clearSdBtn.disabled=false;
}
}catch(err){
console.error('Format error:',err);
sdStatus.textContent='⟳ Device rebooting (connection lost)...';
setTimeout(()=>{location.reload();},10000);
}
};
}
// Fast: Low res, high compression, minimal latency
document.getElementById('preset-smooth').onclick=()=>{
setAndPush('framesize','5');
setAndPush('quality','12');
setAndPush('aec','1');
setAndPush('aec_value','200');
setAndPush('gainceiling','1');
};
// Default: Balanced quality and speed (recommended)
document.getElementById('preset-balanced').onclick=()=>{
setAndPush('framesize','8');
setAndPush('quality','8');
setAndPush('aec','1');
setAndPush('aec_value','300');
setAndPush('gainceiling','2');
};
// High-Quality: Maximum image detail, slower
document.getElementById('preset-detail').onclick=()=>{
setAndPush('framesize','9');
setAndPush('quality','5');
setAndPush('aec','1');
setAndPush('aec_value','300');
setAndPush('gainceiling','3');
};
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
    if (!wasManualOn) {
        digitalWrite(FLASH_PIN, HIGH);
        delay(100);  // Allow flash to reach brightness
    }

    camera_fb_t * fb = capturePhoto();
    
    // Restore flash state after capture
    if (!wasManualOn) {
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
    if (sdReady) {
        char path[64];
        snprintf(path, sizeof(path), "%s/%lu_%lu.jpg", SD_CAPTURE_DIR, millis(), captureCount);
        File file = SD_MMC.open(path, "w");
        if (file) {
            file.write(copyBuf, copyLen);
            file.close();
            Serial.printf("[SD] Saved capture: %s (%u bytes)\n", path, (unsigned int)copyLen);
        } else {
            Serial.println("[SD] Failed to write capture");
        }
    }

    // LED remains in manual state (not controlled by capture)

    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/jpeg", copyBuf, copyLen);
    response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
    response->addHeader("Access-Control-Allow-Origin", "*");
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
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, flashManualOn ? HIGH : LOW);
    
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