#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

// Device identification
#define DEVICE_NAME "ESP32-CAM-Surveillance"
#define FIRMWARE_VERSION "1.0.0-arduino"

// WiFi settings
#define WIFI_CONNECT_TIMEOUT 30000  // 30 seconds
#define WIFI_RECONNECT_INTERVAL 60000  // 1 minute

// MQTT settings
#define MQTT_PORT 1883
#define MQTT_RECONNECT_INTERVAL 5000  // 5 seconds

// MQTT Topics (base paths - device-specific topics built dynamically)
#define MQTT_TOPIC_BASE "surveillance"
#define MQTT_TOPIC_STATUS_SUFFIX "/status"
#define MQTT_TOPIC_IMAGE_SUFFIX "/image"
#define MQTT_TOPIC_MOTION_SUFFIX "/motion"
#define MQTT_TOPIC_COMMAND_SUFFIX "/command"
#define MQTT_TOPIC_METRICS_SUFFIX "/metrics"
#define MQTT_TOPIC_EVENTS_SUFFIX "/events"

// Image capture settings
#define CAPTURE_INTERVAL 60000  // Capture every 60 seconds (configurable)
#define MOTION_DETECTION_ENABLED true

// Camera-based motion detection settings
#define MOTION_CHECK_INTERVAL 3000  // Check for motion every 3 seconds
#define MOTION_BLOCK_SIZE 16        // Divide frame into 16x16 pixel blocks
#define MOTION_THRESHOLD 25         // Pixel difference threshold (0-255)
#define MOTION_CHANGED_BLOCKS 25    // Number of blocks that must change to trigger motion (was 15)

// Web server settings
#define WEB_SERVER_PORT 80

// Status LED (if available)
#define STATUS_LED_PIN 2

// Flash/Strobe LED for motion indication (board-specific)
#if defined(ARDUINO_FREENOVE_ESP32_S3_WROOM) || defined(ARDUINO_ESP32S3_DEV)
  #define FLASH_PIN -1  // ESP32-S3 board has no flash LED
#else
  #define FLASH_PIN 4  // GPIO4 for AI-Thinker ESP32-CAM (Flash LED)
#endif
#define FLASH_PULSE_MS 200  // 200ms pulse duration for motion flash

// PIR Motion Sensor (AM312)
#if defined(ARDUINO_FREENOVE_ESP32_S3_WROOM) || defined(ARDUINO_ESP32S3_DEV)
  #define PIR_PIN 14  // GPIO14 for ESP32-S3 (not used by camera)
#else
  #define PIR_PIN 13  // GPIO13 for AI-Thinker ESP32-CAM
#endif

#define PIR_DEBOUNCE_MS 5000  // 5 seconds between motion triggers

// Triple-reset detector (for entering config portal)
#define RESET_DETECT_TIMEOUT 2       // 2 second window for triple-reset
#define RESET_DETECT_ADDRESS 0x00    // RTC memory address for reset counter
#define RESET_COUNT_THRESHOLD 3      // Number of resets to trigger config portal

// Crash loop recovery
#define CRASH_LOOP_THRESHOLD 5       // 5 consecutive crashes triggers recovery mode
#define CRASH_LOOP_MAGIC 0xDEADBEEF  // Magic number to detect incomplete boots

// WiFi fallback AP mode
#define WIFI_FALLBACK_TIMEOUT 60000  // 60 seconds before starting fallback AP
#define CONFIG_PORTAL_TIMEOUT 120    // 2 minutes portal timeout

#endif // DEVICE_CONFIG_H
