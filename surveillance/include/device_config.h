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

// MQTT Base Topic (will be appended with device name)
#define MQTT_TOPIC_BASE "surveillance"

// MQTT Topic Suffixes
#define MQTT_TOPIC_STATUS_SUFFIX "/status"
#define MQTT_TOPIC_IMAGE_SUFFIX "/image"
#define MQTT_TOPIC_MOTION_SUFFIX "/motion"
#define MQTT_TOPIC_COMMAND_SUFFIX "/command"
#define MQTT_TOPIC_METRICS_SUFFIX "/metrics"
#define MQTT_TOPIC_EVENTS_SUFFIX "/events"

// Image capture settings
#define CAPTURE_INTERVAL 60000  // Capture every 60 seconds (configurable)
#define MOTION_DETECTION_ENABLED true

// Web server settings
#define WEB_SERVER_PORT 80

// Status LED (if available)
#define STATUS_LED_PIN 2

// PIR Motion Sensor (AM312)
#if defined(CAMERA_MODEL_ESP32S3_EYE)
  #define PIR_PIN 14  // GPIO14 for ESP32-S3
#else
  #define PIR_PIN 13  // GPIO13 for AI-Thinker ESP32-CAM
#endif

#define PIR_DEBOUNCE_MS 5000  // 5 seconds between motion triggers

// Double reset detector
#define DRD_TIMEOUT 3
#define DRD_ADDRESS 0x00

#endif // DEVICE_CONFIG_H
