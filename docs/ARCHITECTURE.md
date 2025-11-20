# System Architecture

This document describes the complete architecture of the ESP temperature monitoring system, including hardware, software, network topology, and data flow.

## High-Level Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                        Home Network                               │
│                      (192.168.0.x/24)                            │
│                                                                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ESP8266 #1   │  │ESP8266 #2   │  │ESP8266 #3   │             │
│  │Big Garage   │  │Small Garage │  │Pump House   │  + ESP32    │
│  │+ DS18B20    │  │+ DS18B20    │  │+ DS18B20    │  Main       │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  Cottage    │
│         │                │                │         + DS18B20   │
│         └────────────────┴────────────────┴────────┬────────────┤
│                                                     │            │
│                    WiFi: AA229-2G / AA225-2G-OD    │            │
│                                                     │            │
│                                                     ▼            │
│                                          ┌──────────────────┐   │
│                                          │ Raspberry Pi     │   │
│                                          │ 192.168.0.167    │   │
│                                          │                  │   │
│                                          │ Docker Stack:    │   │
│                                          │ • InfluxDB       │   │
│                                          │ • Grafana        │   │
│                                          │ • Home Assistant │   │
│                                          │ • Prometheus     │   │
│                                          │ • MQTT           │   │
│                                          │ • Nginx PM       │   │
│                                          │ • Cloudflared    │   │
│                                          └────────┬─────────┘   │
│                                                   │              │
└───────────────────────────────────────────────────┼──────────────┘
                                                    │
                                            Cloudflare Tunnel
                                                    │
                                                    ▼
                                            Internet Access
                                            (xgrunt.com)
```

## Component Details

### ESP Devices (Edge Sensors)

#### Hardware
- **Microcontrollers**:
  - 3x ESP8266 (NodeMCU v2)
  - 1x ESP32 (esp32dev)
- **Sensors**: DS18B20 Digital Temperature Sensor (1-Wire protocol)
- **GPIO**: Pin 4 for OneWire communication
- **Power**: USB power (5V)

#### Firmware
- **Framework**: Arduino (PlatformIO)
- **Key Libraries**:
  - `ESPAsyncWebServer` - Non-blocking HTTP server
  - `ESPAsyncTCP` / `AsyncTCP` - Async network stack
  - `OneWire` - DS18B20 communication protocol
  - `DallasTemperature` - DS18B20 temperature reading
  - `ESP8266HTTPClient` / `HTTPClient` - HTTP POST to InfluxDB

#### Network Configuration
- **WiFi Fallback**: Tries 3 networks in order (AA229-2G → AA225-2G-OD → AASTAR)
- **Static IP**: Optional (DHCP by default)
- **Protocols**: HTTP (port 80), InfluxDB Line Protocol

#### Deployed Devices

| Device | Location | Board | IP Address | WiFi Reconnects |
|--------|----------|-------|------------|-----------------|
| Big Garage Temperature | Big Garage | ESP8266 | DHCP | 1487 |
| Small Garage Temperature | Small Garage | ESP8266 | DHCP | 0 |
| Pump House Temperature | Pump House | ESP8266 | DHCP | 0 |
| Main Cottage Temperature | Main Cottage | ESP32 | DHCP | 0 |

### Raspberry Pi (Central Server)

#### Hardware
- **Model**: Raspberry Pi (unspecified model)
- **OS**: Linux (Raspberry Pi OS / Raspbian)
- **IP**: 192.168.0.167 (static)
- **Storage**: SD card + USB drive (optional)

#### Docker Stack

All services run via Docker Compose (repository: [raspberry-pi-docker](https://github.com/aachtenberg/raspberry-pi-docker))

##### Data Storage & Visualization

**InfluxDB 2.7**
- **Port**: 8086
- **Purpose**: Time-series database for sensor data
- **Organization**: `d990ccd978a70382`
- **Bucket**: `sensor_data`
- **Protocol**: HTTP (InfluxDB Line Protocol)
- **Retention**: Infinite (default)

**Grafana**
- **Port**: 3000
- **Purpose**: Dashboards and data visualization
- **Data Source**: InfluxDB
- **Dashboards**: "Temperatures Rue Romain" + custom dashboards

##### Monitoring & Logging

**Prometheus Stack**
- **Prometheus** (port 9090) - Metrics collection
- **Loki** (port 3100) - Log aggregation
- **Promtail** - Log shipping to Loki
- **Node Exporter** (port 9100) - Pi system metrics
- **cAdvisor** (port 8081) - Container metrics

**Mosquitto**
- **Port**: 1883
- **Purpose**: MQTT broker (optional for ESP devices)

##### Automation & Access

**Home Assistant**
- **Port**: 8123
- **Purpose**: Home automation platform
- **Integration**: Reads HTTP endpoints from ESP devices directly

**Nginx Proxy Manager**
- **Ports**: 81 (admin), 8080 (HTTP), 8443 (HTTPS)
- **Purpose**: Reverse proxy (HTTP → HTTPS conversion for Cloudflare)

**Cloudflared**
- **Mode**: Host networking (no port restrictions)
- **Purpose**: Cloudflare Tunnel for remote access
- **Domain**: xgrunt.com
- **Authentication**: Token-based

## Data Flow

### 1. Temperature Reading Cycle (15-second interval)

```
┌───────────────────────────────────────────────────────────────┐
│ ESP Device (every 15 seconds)                                 │
└───────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    Read DS18B20 sensor (GPIO 4)
                              │
                              ▼
                    Convert to Celsius & Fahrenheit
                              │
                              ▼
              ┌───────────────┴───────────────┐
              │                               │
              ▼                               ▼
    Store in local vars          Format InfluxDB Line Protocol
    (for HTTP endpoints)         temperature,device=Big\ Garage,
                                 location=garage tempC=23.56,
                                 tempF=74.41,wifi_reconnects=1487
              │                               │
              │                               ▼
              │                 HTTP POST to 192.168.0.167:8086
              │                 /api/v2/write?org=...&bucket=...
              │                 Authorization: Token ...
              │                               │
              ▼                               ▼
    Serve via AsyncWebServer       InfluxDB stores data
    - GET /                        in "sensor_data" bucket
    - GET /temperaturec
    - GET /temperaturef
```

### 2. Home Assistant Integration

```
┌─────────────────┐
│ Home Assistant  │
│   (Docker)      │
└────────┬────────┘
         │
         │ HTTP GET every 30s
         ▼
┌─────────────────────────────────────────┐
│ ESP Device HTTP Endpoints               │
│ http://<ESP_IP>/temperaturec  → 23.56  │
│ http://<ESP_IP>/temperaturef  → 74.41  │
└─────────────────────────────────────────┘
```

Home Assistant does **NOT** read from InfluxDB. It directly polls ESP HTTP endpoints.

### 3. Grafana Visualization

```
┌─────────────┐
│   Grafana   │
│  (Docker)   │
└──────┬──────┘
       │
       │ Flux queries
       ▼
┌─────────────────────────────────────────────────┐
│ InfluxDB                                        │
│ Query: from(bucket: "sensor_data")             │
│        |> range(start: -24h)                   │
│        |> filter(fn: (r) => r._field == "tempC")│
└─────────────────────────────────────────────────┘
       │
       │ Returns time-series data
       ▼
┌─────────────────────────────────────────┐
│ Grafana renders dashboard:              │
│ • Temperature over time (line graph)    │
│ • Min/Max/Avg (stat panels)             │
│ • Multi-device comparison               │
└─────────────────────────────────────────┘
```

### 4. Remote Access via Cloudflare

```
┌──────────────────┐
│ Internet User    │
│ (mobile/laptop)  │
└────────┬─────────┘
         │
         │ HTTPS (xgrunt.com)
         ▼
┌─────────────────────┐
│ Cloudflare Network  │
│ (Edge Servers)      │
└────────┬────────────┘
         │
         │ Cloudflare Tunnel (encrypted)
         ▼
┌──────────────────────────────┐
│ cloudflared (Docker)         │
│ Host network mode            │
│ Raspberry Pi 192.168.0.167   │
└────────┬─────────────────────┘
         │
         │ HTTP (local network)
         ▼
┌──────────────────────────────┐
│ Nginx Proxy Manager          │
│ HTTP → HTTPS conversion      │
└────────┬─────────────────────┘
         │
         ▼
┌──────────────────────────────┐
│ Home Assistant :8123         │
│ (serves Home Assistant UI)   │
└──────────────────────────────┘
```

**Note**: Cloudflare requires HTTPS, so Nginx converts HTTP (Home Assistant) → HTTPS (Cloudflare Tunnel).

## Network Topology

```
Internet
    │
    │ HTTPS
    ▼
Cloudflare Tunnel (cloudflared)
    │
    │ Host Network
    ▼
┌───────────────────────────────────────────────────────┐
│ Raspberry Pi (192.168.0.167)                          │
│                                                        │
│  ┌──────────────────────────────────────────────┐    │
│  │ Docker Network: "monitoring" (bridge)        │    │
│  │                                               │    │
│  │  influxdb:8086                                │    │
│  │  grafana:3000                                 │    │
│  │  prometheus:9090                              │    │
│  │  loki:3100                                    │    │
│  │  mosquitto:1883                               │    │
│  │  homeassistant:8123                           │    │
│  │  nginx-proxy-manager:81,8080,8443            │    │
│  │                                               │    │
│  └──────────────────────────────────────────────┘    │
│                                                        │
│  cloudflared (host network mode - no bridge)          │
└───────────────────────────────────────────────────────┘
         ▲
         │ WiFi: AA229-2G / AA225-2G-OD
         │
    ┌────┴────┬─────────┬─────────┐
    │         │         │         │
ESP8266   ESP8266   ESP8266   ESP32
Big G     Small G   Pump H    Main C
```

## Configuration Management

### ESP Device Configuration

**[include/secrets.h](../include/secrets.h)**
```cpp
// WiFi Networks (fallback support)
#define NUM_WIFI_NETWORKS 3
static const WiFiNetwork wifi_networks[] = {
  {"AA229-2G", "canada99"},
  {"AA225-2G-OD", "canada99"},
  {"AASTAR", "canada99"}
};

// InfluxDB Configuration
#define USE_LOCAL_INFLUXDB true
static const char* INFLUXDB_URL = "http://192.168.0.167:8086";
static const char* INFLUXDB_ORG = "d990ccd978a70382";
static const char* INFLUXDB_BUCKET = "sensor_data";
static const char* INFLUXDB_TOKEN = "y67e6b...";

// Optional MQTT
static const char* MQTT_BROKER = "192.168.0.167";
static const int MQTT_PORT = 1833;  // Note: Typo, should be 1883

// Cloudflare Tunnel Token (in .env on Pi, not ESP)
```

**[include/device_config.h](../include/device_config.h)**
```cpp
// Device-specific settings
static const char* DEVICE_NAME = "Big Garage Temperature";
static const char* DEVICE_LOCATION = "garage";
```

### Raspberry Pi Configuration

**Location**: `/home/aachten/docker/`
**Repository**: https://github.com/aachtenberg/raspberry-pi-docker

**docker-compose.yml**
- Defines all 11 services
- Volume mappings
- Network configuration
- Environment variables

**.env** (gitignored)
```bash
CLOUDFLARE_TUNNEL_TOKEN=eyJhIjoi...
```

## Security Considerations

### Network Security
- **Local network only**: ESP devices only accessible on LAN (192.168.0.x)
- **Cloudflare Tunnel**: Encrypted tunnel for remote access (no open ports)
- **Token authentication**: InfluxDB uses API tokens (not basic auth)

### Credential Management
- **ESP**: Secrets in `include/secrets.h` (gitignored)
- **Pi**: Secrets in `.env` (gitignored)
- **GitHub**: Both repositories are **private**

### Known Issues
- ⚠️ InfluxDB token visible in docker-compose.yml (mitigated by private repo)
- ⚠️ WiFi passwords in ESP firmware (can't be extracted easily, but visible in source)
- ⚠️ MQTT port typo in secrets.h (1833 instead of 1883) - MQTT may not work

## Performance Characteristics

### ESP Devices
- **Reading interval**: 15 seconds
- **HTTP request time**: ~200-500ms to InfluxDB
- **Memory usage**: ~40KB free (ESP8266), ~200KB free (ESP32)
- **Power consumption**: ~80mA idle, ~170mA during WiFi transmit

### Raspberry Pi
- **InfluxDB write rate**: 4 devices × 4 readings/min = 16 writes/min (~0.27 Hz)
- **Storage growth**: ~1KB/day per device (estimated)
- **Grafana query time**: <1s for 24-hour range
- **Docker CPU usage**: ~5-10% total across all containers

### Network Traffic
- **ESP → InfluxDB**: ~200 bytes every 15s per device = 800 bytes/min total
- **Home Assistant → ESP**: Polls 4 devices every 30s = 8 requests/min
- **Total bandwidth**: Negligible (<1 KB/min)

## Failure Modes & Resilience

### ESP Device Failures

**WiFi Connection Loss**
- **Detection**: `WiFi.status() != WL_CONNECTED`
- **Recovery**: Automatic reconnect with exponential backoff
- **Impact**: Data loss during downtime (no local buffering)
- **Tracking**: `wifi_reconnects` counter in InfluxDB

**Sensor Failure**
- **Detection**: Temperature reads as 85.0°C or `DEVICE_DISCONNECTED_C`
- **Recovery**: Retry on next read cycle (no alert)
- **Impact**: Missing data points (shows as gaps in Grafana)

**InfluxDB Unreachable**
- **Detection**: HTTP POST returns non-200 status
- **Recovery**: Logs error, retries on next cycle (no backoff)
- **Impact**: Data loss (no local persistence)

### Raspberry Pi Failures

**InfluxDB Container Crash**
- **Detection**: Docker healthcheck (if configured)
- **Recovery**: `restart: unless-stopped` policy auto-restarts
- **Impact**: Data loss during downtime

**Grafana Container Crash**
- **Impact**: Dashboards unavailable, data still stored in InfluxDB
- **Recovery**: Auto-restart via Docker

**Power Loss**
- **Impact**: All containers stop, ESP devices lose connection
- **Recovery**: Docker Compose auto-starts on Pi boot
- **Data Loss**: InfluxDB data persists in Docker volume

**SD Card Corruption**
- **Impact**: Complete system failure
- **Recovery**: Restore from backup (see backup.sh script)
- **Mitigation**: Regular backups to external storage

## Monitoring & Observability

### Metrics Collected

**ESP Device Metrics** (in InfluxDB)
```
measurement: temperature
tags:
  - device (e.g., "Big Garage Temperature")
  - location (e.g., "garage")
fields:
  - tempC (float)
  - tempF (float)
  - wifi_reconnects (int)
```

**Prometheus Metrics** (Pi system)
- Node Exporter: CPU, memory, disk, network
- cAdvisor: Docker container stats
- Loki: Log aggregation (not structured metrics)

### Dashboards

**Grafana**
- "Temperatures Rue Romain" - Main temperature dashboard
- Custom dashboards (user-created)

**Prometheus**
- System metrics dashboard (CPU, memory, disk)

### Logging

**ESP Devices**
- Serial output: 115200 baud (UART)
- Logs: Connection status, temperature readings, errors
- **No remote logging** (AWS CloudWatch removed)

**Raspberry Pi**
- Docker logs: `docker compose logs -f [service]`
- Loki: Aggregates container logs
- Promtail: Ships logs to Loki

## Deployment Process

### ESP Device Deployment

1. **Configure**: Edit `include/secrets.h` and `include/device_config.h`
2. **Build**: `platformio run -e esp8266` (or `-e esp32dev`)
3. **Flash**: `platformio run -e esp8266 --target upload`
4. **Verify**: Serial monitor shows WiFi connection and temperature readings

**Automated Script**: `scripts/deploy_all_devices.sh`

### Raspberry Pi Deployment

1. **Clone repository**: `git clone https://github.com/aachtenberg/raspberry-pi-docker`
2. **Configure**: Create `.env` with Cloudflare token
3. **Start stack**: `docker compose up -d`
4. **Verify**: `docker compose ps` shows all services running

**Infrastructure as Code**: All configuration in Git

## Future Improvements

### Potential Enhancements
- [ ] Add local buffering on ESP (SPIFFS/LittleFS) for offline data retention
- [ ] Implement HTTPS for ESP → InfluxDB (currently HTTP)
- [ ] Add InfluxDB write API rate limiting to prevent overload
- [ ] Create InfluxDB retention policies (auto-delete old data)
- [ ] Implement alerting (Grafana alerts or Prometheus AlertManager)
- [ ] Add OTA (Over-The-Air) firmware updates for ESP devices
- [ ] Fix MQTT port typo (1833 → 1883)
- [ ] Add device health checks (temperature reading failures)
- [ ] Implement Grafana provisioning (dashboards as code)
- [ ] Add Home Assistant discovery (auto-detect ESP devices)

### Migration from AWS
Previously used AWS Lambda and CloudWatch (see [archive/AWS_CDK_SETUP.md](archive/AWS_CDK_SETUP.md)). Migrated to local InfluxDB for:
- Cost savings (no AWS charges)
- Faster local network access
- Data ownership and privacy
- Offline operation capability

## References

- **ESP Project**: https://github.com/aachtenberg/esp12f_ds18b20_temp_sensor
- **Pi Infrastructure**: https://github.com/aachtenberg/raspberry-pi-docker
- **InfluxDB Docs**: https://docs.influxdata.com/influxdb/v2.7/
- **Grafana Docs**: https://grafana.com/docs/
- **PlatformIO**: https://docs.platformio.org/
- **Random Nerd Tutorials**: https://randomnerdtutorials.com/esp32-ds18b20-temperature-arduino-ide/
