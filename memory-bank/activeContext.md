# Active Context

*Last updated: December 2025*

## Current Branch
**`feature/battery-deep-sleep-optimizations`** - OTA gating & graceful MQTT disconnect (PR #10)

## Recent Milestone Completed
✅ **ESP32-S3 Triple-Reset Detection Fixed** using NVS (Non-Volatile Storage)

## Current Goals

### Immediate
1. **Review branch for merge** - All work completed and tested on hardware
2. **Tag release** - Version with ESP32-S3 support and NVS-based reset detection
3. **Monitor field deployments** - Verify NVS reliability in production

### Active Work Context
- **Platform**: ESP32-S3 (Freenove ESP32-S3 WROOM)
- **Flash Usage**: 1,176,949 bytes (37.4% of 3.1MB)
- **Status**: Fully functional, tested, documented
- **Network**: Both config portal (192.168.4.1) and station mode working
- **Camera**: OV2640 initializing successfully with PSRAM
- **MQTT**: Connected and publishing trace data

## Technical Implementation Summary

### What Changed
1. **Reset Detection Storage**: RTC memory → NVS (Preferences library)
   - Keys: `crash_flag`, `crash_cnt`, `reset_cnt`, `window`
   - Namespace: "reset"
   - Survives all reset types including hardware resets

2. **Setup Timing**: Moved `checkResetCounter()` to start of `setup()` before delays

3. **WiFi Mode**: `WIFI_STA` → `WIFI_AP_STA` for simultaneous AP/Station operation

4. **Logging**: Enhanced to show both STA and AP IP addresses

### Files Modified
- `surveillance/src/main.cpp` - Core NVS implementation
- `surveillance/README.md` - Complete documentation update
- Root `README.md` - Platform-specific notes added
- `docs/solar-monitor/README.md` - Portal address corrections
- 6 files sanitized for security (IPs, usernames removed)

## Current State

### Hardware Status
- ✅ ESP32-S3 surveillance camera fully operational
- ✅ Triple-reset detection confirmed working
- ✅ Config portal accessible at 192.168.4.1
- ✅ Web server streaming video successfully
- ✅ MQTT connection established and publishing
- ✅ PSRAM detected and utilized

### Code Status
- ✅ All code changes compiled successfully
- ✅ No errors or warnings
- ✅ Testing completed on hardware
- ✅ Serial monitor shows proper reset counting (1/3 → 2/3 → 3/3)

### Documentation Status
- ✅ All READMEs updated with NVS implementation
- ✅ Platform-specific notes added (ESP32-S3 vs ESP32/ESP8266)
- ✅ Troubleshooting sections enhanced
- ✅ Copilot instructions updated

### Security Status
- ✅ No hardcoded IPs in example files
- ✅ No usernames in scripts
- ✅ No secrets or tokens exposed
- ✅ .gitignore properly configured
- ✅ CI/CD workflow validates security

## Platform-Specific Knowledge

### ESP32-S3 Limitations Discovered
**Critical**: ESP32-S3 RTC fast memory (RTC_NOINIT_ATTR) does **not** persist reliably across hardware resets, unlike ESP32 and ESP8266.

**Solution**: Use NVS (Non-Volatile Storage) via Preferences library for guaranteed persistence.

### Implementation Pattern for ESP32-S3
```cpp
#include <Preferences.h>
Preferences resetPrefs;

void checkResetCounter() {
  resetPrefs.begin("reset", false);
  uint32_t resetCount = resetPrefs.getUInt("reset_cnt", 0);
  // ... detection logic ...
  resetPrefs.putUInt("reset_cnt", newCount);
  resetPrefs.end();
}
```

### When to Use NVS vs RTC
- **ESP32-S3**: Always NVS (Preferences)
- **ESP32/ESP8266**: Can use RTC (faster) or NVS (more reliable)
- **Multi-platform**: Use NVS for consistency

## Dependencies

### Core Libraries
- PlatformIO Core 6.1.x
- Arduino ESP32 framework 2.0.x
- WiFiManager 2.0.17
- ArduinoJson 6.x
- PubSubClient (MQTT) 2.8.x
- Preferences library (built-in ESP32)

### Hardware
- ESP32-S3 WROOM (8MB Flash, 8MB PSRAM)
- OV2640 Camera
- PIR motion sensor (GPIO14 for S3)

## Current Blockers

**None** - All work completed successfully

## Next Actions

1. **Immediate**: Merge `feature/mqtt-trace-instrumentation` to main
2. **Short-term**: Test on additional ESP32-S3 boards
3. **Long-term**: Consider ESP32-C6 support (WiFi 6)