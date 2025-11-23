# Wiring Instructions

## Overview

The ESP32 Solar Monitor connects to three Victron devices via VE.Direct protocol:
1. **SmartShunt** - Battery monitoring (voltage, current, SOC)
2. **SmartSolar MPPT 1** - First solar charge controller
3. **SmartSolar MPPT 2** - Second solar charge controller

## VE.Direct Cable Pinout

VE.Direct uses a 4-pin JST-PH connector (similar to 3.5mm audio jack):

| Pin | Signal | Color (typical) | Description |
|-----|--------|-----------------|-------------|
| 1 | GND | Black | Ground |
| 2 | RX | - | Device receive (not used) |
| 3 | TX | Yellow/White | Device transmit (to ESP32) |
| 4 | VCC | Red | 3.3V power |

**Important:** VE.Direct is 3.3V TTL - directly compatible with ESP32, no level shifter needed!

## ESP32 Connections

### UART Configuration

```
┌─────────────────────────────────────────────────────┐
│                   ESP32-WROOM-32                    │
│                                                     │
│  GPIO 16 (UART2 RX) ←── SmartShunt TX (yellow)     │
│  GPIO 19 (UART1 RX) ←── MPPT1 TX (yellow)          │
│  GPIO 18 (SoftSerial) ←── MPPT2 TX (yellow)        │
│  3.3V ─────────────────→ VE.Direct VCC (all 3)     │
│  GND ──────────────────→ VE.Direct GND (all 3)     │
│                                                     │
│  VIN ←── 5V from 12V-to-5V converter               │
│  GND ←── Ground from converter                      │
└─────────────────────────────────────────────────────┘
```

### Pin Summary Table

| ESP32 Pin | Connection | Notes |
|-----------|------------|-------|
| GPIO 16 | SmartShunt TX | UART2 RX (Hardware Serial) |
| GPIO 19 | MPPT1 TX | UART1 RX (Hardware Serial) |
| GPIO 18 | MPPT2 TX | SoftwareSerial RX |
| 3.3V | VE.Direct VCC (all 3) | Powers VE.Direct interface |
| GND | VE.Direct GND (all 3) | Common ground |
| VIN | 5V power supply | From 12V-to-5V converter |

## Power Supply Wiring

```
12V Battery ──┬── Fuse (5A) ──→ 12V-to-5V Converter ──→ ESP32 VIN
              │
              └── To Victron devices (separate circuit)
```

**Note:** Always use a fuse for the ESP32 power circuit!

## Step-by-Step Wiring

### 1. Prepare VE.Direct Cables

Cut the VE.Direct cables and identify wires:
- **Black** = GND
- **Red** = VCC (3.3V)
- **Yellow/White** = TX (data from device)

### 2. Connect SmartShunt

1. Connect SmartShunt VE.Direct TX (yellow) → ESP32 GPIO 16
2. Connect SmartShunt VE.Direct GND (black) → ESP32 GND
3. Connect SmartShunt VE.Direct VCC (red) → ESP32 3.3V

### 3. Connect MPPT1

1. Connect MPPT1 VE.Direct TX (yellow) → ESP32 GPIO 19
2. Connect MPPT1 VE.Direct GND (black) → ESP32 GND
3. Connect MPPT1 VE.Direct VCC (red) → ESP32 3.3V

### 4. Connect MPPT2

1. Connect MPPT2 VE.Direct TX (yellow) → ESP32 GPIO 18
2. Connect MPPT2 VE.Direct GND (black) → ESP32 GND
3. Connect MPPT2 VE.Direct VCC (red) → ESP32 3.3V

### 5. Power the ESP32

1. Connect 12V battery positive → inline fuse → 12V-to-5V converter input (+)
2. Connect 12V battery negative → 12V-to-5V converter input (-)
3. Connect converter 5V output → ESP32 VIN
4. Connect converter GND → ESP32 GND

## Verification

Before powering on:

1. [ ] Check all connections with multimeter (continuity)
2. [ ] Verify no shorts between VCC and GND
3. [ ] Confirm 12V-to-5V converter outputs ~5V
4. [ ] Double-check GPIO pin assignments

## Troubleshooting

### No Data from SmartShunt
- Check GPIO 16 connection
- Verify VE.Direct cable is plugged in firmly
- Check SmartShunt settings (VE.Direct must be enabled)

### No Data from MPPT1
- Check GPIO 19 connection
- Verify VE.Direct cable connection
- Some MPPTs need VE.Direct enabled in settings

### No Data from MPPT2
- Check GPIO 18 connection
- Verify VE.Direct cable connection
- SoftwareSerial is used for MPPT2; ensure no other code conflicts with GPIO 18

### ESP32 Not Powering On
- Check fuse
- Verify 12V-to-5V converter is working
- Measure voltage at ESP32 VIN pin (should be ~5V)

### Garbled/Corrupt Data
- Check baud rate (should be 19200)
- Verify ground connections are solid
- Try shorter cables or add ferrite beads
