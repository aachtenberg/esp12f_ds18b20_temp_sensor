# ESP32 Solar Monitor

WiFi-enabled monitoring system for Victron Energy solar equipment using ESP32 microcontroller.

**Status:** Planning phase

> **Full Implementation Details:** See [IMPLEMENTATION_CONTEXT.md](IMPLEMENTATION_CONTEXT.md) for complete technical specifications, C++ class designs, and code architecture.

## Features (Planned)

- Real-time battery monitoring (Victron SmartShunt)
- Solar charge controller monitoring (Victron SmartSolar MPPT)
- WiFi connectivity with web dashboard
- JSON API for third-party integrations
- Historical data tracking
- Low power consumption (~1.5 Ah/day)

## Hardware Requirements

| Component | Model | Notes |
|-----------|-------|-------|
| Microcontroller | ESP32-WROOM-32 | Dual UART for simultaneous monitoring |
| Battery Monitor | Victron SmartShunt SHU050150050 | 500A/50mV shunt |
| Charge Controller | Victron SmartSolar MPPT SCC110050210 | 100V/50A |
| VE.Direct Cables | ASS030530xxx | 2x required |
| Power Supply | 12V-to-5V 3A Waterproof | Amazon.ca |

**Estimated Cost:** ~$66 (excluding Victron equipment)

## VE.Direct Protocol

VE.Direct is Victron's proprietary serial protocol:
- **Voltage:** 3.3V TTL (ESP32 compatible, no level shifter needed)
- **Baud Rate:** 19200
- **Format:** Text-based key-value pairs
- **Connection:** 3.5mm audio jack style connector

### Key Data Points

**From SmartShunt:**
- Battery voltage (V)
- Current (A) - positive = charging, negative = discharging
- Power (W)
- State of Charge (SOC %)
- Time to Go (TTG minutes)

**From MPPT:**
- PV voltage (V)
- PV power (W)
- Charge current (A)
- Charge state (Off/Bulk/Absorption/Float)
- Yield today (kWh)

## ESP32 Pin Configuration

```
ESP32-WROOM-32
├── GPIO 16 (UART2 RX) → SmartShunt VE.Direct TX
├── GPIO 19 (UART1 RX) → MPPT VE.Direct TX
├── 3.3V → VE.Direct power (both devices)
└── GND → VE.Direct ground (both devices)
```

**Note:** VE.Direct TX only - we only read data, no transmission needed.

## API Endpoints (Planned)

| Endpoint | Description |
|----------|-------------|
| `GET /` | Simple HTML dashboard |
| `GET /api/battery` | SmartShunt data (JSON) |
| `GET /api/solar` | MPPT data (JSON) |
| `GET /api/system` | Combined system status (JSON) |

### Example JSON Response

```json
{
  "smartshunt": {
    "voltage": 13.25,
    "current": -2.34,
    "power": -31,
    "soc": 85.0,
    "ttg": 240
  },
  "mppt": {
    "pv_voltage": 18.65,
    "pv_power": 60,
    "charge_current": 4.5,
    "charge_state": "BULK",
    "yield_today": 1.25
  }
}
```

## Integration

This project will integrate with the same infrastructure as the temperature sensors:
- InfluxDB for data storage
- Grafana for dashboards
- Home Assistant for automation
- MQTT for real-time updates (optional)

## Documentation

- [IMPLEMENTATION_CONTEXT.md](IMPLEMENTATION_CONTEXT.md) - **Complete technical spec** (C++ classes, code architecture)
- [Bill of Materials](BOM.md) - Component list with prices
- [Wiring Guide](WIRING.md) - Connection instructions
- [Quick Reference](QUICK_REFERENCE.md) - Pin connections at a glance

## Resources

- [Victron VE.Direct Protocol FAQ](https://www.victronenergy.com/live/vedirect_protocol:faq)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [DIY Solar Forum](https://diysolarforum.com)
