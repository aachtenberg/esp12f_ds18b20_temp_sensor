# GitHub Copilot Documentation Guidelines

## ⚠️ MANDATORY: Read First, Code Second

**Before making ANY code changes or suggesting modifications:**

### Required Reading Order:
1. **Read relevant documentation first**:
   - `/docs/reference/PLATFORM_GUIDE.md` - Understand architecture and platform
   - `/docs/reference/CONFIG.md` - Review configuration patterns
   - Project-specific READMEs (e.g., `surveillance/README.md`)
   - `memory-bank/activeContext.md` - Current project state
   - `memory-bank/progress.md` - Recent changes and decisions

2. **Read the actual code**:
   - Use `read_file` to examine files you'll modify
   - Use `grep_search` or `semantic_search` to understand patterns
   - Review related files (headers, configs, dependencies)
   - Check existing implementations before creating new ones

3. **Understand platform-specific requirements**:
   - ESP32-S3 vs ESP32 vs ESP8266 differences
   - NVS vs RTC memory considerations
   - Hardware-specific configurations

### ❌ Do NOT:
- Make changes based on assumptions or general knowledge
- Modify code without reading existing implementation
- Ignore platform-specific instructions in documentation
- Create new patterns when existing patterns should be followed
- Skip reading memory-bank context files
- **Copy secrets.h.example to secrets.h without checking if secrets.h already exists**
- **Overwrite existing secrets with example values**
- **Hardcode OTA passwords or credentials in platformio.ini build/upload flags**
- **Use `${sysenv.VAR}` syntax in platformio.ini - it doesn't work reliably with .env files**

### ✅ DO:
- Read documentation thoroughly before suggesting changes
- Examine existing code patterns and follow them
- Check memory-bank for recent decisions and context
- Ask clarifying questions if documentation is unclear
- Reference specific documentation sections in your responses
- **Always check if `secrets.h` exists before suggesting to copy from `secrets.h.example`**
- **Preserve existing secrets.h values when updating credentials**
- **Only use `secrets.h.example` as a template for new projects or missing files**
- **Use environment variable exports for OTA uploads: `export PLATFORMIO_UPLOAD_FLAGS="--auth=password" && pio run -t upload`**
- **Keep platformio.ini clean - no hardcoded credentials or upload flags**
- **ALWAYS bump firmware version before building/deploying: `cd temperature-sensor && ./update_version.sh --patch`**

---

## Critical: Consolidated Documentation Structure

**The project documentation has been consolidated to eliminate redundancy and improve maintainability.**

## ✅ ONLY Update These 3 Files:

### 1. `/docs/reference/PLATFORM_GUIDE.md` - **Primary Documentation**
- **Purpose**: Main platform documentation, architecture, quick start, benefits
- **When to Update**: Architecture changes, new features, platform modifications
- **Content**: Overview, architecture diagrams, project support, deployment basics
- **Target Audience**: New users, architects, decision makers

### 2. `/docs/reference/CONFIG.md` - **Configuration Reference**  
- **Purpose**: Technical configuration details, deployment commands, troubleshooting
- **When to Update**: New deployment options, credential changes, troubleshooting procedures
- **Content**: secrets.h setup, deployment scripts, data queries, detailed troubleshooting
- **Target Audience**: Developers, operators, troubleshooters

### 3. `/README.md` - **Project Entry Point**
- **Purpose**: Project overview, quick start, navigation to detailed docs
- **When to Update**: Project structure changes, new project types, quick start modifications
- **Content**: Project list, system overview, quick start commands, documentation links
- **Target Audience**: Repository visitors, new contributors

## ❌ DO NOT Update These Files:

- **`docs/hardware/OLED_DISPLAY_GUIDE.md`** - Hardware integration reference, only update when display tech changes
- **`docs/pcb_design/`** - PCB design documentation, separate concern
- **`docs/solar-monitor/`** - Solar-specific documentation
- **Any other documentation files** - Changes should go in the 3 primary files above

## Update Strategy by Change Type:

### Architecture Changes
- **Primary**: Update `PLATFORM_GUIDE.md` architecture section and diagrams
- **Secondary**: Update `README.md` system overview if significant
- **Reference**: Update `CONFIG.md` only if deployment procedures change

### New Features  
- **Primary**: Add to `PLATFORM_GUIDE.md` features and benefits sections
- **Secondary**: Update `README.md` if it affects quick start or project list
- **Reference**: Update `CONFIG.md` if new configuration is required

### Configuration Changes
- **Primary**: Update `CONFIG.md` with new setup procedures
- **Secondary**: Update `PLATFORM_GUIDE.md` if it affects architecture
- **Reference**: Update `README.md` quick start if commands change

### Deployment Changes
- **Primary**: Update `CONFIG.md` deployment commands section
- **Secondary**: Update `PLATFORM_GUIDE.md` deployment overview
- **Reference**: Update `README.md` quick start commands

## Documentation Principles:

### ✅ Do This:
- **Single Source of Truth**: Each piece of information should exist in only one place
- **Clear Hierarchy**: README.md → PLATFORM_GUIDE.md → CONFIG.md (general to specific)
- **Cross-Reference**: Link between files but don't duplicate content
- **Update Consistently**: When changing architecture, update all 3 files appropriately

### ❌ Don't Do This:
- **Duplicate Information**: Don't repeat the same content in multiple files
- **Create New Docs**: Don't create additional reference documentation
- **Fragment Updates**: Don't update only one file when changes affect multiple
- **Ignore Hierarchy**: Don't put detailed config in README.md or basic overview in CONFIG.md

## Content Guidelines by File:

### PLATFORM_GUIDE.md Content:
```
✅ Architecture diagrams and explanations
✅ Platform overview and benefits  
✅ Project type support matrix
✅ Basic deployment workflow
✅ High-level troubleshooting
✅ Key features and capabilities
```

### CONFIG.md Content:
```
✅ Common configuration across all projects
✅ secrets.h setup examples
✅ WiFi/MQTT broker setup
✅ Deployment commands (USB flash, OTA)
✅ Common troubleshooting procedures
✅ Links to project-specific configs:
   - docs/temperature-sensor/CONFIG.md
   - docs/surveillance/CONFIG.md
   - docs/solar-monitor/CONFIG.md
```

### README.md Content:
```
✅ Project list and status
✅ System architecture overview
✅ Quick start commands
✅ Documentation navigation
✅ Hardware requirements
✅ Basic project description
```

## Maintenance Workflow:

1. **Identify Change Type**: Architecture, feature, configuration, or deployment
2. **Select Primary File**: Choose the most appropriate file for the main content
3. **Update Related Files**: Ensure consistency across all 3 files
4. **Verify Links**: Check that cross-references still work
5. **Test User Journey**: Ensure new users can follow README → PLATFORM_GUIDE → CONFIG

## Quality Checklist:

- [ ] Information exists in only one authoritative location
- [ ] Cross-references between files are accurate
- [ ] User can navigate: README → PLATFORM_GUIDE → CONFIG logically  
- [ ] No outdated architecture references (CloudWatch, AWS, etc.)
- [ ] All WiFiManager portal and InfluxDB architecture is current
- [ ] Quick start commands in README match detailed commands in CONFIG.md

---

**Remember**: The goal is **maintainable, non-redundant documentation** that provides a clear user journey from project discovery to detailed configuration. Always consider which of the 3 files is the most appropriate home for new information.

**File Consolidation Completed**: November 24, 2025  
**Previous Files Removed**: COPILOT_INSTRUCTIONS.md, PROJECT_SUMMARY.md, SECRETS_SETUP.md, COMPLETION_SUMMARY.txt  
**Current Structure**: 3-file focused documentation with clear responsibilities

---

## Platform-Specific Implementation Notes

### ESP32-S3 Reset Detection (December 2025)

**Critical Implementation Detail**: ESP32-S3 architecture requires NVS (Non-Volatile Storage) for reset detection, **not** RTC memory.

#### Why NVS is Required for ESP32-S3
- RTC fast memory (RTC_NOINIT_ATTR) does **not** persist reliably across hardware resets on ESP32-S3
- This differs from ESP32 and ESP8266 where RTC memory works correctly for reset detection
- NVS provides guaranteed persistence across all reset types (hardware, software, power cycle)

#### Implementation Pattern
```cpp
#include <Preferences.h>

Preferences resetPrefs;

void checkResetCounter() {
  resetPrefs.begin("reset", false);  // namespace "reset", read-write mode
  
  // NVS keys used:
  // - crash_flag: uint32_t (0xDEADBEEF when in crash loop)
  // - crash_cnt: uint32_t (incomplete boot counter)
  // - reset_cnt: uint32_t (reset counter within detection window)
  // - window: uint64_t (reset window start time in milliseconds)
  
  uint32_t resetCount = resetPrefs.getUInt("reset_cnt", 0);
  uint64_t resetWindow = resetPrefs.getULong64("window", 0);
  
  // ... reset detection logic ...
  
  resetPrefs.putUInt("reset_cnt", newCount);
  resetPrefs.putULong64("window", newWindow);
  resetPrefs.end();
}
```

#### Key Configuration Constants
```cpp
// In device_config.h
#define RESET_DETECT_TIMEOUT 2000      // 2 second window for reset detection
#define RESET_COUNT_THRESHOLD 3         // 3 resets to trigger config portal
#define CRASH_LOOP_THRESHOLD 5          // 5 incomplete boots to trigger portal
#define CRASH_LOOP_MAGIC 0xDEADBEEF     // Magic number for crash detection
```

#### Critical Setup Timing
- **Must** call `checkResetCounter()` at the **very start** of `setup()` before any delays
- Even a 2-second Serial.begin() delay can prevent proper reset detection
- Early execution ensures the reset counter starts immediately on boot

```cpp
void setup() {
  checkResetCounter();  // FIRST thing in setup() - before any delays
  
  Serial.begin(115200);
  delay(2000);  // Now safe to delay after reset check
  // ... rest of setup ...
}
```

#### WiFi Mode for Config Portal
- Use `WIFI_AP_STA` mode, not `WIFI_STA` alone
- Allows simultaneous Access Point (config portal) and Station (network connection) operation
- Config portal accessible at: **192.168.4.1**
- Main application accessible at Station IP (DHCP assigned)

```cpp
void setupWiFi() {
  WiFi.mode(WIFI_AP_STA);  // Critical for simultaneous AP and STA
  // ... WiFiManager setup ...
}
```

#### Platform Selection Guide
- **ESP32-S3**: Always use NVS (Preferences library) - required for reliability
- **ESP32 (original)**: Can use RTC (ESP_DoubleResetDetector) or NVS - both work
- **ESP8266**: Can use RTC (ESP_DoubleResetDetector) or NVS - both work
- **Multi-platform projects**: Use NVS for consistency across all ESP32 variants

#### Files to Reference
- **Implementation**: `surveillance/src/main.cpp` - Complete NVS implementation with `checkResetCounter()` and `clearCrashLoop()`
- **Configuration**: `surveillance/include/device_config.h` - Reset detection constants
- **Documentation**: `surveillance/README.md` - Detailed reset detection section with platform notes
- **Copilot Notes**: `surveillance/.github/copilot-instructions.md` - Additional implementation details

#### Verification Steps
1. Flash firmware to ESP32-S3
2. Open serial monitor (115200 baud)
3. Press reset button 3 times within 2 seconds
4. Should see: `[RESET] Reset count: 1/3`, `[RESET] Reset count: 2/3`, `[RESET] Reset count: 3/3 - Starting portal!`
5. Config portal should start at 192.168.4.1

**Implementation Completed**: December 2025  
**Tested On**: Freenove ESP32-S3 WROOM (8MB Flash, 8MB PSRAM)  
**Status**: Production-ready, fully functional

---

## MQTT Buffer Size Configuration (December 2025)

**Critical Configuration**: All ESP devices must include increased MQTT buffer size to prevent silent publish failures.

### Root Cause
PubSubClient library defaults to 128-byte buffer, but JSON payloads exceed this limit:
- Temperature messages: ~350+ bytes
- Status messages: ~350+ bytes  
- Event messages: ~300+ bytes

### Symptoms of Buffer Issues
- Device reports `mqtt_publish_failures: 0` (no recorded failures)
- Status messages work but temperature messages missing
- Device appears connected but data not reaching broker
- `mqttClient.publish()` returns `true` but payload truncated

### Required Configuration
**All PlatformIO environments must include**:
```ini
build_flags =
    -D MQTT_MAX_PACKET_SIZE=2048  # ESP32: Increased for battery monitoring payloads
    -D MQTT_MAX_PACKET_SIZE=512   # ESP8266: Standard size sufficient
    # ... other flags ...
```

### Affected Environments
- `esp32dev` - ESP32 with display
- `esp32dev-serial` - ESP32 serial upload  
- `esp8266` - ESP8266 API-only
- `esp32s3` - ESP32-S3

### Verification Steps
1. Check platformio.ini has `-D MQTT_MAX_PACKET_SIZE=2048` for ESP32 and `=512` for ESP8266 in all environments
2. Monitor MQTT broker: `mosquitto_sub -h broker -t "esp-sensor-hub/#" -v`
3. Verify both temperature and status messages appear
4. Check device health shows recent MQTT activity

### Files to Reference
- **Configuration**: `temperature-sensor/platformio.ini` - Buffer size flags
- **Implementation**: `temperature-sensor/src/main.cpp` - JSON payload generation
- **Documentation**: `docs/reference/CONFIG.md` - Troubleshooting section
- **Testing**: Use health API `/health` to verify `mqtt_seconds_ago` updates

### Prevention
- Always include buffer size flag in new environments
- Test MQTT publishing after firmware changes
- Monitor for missing temperature data vs status data

**Issue Discovered**: December 2025  
**Platforms Affected**: All ESP8266/ESP32 devices  
**Status**: Fixed, documented, required for all builds

---

## WSL2 OTA Upload Issues (December 2025)

**Critical Development Issue**: OTA uploads fail on WSL2/Windows with "No response from device" after authentication succeeds.

### Root Cause
Windows Firewall blocks ESP32 OTA port (3232) from WSL2 network traffic, even when firewall rules appear correctly configured.

### Symptoms
- `pio run -t upload` shows authentication success but then "No response from device"
- Device shows "OTA:ready" in serial monitor
- Ping works: `ping YOUR_DEVICE_IP` succeeds
- Port test fails: `nc -zv YOUR_DEVICE_IP 3232` shows "Connection refused"

### PlatformIO Configuration (Must Be Present)
```ini
[env:esp32dev]
upload_protocol = espota
upload_port = YOUR_DEVICE_IP  # Device IP address
upload_flags = 
    --auth=YOUR_OTA_PASSWORD          # Must match OTA_PASSWORD in secrets.h
    --port=3232              # ESP32 OTA port
```

### Solution Options

#### Option 1: Temporary Firewall Disable (Recommended for Development)
```powershell
# Run in Windows PowerShell as Administrator
Set-NetFirewallProfile -Profile Private -Enabled False
# Run OTA upload
pio run -e esp32dev -t upload
# Re-enable firewall
Set-NetFirewallProfile -Profile Private -Enabled True
```

#### Option 2: Create Permanent Firewall Rule
1. Open Windows Firewall with Advanced Security
2. Create new Inbound Rule:
   - Rule Type: Port
   - TCP Port: 3232
   - Action: Allow connection
   - Profile: Private
   - Name: ESP32 OTA

### Verification Steps
1. Confirm device is running: `pio device monitor` shows "OTA:ready"
2. Test network: `ping YOUR_DEVICE_IP` works
3. Test port access: `nc -zv YOUR_DEVICE_IP 3232` succeeds after firewall adjustment
4. Run upload: `pio run -e esp32dev -t upload` completes successfully

### Code Requirements
- ArduinoOTA library must be included and configured
- OTA_PASSWORD defined in secrets.h (default: "YOUR_OTA_PASSWORD")
- Device hostname set via ArduinoOTA.setHostname()
- OTA handler calls in main loop: `ArduinoOTA.handle()`

### Files to Reference
- **Configuration**: `temperature-sensor/platformio.ini` - Correct OTA settings
- **Implementation**: `temperature-sensor/src/main.cpp` - ArduinoOTA setup
- **Secrets**: `temperature-sensor/include/secrets.h` - OTA_PASSWORD definition
- **Documentation**: `docs/reference/CONFIG.md` - Complete troubleshooting section

### Prevention
- Always test OTA after initial device setup
- Document device IP addresses for team reference
- Include firewall note in development environment setup

**Issue Discovered**: December 2025  
**Platforms Affected**: WSL2 on Windows 10/11  
**Status**: Documented, workaround available

---

## PlatformIO Configuration Best Practices (December 2025)

**CRITICAL: Never hardcode credentials or authentication in platformio.ini**

### ❌ WRONG - Do NOT Do This:
```ini
[env:esp32dev]
upload_protocol = espota
upload_flags =
    --auth=hardcoded_password  # NEVER hardcode credentials!
    --port=3232
```

### ✅ CORRECT - Use Environment Variables:
```bash
# Export OTA password before upload command
export PLATFORMIO_UPLOAD_FLAGS="--auth=your_password" && pio run -e esp32dev -t upload --upload-port 192.168.0.x
```

### Why This Approach?
1. **Security**: Credentials not committed to git
2. **Flexibility**: Different passwords per developer/device
3. **Safety**: No accidental credential exposure in diffs/logs
4. **Simplicity**: Works reliably without .env file parsing issues

### platformio.ini Configuration
Keep upload configuration minimal and credential-free:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 115200
upload_protocol = espota
; upload_port and upload_flags configured via environment variables
; Use: export PLATFORMIO_UPLOAD_FLAGS="--auth=password" && pio run -t upload --upload-port IP
```

### Common Mistakes to Avoid
1. **Using `${sysenv.VAR}` in platformio.ini**: Doesn't work reliably with .env files
2. **Hardcoding `upload_flags` in platformio.ini**: Creates security risks
3. **Committing credentials to version control**: Major security violation
4. **Assuming .env file location**: PlatformIO .env handling is inconsistent

### Deployment Scripts
Create helper scripts in `scripts/` that handle authentication:
```bash
#!/bin/bash
# scripts/deploy-ota.sh
DEVICE_NAME=$1
DEVICE_IP=$2
OTA_PASSWORD=${OTA_PASSWORD:-"default_password"}

export PLATFORMIO_UPLOAD_FLAGS="--auth=$OTA_PASSWORD"
pio run -e esp32dev -t upload --upload-port $DEVICE_IP
```

**Issue Identified**: December 24, 2025  
**Status**: Best practices documented, enforcement via copilot-instructions.md

---

## Firmware Version Tracking (December 2025)

**All ESP32 devices now include automatic firmware version tracking** for deployment management and OTA verification.

### Version Format
```
MAJOR.MINOR.PATCH-buildYYYYMMDD
Example: 1.0.3-build20251222
```

### Implementation Details

#### Build Configuration (platformio.ini)
```ini
build_flags =
    -D CPU_FREQ_MHZ=80
    -D WIFI_PS_MODE=WIFI_PS_MIN_MODEM
    -D FIRMWARE_VERSION_MAJOR=1
    -D FIRMWARE_VERSION_MINOR=0
    -D FIRMWARE_VERSION_PATCH=2
    -D BUILD_TIMESTAMP=20251222
```

#### Version Header (include/version.h)
```cpp
#define FIRMWARE_VERSION_STRING String(FIRMWARE_VERSION_MAJOR) + "." + String(FIRMWARE_VERSION_MINOR) + "." + String(FIRMWARE_VERSION_PATCH) + "-build" + String(BUILD_TIMESTAMP)
inline String getFirmwareVersion() { return FIRMWARE_VERSION_STRING; }
```

#### MQTT Integration
All MQTT messages include `firmware_version` field:
```json
{
  "device": "Temp Sensor",
  "firmware_version": "1.0.3-build20251222",
  "current_temp_c": 23.5
}
```

### Version Update Process

#### Automatic Version Bumping Script
**Always use `update_version.sh` before building to ensure consistent versioning across all environments.**

The script automatically updates version across ALL platformio.ini environments:

```bash
cd temperature-sensor

# Bump patch version (bug fixes) - 1.0.5 → 1.0.6
./update_version.sh --patch

# Bump minor version (features) - 1.0.5 → 1.1.0
./update_version.sh --minor

# Bump major version (breaking changes) - 1.0.5 → 2.0.0
./update_version.sh --major

# Or just update timestamp to current date (keep same version)
./update_version.sh
```

**Build Process:**
1. Run `./update_version.sh --patch` (or appropriate bump level)
2. Run `pio run -e esp32dev -t upload --upload-port 192.168.x.x`
3. Verify device reports new version in MQTT status

**What the script does:**
- Updates `FIRMWARE_VERSION_MAJOR`, `FIRMWARE_VERSION_MINOR`, `FIRMWARE_VERSION_PATCH` in all environments
- Updates `BUILD_TIMESTAMP` to current date (YYYYMMDD format)
- Maintains consistency across esp32dev, esp32dev-serial, esp8266, esp32s3, etc.
- Produces version string: `MAJOR.MINOR.PATCH-buildYYYYMMDD`

### OTA Version Tracking

**Before OTA**: Device reports current version  
**OTA Start**: Publishes `ota_start` event with current version  
**OTA Complete**: Publishes `ota_complete` event with new version  
**After Reboot**: All messages show updated version  

### Verification Commands
```bash
# Check current version in MQTT messages
mosquitto_sub -h localhost -t "home/temperature-sensor/+/temperature"

# Update version before build
cd temperature-sensor && ./update_version.sh

# Build and upload
pio run -e esp32dev -t upload
```

### Files to Reference
- **Version Header**: `temperature-sensor/include/version.h`
- **Build Config**: `temperature-sensor/platformio.ini` 
- **Update Script**: `temperature-sensor/update_version.sh`
- **Implementation**: `temperature-sensor/src/main.cpp` (getFirmwareVersion() usage)
- **Documentation**: `temperature-sensor/README_VERSION.md`

### Benefits
- **OTA Success Verification**: Confirm updates by checking version changes in MQTT
- **Device Inventory**: Track which firmware versions are deployed
- **Debugging**: Correlate issues with specific firmware versions
- **Compliance**: Version tracking for regulatory requirements

**Implementation Date**: December 2025  
**Status**: Active, required for all new builds


### Surveillance-arduino project - Arduino CLI Build

**CRITICAL: Partition Table Alignment**

The COMPILE_ESP32S3.sh script uses `--output-dir ESP32CAM_Surveillance/build` which generates the correct `huge_app` partition scheme:
- App: 0x10000, 3MB
- **SPIFFS/LittleFS: 0x310000, 896KB** (NOT 0x290000)

**MUST follow this workflow:**
1. Compile: `./COMPILE_ESP32S3.sh` (uses --output-dir, generates huge_app partitions)
2. Upload firmware: `./bin/arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app,FlashMode=qio ESP32CAM_Surveillance`
3. Upload LittleFS: `./UPLOAD_LITTLEFS.sh /dev/ttyACM0 s3` (uses 0x310000 address to match huge_app)
4. Both uploads use MATCHING partition tables - DO NOT MIX

**Why this matters:**
- Without --output-dir: Arduino CLI uses default partition table (SPIFFS at 0x009000, only 20KB)
- With --output-dir: Arduino CLI uses huge_app partition table (SPIFFS at 0x310000, 896KB)
- UPLOAD_LITTLEFS.sh MUST use the same address as the compiled firmware

For more details, see:
- surveillance-arduino/ARDUINO_CLI_SETUP.md
- surveillance-arduino/BUILD_SYSTEMS.md
- surveillance-arduino/README.md

---

## ESP32 Deep Sleep Mode (December 2025)

**ESP32 devices support battery-powered deep sleep mode with automatic wake cycles**. This is critical for remote, battery-powered temperature sensors.

### Critical Implementation Requirements

**Two fixes required for ESP32 deep sleep to work**:

1. **WiFi/MQTT Disconnect Before Sleep** ([main.cpp:576-582](../temperature-sensor/src/main.cpp#L576-L582))
   - Must call `WiFi.disconnect(true)` and `mqttClient.disconnect()` before `esp_deep_sleep_start()`
   - Without this, RTC timer may not properly configure or wake device
   - Device will enter deep sleep but **never wake** from timer

2. **MQTT Command Processing Window** ([main.cpp:1023-1029](../temperature-sensor/src/main.cpp#L1023-L1029))
   - Must add 2-second `mqttClient.loop()` delay after publishing before entering sleep
   - Allows device to receive and process MQTT commands during wake cycle
   - Without this, remote configuration via MQTT is **impossible**

### Implementation Pattern

```cpp
void enterDeepSleepIfEnabled() {
  if (deepSleepSeconds > 0) {
    #ifdef ESP32
      // CRITICAL: Disconnect WiFi/MQTT before sleep
      Serial.println("[DEEP SLEEP] Disconnecting MQTT and WiFi...");
      if (mqttClient.connected()) {
        mqttClient.disconnect();
      }
      WiFi.disconnect(true);  // true = turn off WiFi radio
      delay(100);
    #endif

    Serial.flush();
    delay(50);

    #ifdef ESP32
      uint64_t sleepTime = deepSleepSeconds * 1000000ULL;
      esp_sleep_enable_timer_wakeup(sleepTime);
      esp_deep_sleep_start();
    #endif
  }
}
```

```cpp
void setup() {
  // ... WiFi and MQTT connection ...

  if (deepSleepSeconds > 0) {
    // Skip OTA/web server in deep sleep mode
    updateTemperatures();
    publishTemperature();
    publishStatus();

    // CRITICAL: Wait for MQTT commands before sleeping
    Serial.println("[DEEP SLEEP] Waiting 5 seconds for MQTT commands...");
    unsigned long commandWaitStart = millis();
    while (millis() - commandWaitStart < 5000) {
      mqttClient.loop();  // Process incoming MQTT messages
      delay(10);
    }

    // Now safe to enter deep sleep
    enterDeepSleepIfEnabled();
  }
}
```

### Deep Sleep Behavior

**When enabled** (`deepSleepSeconds > 0`):
1. Device wakes from RTC timer
2. Connects to WiFi and MQTT (2-3 seconds)
3. Publishes temperature and status
4. **Waits 5 seconds** processing MQTT commands
5. Disconnects WiFi/MQTT cleanly
6. Enters deep sleep
7. **No web server** or OTA during sleep cycles

**Power Profile**:
- Active: ~80mA for 8-9 seconds (including 5s command window)
- Sleep: ~10µA
- Average (30s cycle): ~3mA

### Remote Configuration via MQTT

**Available Commands**:
```bash
# Enable deep sleep (30 seconds)
mosquitto_pub -h BROKER -t "esp-sensor-hub/DEVICE/command" -m "deepsleep 30"

# Disable deep sleep
mosquitto_pub -h BROKER -t "esp-sensor-hub/DEVICE/command" -m "deepsleep 0"

# Restart device
mosquitto_pub -h BROKER -t "esp-sensor-hub/DEVICE/command" -m "restart"

# Request status
mosquitto_pub -h BROKER -t "esp-sensor-hub/DEVICE/command" -m "status"
```

**MQTT Topics**:
- Command: `esp-sensor-hub/{DEVICE}/command`
- Status: `esp-sensor-hub/{DEVICE}/status`
- Events: `esp-sensor-hub/{DEVICE}/events`
- Temperature: `esp-sensor-hub/{DEVICE}/temperature`

### Troubleshooting

**Device won't wake from deep sleep**:
- Missing WiFi/MQTT disconnect before `esp_deep_sleep_start()`
- Check serial for: `[DEEP SLEEP] Disconnecting MQTT and WiFi...`
- Monitor MQTT for temperature publishes (proves wake cycles work)
- Solution: Add disconnect calls before sleep

**Cannot configure device remotely**:
- Missing `mqttClient.loop()` processing window after wake
- Commands sent but device sleeps before processing
- Check serial for: `[DEEP SLEEP] Waiting 5 seconds for MQTT commands...`
- Solution: Add 5-second loop delay before entering sleep

**Configuration not persisting**:
- Deep sleep config saves to SPIFFS/LittleFS
- Survives firmware updates (unless filesystem erased)
- Requires device restart to take effect
- Use MQTT `restart` command after configuration change

### ESP8266 Limitations

**ESP8266 deep sleep is DISABLED by default** (`DISABLE_DEEP_SLEEP=1` in platformio.ini):
- Requires GPIO 16 → RST hardware modification
- Without modification, device enters permanent sleep
- Do NOT enable deep sleep on ESP8266 without hardware mod
- See CONFIG.md for required circuit diagram

### Files to Reference

- **Implementation**: `temperature-sensor/src/main.cpp`
  - Deep sleep entry: `enterDeepSleepIfEnabled()` (lines 548-603)
  - MQTT processing: `setup()` deep sleep branch (lines 1023-1036)
- **Configuration**: `temperature-sensor/platformio.ini`
  - ESP8266: `-D DISABLE_DEEP_SLEEP=1`
  - ESP32: No flag (deep sleep available)
- **Documentation**: `docs/temperature-sensor/CONFIG.md`
  - Complete deep sleep section with all commands
  - Troubleshooting procedures
  - Power consumption details

### Verification Steps

1. Enable deep sleep: `mosquitto_pub -h BROKER -t "esp-sensor-hub/Spa/command" -m "deepsleep 30"`
2. Watch events: `mosquitto_sub -h BROKER -t "esp-sensor-hub/Spa/events" -v`
3. Confirm message: `{"event":"deep_sleep_config","message":"Deep sleep set to 30 seconds via MQTT"}`
4. Monitor wake cycles: `mosquitto_sub -h BROKER -t "esp-sensor-hub/Spa/#" -v`
5. See temperature publishes every ~30 seconds
6. Check serial output for: `*** WOKE FROM DEEP SLEEP (TIMER) ***`

**Issue Discovered**: December 23, 2025
**Root Causes**: Missing WiFi disconnect, missing MQTT command processing
**Status**: Fixed, tested, documented
**Platforms**: ESP32 only (ESP8266 disabled by default)