# OLED Display Integration Guide

Reference guide for adding SSD1306 OLED displays to ESP sensor devices.

## Recommended Hardware

### Display: SSD1306 0.96" OLED (I2C)

**Amazon**: [ELEGOO 0.96" OLED 4-Pack (~$17 CAD)](https://www.amazon.ca/dp/B0D2RLXLBZ)

| Spec | Value |
|------|-------|
| Chip | SSD1306 |
| Size | 0.96" diagonal |
| Resolution | 128x64 pixels |
| Interface | I2C (4 wires) |
| Voltage | 3.3V compatible |
| Power | ~20mA |

## Wiring

### ESP32 (Solar Monitor)

```
ESP32 Pin    OLED Pin
---------    --------
3.3V    →    VCC
GND     →    GND
GPIO 21 →    SDA
GPIO 22 →    SCL
```

### ESP8266 (Temperature Sensor)

```
NodeMCU Pin    OLED Pin
-----------    --------
3.3V (3V3) →   VCC
GND        →   GND
D2 (GPIO 4) →  SDA
D1 (GPIO 5) →  SCL
```

## Adding OLED to Existing Temperature Sensor PCB

Since the PCB is already manufactured, use one of these methods:

### Option A: Solder to NodeMCU Pins (Recommended)

Solder 4 thin wires directly to the NodeMCU module pins on your PCB:

```
Your Existing PCB (Top View)
┌─────────────────────────────────┐
│  ┌─────────────────────────┐    │
│  │       NodeMCU           │    │
│  │  ●  ●  ●  ●  ●  ●  ●   │    │  ← Solder wires to
│  │ 3V3 GND D1 D2  ...     │    │    3V3, GND, D1, D2
│  └─────────────────────────┘    │
│                                 │
│        DS18B20                  │
└─────────────────────────────────┘
           │
           │ 4 wires (~10cm)
           ↓
      ┌─────────┐
      │  OLED   │
      │ Display │
      └─────────┘
```

**Materials:**
- 4x thin wires (~10cm each)
- Soldering iron
- OLED display

### Option B: Dupont Jumper Wires (No Soldering)

If NodeMCU header pins are exposed/accessible:
- Plug female Dupont wires onto exposed pins
- Connect to OLED header

### Option C: Tap Into Existing Traces

For advanced users:
1. Find 3.3V/GND traces on PCB
2. Solder thin wires to traces
3. Solder to NodeMCU D1/D2 pins directly

## OLED Mounting Ideas

Since OLED isn't integrated into PCB:

1. **Velcro/tape** - Attach to enclosure
2. **3D print** - Small bracket/holder
3. **Hot glue** - Mount near sensor
4. **Longer wires** - Mount separately in visible location

## Display Content

### Solar Monitor Display
```
┌────────────────┐
│   BATTERY      │
│     85%        │
│  ████████░░    │
│ 12.8V   -2.3A  │
│ Solar:   145W  │
└────────────────┘
```

### Temperature Sensor Display
```
┌────────────────┐
│  Main Cottage  │
│                │
│    23.5°C      │
│    74.3°F      │
└────────────────┘
```

## Implementation Status

| Project | Status | Notes |
|---------|--------|-------|
| Solar Monitor | ✅ Code Ready | ESP32, GPIO 21/22. Set `OLED_ENABLED 1` in display.h to enable |
| Temperature Sensor | 🔲 Planned | ESP8266, I2C on D1/D2 |

## Enabling/Disabling OLED

The OLED display can be enabled or disabled at compile time in `solar-monitor/src/display.h`:

```cpp
// Set to 0 to disable OLED (when hardware not connected)
// Set to 1 to enable OLED display
#define OLED_ENABLED 0
```

**When to disable:** Hardware not yet connected (causes crash on boot)  
**When to enable:** OLED display is wired up and ready

## Library Dependencies

Add to `platformio.ini`:

```ini
lib_deps =
    u8g2              ; Universal 8bit Graphics Library
```

## Code Implementation

### Initialization

```cpp
#include <U8g2lib.h>
#include <Wire.h>

// ESP32: Hardware I2C on GPIO 21 (SDA), GPIO 22 (SCL)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

void initDisplay() {
    display.begin();
    display.setFont(u8g2_font_6x10_tf);
    display.clearBuffer();
    display.drawStr(0, 20, "Initializing...");
    display.sendBuffer();
}
```

### Update Display

```cpp
void updateDisplay(int batteryPercent, float voltage, float current, float solarPower) {
    display.clearBuffer();
    
    // Title
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(40, 10, "BATTERY");
    
    // Battery percentage (large)
    display.setFont(u8g2_font_logisoso22_tn);
    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", batteryPercent);
    display.drawStr(35, 38, pctStr);
    
    // Progress bar
    int barWidth = (batteryPercent * 100) / 100;
    display.drawFrame(14, 42, 100, 8);
    display.drawBox(14, 42, barWidth, 8);
    
    // Voltage and current
    display.setFont(u8g2_font_6x10_tf);
    char infoStr[32];
    snprintf(infoStr, sizeof(infoStr), "%.1fV  %.1fA", voltage, current);
    display.drawStr(25, 56, infoStr);
    
    // Solar power
    snprintf(infoStr, sizeof(infoStr), "Solar: %.0fW", solarPower);
    display.drawStr(30, 64, infoStr);
    
    display.sendBuffer();
}
```

## Parts List

| Item | Quantity | Source | Est. Cost |
|------|----------|--------|-----------|
| SSD1306 0.96" OLED | 1 per device | [Amazon](https://www.amazon.ca/dp/B0D2RLXLBZ) | ~$4.25 CAD |
| Hookup wire | ~40cm | Any | ~$0.50 |
| **Total per device** | | | **~$5 CAD** |

---

**Created**: November 24, 2025  
**Updated**: November 25, 2025  
**Status**: Code complete - awaiting display delivery  
**Branch**: `feature/oled-display`
