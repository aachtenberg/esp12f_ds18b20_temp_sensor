#include "camera_config.h"
#include <Arduino.h>

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
    config.xclk_freq_hz = CAMERA_XCLK_FREQ;
    config.pixel_format = PIXFORMAT_JPEG;

    // Per-board tuning: each sensor has different JPEG encoder characteristics
    #if defined(CAMERA_MODEL_ESP32S3_EYE)
        // S3 board with OV3660: Optimized settings for high performance streaming
        if (psramFound()) {
            config.frame_size = FRAMESIZE_SVGA;  // 800x600 - documented working resolution
            config.jpeg_quality = 12;            // Optimal quality/speed balance per docs
            config.fb_count = 2;                 // Double buffering for smooth streaming
            Serial.println("PSRAM found (S3/OV3660) - using SVGA@Q12 for streaming");
        } else {
            config.frame_size = FRAMESIZE_VGA;   // 640x480 fallback without PSRAM
            config.jpeg_quality = 15;
            config.fb_count = 1;
            Serial.println("PSRAM not found (S3/OV3660) - using VGA fallback");
        }
    #else
        // ESP32-CAM with OV2640: More forgiving JPEG encoder, can use lower quality for responsiveness
        if (psramFound()) {
            config.frame_size = FRAMESIZE_VGA;  // 640x480 - decent resolution
            config.jpeg_quality = 10;           // OV2640: Lower quality (less compression) = faster, still stable
            config.fb_count = 2;                // double buffering for smoother stream
            Serial.println("PSRAM found (ESP32-CAM/OV2640) - using VGA@Q10 for speed");
        } else {
            config.frame_size = FRAMESIZE_HVGA; // 480x320 when PSRAM missing (better than QVGA)
            config.jpeg_quality = 12;
            config.fb_count = 1;
            Serial.println("PSRAM not found (ESP32-CAM/OV2640) - using HVGA quality fallback");
        }
    #endif

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;  // Always get latest frame for live streaming

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
        // Sensor ID detection for specific optimizations
        bool isOV3660 = (s->id.PID == OV3660_PID);
        bool isOV2640 = (s->id.PID == OV2640_PID);
        
        if (isOV3660) {
            Serial.println("OV3660 detected - applying optimizations");
            s->set_vflip(s, 1);        // Flip vertically
            s->set_brightness(s, 1);   // Slightly brighter
            s->set_saturation(s, -2);  // Lower saturation for OV3660
        } else if (isOV2640) {
            Serial.println("OV2640 detected - applying optimizations");
        }

        // Sensor adjustments optimized for quality + speed
        s->set_brightness(s, 0);     // -2 to 2 (neutral) - override OV3660 custom if needed
        s->set_contrast(s, 1);       // -2 to 2 (slight boost for clarity)
        s->set_saturation(s, 0);     // -2 to 2 (natural colors) - overrides OV3660 -2
        s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect)
        s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
        s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
        s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled
        s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable (auto exposure)
        s->set_aec2(s, 0);           // 0 = disable , 1 = enable
        s->set_ae_level(s, 0);       // -2 to 2
        s->set_aec_value(s, 300);    // Balanced exposure time (0 to 1200)
        s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
        s->set_agc_gain(s, 0);       // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)3);  // Moderate gain for better detail
        s->set_bpc(s, 1);            // 0 = disable , 1 = enable (enabled for quality)
        s->set_wpc(s, 1);            // 0 = disable , 1 = enable
        s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
        s->set_lenc(s, 1);           // 0 = disable , 1 = enable (enabled for quality)
        s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
        s->set_vflip(s, isOV3660 ? 1 : 0);  // Keep vflip for OV3660, disable for others
        s->set_dcw(s, 1);            // 0 = disable , 1 = enable
        s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
    }

    Serial.println("Camera initialized successfully");
    return true;
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