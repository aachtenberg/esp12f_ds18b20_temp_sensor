# Copilot Instructions for esp-sensor-hub (surveillance)

## ⚠️ MANDATORY: Context Before Code

**Before making ANY changes to this surveillance project:**

### Step 1: Read Documentation (Required)
1. **This file** - Understand patterns, conventions, gotchas
2. **`surveillance/README.md`** - Current features, setup, platform notes
3. **`/docs/reference/PLATFORM_GUIDE.md`** - Architecture overview
4. **`/memory-bank/activeContext.md`** - Current development state
5. **`/memory-bank/progress.md`** - Recent changes and decisions

### Step 2: Read The Code (Required)
1. **`src/main.cpp`** - Core application logic (WiFi/MQTT/web/ISR)
2. **`include/device_config.h`** - Configuration constants
3. **`include/camera_config.h`** & `src/camera_config.cpp` - Camera setup
4. **Related files** - Any dependencies or shared code

### Step 3: Understand Platform Specifics (Critical)
- **ESP32-S3**: Uses NVS for reset detection (RTC memory unreliable)
- **ESP32-CAM**: Can use RTC or NVS
- **Camera models**: AI-Thinker vs ESP32-S3 Eye have different pins
- **PIR pins**: GPIO14 (S3) vs GPIO13 (AI-Thinker)

### ❌ Do NOT:
- Change code without reading existing implementation
- Ignore board-specific conditionals (`CAMERA_MODEL_*`)
- Modify camera pins or board IDs without verification
- Create new patterns when existing helpers exist
- Skip reading the "Gotchas" section below

### ✅ DO:
- Use existing LittleFS helper functions
- Follow existing logging patterns
- Maintain board-specific conditionals
- Keep PSRAM enabled and huge_app partition
- Test changes on actual hardware when possible

---

## Big Picture
- PlatformIO Arduino project with two targets: `env:esp32-s3-devkitc-1` (ESP32-S3 Eye pins) and `env:esp32cam` (AI-Thinker ESP32-CAM). Camera pins and flags set via `camera_config.h` and `build_flags`.
- `src/main.cpp` is the app: WiFiManager config portal (triple-reset with NVS persistence on S3), MQTT, web UI, OTA, LittleFS device/motion config, PIR motion ISR, camera capture/stream.
- **Reset detection**: ESP32-S3 uses Preferences (NVS) for reliable triple-reset tracking across hardware resets; ESP32-CAM uses ESP_DoubleResetDetector (RTC memory).
- Web UI is embedded HTML/JS in `handleRoot` inside `main.cpp`; endpoints: `/`, `/capture`, `/stream`, `/control`, `/status`, `/update`, `/motion-control`, `/flash-control`.

## Build / Run
- Build/upload per target: `platformio run -e esp32-s3-devkitc-1 --target upload` or `-e esp32cam`.
- Serial monitor: `pio device monitor -e esp32-s3-devkitc-1` (baud 115200). Upload speed 921600.
- Huge app partition (`huge_app.csv`) and LittleFS filesystem; PSRAM required.

## Config / Secrets
- Copy `include/secrets.h.example` → `include/secrets.h`; contains WiFi/MQTT/Influx creds (gitignored).
- Device name stored in LittleFS `/device_name.txt`; motion config in `/motion_config.txt`. Use provided load/save helpers, not hard-coded strings.
- PIR pins: S3 → GPIO14; AI-Thinker → GPIO13 (`PIR_PIN` in `device_config.h`).

## Camera & Motion Behavior
- Camera defaults per board in `camera_config.cpp`: S3 uses SVGA/quality 12; ESP32-CAM uses VGA/quality 12 (QVGA fallback without PSRAM). JPEG, double buffers, `grab_mode = CAMERA_GRAB_LATEST`.
- Motion ISR sets `motionDetected`; loop debounces with `PIR_DEBOUNCE_MS` (5s), increments `motionDetectCount`, logs to Influx, triggers `captureAndPublish()` when `cameraReady`.
- Motion enable/disable persisted; `/motion-control?enabled=1|0` toggles; UI checkbox in settings maps to that endpoint.

## MQTT / Data
- Topics in `device_config.h`: `surveillance/status`, `surveillance/image`, `surveillance/motion`, `surveillance/command`. Commands handled in `mqttCallback` (`capture`, `status`, `restart`, `capture_with_image`).
- Status payload includes `motion_enabled` and `motion_count`. Image publish currently metadata-only; base64 variant available via `captureAndPublishWithImage()`.
- InfluxDB: `sensorData` metrics every 60s; `eventData` for events (including motion). Uses v2-style client; v3 requires Telegraf bridge.

## Web UI Notes
- HTML/JS inline in `main.cpp`. Uses `/status` JSON to prefill settings, `/control` for sensor params, `/motion-control` for PIR enable. Add UI controls by editing that string and matching JS handlers.
- Stream endpoint is MJPEG via `AsyncJpegStreamResponse`; capture endpoint returns JPEG buffer directly.

## Patterns / Conventions
- Use LittleFS helper functions for persistence (device name, motion) and keep them idempotent.
- Prefer existing logging patterns (`Serial.printf`, Influx event log) and debounce checks for PIR.
- Maintain board-specific conditionals with `CAMERA_MODEL_ESP32S3_EYE` / `CAMERA_MODEL_AI_THINKER` and `PIR_PIN` mapping.
- AsyncWebServer handlers keep CORS open (`Access-Control-Allow-Origin: *`).

## Gotchas
- Do not change board IDs (`esp32cam`) or camera pins; AI-Thinker pins are in `camera_config.h` default branch.
- PSRAM must remain enabled; keep `BOARD_HAS_PSRAM` and high XCLK (20MHz) as set.
- OTA uses `/update`; ensure partition stays huge_app.
- Motion ISR + debounce expects clean transitions; noisy pins may need external filtering.

## Where to Look
- `platformio.ini` for env flags/boards.
- `include/device_config.h` for topics, intervals, PIR pin.
- `include/camera_config.h` & `src/camera_config.cpp` for camera models/pins/defaults.
- `src/main.cpp` for everything else (WiFi/MQTT/web/ISR/UI/Influx).

---

## Recent Updates (December 2025)

### ESP32-S3 Triple-Reset Detection - NVS Implementation

**Major Change**: ESP32-S3 now uses NVS (Preferences library) for reset detection instead of RTC memory.

#### Why This Changed
- **Problem**: RTC fast memory (RTC_NOINIT_ATTR) doesn't persist reliably across hardware resets on ESP32-S3
- **Solution**: Switched to NVS (Non-Volatile Storage) which survives all reset types
- **Impact**: Triple-reset detection now works correctly on ESP32-S3 hardware

#### Implementation Details
```cpp
// Added to main.cpp
#include <Preferences.h>
Preferences resetPrefs;

// NVS namespace: "reset"
// Keys used:
// - crash_flag: uint32_t (0xDEADBEEF when in crash loop)
// - crash_cnt: uint32_t (incomplete boot counter)
// - reset_cnt: uint32_t (reset counter within window)
// - window: uint64_t (reset window start time in millis)
```

#### Critical Setup Timing
- `checkResetCounter()` called at **very start** of `setup()` before any delays
- Even Serial.begin() 2-second delay was preventing proper detection
- Early execution ensures reset counter updates immediately on boot

#### WiFi Mode Change
- Changed from `WIFI_STA` to `WIFI_AP_STA`
- Allows simultaneous Access Point (config portal) and Station (network connection)
- Config portal: 192.168.4.1
- Station IP: DHCP assigned (e.g., 192.168.0.206)

#### Constants (device_config.h)
- `RESET_DETECT_TIMEOUT`: 2000ms (2-second window)
- `RESET_COUNT_THRESHOLD`: 3 (resets needed to trigger portal)
- `CRASH_LOOP_THRESHOLD`: 5 (incomplete boots)
- `CRASH_LOOP_MAGIC`: 0xDEADBEEF

#### Platform Comparison
- **ESP32-S3**: Must use NVS (Preferences) - RTC memory unreliable
- **ESP32/ESP8266**: Can use RTC (ESP_DoubleResetDetector) or NVS
- **Recommendation**: Use NVS for multi-platform compatibility

#### Files Updated
- `src/main.cpp` - Added NVS implementation
- `README.md` - Added platform-specific reset detection notes
- Root `README.md` - Added ESP32-S3 distinction
- Multiple docs - Updated portal addresses and troubleshooting

#### Testing Confirmation
- ✅ Triple-reset triggers config portal correctly
- ✅ Reset counter increments: 1/3 → 2/3 → 3/3
- ✅ Portal accessible at 192.168.4.1
- ✅ Web server accessible at STA IP after connection
- ✅ Camera initializes with PSRAM detected
- ✅ MQTT connection successful

**Status**: Production-ready, tested on ESP32-S3 hardware
