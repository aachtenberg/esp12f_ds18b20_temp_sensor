#include <Arduino.h>
#include "camera_config.h"

camera_config_t getCameraConfig() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    // Set sensor clock based on camera model
    // OV3660 (ESP32-S3) prefers 10MHz for stability
    // OV2640 (ESP32-CAM AI-Thinker) runs faster at 20MHz
    #if defined(ARDUINO_FREENOVE_ESP32_S3_WROOM) || defined(ARDUINO_ESP32S3_DEV)
        config.xclk_freq_hz = CAMERA_XCLK_FREQ; // 10MHz from header
    #else
        config.xclk_freq_hz = 20000000; // 20MHz for OV2640
    #endif
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;  // Initial mode before PSRAM check

    // Per-board tuning: match working Freenove sketch pattern
    #if defined(ARDUINO_FREENOVE_ESP32_S3_WROOM) || defined(ARDUINO_ESP32S3_DEV)
        // S3 board with OV3660: Match working Freenove configuration
        config.frame_size = FRAMESIZE_SVGA;  // 800x600
        config.jpeg_quality = 12;
        config.fb_count = 2;
        
        if (psramFound()) {
            config.jpeg_quality = 10;            // Better quality with PSRAM (matches Freenove)
            config.fb_count = 2;
            config.fb_location = CAMERA_FB_IN_PSRAM;
            config.grab_mode = CAMERA_GRAB_LATEST;  // Switch to LATEST with PSRAM
            Serial.println("PSRAM found (S3/OV3660) - using SVGA@Q10 with GRAB_LATEST");
        } else {
            config.frame_size = FRAMESIZE_QVGA;  // 320x240 fallback without PSRAM to fit DRAM
            config.jpeg_quality = 12;
            config.fb_count = 1;
            config.fb_location = CAMERA_FB_IN_DRAM;
            config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
            Serial.println("PSRAM not found (S3/OV3660) - using QVGA fallback in DRAM");
        }
    #else
        // ESP32-CAM with OV2640: tuned for responsiveness
        config.frame_size = FRAMESIZE_VGA;   // 640x480 - good balance
        config.jpeg_quality = 10;            // lower compression → faster encode
        config.fb_count = 2;                 // will bump to 3 when PSRAM is present
        
        if (psramFound()) {
            config.fb_count = 3;                 // triple buffering reduces stalls
            config.fb_location = CAMERA_FB_IN_PSRAM;
            config.grab_mode = CAMERA_GRAB_LATEST;  // prefer latest frame for streaming
            Serial.println("PSRAM found (ESP32-CAM/OV2640) - VGA@Q10, 3FB, GRAB_LATEST, XCLK=20MHz");
        } else {
            config.frame_size = FRAMESIZE_HVGA; // 480x320 when PSRAM missing (better than QVGA)
            config.jpeg_quality = 12;
            config.fb_count = 1;
            config.fb_location = CAMERA_FB_IN_DRAM;
            config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
            Serial.println("PSRAM not found (ESP32-CAM/OV2640) - using HVGA quality fallback");
        }
    #endif

    return config;
}

bool initCamera() {
    camera_config_t config = getCameraConfig();

    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    // Adjust sensor settings
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
        #if defined(ARDUINO_FREENOVE_ESP32_S3_WROOM) || defined(ARDUINO_ESP32S3_DEV)
            // ESP32-S3 with OV3660: Match Freenove sketch EXACTLY - only these 3 settings
            s->set_vflip(s, 1);          // Flip it back
            s->set_brightness(s, 1);     // Up the brightness just a bit
            s->set_saturation(s, 0);     // Lower the saturation
            Serial.println("OV3660 sensor settings applied (Freenove pattern)");
        #else
            // ESP32-CAM with OV2640: Keep working settings (was already functional)
            // OV2640 is more forgiving, doesn't need special treatment
            Serial.println("OV2640 sensor - using defaults");
        #endif
    }

    Serial.println("Camera initialized successfully");
    return true;
}

void resetCameraSettings() {
    sensor_t * s = esp_camera_sensor_get();
    if (s == NULL) {
        Serial.println("[CAMERA] Reset failed: sensor not found");
        return;
    }

    // 1. Hardware reset to sensor defaults
    s->reset(s);
    delay(100); // Give it a moment to stabilize

    // 2. Re-apply our project-specific defaults
    #if defined(ARDUINO_FREENOVE_ESP32_S3_WROOM) || defined(ARDUINO_ESP32S3_DEV)
        // ESP32-S3 with OV3660: Match Freenove sketch EXACTLY
        if (psramFound()) {
            s->set_framesize(s, FRAMESIZE_SVGA);
            s->set_quality(s, 10);
        } else {
            s->set_framesize(s, FRAMESIZE_VGA);
            s->set_quality(s, 12);
        }
        s->set_vflip(s, 1);          // Flip it back
        s->set_brightness(s, 1);     // Up the brightness just a bit
        s->set_saturation(s, 0);     // Lower the saturation
        Serial.println("[CAMERA] Reset to S3/OV3660 defaults");
    #else
        // ESP32-CAM with OV2640
        if (psramFound()) {
            s->set_framesize(s, FRAMESIZE_VGA);
            s->set_quality(s, 10);
        } else {
            s->set_framesize(s, FRAMESIZE_HVGA);
            s->set_quality(s, 12);
        }
        // OV2640 defaults are usually fine after s->reset(s)
        Serial.println("[CAMERA] Reset to ESP32-CAM/OV2640 defaults");
    #endif
}

camera_fb_t* capturePhoto() {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return NULL;
    }
    
    // Validate JPEG frame: should start with 0xFFD8 and end with 0xFFD9
    if (fb->len < 4) {
        Serial.printf("Frame too small: %zu bytes\n", fb->len);
        esp_camera_fb_return(fb);
        return NULL;
    }
    
    uint8_t* buf = (uint8_t*)fb->buf;
    if (buf[0] != 0xFF || buf[1] != 0xD8) {
        Serial.printf("Invalid JPEG header: %02X %02X (expected FFD8)\n", buf[0], buf[1]);
        esp_camera_fb_return(fb);
        return NULL;
    }
    
    if (buf[fb->len-2] != 0xFF || buf[fb->len-1] != 0xD9) {
        Serial.printf("Invalid JPEG footer: %02X %02X (expected FFD9)\n", 
                      buf[fb->len-2], buf[fb->len-1]);
        esp_camera_fb_return(fb);
        return NULL;
    }
    
    // Give camera sensor time to settle between frames (prevents tearing)
    delayMicroseconds(100);
    return fb;
}

void returnFrameBuffer(camera_fb_t* fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}
