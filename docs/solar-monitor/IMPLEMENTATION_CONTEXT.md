# ESP32 Solar Monitor - Implementation Context

**Date**: November 22, 2025  
**Project**: WiFi-enabled solar monitoring system for Victron equipment  
**Repository**: `aachtenberg/esp12f_ds18b20_temp_sensor` → `esp-iot-sensors`

---

## Table of Contents
1. [Project Overview](#project-overview)
2. [Hardware Decisions](#hardware-decisions)
3. [System Architecture](#system-architecture)
4. [Repository Integration Strategy](#repository-integration-strategy)
5. [Implementation Requirements](#implementation-requirements)
6. [VE.Direct Protocol Details](#vedirect-protocol-details)
7. [Code Architecture](#code-architecture)
8. [Success Criteria](#success-criteria)

---

## Project Overview

### Goal
Add WiFi connectivity and remote monitoring to a solar-powered generator setup that previously lacked these features. The system needs to provide real-time visibility into:
- Battery state-of-charge
- Solar production
- Charging states
- Energy yield statistics
- Time-remaining estimates

### User Context (Andrew)
- Experienced embedded systems developer
- Prefers direct hardware control without abstraction layers
- Uses PlatformIO with C/C++ (not ESPHome or YAML approaches)
- Has established patterns from temperature sensor projects
- Works within Docker-based Raspberry Pi infrastructure
- Follows consistent patterns across IoT monitoring solutions

### Existing Infrastructure
- Repository: `aachtenberg/esp12f_ds18b20_temp_sensor`
- Development: PlatformIO with VSCode
- Data collection: Docker-based Raspberry Pi system
- Pattern: Reusable C++ classes for sensor integration

---

## Hardware Decisions

### Final Hardware Selection

**Battery Monitor**: Victron SmartShunt SHU050150050
- Model: SHU050150050 (500A / 50mV shunt)
- Protocol: VE.Direct ASCII
- Communication: Single TX line (read-only monitoring)
- Baud rate: 19200
- Voltage: 3.3V logic compatible
- Professional-grade battery monitoring with comprehensive data

**Charge Controller**: Victron SmartSolar MPPT SCC110050210
- Model: MPPT 100/50 (100V, 50A)
- Protocol: VE.Direct ASCII
- Communication: Single TX line (read-only monitoring)
- Baud rate: 19200
- Voltage: 3.3V logic compatible
- Already owned and installed

**Microcontroller**: ESP32-WROOM-32
- Dual UART ports for simultaneous device monitoring
- WiFi capability for remote access
- 3.3V logic levels (direct connection to VE.Direct)
- Sufficient GPIO pins for expansion

**Power Supply**: 12V-to-5V 3A Waterproof Micro-USB Converter
- Source: Amazon.ca
- Integrates with existing DC fuse box
- Waterproof for outdoor installation
- Provides stable 5V for ESP32

### Hardware NOT Used
- **Renogy RBM500-CA**: Initially considered but rejected (display-only, no communication ports)
- **MAX485 RS485 adapter**: Not needed (both devices use VE.Direct, not RS485)
- **MAX3232 level shifter**: Not needed (VE.Direct is already 3.3V logic)

---

## System Architecture

### Physical Connections

```
┌─────────────────────────────────────────────────────────┐
│                     ESP32-WROOM-32                      │
│                                                         │
│  UART2 (SmartShunt):                                   │
│    GPIO 16 (RX) ←─── SmartShunt TX (white wire)       │
│    GND          ←─── SmartShunt GND (black wire)      │
│                                                         │
│  UART1 (MPPT):                                         │
│    GPIO 19 (RX) ←─── MPPT TX (white wire)             │
│    GND          ←─── MPPT GND (black wire)            │
│                                                         │
│  Power:                                                 │
│    Micro-USB    ←─── 12V-to-5V converter              │
│                                                         │
└─────────────────────────────────────────────────────────┘
          │                           │
          ↓                           ↓
    SmartShunt               MPPT Charge Controller
    (Battery Monitor)        (Solar Input)
```

### Communication Details

**Both devices use VE.Direct protocol**:
- **Baud rate**: 19200, 8N1
- **Direction**: TX only (read-only monitoring)
- **Voltage**: 3.3V logic (direct ESP32 connection)
- **Protocol**: ASCII text-based
- **Update rate**: ~1 second per block

**No level shifters or RS485 adapters required** - both Victron devices output 3.3V logic directly compatible with ESP32.

### Pin Assignments

| Function | ESP32 GPIO | Device Connection |
|----------|------------|-------------------|
| SmartShunt RX | GPIO 16 (UART2) | SmartShunt TX (white) |
| SmartShunt GND | GND | SmartShunt GND (black) |
| MPPT RX | GPIO 19 (UART1) | MPPT TX (white) |
| MPPT GND | GND | MPPT GND (black) |
| Power | Micro-USB | 5V from converter |

### Power Budget
- ESP32 idle: ~60 mA
- ESP32 WiFi active: ~120 mA
- Total typical: ~150 mA
- Total peak: ~300 mA
- Supply: 3A (ample headroom)

---

## Repository Integration Strategy

### Current State
- **Repo name**: `aachtenberg/esp12f_ds18b20_temp_sensor`
- **Purpose**: ESP-based temperature sensor with cloud logging
- **Structure**: Single PlatformIO project

### Migration Plan

#### Step 1: Rename Repository
On GitHub: Settings → General → Repository name
- **Old**: `esp12f_ds18b20_temp_sensor`
- **New**: `esp-iot-sensors` (or similar)

**Note**: GitHub automatically redirects all old URLs, preserves history, stars, forks, etc.

#### Step 2: Restructure to Multi-Project

```
esp-iot-sensors/
├── temperature-sensor/          # Existing temperature code moved here
│   ├── src/
│   │   └── main.cpp
│   ├── platformio.ini
│   └── README.md
│
├── solar-monitor/               # New solar monitoring project
│   ├── src/
│   │   ├── main.cpp
│   │   ├── VictronSmartShunt.h
│   │   ├── VictronSmartShunt.cpp
│   │   ├── VictronMPPT.h
│   │   └── VictronMPPT.cpp
│   ├── platformio.ini
│   ├── docs/
│   │   ├── QUICK_REFERENCE.md
│   │   ├── WIRING_GUIDE.md
│   │   └── VE_DIRECT_PROTOCOL.md
│   └── README.md
│
├── lib/                         # Shared libraries (optional)
│   └── common/
│       ├── WiFiManager/
│       └── CloudLogger/
│
└── README.md                    # Main repo overview
```

### Rationale
- **Unified approach**: All ESP IoT sensors in one repository
- **Shared infrastructure**: Common WiFi, cloud logging, patterns
- **Maintainability**: Single place for all embedded monitoring projects
- **Consistency**: Follow established development patterns across projects

---

## Implementation Requirements

### Primary Requirements

1. **Real-time monitoring** of both devices simultaneously
2. **Non-blocking** sensor updates (both UARTs read continuously)
3. **Web server** with JSON endpoints for data access
4. **WiFi connectivity** with auto-reconnect
5. **Serial debugging** at 115200 baud for development
6. **Clean C++ classes** following existing sensor patterns

### Data to Monitor

**From SmartShunt**:
- Battery voltage (V)
- Battery current (A, signed: negative = discharge)
- State of charge (%, 0-100)
- Time to go (minutes remaining)
- Consumed amp-hours (Ah)
- Alarm states
- Relay state

**From MPPT**:
- Battery voltage (V)
- Charge current (A)
- Panel voltage (V)
- Panel power (W)
- Charge state (Off, Bulk, Absorption, Float)
- Error codes
- Yield today (kWh)
- Yield yesterday (kWh)
- Max power today (W)
- Max power yesterday (W)

### Web API Endpoints

```
GET /api/battery      → SmartShunt data (JSON)
GET /api/solar        → MPPT data (JSON)
GET /api/system       → Combined system status (JSON)
GET /                 → Simple HTML dashboard (optional)
```

### Example JSON Response

```json
{
  "battery": {
    "voltage": 13.2,
    "current": -5.4,
    "soc": 87,
    "time_remaining": 185,
    "consumed_ah": 52.3
  },
  "solar": {
    "pv_voltage": 22.4,
    "pv_power": 145,
    "charge_current": 11.2,
    "state": "BULK",
    "yield_today": 2.34,
    "yield_yesterday": 3.12
  },
  "timestamp": 1732320000
}
```

---

## VE.Direct Protocol Details

### Protocol Specification

**Communication Parameters**:
- Baud rate: 19200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Direction: TX only (device transmits continuously)

**Data Format**:
- ASCII text-based
- Line format: `Key<TAB>Value<LF>`
- Block terminator: `Checksum<TAB><byte><LF>`
- Updates approximately every second

### Example Data Stream

```
V       13245
I       -5400
SOC     870
TTG     185
Alarm   OFF
Relay   OFF
CE      -52300
Checksum        \xB4
V       13248
I       -5385
...
```

### Field Definitions

#### SmartShunt Fields

| Key | Description | Unit | Notes |
|-----|-------------|------|-------|
| V | Battery voltage | mV | Divide by 1000 for volts |
| I | Battery current | mA | Signed: negative = discharge |
| SOC | State of charge | 0.1% | Divide by 10 for percentage |
| TTG | Time to go | minutes | -1 = infinite |
| CE | Consumed Ah | mAh | Negative = consumed |
| Alarm | Alarm condition | text | ON/OFF |
| Relay | Relay state | text | ON/OFF |
| AR | Alarm reason | bitfield | See protocol docs |
| H1 | Depth of deepest discharge | mAh | |
| H2 | Depth of last discharge | mAh | |
| H3 | Depth of average discharge | mAh | |
| H4 | Number of charge cycles | count | |
| H5 | Number of full discharges | count | |
| H6 | Cumulative Ah drawn | mAh | |
| H7 | Minimum battery voltage | mV | |
| H8 | Maximum battery voltage | mV | |
| H9 | Seconds since last full | seconds | |
| H10 | Number of automatic syncs | count | |
| H11 | Number of low voltage alarms | count | |
| H12 | Number of high voltage alarms | count | |

#### MPPT Fields

| Key | Description | Unit | Notes |
|-----|-------------|------|-------|
| V | Battery voltage | mV | Divide by 1000 for volts |
| I | Battery current | mA | Charge current |
| VPV | Panel voltage | mV | |
| PPV | Panel power | W | |
| CS | Charge state | enum | See below |
| ERR | Error code | enum | 0 = no error |
| H19 | Yield today | 0.01 kWh | Multiply by 0.01 |
| H20 | Yield yesterday | 0.01 kWh | |
| H21 | Max power today | W | |
| H22 | Yield today | 0.01 kWh | Duplicate of H19 |
| H23 | Max power yesterday | W | |
| HSDS | Day sequence number | days | Since installation |

**Charge State (CS) Values**:
- 0 = Off
- 2 = Fault
- 3 = Bulk
- 4 = Absorption
- 5 = Float

**Error Codes (ERR)**:
- 0 = No error
- 2 = Battery voltage too high
- 17 = Charger temperature too high
- 18 = Charger over current
- 19 = Charger current reversed
- 20 = Bulk time limit exceeded
- 33 = Input voltage too high (solar panel)
- 34 = Input current too high (solar panel)

### Checksum Validation

The protocol includes a simple checksum at the end of each block:
```
Checksum = (256 - (sum of all bytes in block % 256)) % 256
```

**Implementation note**: For read-only monitoring, checksum validation is optional but recommended for data integrity verification.

### Parsing Strategy

```cpp
// Pseudocode for parsing VE.Direct stream
void parseVEDirectLine(String line) {
    int tabIndex = line.indexOf('\t');
    if (tabIndex == -1) return;
    
    String key = line.substring(0, tabIndex);
    String value = line.substring(tabIndex + 1);
    
    // Handle known keys
    if (key == "V") {
        voltage = value.toFloat() / 1000.0;  // mV to V
    } else if (key == "I") {
        current = value.toFloat() / 1000.0;  // mA to A
    } else if (key == "SOC") {
        soc = value.toFloat() / 10.0;        // 0.1% to %
    } else if (key == "Checksum") {
        // Optional: validate block checksum
        // Then reset for next block
    }
    // ... handle other keys
}
```

---

## Code Architecture

### Class Structure

Following Andrew's preference for direct C++ implementations with reusable classes:

#### VictronSmartShunt Class

```cpp
// VictronSmartShunt.h
#ifndef VICTRON_SMARTSHUNT_H
#define VICTRON_SMARTSHUNT_H

#include <Arduino.h>
#include <HardwareSerial.h>

class VictronSmartShunt {
public:
    VictronSmartShunt(HardwareSerial* serial);
    void begin();
    void update();  // Non-blocking, call in loop()
    
    // Getters for primary data
    float getBatteryVoltage() const;
    float getBatteryCurrent() const;
    float getStateOfCharge() const;
    int getTimeRemaining() const;
    float getConsumedAh() const;
    bool getAlarmState() const;
    bool getRelayState() const;
    
    // Getters for historical data
    float getMinVoltage() const;
    float getMaxVoltage() const;
    int getChargeCycles() const;
    
    // Status
    bool isDataValid() const;
    unsigned long getLastUpdate() const;
    
private:
    HardwareSerial* _serial;
    void parseLine(String line);
    void validateChecksum();
    
    // Data storage
    float _voltage;
    float _current;
    float _soc;
    int _ttg;
    float _consumed_ah;
    bool _alarm;
    bool _relay;
    float _min_voltage;
    float _max_voltage;
    int _charge_cycles;
    
    unsigned long _last_update;
    bool _data_valid;
    String _line_buffer;
};

#endif
```

#### VictronMPPT Class

```cpp
// VictronMPPT.h
#ifndef VICTRON_MPPT_H
#define VICTRON_MPPT_H

#include <Arduino.h>
#include <HardwareSerial.h>

class VictronMPPT {
public:
    VictronMPPT(HardwareSerial* serial);
    void begin();
    void update();  // Non-blocking, call in loop()
    
    // Getters for primary data
    float getBatteryVoltage() const;
    float getChargeCurrent() const;
    float getPanelVoltage() const;
    float getPanelPower() const;
    String getChargeState() const;
    int getErrorCode() const;
    
    // Getters for yield data
    float getYieldToday() const;
    float getYieldYesterday() const;
    int getMaxPowerToday() const;
    int getMaxPowerYesterday() const;
    
    // Status
    bool isDataValid() const;
    unsigned long getLastUpdate() const;
    
private:
    HardwareSerial* _serial;
    void parseLine(String line);
    void validateChecksum();
    String chargeStateToString(int state);
    
    // Data storage
    float _batt_voltage;
    float _charge_current;
    float _pv_voltage;
    float _pv_power;
    int _charge_state;
    int _error_code;
    float _yield_today;
    float _yield_yesterday;
    int _max_power_today;
    int _max_power_yesterday;
    
    unsigned long _last_update;
    bool _data_valid;
    String _line_buffer;
};

#endif
```

### Main Application Structure

```cpp
// main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "VictronSmartShunt.h"
#include "VictronMPPT.h"

// WiFi credentials
const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASSWORD = "your-password";

// Hardware serial ports
HardwareSerial shuntSerial(2);  // UART2 for SmartShunt
HardwareSerial mpptSerial(1);   // UART1 for MPPT

// Device instances
VictronSmartShunt shunt(&shuntSerial);
VictronMPPT mppt(&mpptSerial);

// Web server
WebServer server(80);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== ESP32 Solar Monitor ===\n");
    
    // Initialize UART ports
    shuntSerial.begin(19200, SERIAL_8N1, 16, -1);  // RX on GPIO 16
    mpptSerial.begin(19200, SERIAL_8N1, 19, -1);   // RX on GPIO 19
    
    // Initialize devices
    shunt.begin();
    mppt.begin();
    
    // Connect to WiFi
    setupWiFi();
    
    // Setup web server
    setupWebServer();
    
    Serial.println("Setup complete. Monitoring active.\n");
}

void loop() {
    // Update device data (non-blocking)
    shunt.update();
    mppt.update();
    
    // Handle web requests
    server.handleClient();
    
    // Optional: periodic status output
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 10000) {
        printStatus();
        lastStatus = millis();
    }
}

void setupWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected!");
        Serial.println("IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nFailed to connect. Running in offline mode.");
    }
}

void setupWebServer() {
    server.on("/api/battery", handleBatteryData);
    server.on("/api/solar", handleSolarData);
    server.on("/api/system", handleSystemData);
    server.on("/", handleRoot);
    
    server.begin();
    Serial.println("Web server started");
}

void handleBatteryData() {
    StaticJsonDocument<512> doc;
    
    doc["voltage"] = shunt.getBatteryVoltage();
    doc["current"] = shunt.getBatteryCurrent();
    doc["soc"] = shunt.getStateOfCharge();
    doc["time_remaining"] = shunt.getTimeRemaining();
    doc["consumed_ah"] = shunt.getConsumedAh();
    doc["alarm"] = shunt.getAlarmState();
    doc["relay"] = shunt.getRelayState();
    doc["last_update"] = shunt.getLastUpdate();
    doc["valid"] = shunt.isDataValid();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSolarData() {
    StaticJsonDocument<512> doc;
    
    doc["pv_voltage"] = mppt.getPanelVoltage();
    doc["pv_power"] = mppt.getPanelPower();
    doc["charge_current"] = mppt.getChargeCurrent();
    doc["charge_state"] = mppt.getChargeState();
    doc["error"] = mppt.getErrorCode();
    doc["yield_today"] = mppt.getYieldToday();
    doc["yield_yesterday"] = mppt.getYieldYesterday();
    doc["max_power_today"] = mppt.getMaxPowerToday();
    doc["last_update"] = mppt.getLastUpdate();
    doc["valid"] = mppt.isDataValid();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSystemData() {
    StaticJsonDocument<1024> doc;
    
    // Battery subsystem
    JsonObject battery = doc.createNestedObject("battery");
    battery["voltage"] = shunt.getBatteryVoltage();
    battery["current"] = shunt.getBatteryCurrent();
    battery["soc"] = shunt.getStateOfCharge();
    battery["time_remaining"] = shunt.getTimeRemaining();
    
    // Solar subsystem
    JsonObject solar = doc.createNestedObject("solar");
    solar["pv_voltage"] = mppt.getPanelVoltage();
    solar["pv_power"] = mppt.getPanelPower();
    solar["charge_current"] = mppt.getChargeCurrent();
    solar["charge_state"] = mppt.getChargeState();
    solar["yield_today"] = mppt.getYieldToday();
    
    // System info
    doc["timestamp"] = millis();
    doc["uptime"] = millis() / 1000;
    doc["wifi_rssi"] = WiFi.RSSI();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Solar Monitor</title></head><body>";
    html += "<h1>ESP32 Solar Monitor</h1>";
    html += "<p><a href='/api/battery'>Battery Data</a></p>";
    html += "<p><a href='/api/solar'>Solar Data</a></p>";
    html += "<p><a href='/api/system'>System Data</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void printStatus() {
    Serial.println("\n--- Status Update ---");
    Serial.printf("Battery: %.2fV, %.2fA, %.1f%% SOC\n", 
        shunt.getBatteryVoltage(),
        shunt.getBatteryCurrent(),
        shunt.getStateOfCharge());
    Serial.printf("Solar: %.2fV, %.0fW, %s\n",
        mppt.getPanelVoltage(),
        mppt.getPanelPower(),
        mppt.getChargeState().c_str());
    Serial.println("-------------------\n");
}
```

### PlatformIO Configuration

```ini
; platformio.ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps = 
    bblanchon/ArduinoJson @ ^6.21.3
    ESP Async WebServer
```

---

## Success Criteria

### Functional Requirements
- ✅ Read SmartShunt data continuously at ~1Hz
- ✅ Read MPPT data continuously at ~1Hz
- ✅ Both devices update independently (non-blocking)
- ✅ Web server responds to API requests
- ✅ JSON data is properly formatted and complete
- ✅ WiFi auto-reconnects after dropout
- ✅ Serial debug output is readable and useful

### Performance Requirements
- ✅ No memory leaks over 24-hour operation
- ✅ Web requests respond in <100ms
- ✅ Data updates reflect actual device state
- ✅ System stable with continuous WiFi usage

### Code Quality Requirements
- ✅ Classes follow existing sensor patterns
- ✅ Clear separation of concerns (device classes vs web server)
- ✅ Non-blocking implementations throughout
- ✅ Proper error handling and validation
- ✅ Well-commented and maintainable

### Integration Requirements
- ✅ Fits cleanly into existing repo structure
- ✅ Documentation matches style of temperature project
- ✅ Can be extended for future features (MQTT, SD logging, etc.)
- ✅ Reusable classes for other VE.Direct projects

---

## Testing Checklist

### Hardware Testing
- [ ] Both UART ports receive data (verify with Serial.print of raw bytes)
- [ ] SmartShunt data matches VictronConnect app
- [ ] MPPT data matches VictronConnect app
- [ ] No data corruption or garbled characters
- [ ] Common ground is properly connected
- [ ] Power supply provides stable voltage

### Software Testing
- [ ] Classes compile without errors
- [ ] Web server accessible from browser
- [ ] JSON endpoints return valid data
- [ ] All data fields populate correctly
- [ ] Charge state strings are correct
- [ ] Error codes handled properly
- [ ] WiFi reconnection works after dropout

### Integration Testing
- [ ] Both devices update simultaneously
- [ ] No UART conflicts or data collision
- [ ] Serial debug doesn't interfere with web server
- [ ] System runs stable for 24+ hours
- [ ] Memory usage remains constant
- [ ] No watchdog resets or crashes

### Edge Case Testing
- [ ] Handle missing data gracefully
- [ ] Handle invalid checksums
- [ ] Handle disconnected devices
- [ ] Handle WiFi disconnection
- [ ] Handle rapid web requests
- [ ] Handle overnight operation (no solar input)

---

## Reference Documents

Three detailed reference documents are available in the project:

1. **QUICK_REFERENCE.txt** - Quick lookup for pins, protocols, troubleshooting
2. **WIRING_INSTRUCTIONS.txt** - Step-by-step hardware setup guide
3. **ESP32_Solar_Monitor_Guide.md** - Complete implementation guide

**Note**: These documents currently reference Renogy hardware and RS485 connections. They need to be updated to reflect:
- Victron SmartShunt instead of Renogy 500A
- VE.Direct connections instead of RS485
- No MAX485/MAX3232 adapters needed
- Simplified wiring (direct UART connection)

---

## Development Notes

### Key Design Decisions

1. **No abstraction layers**: Direct C++ classes for hardware control
2. **Non-blocking design**: Both UARTs read continuously in main loop
3. **Minimal dependencies**: Only ArduinoJson for web responses
4. **Read-only monitoring**: TX pins not connected (safety + simplicity)
5. **3.3V logic**: Direct connection without level shifters

### Future Enhancements (Not in Initial Scope)

- MQTT publishing to Raspberry Pi infrastructure
- SD card logging for offline data collection
- Battery state-of-health calculations
- Solar panel cleaning reminders (based on efficiency)
- Load shedding automation (relay control)
- Email/SMS alerts for alarms
- Historical data graphing
- Mobile app with push notifications

### Known Limitations

- **Read-only**: Cannot send commands to devices (by design)
- **No checksums**: Optional validation not implemented initially
- **Single WiFi network**: No captive portal or multi-network support
- **No HTTPS**: Local network only, no SSL/TLS
- **No authentication**: Open web server (secure via network isolation)

---

## Quick Command Reference

### PlatformIO Commands

```bash
# Build project
pio run

# Upload to ESP32
pio run --target upload

# Open serial monitor
pio device monitor

# Clean build
pio run --target clean

# Update libraries
pio lib update
```

### Git Workflow

```bash
# Clone repo
git clone https://github.com/aachtenberg/esp-iot-sensors.git

# Create feature branch
git checkout -b feature/solar-monitor

# Commit changes
git add .
git commit -m "Add solar monitoring support"

# Push to remote
git push origin feature/solar-monitor
```

### Testing Web API

```bash
# Test battery endpoint
curl http://192.168.1.XXX/api/battery | jq

# Test solar endpoint
curl http://192.168.1.XXX/api/solar | jq

# Test system endpoint
curl http://192.168.1.XXX/api/system | jq
```

---

## Contact and Support

**Developer**: Andrew (aachtenberg)  
**Repository**: https://github.com/aachtenberg/esp-iot-sensors  
**Hardware**: Victron SmartShunt SHU050150050 + SmartSolar MPPT SCC110050210  
**Platform**: ESP32-WROOM-32 with PlatformIO

---

**Document Version**: 1.0  
**Last Updated**: November 22, 2025  
**Status**: Ready for implementation

---

## Appendix: VE.Direct Protocol Resources

### Official Documentation
- Victron VE.Direct Protocol FAQ: https://www.victronenergy.com/live/vedirect_protocol:faq
- VE.Direct Protocol Whitepaper: https://www.victronenergy.com/upload/documents/Whitepaper-Data-communication-with-Victron-Energy-products_EN.pdf

### Community Resources
- DIY Solar Forum: https://diysolarforum.com
- Victron Community: https://community.victronenergy.com
- ESP32 Arduino Docs: https://docs.espressif.com/projects/arduino-esp32/

### Example Projects
- VeDirectFrameHandler: https://github.com/karioja/VeDirectFrameHandler
- ESP32 VE.Direct: https://github.com/cterwilliger/VEDirect

