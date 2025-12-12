# Camera Performance Optimizations

## Current Hardware Configuration

### Clock Frequency (XCLK)
**Current: 10 MHz** (stabilized for OV3660)
- Previously tested: 25 MHz (too aggressive, caused frame corruption)
- 10 MHz provides stable operation matching working Freenove sketch patterns
- OV2640 (ESP32-CAM) handles this stably; OV3660 (S3) requires this conservative setting

### Camera Resolution & JPEG Quality (Board-Specific)

**ESP32-S3 with OV3660:**
- Default resolution: SVGA (800x600)
- JPEG quality with PSRAM: Q10 (better quality)
- JPEG quality without PSRAM: Q12 (fallback to VGA/DRAM)
- Grab mode: `CAMERA_GRAB_LATEST` with PSRAM; `CAMERA_GRAB_WHEN_EMPTY` without

**ESP32-CAM with OV2640:**
- Default resolution: VGA (640x480)
- JPEG quality: Q10 (OV2640 encodes faster, Q10 still provides good quality)
- Grab mode: `CAMERA_GRAB_LATEST` with PSRAM; `CAMERA_GRAB_WHEN_EMPTY` without
- Fallback: HVGA (480x320) without PSRAM

### Sensor Settings
**Simplified approach** for reliability:
- **OV3660 (ESP32-S3):** Only 3 settings applied (vflip, brightness, saturation) - matches Freenove working configuration
- **OV2640 (ESP32-CAM):** Default settings - sensor is more forgiving, no aggressive tuning needed

### Previous Optimization Attempts (Superseded)
The following aggressive optimizations were tried but reverted for stability:
- Excessive sensor register tweaking (brightness, contrast, exposure, gain, color effects)
- Higher XCLK frequencies (20-25 MHz) causing frame corruption
- Hardware-specific tuning that didn't generalize

**New philosophy:** Stability > Performance. A stable 10 fps stream is better than a 25 fps stream with corrupted frames.

## Expected Performance (After Stabilization)

| Metric | Current | Notes |
|--------|---------|-------|
| Frame Rate | 10-15 fps | Stable; limited by JPEG encoding and WiFi |
| Response Time | 100-200ms | Reasonable for motion detection |
| Memory Usage | Optimized | Double buffering with PSRAM |
| CPU Usage | 60-70% | Acceptable, no excessive overhead |
| Reliability | High | Board-specific tuning prevents crashes |

## Recommended Configuration by Use Case

### Production Surveillance (Default)
```cpp
// ESP32-CAM (AI-Thinker)
config.frame_size = FRAMESIZE_VGA;      // 640x480
config.jpeg_quality = 10;                // Good balance
config.fb_count = 2;                     // Double buffering
config.grab_mode = CAMERA_GRAB_LATEST;   // (with PSRAM)

// ESP32-S3
config.frame_size = FRAMESIZE_SVGA;      // 800x600  
config.jpeg_quality = 10;                // (with PSRAM) or 12 (without)
config.fb_count = 2;
config.grab_mode = CAMERA_GRAB_LATEST;   // (with PSRAM)
```

### Low-Power / Constrained Memory
```cpp
config.frame_size = FRAMESIZE_HVGA;      // 480x320
config.jpeg_quality = 12;
config.fb_count = 1;
config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
config.fb_location = CAMERA_FB_IN_DRAM;  // No PSRAM available
```

## How to Rebuild

```bash
cd /home/aachten/PlatformIO/esp12f_ds18b20_temp_sensor/surveillance
pio run --target clean
pio run --target upload
```

## Additional Optimizations (Future)

1. **WiFi Power Management:** Disable WiFi sleep for consistent performance
2. **CPU Frequency:** Ensure running at 240MHz (default)
3. **Task Priorities:** Use RTOS tasks for camera capture
4. **Hardware JPEG Encoder:** ESP32-S3 has hardware JPEG support
5. **Network Buffer Tuning:** Optimize TCP window sizes

## Testing Performance

Access the web interface and check:
- Frame rate in browser (aim for 10-15 fps as baseline)
- Latency (click capture, measure delay - should be <500ms)
- Memory usage via `/status` endpoint
- Serial output for PSRAM detection and grab mode confirmation

## Troubleshooting

If performance is still slow:
1. Check WiFi signal strength (should be > -70 dBm)
2. Verify PSRAM is detected (check serial output)
3. Ensure CPU is at 240MHz: `ESP.getCpuFreqMHz()`
4. Monitor free heap: `ESP.getFreeHeap()`
