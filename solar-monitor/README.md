# ESP32 Solar Monitor Firmware

PlatformIO firmware for ESP32-based solar monitoring using Victron VE.Direct protocol.

## Hardware

| Component | Model | Connection |
|-----------|-------|------------|
| Microcontroller | ESP32-WROOM-32 | - |
| Battery Monitor | SmartShunt SHU050150050 | GPIO 16 (UART2 RX) |
| Charge Controller | SmartSolar MPPT SCC110050210 | GPIO 19 (UART1 RX) |
| Power Supply | 12V-to-5V 3A converter | VIN |

## Quick Start

1. Copy `include/secrets.h.example` to `include/secrets.h`
2. Edit `secrets.h` with your InfluxDB credentials
3. Build and upload:
   ```bash
   pio run --target upload
   pio device monitor
   ```
4. **Configure WiFi**: On first boot (or double-reset within 3 seconds), connect to the "Solar-Monitor-Setup" AP
5. Open browser to `http://192.168.4.1` for WiFiManager captive portal
6. Configure WiFi credentials and device settings

## API Endpoints

| Endpoint | Description |
|----------|-------------|
| `GET /` | HTML dashboard |
| `GET /api/battery` | SmartShunt data (JSON) |
| `GET /api/solar` | MPPT data (JSON) |
| `GET /api/system` | Combined status (JSON) |

## Project Structure

```
solar-monitor/
├── src/
│   ├── main.cpp             # Main application
│   ├── VictronSmartShunt.h  # SmartShunt driver
│   ├── VictronSmartShunt.cpp
│   ├── VictronMPPT.h        # MPPT driver
│   └── VictronMPPT.cpp
├── include/
│   └── secrets.h.example    # WiFi credentials template
├── platformio.ini           # PlatformIO config
└── README.md
```

## Documentation

See [docs/solar-monitor/](../docs/solar-monitor/) for:
- Complete implementation context
- Wiring guide
- Bill of materials
- Quick reference
