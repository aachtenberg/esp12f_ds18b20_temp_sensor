# Quick Reference Card

## ESP32 Pin Connections

```
ESP32 GPIO 16 (RX2) ←── SmartShunt TX
ESP32 GPIO 19 (RX1) ←── MPPT TX
ESP32 3.3V ──────────→ VE.Direct VCC (both)
ESP32 GND ───────────→ VE.Direct GND (both)
ESP32 VIN ←────────── 5V from 12V-to-5V converter
```

## VE.Direct Settings

- **Baud Rate:** 19200
- **Voltage:** 3.3V TTL
- **Protocol:** Text mode (key=value pairs)

## API Endpoints

| Endpoint | Description |
|----------|-------------|
| `GET /` | Simple HTML dashboard |
| `GET /api/battery` | SmartShunt data (JSON) |
| `GET /api/solar` | MPPT data (JSON) |
| `GET /api/system` | Combined system status (JSON) |

## SmartShunt Data Fields

| Key | Description | Unit | Notes |
|-----|-------------|------|-------|
| V | Battery voltage | mV | Divide by 1000 for volts |
| I | Battery current | mA | Signed: negative = discharge |
| SOC | State of charge | 0.1% | Divide by 10 for percentage |
| TTG | Time to go | minutes | -1 = infinite |
| CE | Consumed Ah | mAh | Negative = consumed |
| Alarm | Alarm condition | text | ON/OFF |
| Relay | Relay state | text | ON/OFF |
| H1 | Depth of deepest discharge | mAh | |
| H2 | Depth of last discharge | mAh | |
| H4 | Number of charge cycles | count | |
| H7 | Min battery voltage | mV | |
| H8 | Max battery voltage | mV | |

## MPPT Data Fields

| Key | Description | Unit | Notes |
|-----|-------------|------|-------|
| V | Battery voltage | mV | Divide by 1000 |
| I | Battery current | mA | Charge current |
| VPV | Panel voltage | mV | |
| PPV | Panel power | W | |
| CS | Charge state | enum | See below |
| ERR | Error code | enum | 0 = no error |
| H19 | Yield total | 0.01kWh | |
| H20 | Yield today | 0.01kWh | |
| H21 | Max power today | W | |
| H22 | Yield yesterday | 0.01kWh | |
| H23 | Max power yesterday | W | |

## Charge States

| Code | State |
|------|-------|
| 0 | Off |
| 2 | Fault |
| 3 | Bulk |
| 4 | Absorption |
| 5 | Float |
| 6 | Storage |
| 7 | Equalize |

## Error Codes (ERR)

| Code | Description |
|------|-------------|
| 0 | No error |
| 2 | Battery voltage too high |
| 17 | Charger temperature too high |
| 18 | Charger over current |
| 19 | Charger current reversed |
| 20 | Bulk time limit exceeded |
| 33 | Input voltage too high (solar) |
| 34 | Input current too high (solar) |

## Troubleshooting

| Symptom | Check |
|---------|-------|
| No SmartShunt data | GPIO 16, cable, VE.Direct enabled |
| No MPPT data | GPIO 19, cable, VE.Direct enabled |
| No power | Fuse, 5V converter, VIN connection |
| Corrupt data | Ground connection, baud rate 19200 |

## Full Documentation

See [IMPLEMENTATION_CONTEXT.md](IMPLEMENTATION_CONTEXT.md) for complete protocol details and C++ code architecture.
