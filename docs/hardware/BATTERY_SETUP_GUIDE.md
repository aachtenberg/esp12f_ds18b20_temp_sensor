# Battery-Powered ESP32 Setup Guide

Complete guide for building battery-powered ESP32 devices using TP4056 lithium battery charger module with voltage monitoring.

## Overview

This guide covers hardware setup for battery-powered deployments of any ESP32 device in this project (temperature sensors, BME280 sensors, surveillance cameras, etc.). Battery power enables:

- **Remote/outdoor deployments** without mains power
- **Deep sleep operation** for extended battery life (weeks/months)
- **Solar charging** capability for permanent outdoor installations
- **Portable sensors** that can be moved without power concerns

**Supported Platforms:**
- ✅ **ESP32** (all variants) - Full battery support with voltage monitoring
- ✅ **ESP32-S3** - Full battery support with voltage monitoring
- ❌ **ESP8266** - Not recommended (limited deep sleep support, lower power efficiency)

## What You Need

### Required Components

| Component | Specification | Quantity | Est. Cost | Notes |
|-----------|---------------|----------|-----------|-------|
| TP4056 Module | With protection circuit (DW01A + FS8205A) | 1 | $1-2 | **Must have protection circuit** |
| Li-Ion Battery | 3.7V, 18650 or LiPo, 2000-3000mAh | 1 | $5-10 | Higher capacity = longer runtime |
| Resistors | 10kΩ, 1/4W, ±5% tolerance | 2 | $0.10 | For voltage divider circuit |
| ESP32 Board | Any ESP32 or ESP32-S3 dev board | 1 | $5-15 | Already in your project |
| Wire | 22-24 AWG stranded | ~1ft | $1 | Red/black recommended |
| Battery Holder | 18650 holder with leads (if using 18650) | 1 | $1-2 | Optional but recommended |
| USB Cable | USB-C or Micro-USB (matches TP4056) | 1 | $2 | For charging |

**Optional:**
- Solar panel: 5-6V, 100-200mA minimum (for outdoor deployments)
- JST connectors: 2-pin JST-PH 2.0mm (for clean connections)
- Heat shrink tubing: For wire insulation
- Multimeter: For testing and calibration

### Where to Buy

**TP4056 Modules:**
- AliExpress: Search "TP4056 with protection" (~$1 each in 5-pack)
- Amazon: "TP4056 charging module with protection circuit" (~$2 each)
- **CRITICAL:** Must say "with protection" or show DW01A + FS8205A chips on board

**Batteries:**
- 18650 cells: AliExpress, Amazon (Samsung, LG, Panasonic brands recommended)
- LiPo batteries: Adafruit, SparkFun (with JST connector)
- **WARNING:** Avoid cheap/fake capacity batteries - stick to known brands

## TP4056 Module Overview

### What is TP4056?

The TP4056 is a complete lithium-ion battery charger IC designed for single-cell Li-Ion/LiPo batteries. The module version includes:

- **Constant current/constant voltage (CC/CV) charging** - Proper Li-Ion charge profile
- **Charge termination** - Automatically stops when battery full (4.2V)
- **LED indicators** - Red=charging, Blue/Green=charged
- **Micro-USB or USB-C input** - Standard 5V power source
- **Protection circuit** (if included) - Prevents overcharge, overdischarge, overcurrent, short circuit

### TP4056 Module Pinout

```
     ┌─────────────────────────────────┐
     │         TP4056 Module           │
     │   (WITH Protection Circuit)     │
     │                                 │
     │  [LED] [LED]                    │
     │   Red   Blue/Green              │
     │                                 │
USB  │  IN+  IN-  OUT+ OUT- BAT+ BAT-  │  Pads/Pins
─────┤   │    │    │    │    │    │   │
     │   │    │    │    │    │    │   │
     └───┼────┼────┼────┼────┼────┼───┘
         │    │    │    │    │    │
         │    │    │    │    │    └─── Battery (-)
         │    │    │    │    └──────── Battery (+)
         │    │    │    └───────────── Load GND (ESP32 GND)
         │    │    └────────────────── Load VCC (ESP32 VIN/3.3V)
         │    └─────────────────────── Power GND
         └──────────────────────────── Power VCC (5V from USB/Solar)

Connections:
  IN+  : Power input (+) - 5V from USB or solar panel
  IN-  : Power input (-) - Ground from USB or solar panel
  BAT+ : Battery positive terminal
  BAT- : Battery negative terminal
  OUT+ : Output positive - Powers ESP32 (3.7-4.2V depending on charge)
  OUT- : Output negative - ESP32 ground
```

### LED Indicators

| Red LED | Blue/Green LED | Status |
|---------|---------------|--------|
| ON | OFF | Charging |
| OFF | ON | Fully charged |
| OFF | OFF | No power input or battery not connected |

### Charging Current Configuration

The TP4056 has a programming resistor (R_PROG) that sets the charging current. Most modules come with a default 1.2kΩ resistor for 1A charging.

**Charging Current Table:**

| R_PROG Value | Charge Current | Recommended For |
|--------------|----------------|-----------------|
| 10kΩ | 130mA | Small LiPo <500mAh |
| 5kΩ | 250mA | LiPo 500-1000mAh |
| 2kΩ | 580mA | LiPo 1000-2000mAh |
| **1.2kΩ** | **1000mA (1A)** | **18650 cells, LiPo >2000mAh (default)** |

**Note:** Most pre-built modules come with 1.2kΩ (1A). This is safe for standard 18650 cells (2000-3000mAh). For smaller batteries, consider adding a higher-value resistor in series to reduce current.

## Complete Wiring Diagram

### System Overview

```
┌────────────────────────────────────────────────────────────────┐
│                 BATTERY-POWERED ESP32 SYSTEM                    │
└────────────────────────────────────────────────────────────────┘

 Power Input                 TP4056              Battery
 (USB or Solar)              Charger             (3.7V Li-Ion)
      │                         │                     │
      │                         │                     │
      ▼                         ▼                     ▼
┌──────────┐            ┌──────────────┐      ┌─────────────┐
│  USB-C   │            │   TP4056     │      │  18650 Cell │
│   or     │───────────▶│   Module     │◀────▶│     or      │
│  Solar   │  5V Power  │              │      │  LiPo Pack  │
│  Panel   │            │  with Prot.  │      │  2000-3000  │
└──────────┘            └──────┬───────┘      │    mAh      │
                               │              └─────────────┘
                               │ Regulated
                               │ 3.7-4.2V
                               ▼
                        ┌──────────────┐
                        │   ESP32      │
                        │   Board      │
                        │              │
                        │   VIN  GND   │◀── Power from TP4056 OUT+/OUT-
                        │              │
                        │  GPIO34      │◀── Voltage Divider (battery monitor)
                        │              │
                        │  GPIO4/21    │◀── Sensors (temp, BME280, etc.)
                        └──────────────┘
```

### Detailed Wiring (Step-by-Step)

```
COMPLETE CONNECTION DIAGRAM
═══════════════════════════════════════════════════════════════

Power Input (Choose One):
  ┌─────────────┐
  │  USB Cable  │  OR  ┌──────────────┐
  │   5V DC     │      │ Solar Panel  │
  │             │      │   5-6V       │
  └──┬─────┬────┘      │  100-200mA   │
     │     │           └───┬─────┬────┘
     │     │               │     │
    (+)   (-)             (+)   (-)
     │     │               │     │
     │     │               │     │
     └─────┴───────────────┴─────┘
           │
           │ Connect to TP4056 input
           ▼
    ┌──────────────────────────────┐
    │      TP4056 Module           │
    │  (with protection circuit)   │
    │                              │
IN+ │◀─── Red Wire (5V+)           │
IN- │◀─── Black Wire (GND)         │
    │                              │
    │   ┌──────────────┐           │
BAT+│───┤Battery (+)   │           │  [3.7V Li-Ion]
BAT-│───┤Battery (-)   │           │  [18650 or LiPo]
    │   └──────┬───────┘           │  [2000-3000mAh]
    │          │                   │
    │          │ Voltage Divider   │
    │          │ (reads battery    │
    │          │  directly)        │
    │          │                   │
    │          ├── 10kΩ ──┬── GPIO34 (ESP32)
    │          │          │
    │          │      10kΩ
    │          │          │
    │          └─────── GND
    │                              │
OUT+│──────────────────────────────┤───▶ To ESP32 VIN (or 3.3V pin)
OUT-│──────────────────────────────┤───▶ To ESP32 GND
    └──────────────────────────────┘


ESP32 GPIO Connections:
  VIN (or 3.3V) ◀─── TP4056 OUT+ (Red wire)
  GND ◀─────────── TP4056 OUT- (Black wire) ← COMMON GROUND
  GPIO34 (ADC) ◀─── Voltage divider midpoint (between 10kΩ resistors)
  GPIO4 ◀───────── DS18B20 or other sensors
  GPIO21/22 ◀──── BME280 (if using)
```

### Connection Table

| From | To | Wire Color | Notes |
|------|-----|-----------|-------|
| USB/Solar (+) | TP4056 IN+ | Red | 5V power input |
| USB/Solar (-) | TP4056 IN- | Black | Ground |
| Battery (+) | TP4056 BAT+ | Red | **Check polarity!** |
| Battery (-) | TP4056 BAT- | Black | **Check polarity!** |
| Battery (+) | 10kΩ R1 | Red | Start of voltage divider |
| 10kΩ R1 | 10kΩ R2 | Wire | Divider midpoint |
| 10kΩ R1 | GPIO34 | Wire | ADC input (midpoint tap) |
| 10kΩ R2 | Battery (-) | Black | Divider bottom (common GND) |
| TP4056 OUT+ | ESP32 VIN | Red | Power to ESP32 |
| TP4056 OUT- | ESP32 GND | Black | **Common ground critical** |

## Voltage Divider Circuits

### Battery Voltage Monitoring

The voltage divider allows the ESP32 to safely monitor battery voltage. ESP32 ADC can only handle 0-3.3V, but a full Li-Ion battery reaches 4.2V. The 2:1 divider scales 4.2V down to 2.1V (safe for ADC).

**Important:** The voltage divider reads the **battery voltage directly** (BAT+ to BAT-), not the TP4056 output. This gives accurate battery state regardless of load.

#### Why 2:1 Ratio?

```
Battery Voltage Range: 3.0V (empty) to 4.2V (full)
After 2:1 divider:     1.5V (empty) to 2.1V (full)

ESP32 ADC safe range: 0-3.3V
2.1V is well within safe range with margin for voltage spikes
```

### Schematic

```
              Battery (+)
              (Direct from battery, not TP4056 OUT+)
                  │
                  │
              ┌───┴───┐
              │ 10kΩ  │  R1 (Top resistor)
              │  R1   │
              └───┬───┘
                  │
                  ├──────────────▶ ESP32 GPIO34 (ADC1_CH6)
                  │                (Voltage = Battery_V / 2)
              ┌───┴───┐
              │ 10kΩ  │  R2 (Bottom resistor)
              │  R2   │
              └───┬───┘
                  │
                  │
                 GND ◀─── Battery (-) / TP4056 OUT- (Common ground)


Voltage calculation:
  V_GPIO34 = V_Battery × (R2 / (R1 + R2))
  V_GPIO34 = V_Battery × (10kΩ / (10kΩ + 10kΩ))
  V_GPIO34 = V_Battery × 0.5
  V_GPIO34 = V_Battery / 2

Example:
  Battery at 4.2V (full)  → GPIO34 reads 2.1V
  Battery at 3.7V (nominal) → GPIO34 reads 1.85V
  Battery at 3.0V (empty)  → GPIO34 reads 1.5V
```

### Component Specifications

**Resistors:**
- **Value:** 10kΩ each (2 required)
- **Tolerance:** ±5% is fine (±1% for better accuracy)
- **Power rating:** 1/4W (0.25W) is sufficient
- **Type:** Metal film or carbon film

**Why 10kΩ?**
- High impedance minimizes current drain (<0.2mA)
- Common value (cheap and available)
- Low enough to not be affected by ADC input impedance
- Power dissipation: P = V²/R = (4.2V)² / 20kΩ = 0.88mW (well under 1/4W rating)

**Optional filtering capacitor:**
- 0.1µF ceramic capacitor from GPIO34 to GND
- Reduces ADC noise for more stable readings
- Not required but recommended for noisy environments

## Step-by-Step Assembly

### 1. Prepare Components

**Safety check before starting:**
- [ ] Battery polarity marked clearly
- [ ] Multimeter available for testing
- [ ] Work area clear of conductive materials
- [ ] Fire extinguisher nearby (lithium battery safety)

### 2. Connect Battery to TP4056

**⚠️ CRITICAL: Check battery polarity with multimeter first!**

```
1. Use multimeter to verify battery voltage (should be 3.0-4.2V)
2. Identify battery positive (+) and negative (-) terminals
3. Connect battery:
   - Battery (+) → TP4056 BAT+ pad
   - Battery (-) → TP4056 BAT- pad
4. Solder or use screw terminals if available
5. VERIFY: No smoke, no heat, LEDs may light up
```

**If using 18650 cell:**
- Use proper 18650 holder with pre-attached wires
- Red wire = positive (+)
- Black wire = negative (-)

### 3. Connect Power Input (USB or Solar)

```
1. Plug USB cable into TP4056 module, OR
2. Connect solar panel wires:
   - Solar (+) → TP4056 IN+
   - Solar (-) → TP4056 IN-
3. VERIFY: Red LED lights up (charging)
4. After ~1-2 hours (depending on battery state), blue/green LED should turn on (charged)
```

**For solar panels:**
- Use 5-6V panel rated for at least 100mA
- Add Schottky diode (1N5819) in series with solar (+) to prevent backfeed at night
- Panel must provide >4.5V under load to charge effectively

### 4. Build Voltage Divider Circuit

**Battery Monitor (GPIO34):**

On breadboard (for testing):
```
1. Insert two 10kΩ resistors in series
2. Connect top of R1 to Battery (+) - directly from battery, NOT TP4056 OUT+
3. Connect bottom of R2 to GND (common ground with ESP32)
4. Tap the midpoint (between R1 and R2) for GPIO34 connection
```

Permanent installation:
```
1. Solder two 10kΩ resistors in series
2. Connect top to Battery (+) terminal
3. Connect bottom to Battery (-) / GND
4. Tap midpoint with wire to GPIO34
5. Use heat shrink tubing to insulate solder joints
6. Secure with hot glue or tape to prevent movement
```

**Important:** The voltage divider connects directly to the battery terminals (BAT+ and BAT-), not to the TP4056 output. This gives accurate battery voltage readings.

### 5. Connect to ESP32

```
Power connections:
1. TP4056 OUT+ (red) → ESP32 VIN pin (or 3.3V pin if using LDO)
2. TP4056 OUT- (black) → ESP32 GND pin

Battery monitoring:
3. Battery (+) → 10kΩ → GPIO34 → 10kΩ → Battery (-)/GND

Verification:
4. VERIFY: ESP32 powers on (LED blinks if present)
5. VERIFY: Serial output shows boot messages
```

**Important:** Some ESP32 boards have onboard voltage regulators:
- If board has 5V pin: Connect OUT+ to VIN or 5V pin
- If board is 3.3V only: Use OUT+ directly to 3.3V pin (battery voltage is already 3.7V)
- Check your board schematic to confirm

### 6. Test Voltage Reading

**Upload test sketch:**
```cpp
void setup() {
  Serial.begin(115200);
  pinMode(34, INPUT);  // GPIO34 = Battery voltage ADC
}

void loop() {
  int rawBattery = analogRead(34);
  float batteryVoltage = (rawBattery / 4095.0) * 3.3 * 2.0;  // 2:1 divider
  
  Serial.print("Raw ADC: ");
  Serial.print(rawBattery);
  Serial.print("  Battery Voltage: ");
  Serial.print(batteryVoltage, 2);
  Serial.println("V");
  
  delay(1000);
}
```

**Expected readings:**
- Raw ADC: 2500-2600 (at 4.2V full charge)
- Battery Voltage: 3.8-4.2V (recently charged)
- Raw ADC: 1800-1900 (at 3.0V empty)
- Battery Voltage: 2.9-3.1V (needs charging)

**Test procedure:**
1. Fully charge battery (blue/green LED on TP4056)
2. Upload test sketch
3. Should read ~4.0-4.2V
4. Unplug USB, let device run for a while
5. Voltage should gradually decrease as battery drains
6. Plug USB back in - voltage should climb back to 4.2V as it charges

## Software Configuration

### Enable Battery Monitoring

Edit `include/device_config.h` in your project (temperature-sensor, bme280-sensor, etc.):

```cpp
// =============================================================================
// BATTERY MONITORING (Optional)
// =============================================================================
// Uncomment to enable battery voltage monitoring (ESP32 only)
// Requires voltage divider on GPIO 34: Battery+ -> 10K -> GPIO34 -> 10K -> GND
// Complete battery setup: see docs/hardware/BATTERY_SETUP_GUIDE.md
#define BATTERY_MONITOR_ENABLED

#ifdef BATTERY_MONITOR_ENABLED
  // Hardware configuration
  #define BATTERY_PIN 34                    // GPIO34 (ADC1_CH6)
  #define VOLTAGE_DIVIDER 2.0               // 2:1 divider (10K + 10K resistors)
  #define ADC_MAX 4095.0                    // 12-bit ADC
  #define REF_VOLTAGE 3.3                   // ESP32 ADC reference
  
  // Calibration (adjust if readings are off)
  #define BATTERY_CALIBRATION 1.0           // Fine-tune multiplier (0.9-1.1)
  
  // Battery voltage thresholds
  #define BATTERY_MIN_V 3.0                 // 0% (deep discharge protection)
  #define BATTERY_MAX_V 4.2                 // 100% (full charge)
#endif
```

### Reading Battery Voltage in Code

The firmware automatically reads battery voltage when `BATTERY_MONITOR_ENABLED` is defined:

```cpp
// Read battery voltage
int raw = analogRead(BATTERY_PIN);
float voltage = (raw / ADC_MAX) * REF_VOLTAGE * VOLTAGE_DIVIDER * BATTERY_CALIBRATION;
float percent = ((voltage - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V)) * 100.0;

// Published in MQTT messages
{
  "device": "Sensor Name",
  "battery_voltage": 3.85,
  "battery_percent": 85.0,
  "temperature": 23.5
}
```

### Calibration

If voltage readings don't match multimeter:

1. **Measure actual battery voltage with multimeter** (e.g., 3.95V)
2. **Check firmware reading** (e.g., reports 3.85V)
3. **Calculate calibration factor:**
   ```
   CALIBRATION = Actual_Voltage / Reported_Voltage
   CALIBRATION = 3.95 / 3.85 = 1.026
   ```
4. **Update device_config.h:**
   ```cpp
   #define BATTERY_CALIBRATION 1.026
   ```
5. **Rebuild and upload firmware**

Repeat until readings match within ±0.05V.

### Deep Sleep Configuration

For maximum battery life, enable deep sleep mode:

```bash
# Enable 30-second deep sleep cycles
mosquitto_pub -h YOUR_BROKER -t "esp-sensor-hub/DEVICE_NAME/command" -m "deepsleep 30"

# Device will wake every 30 seconds, publish data, then sleep
# Average power: ~3-4mA (vs ~80mA awake)
```

See [docs/temperature-sensor/CONFIG.md](../temperature-sensor/CONFIG.md#deep-sleep-mode) for complete deep sleep documentation.

## Battery Runtime Calculations

### Power Consumption

**ESP32 Active (WiFi on, publishing data):**
- Current draw: 80-150mA
- Duration: ~5-8 seconds per wake cycle

**ESP32 Deep Sleep:**
- Current draw: 10-50µA (depending on board/peripherals)
- Duration: Most of the time (e.g., 30 seconds between wakes)

**TP4056 Quiescent Current:**
- Charging: 0mA (passes through to load)
- Not charging: ~2mA (protection circuit)

### Example: 30-Second Wake Cycle

**Assumptions:**
- Battery: 2500mAh (18650 cell)
- Wake cycle: 30 seconds
- Active time: 6 seconds
- Sleep time: 24 seconds
- Active current: 100mA
- Sleep current: 20µA (0.02mA)
- TP4056 quiescent: 2mA

**Average current:**
```
Active: (6s / 30s) × 100mA = 20mA
Sleep:  (24s / 30s) × 0.02mA = 0.016mA
TP4056: 2mA

Total average: 20 + 0.016 + 2 = ~22mA
```

**Battery life:**
```
Runtime = Battery_Capacity / Average_Current
Runtime = 2500mAh / 22mA
Runtime = 113.6 hours = ~4.7 days
```

### Extending Battery Life

| Optimization | Impact | Implementation |
|-------------|--------|----------------|
| Increase sleep interval (60s vs 30s) | 2x runtime | MQTT command: `deepsleep 60` |
| Larger battery (3000mAh vs 2500mAh) | 1.2x runtime | Use higher capacity cell |
| Disable OLED display | +10-20mA savings | Set `OLED_ENABLED=0` in platformio.ini |
| Reduce WiFi transmit power | +5-10mA savings | Possible but reduces range |
| **Add solar panel** | **Infinite runtime** | See solar section below |

## Solar Panel Integration (Optional)

For permanent outdoor installations, add a solar panel for continuous operation.

### Solar Panel Requirements

**Minimum specifications:**
- Voltage: 5-6V (open circuit ~6V is ideal)
- Current: 100-200mA minimum
- Power: 0.5-1W
- Size: ~60mm × 60mm for 0.5W panel

**Recommended panels:**
- 5V 200mA polycrystalline panel (~$3-5 on AliExpress)
- 6V 150mA monocrystalline panel (better low-light performance)

### Wiring Solar Panel

```
Solar Panel → TP4056 IN+ / IN-

With reverse current protection:

    Solar Panel
    ┌───────┐
    │   +   │───┬─────────────────▶ TP4056 IN+
    │   -   │   │
    └───────┘   │ 1N5819 Schottky Diode
                │ (Cathode toward TP4056)
                └─▶│├──────────────▶ TP4056 IN+

    Solar (-)  ─────────────────────▶ TP4056 IN-


The diode prevents battery from powering the solar panel at night.
Use 1N5819 Schottky diode (low voltage drop ~0.4V).
```

### Power Budget with Solar

**Daily energy balance:**

```
Energy consumed per day:
  Average current: 22mA (from calculation above)
  Daily consumption: 22mA × 24h = 528mAh

Energy generated per day (example):
  Solar panel: 5V × 150mA = 0.75W
  Effective sunlight: 4 hours/day (varies by location)
  Daily generation: 150mA × 4h = 600mAh

Net balance: 600mAh - 528mAh = +72mAh/day (battery charges slowly)
```

**Result:** With adequate sunlight (4+ hours/day), the system is self-sustaining.

### Cloudy Day Protection

The battery provides backup power during:
- Nighttime (no solar)
- Cloudy days (reduced solar)
- Winter months (less sunlight)

**Example:** 2500mAh battery with 22mA average draw = 4.7 days of backup power without any sunlight.

## Safety Warnings

### ⚠️ Lithium Battery Safety

**FIRE HAZARD - READ CAREFULLY:**

1. **Never reverse battery polarity** - Can cause fire or explosion
2. **Don't short-circuit battery** - Use insulated wire, keep tools away
3. **Never charge damaged/swollen batteries** - Dispose of safely
4. **Don't exceed 4.2V charging voltage** - TP4056 handles this automatically
5. **Don't discharge below 3.0V** - TP4056 protection circuit prevents this
6. **Store at 3.7-3.8V for long term** - Not fully charged (4.2V)
7. **Don't expose to heat** - Keep below 60°C (140°F)
8. **Use proper battery chemistry** - Only Li-Ion or LiPo (not alkaline, NiMH)

### Protection Circuit Requirements

**Always use TP4056 modules WITH protection circuit:**
- Look for DW01A IC and FS8205A dual MOSFET on module
- Protection prevents overcharge (>4.2V), overdischarge (<3.0V), overcurrent, short circuit
- Modules without protection are cheaper but UNSAFE

**How to identify:**
```
WITH Protection:           WITHOUT Protection:
┌─────────────────┐        ┌─────────────────┐
│  TP4056 IC      │        │  TP4056 IC      │
│                 │        │                 │
│  DW01A IC       │        │                 │
│  FS8205A        │        │  (no extra ICs) │
│                 │        │                 │
│  6 pads/pins    │        │  4 pads/pins    │
└─────────────────┘        └─────────────────┘
   ✅ SAFE                    ❌ UNSAFE
```

### Fire Safety

- **Keep fire extinguisher nearby** when working with lithium batteries
- **Work in well-ventilated area** - Avoid flammable materials nearby
- **If battery gets hot/swells** - Disconnect immediately, move outdoors, let cool
- **Dispose of damaged batteries** at proper recycling center (not trash)

### Enclosure Considerations

If housing the battery/electronics in an enclosure:
- **Ventilation required** - Don't seal completely (battery can vent gas if damaged)
- **Heat dissipation** - ESP32 + charging can generate heat
- **Access to USB port** - For charging/programming
- **Strain relief** - Secure wires so they don't pull on solder joints

## Troubleshooting

### Battery Not Charging

**Symptom:** Red LED doesn't turn on when USB connected

**Possible causes:**
- [ ] USB cable doesn't provide power (try different cable)
- [ ] TP4056 module damaged (check with multimeter: should see 5V at IN+ to IN-)
- [ ] Battery already fully charged (blue/green LED should be on)
- [ ] Battery polarity reversed (check with multimeter before connecting)

**Fix:**
1. Test USB power: Measure 5V between IN+ and IN- with multimeter
2. Test battery voltage: Should be 3.0-4.2V (if <3.0V, battery may be over-discharged)
3. Check solder joints on BAT+/BAT- connections

### ESP32 Won't Power On

**Symptom:** ESP32 doesn't boot when powered from TP4056

**Possible causes:**
- [ ] Battery voltage too low (<3.0V)
- [ ] TP4056 OUT+/OUT- not connected properly
- [ ] ESP32 draws more current than TP4056 protection allows (~1A limit)
- [ ] Ground connection missing or poor

**Fix:**
1. Measure voltage at OUT+ to OUT-: Should be 3.7-4.2V
2. Charge battery fully before testing
3. Check ground continuity: OUT- to ESP32 GND should be 0Ω
4. Try powering ESP32 via USB to verify it works

### Voltage Reading is Wrong

**Symptom:** Firmware reports incorrect battery voltage (e.g., reads 5.2V or 2.1V)

**Possible causes:**
- [ ] Resistor values wrong (not 10kΩ each)
- [ ] Voltage divider connected to wrong GPIO pin
- [ ] Calibration factor needs adjustment
- [ ] ADC pin shared with another function (unlikely on GPIO34)

**Fix:**
1. **Verify resistor values with multimeter:**
   - Each should be ~10kΩ ±5%
   - Series total should be ~20kΩ
2. **Check voltage divider output:**
   - Measure voltage at GPIO34 pin with multimeter
   - Should be ~half of battery voltage
   - Example: 4.0V battery → 2.0V at GPIO34
3. **Adjust calibration in device_config.h:**
   ```cpp
   #define BATTERY_CALIBRATION 1.05  // Increase if reads low, decrease if reads high
   ```
4. **Verify GPIO34 connection:**
   - GPIO34 is input-only on ESP32 (ADC1_CH6)
   - Don't use GPIO36, 39 (ADC1_CH0, CH3) as they may have issues on some boards

### Battery Drains Too Fast

**Symptom:** Battery only lasts a few hours instead of days

**Possible causes:**
- [ ] Deep sleep not enabled (device stays awake)
- [ ] OLED display enabled (uses 10-20mA continuously)
- [ ] WiFi constantly reconnecting (check signal strength)
- [ ] Sensor polling too frequently
- [ ] TP4056 quiescent current higher than expected

**Fix:**
1. **Enable deep sleep:** Send MQTT command `deepsleep 30`
2. **Verify deep sleep working:**
   ```
   Serial monitor should show:
   [DEEP SLEEP] Entering deep sleep for 30 seconds...
   *** WOKE FROM DEEP SLEEP (TIMER) ***
   ```
3. **Disable OLED if not needed:** Set `OLED_ENABLED=0` in platformio.ini
4. **Measure actual current draw:**
   - Use multimeter in series with battery
   - Should see <1mA in deep sleep
   - If >10mA, something is keeping ESP32 awake
5. **Check WiFi signal:**
   - Weak signal = more power for retries
   - Move closer to router or add WiFi extender

### Solar Not Charging

**Symptom:** Solar panel connected but battery not charging

**Possible causes:**
- [ ] Solar panel voltage too low (<4.5V under load)
- [ ] Insufficient sunlight (cloudy, indirect light)
- [ ] Diode connected backward (if using reverse protection)
- [ ] Panel too small for power requirements

**Fix:**
1. **Measure solar panel output:**
   - Open circuit (no load): Should be ~5-6V in direct sunlight
   - Under load (charging): Should be >4.5V
   - If <4V, panel is undersized or not in adequate light
2. **Test without diode:** Connect panel directly to TP4056 to isolate diode issue
3. **Verify LED indicators:** Red LED should turn on in sunlight
4. **Calculate power balance:** See "Power Budget with Solar" section above

## Testing Checklist

Before deploying to field:

- [ ] Battery voltage measured: 3.7-4.2V
- [ ] TP4056 charges battery (red LED during charge, blue/green when complete)
- [ ] ESP32 boots from battery power
- [ ] Voltage reading accurate (within ±0.1V of multimeter)
- [ ] Deep sleep functioning (verify wake cycles)
- [ ] MQTT publishing battery voltage and percentage
- [ ] Solar charging working (if using solar panel)
- [ ] Enclosure has ventilation
- [ ] All connections secured (no loose wires)
- [ ] Battery runtime tested (at least 8 hours)

## Advanced Topics

### Multiple Batteries in Parallel

For longer runtime, you can connect multiple batteries in parallel (same voltage, capacities add):

```
Battery 1 (2500mAh) ──┬──▶ TP4056 BAT+
Battery 2 (2500mAh) ──┘
                          Total: 5000mAh

⚠️ WARNING:
  - Batteries must be same voltage (±0.1V)
  - Batteries must be same chemistry (both Li-Ion)
  - Ideally same brand/model/age
  - Don't mix old and new batteries
  - Use proper battery management system (BMS) for >2 cells
```

### Custom PCB Integration

For production designs, integrate TP4056 directly on PCB:

- Use discrete TP4056 IC (SOIC-8 package)
- Add DW01A protection IC and FS8205A MOSFETs
- Include voltage divider resistors on PCB
- Add battery connector (JST-PH 2.0mm)
- See [docs/pcb_design/](../pcb_design/) for future PCB designs

### Power Path Management

The TP4056 module has a limitation: when USB is plugged in, it powers the load AND charges the battery. If load draws >1A (charging current limit), charging stops.

**For high-current applications (cameras, motors):**
- Consider dedicated power path management IC (e.g., MCP73831 + load switch)
- Or use separate power supply for high-current loads

## Related Documentation

- **Deep Sleep Configuration:** [docs/temperature-sensor/CONFIG.md](../temperature-sensor/CONFIG.md#deep-sleep-mode)
- **Device Configuration:** Each project's `include/device_config.h`
- **OLED Display Setup:** [docs/hardware/OLED_DISPLAY_GUIDE.md](OLED_DISPLAY_GUIDE.md)
- **Platform Guide:** [docs/reference/PLATFORM_GUIDE.md](../reference/PLATFORM_GUIDE.md)
- **PCB Designs:** [docs/pcb_design/](../pcb_design/) (future battery-powered PCB)

## Project-Specific Notes

### Temperature Sensor
- Battery monitoring fully supported on ESP32
- Deep sleep recommended for battery operation: 30-60 second intervals
- See [temperature-sensor/README_VERSION.md](../../temperature-sensor/README_VERSION.md) for deployment

### BME280 Sensor
- Battery monitoring configuration identical to temperature sensor
- Supports temperature, humidity, pressure monitoring
- See [bme280-sensor/README.md](../../bme280-sensor/README.md)

### Surveillance Camera
- Higher power consumption (~200-300mA active with camera)
- Battery runtime limited (consider larger battery or solar mandatory)
- Deep sleep may interfere with motion detection (use cautiously)

### Solar Monitor
- Already has battery monitoring for Victron system
- This guide for ESP32 board power (not monitored Victron battery)

---

**Last Updated:** January 3, 2026  
**Tested Platforms:** ESP32-DevKitC, ESP32-S3-WROOM  
**Battery Types Tested:** 18650 (Samsung INR18650-25R), 3.7V 2000mAh LiPo  
**Status:** Production-ready, field-tested
