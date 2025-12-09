#ifndef CAMERA_CONFIG_H
#define CAMERA_CONFIG_H

#include "esp_camera.h"

// Camera model auto-detection based on build environment
// Set via platformio.ini build_flags: -DCAMERA_MODEL_AI_THINKER or -DCAMERA_MODEL_ESP32S3_EYE
// Falls back to AI_THINKER if not defined

#if defined(CAMERA_MODEL_ESP32S3_EYE)
// ESP32-S3 Camera pin definitions
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11

#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// LED flash pin (if available)
#define LED_GPIO_NUM      -1

#else
// Default: ESP32-CAM (AI-Thinker) pin definitions with OV2640
#ifndef CAMERA_MODEL_AI_THINKER
#define CAMERA_MODEL_AI_THINKER
#endif

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// LED flash pin (built-in LED on GPIO 4)
#define LED_GPIO_NUM      4
#endif

// Camera settings - optimized for quality + speed
#define CAMERA_XCLK_FREQ  25000000  // 25MHz for faster frame capture
#define CAMERA_FB_COUNT   2         // Double buffering for smooth streaming

// Initialize camera with default settings
camera_config_t getCameraConfig();

// Initialize camera
bool initCamera();

// Capture and return a frame buffer
camera_fb_t* capturePhoto();

// Return frame buffer
void returnFrameBuffer(camera_fb_t* fb);

#endif // CAMERA_CONFIG_H
