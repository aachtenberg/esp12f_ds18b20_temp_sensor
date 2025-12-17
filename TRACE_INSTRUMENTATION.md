# MQTT Trace Instrumentation Implementation

## Overview
This document describes the trace ID instrumentation added to all MQTT payloads in the temperature-sensor and surveillance projects. The implementation includes both backward-compatible trace fields and W3C Trace Context (traceparent) headers for distributed tracing.

## Purpose
Enable end-to-end request tracking and message correlation across MQTT topics and backend systems using:
- Unique trace IDs (UUID format for backward compatibility)
- W3C traceparent headers (for distributed tracing standards compliance)
- Sequence numbers (for message ordering)
- Span IDs (for distributed tracing)

## Implementation Details

### Trace ID Generation (Dual Format)

The trace system generates trace IDs in two formats simultaneously:

1. **UUID Format (Backward Compatibility)**
   - Format: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` (36 characters)
   - Used for: Legacy backend systems, readable logs
   - Generation:
     - ESP32: Derived from EfuseMac + millis()
     - ESP8266: Derived from ChipID + millis()

2. **32-Character Hex Format (W3C Standard)**
   - Format: `xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx` (32 hex characters, no dashes)
   - Used for: W3C traceparent header, distributed tracing systems
   - Generation:
     - ESP32: First 16 chars from EfuseMac, next 16 from boot_ms
     - ESP8266: Padded combination of ChipID and boot_ms

- **Persistence**: Constant per boot session; regenerated on device restart
- **No External Dependencies**: Uses only Arduino.h and standard C++ (sstream)

### W3C Traceparent Header
- **Format**: `00-{trace_id}-{span_id}-01`
  - `00`: Version (W3C Trace Context v1.0)
  - `{trace_id}`: 32 hex character trace identifier
  - `{span_id}`: 16 hex character span identifier
  - `01`: Trace flags (sampled; 01 = recorded, 00 = not recorded)
- **Example**: `00-a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6-f7b8d9c0e1a2b3c4-01`
- **Standard**: Compliant with [W3C Trace Context v1.0](https://www.w3.org/TR/trace-context/)

### Span ID Generation
- **Type**: 16 hex character string
- **Format**: `xxxxxxxxxxxxxxxx` (derived from chip_id XOR boot_ms)
- **Purpose**: Identifies individual operations within a trace
- **Uniqueness**: Combined with trace_id, creates globally unique span identifier

### Sequence Numbering
- **Type**: Monotonic unsigned 32-bit counter
- **Range**: 1 to 4,294,967,295 per boot
- **Reset**: Counter resets to 0 on device reboot
- **Increment**: Increments on every `getSequenceNumber()` call
- **Overflow**: At 1 message/second, counter runs for ~136 years before overflow

### Schema Versioning
- **Field**: `schema_version`
- **Type**: Integer
- **Current Version**: 1
- **Purpose**: Enables backward-compatible schema evolution in backend systems

## Files Modified

### New Trace Utility Files
```
temperature-sensor/include/trace.h
temperature-sensor/src/trace.cpp
surveillance/include/trace.h
surveillance/src/trace.cpp
```

### Modified Main Files
```
temperature-sensor/src/main.cpp
  - Added #include "trace.h"
  - Added Trace::init() call in setup()
  - Instrumented: publishEvent(), publishTemperature(), publishStatus()

surveillance/src/main.cpp
  - Added #include "trace.h"
  - Added Trace::init() call in setup()
  - Instrumented: publishMetricsToMQTT(), logEventToMQTT()
```

## MQTT Payload Examples

### Temperature Sensor - Temperature Reading
```json
{
  "device": "temp-sensor-01",
  "chip_id": "a1b2c3d4",
  "trace_id": "a1b2c3d4-1a2b-3c4d-5e6f-7a8b9c0d1e2f",
  "traceparent": "00-a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6-f7b8d9c0e1a2b3c4-01",
  "seq_num": 1,
  "schema_version": 1,
  "timestamp": 1234567890,
  "celsius": 22.5,
  "fahrenheit": 72.5
}
```

### Surveillance - Metrics
```json
{
  "device": "esp32-cam-01",
  "chip_id": "f1e2d3c4",
  "trace_id": "f1e2d3c4-2b3c-4d5e-6f7a-8b9c0d1e2f3a",
  "traceparent": "00-f1e2d3c42b3c4d5e6f7a8b9c0d1e2f3a-4f7b8d9c0e1a2b3c-01",
  "seq_num": 145,
  "schema_version": 1,
  "location": "surveillance",
  "timestamp": 1234567890,
  "uptime": 3600,
  "wifi_rssi": -65,
  "free_heap": 262144,
  "camera_ready": 1,
  "mqtt_connected": 1,
  "capture_count": 42
}
```

## Build Verification

### Successful Builds
- ✅ ESP32 (esp32dev)
  - RAM: 14.1% (46,336 bytes / 327,680 bytes)
  - Flash: 75.4% (988,417 bytes / 1,310,720 bytes)

- ✅ ESP8266 (nodemcuv2)
  - RAM: 41.7% (34,124 bytes / 81,920 bytes)
  - Flash: 39.3% (410,199 bytes / 1,044,464 bytes)

- ✅ ESP32-S3 (freenove_esp32_s3_wroom)
  - RAM: 16.8% (55,016 bytes / 327,680 bytes)
  - Flash: 36.3% (1,141,725 bytes / 3,145,728 bytes)

- ✅ Surveillance ESP32-S3 (esp32-s3-devkitc-1)
  - All environments compile successfully

## API Reference

### Trace Namespace Functions

#### `void init()`
Initialize trace system at device boot.
- Must be called once during setup()
- Generates unique trace ID for this session
- Resets sequence counter to 0

**Example:**
```cpp
void setup() {
  Serial.begin(115200);
  Trace::init();  // Initialize after Serial is ready
  // ... rest of setup
}
```

#### `std::string getTraceId()`
Get the current trace ID (UUID-like string).
- Returns: 36-character UUID-formatted string
- Same value for entire boot session
- Safe to call multiple times

**Example:**
```cpp
std::string id = Trace::getTraceId();  // "a1b2c3d4-1a2b-3c4d-5e6f-7a8b9c0d1e2f"
```

#### `uint32_t getSequenceNumber()`
Get next sequence number for this message.
- Increments counter on each call
- First call returns 1, second returns 2, etc.
- Safe for concurrent calls (monotonic)

**Example:**
```cpp
uint32_t seq = Trace::getSequenceNumber();  // First call: 1, Second call: 2
```

#### `std::string getTraceIdentifier()`
Get human-readable trace identifier.
- Returns: `{trace_id}:{seq_num}` format
- Useful for logs and debugging
- Example: `"a1b2c3d4-1a2b-3c4d-5e6f-7a8b9c0d1e2f:42"`

#### `std::string getTraceparent()`
Get the W3C traceparent header for distributed tracing.
- Returns: String in W3C traceparent format `00-{trace_id}-{span_id}-01`
- Compliant with [W3C Trace Context v1.0](https://www.w3.org/TR/trace-context/)
- Safe to call multiple times, same value for entire boot session
- Example: `"00-a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6-f7b8d9c0e1a2b3c4-01"`

**Use in MQTT payload:**
```cpp
JsonDocument doc;
doc["trace_id"] = Trace::getTraceId();        // UUID format: for backward compatibility
doc["traceparent"] = Trace::getTraceparent(); // W3C format: for distributed tracing
doc["seq_num"] = Trace::getSequenceNumber();
```

#### `std::string getSpanId()`
Get the span ID for distributed tracing.
- Returns: 16-character hex string
- Unique per device boot, same for all messages in session
- Used in W3C traceparent header
- Example: `"f7b8d9c0e1a2b3c4"`

## W3C Trace Context Integration

The W3C traceparent header is included in all MQTT payloads for compatibility with distributed tracing systems like:
- [OpenTelemetry](https://opentelemetry.io/)
- [Jaeger](https://www.jaegertracing.io/)
- [Zipkin](https://zipkin.io/)
- [DataDog](https://www.datadoghq.com/)
- [New Relic](https://newrelic.com/)

### Traceparent Format Breakdown
For example: `00-a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6-f7b8d9c0e1a2b3c4-01`

| Component | Format | Description | Example |
|-----------|--------|-------------|---------|
| Version | `00` | W3C Trace Context v1.0 | `00` |
| Trace-ID | 32 hex chars | Unique trace identifier | `a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6` |
| Span-ID | 16 hex chars | Current span identifier | `f7b8d9c0e1a2b3c4` |
| Trace-Flags | `01` or `00` | `01`=recorded, `00`=not recorded | `01` (always sampled) |

### Backend Integration

To use traceparent in your backend:

```python
# Python example with OpenTelemetry
from opentelemetry import trace
from opentelemetry.trace import set_span_in_context

def process_mqtt_message(payload):
    # Extract traceparent from MQTT payload
    traceparent = payload.get("traceparent")
    
    # Parse and propagate context
    ctx = TraceContextPropagator().extract({"traceparent": traceparent})
    
    with trace.get_tracer(__name__).start_as_current_span(
        "process_sensor_reading", context=ctx
    ) as span:
        # Process the message with trace context
        process_sensor_data(payload)
```

## Integration Patterns

### Adding to Existing Publish Functions

1. **Include the header:**
   ```cpp
   #include "trace.h"
   ```

2. **Initialize in setup():**
   ```cpp
   void setup() {
     // ... other initialization
     Trace::init();
     // ... rest of setup
   }
   ```

3. **Add fields to payload (both backward-compatible and W3C):**
   ```cpp
   JsonDocument doc;
   doc["device"] = deviceName;
   doc["trace_id"] = Trace::getTraceId();        // UUID format for backward compatibility
   doc["traceparent"] = Trace::getTraceparent(); // W3C Trace Context header
   doc["seq_num"] = Trace::getSequenceNumber();
   doc["schema_version"] = 1;
   // ... rest of payload
   ```

## Future Enhancements

### Backward Compatibility Considerations
- `schema_version = 1`: Current schema with trace fields
- Backend should verify `schema_version` before accessing trace fields
- New devices always send `schema_version = 1`

### Schema Evolution
To add new fields in future:
1. Increment `schema_version` to 2
2. Send both old and new fields for compatibility
3. Backends can parse based on `schema_version`

### Multi-Message Correlation
- Use `trace_id` to correlate all messages from a single device session
- Use `seq_num` to order messages chronologically
- Combined `{trace_id}:{seq_num}` creates globally unique message identifier

## Testing Recommendations

1. **Verify trace IDs are unique across boots:**
   - Restart device, check trace ID changes
   - Different devices should have different trace IDs

2. **Verify sequence numbers monotonically increase:**
   - Publish multiple messages
   - Confirm seq_num increments by 1 each time
   - Check seq_num resets to 0 after device restart

3. **Verify payload size remains within MQTT limits:**
   - Maximum payload: 1024 bytes (configured)
   - Trace fields add ~70 bytes overhead
   - Remaining space: ~950 bytes for other data

4. **Test on actual hardware:**
   - Flash firmware to ESP32, ESP8266, ESP32-S3 devices
   - Monitor MQTT traffic
   - Verify trace fields present in payloads

## Deployment Notes

### No Breaking Changes
- Adds new fields to JSON payloads
- Existing fields unchanged
- Backend systems can ignore trace fields if not needed
- Gradual rollout possible

### Recommended Actions
1. Update MQTT message consumer to extract and log `trace_id`
2. Update backend to use `trace_id` for request correlation
3. Add monitoring/alerting on trace ID uniqueness
4. Document trace ID format in API specifications

## References
- [ArduinoJson Documentation](https://arduinojson.org/)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [ESP32 Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [ESP8266 Reference](https://arduino-esp8266.readthedocs.io/)
