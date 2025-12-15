# ESP32 Solar Monitor

WiFi-enabled monitoring system for Victron Energy solar equipment using ESP32 microcontroller.

**Status:** ✅ Implemented and deployed

## Table of Contents
1. [Overview](#overview)
2. [Hardware Configuration](#hardware-configuration)
3. [Wiring Instructions](#wiring-instructions)
4. [Pin Assignments](#pin-assignments)
5. [VE.Direct Protocol](#vedirect-protocol)
6. [API Endpoints](#api-endpoints)
7. [Quick Reference](#quick-reference)
8. [Troubleshooting](#troubleshooting)

---

## Overview

### Features

- Real-time monitoring of Victron equipment via VE.Direct protocol
- Dual MPPT charge controller monitoring
- Battery state monitoring via SmartShunt
- OLED display (optional, can be disabled if hardware not connected)
- WiFi connectivity with WiFiManager (captive portal configuration)
- JSON API for third-party integrations
- Web dashboard
- InfluxDB data logging integration

### Monitored Equipment

1. **SmartShunt SHU050150050** - Battery monitoring
   - Battery voltage, current, power
   - State of charge (SOC %)
   - Time to go (TTG minutes)
   - Historical discharge data

2. **SmartSolar MPPT 1 & 2** - Charge controllers
   - PV voltage and power
   - Charge current and state
   - Daily/total yield statistics
   - Error monitoring

---

## Hardware Configuration

### Components

| Component | Model | Notes |
|-----------|-------|-------|
| Microcontroller | ESP32-WROOM-32 | Dual UART + SoftwareSerial for 3 devices |
| Battery Monitor | Victron SmartShunt SHU050150050 | 500A/50mV shunt |
| Charge Controller 1 | Victron SmartSolar MPPT SCC110050210 | 100V/50A |
| Charge Controller 2 | Victron SmartSolar MPPT SCC110050210 | 100V/50A |
| Display (optional) | SSD1306 OLED | 128x64 I2C display |
| Power Supply | 12V-to-5V converter | 3A waterproof micro-USB |

### System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   ESP32-WROOM-32                        │
│                                                         │
│  UART2 (SmartShunt):                                   │
│    GPIO 16 (RX) ←─── SmartShunt TX                    │
│                                                         │
│  UART1 (MPPT1):                                        │
│    GPIO 19 (RX) ←─── MPPT1 TX                         │
│                                                         │
│  SoftwareSerial (MPPT2):                               │
│    GPIO 18 (RX) ←─── MPPT2 TX                         │
│                                                         │
│  I2C (OLED - optional):                                │
│    GPIO 21 (SDA) ←──→ OLED SDA                        │
│    GPIO 22 (SCL) ←──→ OLED SCL                        │
│                                                         │
│  Power:                                                 │
│    VIN ←────────── 5V from 12V-to-5V converter         │
│    3.3V ─────────→ VE.Direct VCC (all 3 devices)       │
│    GND ──────────→ Common ground                        │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## Wiring Instructions

### VE.Direct Cable Pinout

VE.Direct uses a 4-pin connector with the following signals:

| Pin | Signal | Color (typical) | Description |
|-----|--------|-----------------|-------------|
| 1 | GND | Black | Ground |
| 2 | RX | - | Device receive (not used) |
| 3 | TX | Yellow/White | Device transmit (to ESP32) |
| 4 | VCC | Red | 3.3V power |

**Important:** VE.Direct is 3.3V TTL - directly compatible with ESP32, no level shifter needed!

### Step-by-Step Wiring

#### 1. Connect SmartShunt (UART2)
- SmartShunt VE.Direct TX (yellow) → ESP32 GPIO 16
- SmartShunt VE.Direct GND (black) → ESP32 GND
- SmartShunt VE.Direct VCC (red) → ESP32 3.3V

#### 2. Connect MPPT1 (UART1)
- MPPT1 VE.Direct TX (yellow) → ESP32 GPIO 19
- MPPT1 VE.Direct GND (black) → ESP32 GND
- MPPT1 VE.Direct VCC (red) → ESP32 3.3V

#### 3. Connect MPPT2 (SoftwareSerial)
- MPPT2 VE.Direct TX (yellow) → ESP32 GPIO 18
- MPPT2 VE.Direct GND (black) → ESP32 GND
- MPPT2 VE.Direct VCC (red) → ESP32 3.3V

#### 4. Connect OLED Display (Optional)
- OLED SDA → ESP32 GPIO 21
- OLED SCL → ESP32 GPIO 22
- OLED VCC → ESP32 3.3V
- OLED GND → ESP32 GND

**Note:** Set `OLED_ENABLED = false` in code if OLED hardware is not connected to prevent crashes.

#### 5. Power the ESP32
- 12V battery positive → inline fuse (5A) → 12V-to-5V converter input (+)
- 12V battery negative → 12V-to-5V converter input (-)
- Converter 5V output → ESP32 VIN
- Converter GND → ESP32 GND

### Verification Checklist

Before powering on:
- [ ] Check all connections with multimeter (continuity)
- [ ] Verify no shorts between VCC and GND
- [ ] Confirm 12V-to-5V converter outputs ~5V
- [ ] Double-check GPIO pin assignments
- [ ] Verify common ground connections

---

## Pin Assignments

### ESP32 GPIO Pin Configuration

| GPIO Pin | Function | Connection | Protocol |
|----------|----------|------------|----------|
| GPIO 16 | SmartShunt RX | UART2 RX | VE.Direct 19200 baud |
| GPIO 19 | MPPT1 RX | UART1 RX | VE.Direct 19200 baud |
| GPIO 18 | MPPT2 RX | SoftwareSerial RX | VE.Direct 19200 baud |
| GPIO 21 | OLED SDA | I2C Data | 100 kHz |
| GPIO 22 | OLED SCL | I2C Clock | 100 kHz |
| 3.3V | VE.Direct VCC | Power output | Powers all VE.Direct interfaces |
| GND | Common ground | Ground | All devices |
| VIN | 5V power input | From converter | ESP32 power |

---

## VE.Direct Protocol

### Protocol Specification

**Communication Parameters:**
- Baud rate: 19200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Voltage: 3.3V TTL
- Direction: TX only (read-only monitoring)

**Data Format:**
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
```

### SmartShunt Data Fields

| Key | Description | Unit | Conversion |
|-----|-------------|------|------------|
| V | Battery voltage | mV | Divide by 1000 for volts |
| I | Battery current | mA | Divide by 1000 for amps (negative = discharge) |
| SOC | State of charge | 0.1% | Divide by 10 for percentage |
| TTG | Time to go | minutes | -1 = infinite |
| CE | Consumed Ah | mAh | Negative = consumed |
| Alarm | Alarm condition | text | ON/OFF |
| Relay | Relay state | text | ON/OFF |
| H1 | Depth of deepest discharge | mAh | |
| H2 | Depth of last discharge | mAh | |
| H4 | Number of charge cycles | count | |
| H7 | Minimum battery voltage | mV | |
| H8 | Maximum battery voltage | mV | |

### MPPT Data Fields

| Key | Description | Unit | Conversion |
|-----|-------------|------|------------|
| PID | Product ID | hex | Device model identifier |
| SER# | Serial number | text | Unique device identifier |
| V | Battery voltage | mV | Divide by 1000 |
| I | Battery current | mA | Charge current |
| VPV | Panel voltage | mV | Divide by 1000 |
| PPV | Panel power | W | Direct value |
| CS | Charge state | enum | See charge states below |
| ERR | Error code | enum | 0 = no error |
| LOAD | Load output state | text | ON/OFF |
| IL | Load current | mA | Current from load output |
| H19 | Yield total | 0.01kWh | Multiply by 0.01 |
| H20 | Yield today | 0.01kWh | Multiply by 0.01 |
| H21 | Max power today | W | Direct value |
| H22 | Yield yesterday | 0.01kWh | Multiply by 0.01 |
| H23 | Max power yesterday | W | Direct value |

### Charge States (CS)

| Code | State |
|------|-------|
| 0 | Off |
| 2 | Fault |
| 3 | Bulk |
| 4 | Absorption |
| 5 | Float |
| 6 | Storage |
| 7 | Equalize |

### Error Codes (ERR)

| Code | Description |
|------|-------------|
| 0 | No error |
| 2 | Battery voltage too high |
| 17 | Charger temperature too high |
| 18 | Charger over current |
| 19 | Charger current reversed |
| 20 | Bulk time limit exceeded |
| 33 | Input voltage too high (solar panel) |
| 34 | Input current too high (solar panel) |

---

## API Endpoints

The ESP32 runs a web server providing JSON API endpoints and an HTML dashboard.

### Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Simple HTML dashboard |
| `/api/battery` | GET | SmartShunt data (JSON) |
| `/api/solar` | GET | Both MPPTs data (JSON) |
| `/api/system` | GET | Combined system status (JSON) |

### Example Response: `/api/battery`

```json
{
  "voltage": 13.25,
  "current": -2.34,
  "soc": 85.0,
  "time_remaining": 240,
  "consumed_ah": 15.2,
  "alarm": false,
  "relay": false,
  "last_update": 1234567890,
  "valid": true
}
```

### Example Response: `/api/solar`

```json
{
  "mppt1": {
    "product_id": "0xA060",
    "serial_number": "HQ2145ABCDE",
    "pv_voltage": 18.65,
    "pv_power": 145,
    "charge_current": 4.5,
    "charge_state": "BULK",
    "error": 0,
    "yield_today": 2.34,
    "yield_yesterday": 3.12,
    "max_power_today": 250,
    "valid": true
  },
  "mppt2": {
    "product_id": "0xA060",
    "serial_number": "HQ2145FGHIJ",
    "pv_voltage": 19.23,
    "pv_power": 156,
    "charge_current": 5.1,
    "charge_state": "BULK",
    "error": 0,
    "yield_today": 2.56,
    "yield_yesterday": 3.45,
    "max_power_today": 275,
    "valid": true
  }
}
```

### Example Response: `/api/system`

```json
{
  "battery": {
    "voltage": 13.25,
    "current": -2.34,
    "soc": 85.0,
    "time_remaining": 240
  },
  "solar": {
    "total_pv_power": 301,
    "total_charge_current": 9.6,
    "yield_today_total": 4.90
  },
  "system": {
    "uptime": 86400,
    "wifi_rssi": -45,
    "timestamp": 1732320000
  }
}
```

---

## Quick Reference

### Connection Summary

```
ESP32 GPIO 16 (RX2) ←── SmartShunt TX
ESP32 GPIO 19 (RX1) ←── MPPT1 TX
ESP32 GPIO 18 (SW)  ←── MPPT2 TX
ESP32 GPIO 21 (SDA) ←──→ OLED SDA (optional)
ESP32 GPIO 22 (SCL) ←──→ OLED SCL (optional)
ESP32 3.3V ──────────→ VE.Direct VCC (all 3)
ESP32 GND ───────────→ Common ground
ESP32 VIN ←────────── 5V from converter
```

### VE.Direct Settings

- **Baud Rate:** 19200, 8N1
- **Voltage:** 3.3V TTL (ESP32 compatible)
- **Protocol:** ASCII text mode (key=value pairs)
- **Update Rate:** ~1 second per block

### WiFi Configuration

- **Initial Setup:** Connect to "ESP-Setup" AP on first boot or after double-reset
- **Portal:** WiFiManager captive portal (device-hostname.local)
- **Double Reset:** Reset device twice within 3 seconds to enter config mode
- **Configuration:** WiFi credentials, device name, InfluxDB settings

### OLED Display Pages (if enabled)

The OLED cycles through multiple pages showing:
1. Battery status (voltage, current, SOC)
2. MPPT1 status (PV power, charge state)
3. MPPT2 status (PV power, charge state)
4. Daily statistics (yield, max power)
5. System info (uptime, WiFi, IP)

---

## Troubleshooting

### No Data from SmartShunt

**Symptoms:** SmartShunt data shows as invalid or zero
**Checks:**
- Verify GPIO 16 connection to SmartShunt TX
- Check VE.Direct cable is plugged in firmly
- Ensure VE.Direct is enabled in SmartShunt settings (via VictronConnect app)
- Verify common ground connection
- Check baud rate is 19200 in code
- Monitor serial output for raw data stream

### No Data from MPPT1

**Symptoms:** MPPT1 data shows as invalid or zero
**Checks:**
- Verify GPIO 19 connection to MPPT1 TX
- Check VE.Direct cable connection
- Some MPPTs need VE.Direct enabled in settings
- Verify common ground connection

### No Data from MPPT2

**Symptoms:** MPPT2 data shows as invalid or zero
**Checks:**
- Verify GPIO 18 connection to MPPT2 TX
- Check VE.Direct cable connection
- SoftwareSerial used for MPPT2 - ensure no pin conflicts
- Verify common ground connection

### ESP32 Not Powering On

**Symptoms:** No LED activity, no WiFi AP
**Checks:**
- Check fuse (should be intact)
- Verify 12V-to-5V converter is working
- Measure voltage at ESP32 VIN pin (should be ~5V)
- Check converter input voltage (should be 12V from battery)

### Garbled/Corrupt Data

**Symptoms:** Invalid values, random characters in serial output
**Checks:**
- Verify baud rate is 19200 for all VE.Direct connections
- Check ground connections are solid and low resistance
- Try shorter VE.Direct cables
- Add ferrite beads to VE.Direct cables if near noisy equipment
- Verify 3.3V supply is stable

### OLED Display Not Working

**Symptoms:** OLED remains blank or shows garbage
**Checks:**
- Set `OLED_ENABLED = false` if hardware not connected
- Verify I2C connections (GPIO 21 = SDA, GPIO 22 = SCL)
- Check OLED I2C address (usually 0x3C)
- Verify OLED power (3.3V and GND)
- Check serial output for I2C initialization errors

### WiFi Connection Issues

**Symptoms:** Cannot connect to WiFi, frequent disconnections
**Checks:**
- Double-reset to enter WiFiManager portal
- Verify WiFi credentials are correct
- Check WiFi signal strength (RSSI in system API)
- Ensure 2.4GHz WiFi is enabled (ESP32 doesn't support 5GHz)
- Check router allows new devices

### Web Server Not Accessible

**Symptoms:** Cannot reach API endpoints or dashboard
**Checks:**
- Verify ESP32 is connected to WiFi (check serial output)
- Confirm IP address (shown on OLED or in serial output)
- Try accessing from same network/subnet
- Check firewall settings
- Verify web server started (check serial output for "Web server started")

---

## Integration with Backend

This solar monitor integrates with the same Raspberry Pi infrastructure as the temperature sensors:

- **InfluxDB:** Time-series data storage
- **Grafana:** Dashboard visualization
- **Home Assistant:** Automation and alerts

See the main [repository README](../../README.md) for backend setup details.

---

## Resources

### Official Documentation
- [Victron VE.Direct Protocol FAQ](https://www.victronenergy.com/live/vedirect_protocol:faq)
- [VE.Direct Protocol Whitepaper](https://www.victronenergy.com/upload/documents/Whitepaper-Data-communication-with-Victron-Energy-products_EN.pdf)
- [ESP32 Arduino Core Documentation](https://docs.espressif.com/projects/arduino-esp32/)

### Community Resources
- [DIY Solar Forum](https://diysolarforum.com)
- [Victron Community](https://community.victronenergy.com)

### Example Projects
- [VeDirectFrameHandler](https://github.com/karioja/VeDirectFrameHandler)
- [ESP32 VE.Direct](https://github.com/cterwilliger/VEDirect)

---

**Last Updated:** December 2, 2025
**Status:** ✅ Implemented and operational
