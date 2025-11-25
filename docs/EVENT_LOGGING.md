# Event Logging to InfluxDB

The temperature sensor firmware now includes comprehensive event logging to track device activity, errors, and configuration changes in InfluxDB.

## Overview

All events are logged to the `device_events` measurement in InfluxDB with the following structure:

**Tags:**
- `device` - Device name (e.g., "Main_Cottage")
- `board` - Board type (e.g., "esp8266")
- `event_type` - Type of event (see Event Types below)
- `severity` - Severity level: `info`, `warning`, or `error`

**Fields:**
- `message` - Human-readable event description with details
- `value` - Always 1 (used for counting events)

## Event Types

### device_boot
Logged when the device starts up or reboots.

**Severity:** `info`

**Message includes:**
- Reset reason (Power on, External System, Software restart, etc.)
- Uptime at boot (always 0s)
- Free heap memory in bytes

**Example:**
```
Device started - Reset reason: External System, Uptime: 0s, Free heap: 44048 bytes
```

### wifi_connected
Logged when WiFi successfully connects.

**Severity:** `info`

**Message includes:**
- SSID of connected network
- Assigned IP address

**Example:**
```
Connected to AA229-2G with IP 192.168.0.139
```

### wifi_reconnect
Logged when WiFi disconnects and reconnection is attempted.

**Severity:** `warning`

**Message includes:**
- Reconnection attempt number
- Note: Only logged every 5th reconnect attempt to avoid spam

**Example:**
```
WiFi disconnected, reconnect attempt #1
```

### device_configured
Logged when device configuration changes through WiFi portal.

**Severity:** `info`

**Message includes (when name changes):**
- Old device name
- New device name
- Connected SSID
- IP address

**Message includes (when WiFi changes but name doesn't):**
- SSID
- IP address
- Confirmation that name is unchanged

**Examples:**
```
Name: 'Main Cottage' -> 'Guest House', SSID: AA229-2G, IP: 192.168.0.139
WiFi reconfigured - SSID: NewNetwork, IP: 192.168.1.100, Name unchanged: Main Cottage
```

### sensor_error
Logged when the DS18B20 temperature sensor read fails.

**Severity:** `error`

**Message:**
```
DS18B20 read failed
```

### influxdb_error
Logged when sending data to InfluxDB fails.

**Severity:** `error`

**Message includes:**
- Error description
- Note: Only logged every 10th failure to avoid spam

**Example:**
```
POST failed: connection failed
```

## Querying Events in Grafana

### Basic Query (All Events)
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r._field == "message")
```

### Formatted Table View
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r._field == "message")
  |> map(fn: (r) => ({
      timestamp: r._time,
      device_name: r.device,
      event: r.event_type,
      details: r._value
  }))
  |> sort(columns: ["timestamp"], desc: true)
```

### Filter by Device
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r.device == "Main_Cottage")
  |> filter(fn: (r) => r._field == "message")
```

### Filter by Event Type
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r.event_type == "device_boot")
  |> filter(fn: (r) => r._field == "message")
```

### Errors Only
```flux
from(bucket: "sensor_data")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r.severity == "error")
  |> filter(fn: (r) => r._field == "message")
```

## Grafana Visualizations

### Table Panel (Event Log)
Use the "Formatted Table View" query above with **Table** visualization.

### Stat Panel (Error Count)
```flux
from(bucket: "sensor_data")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r.severity == "error")
  |> filter(fn: (r) => r._field == "value")
  |> sum()
```

**Panel Settings:**
- Set **No value** to `0` to display 0 instead of "No data"
- Add color thresholds: Green = 0, Red > 0

### Time Series (Errors Over Time)
```flux
from(bucket: "sensor_data")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r.severity == "error")
  |> filter(fn: (r) => r._field == "value")
  |> aggregateWindow(every: v.windowPeriod, fn: sum, createEmpty: false)
  |> group(columns: ["device"])
```

### Stat Panel (Errors by Device)
```flux
from(bucket: "sensor_data")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r._measurement == "device_events")
  |> filter(fn: (r) => r.severity == "error")
  |> filter(fn: (r) => r._field == "value")
  |> group(columns: ["device"])
  |> sum()
```

## Use Cases

### Monitoring Device Health
- Track boot events to detect unexpected restarts
- Monitor WiFi reconnection frequency
- Identify devices with persistent sensor errors

### Troubleshooting
- Review reset reasons to identify crash patterns
- Analyze InfluxDB connection failures
- Track configuration changes that preceded issues

### Configuration Tracking
- Audit device name changes
- Monitor network changes
- Track when devices were reconfigured

### Alerting
Create Grafana alerts for:
- Error severity events
- Frequent WiFi reconnections
- Multiple sensor failures
- Unexpected device reboots

## Technical Implementation

Events are sent using the `sendEventToInfluxDB()` function:

```cpp
void sendEventToInfluxDB(const String& eventType, const String& message, const String& severity = "info")
```

The function:
1. Checks WiFi connectivity
2. Formats the InfluxDB line protocol payload
3. POSTs to InfluxDB with proper authentication
4. Logs success/failure to Serial

Events are throttled where appropriate:
- WiFi reconnect: Every 5th attempt
- InfluxDB errors: Every 10th failure
- Sensor errors: Every occurrence

This prevents log flooding while maintaining visibility into issues.
