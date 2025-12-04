#ifndef CAMERA_CONFIG_H
#define CAMERA_CONFIG_H

#include "esp_camera.h"

// ESP32-S3 Camera pin definitions
// Adjust these based on your specific ESP32-S3 camera module
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

// Camera settings - optimized for performance
#define CAMERA_XCLK_FREQ  20000000  // 20MHz is stable for most cameras
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
