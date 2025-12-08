#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

// Device identification
#define DEVICE_NAME "ESP32-S3-Surveillance"
#define FIRMWARE_VERSION "1.0.0"

// WiFi settings
#define WIFI_CONNECT_TIMEOUT 30000  // 30 seconds
#define WIFI_RECONNECT_INTERVAL 60000  // 1 minute

// MQTT settings
#define MQTT_PORT 1883
#define MQTT_RECONNECT_INTERVAL 5000  // 5 seconds

// MQTT Topics
#define MQTT_TOPIC_STATUS "surveillance/status"
#define MQTT_TOPIC_IMAGE "surveillance/image"
#define MQTT_TOPIC_MOTION "surveillance/motion"
#define MQTT_TOPIC_COMMAND "surveillance/command"

// Image capture settings
#define CAPTURE_INTERVAL 60000  // Capture every 60 seconds (configurable)
#define MOTION_DETECTION_ENABLED true

// Web server settings
#define WEB_SERVER_PORT 80

// Status LED (if available)
#define STATUS_LED_PIN 2

// PIR Motion Sensor (AM312)
#if defined(CAMERA_MODEL_ESP32S3_EYE)
  #define PIR_PIN 14  // GPIO14 for ESP32-S3 (not used by camera)
#else
  #define PIR_PIN 14  // GPIO14 for AI-Thinker ESP32-CAM (GPIO 13 is not available)
#endif

#define PIR_DEBOUNCE_MS 5000  // 5 seconds between motion triggers

// Double reset detector
#define DRD_TIMEOUT 10
#define DRD_ADDRESS 0x00

#endif // DEVICE_CONFIG_H
