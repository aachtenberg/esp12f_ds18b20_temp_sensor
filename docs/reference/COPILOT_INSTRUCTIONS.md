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

### ✅ DO:
- Read documentation thoroughly before suggesting changes
- Examine existing code patterns and follow them
- Check memory-bank for recent decisions and context
- Ask clarifying questions if documentation is unclear
- Reference specific documentation sections in your responses
- **Always check if `secrets.h` exists before suggesting to copy from `secrets.h.example`**
- **Preserve existing secrets.h values when updating credentials**
- **Only use `secrets.h.example` as a template for new projects or missing files**

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
✅ secrets.h setup examples
✅ Detailed deployment commands
✅ Data query examples
✅ Specific troubleshooting procedures
✅ Configuration file formats
✅ API reference examples
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
