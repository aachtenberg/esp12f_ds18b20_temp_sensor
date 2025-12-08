# Copilot Instructions for esp-sensor-hub (surveillance)

## Big Picture
- PlatformIO Arduino project with two targets: `env:esp32-s3-devkitc-1` (ESP32-S3 Eye pins) and `env:esp32cam` (AI-Thinker ESP32-CAM). Camera pins and flags set via `camera_config.h` and `build_flags`.
- `src/main.cpp` is the app: WiFiManager config portal (double reset), MQTT, web UI, OTA, LittleFS device/motion config, PIR motion ISR, camera capture/stream, InfluxDB (v2-style client currently in use).
- Web UI is embedded HTML/JS in `handleRoot` inside `main.cpp`; endpoints: `/`, `/capture`, `/stream`, `/control`, `/status`, `/update`, `/motion-control`.

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
