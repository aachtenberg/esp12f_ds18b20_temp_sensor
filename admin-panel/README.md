# ESP Sensor Hub - Admin Panel v2.0

Modern web interface for monitoring and controlling ESP32/ESP8266 devices via MQTT.

## Features

- 📊 **Dashboard**: Device status overview, message stats, recent activity
- 🔧 **Device Management**: Status table with inline configuration controls
- 📨 **MQTT Log**: Real-time message monitoring with smart auto-scroll
- ⚙️ **Settings**: Configurable polling interval, device visibility, MQTT broker config

### Device Control
- **Status Table**: Device name, online/offline, type, IP, WiFi, deep sleep status
- **Slide-out Drawer** (900px): Click any device row to access:
  - Device-specific message log (last 50 messages)
  - Status & Restart commands
  - Deep Sleep configuration (0-3600s, 0=disabled)
  - Sensor Interval configuration (5-3600s)
- **Command Retry**: Auto-retries every 2s until confirmed (15 attempts max)
- **Toast Notifications**: Auto-dismiss feedback (2-3s)

## Quick Start

### Docker (Recommended)

```bash
cd admin-panel
cp .env.example .env
# Edit .env: set MQTT_BROKER to your broker IP
docker-compose up -d
```

**Access**: http://localhost:5000

### Manual Setup

```bash
cd admin-panel
python -m venv venv
source venv/bin/activate  # or venv\Scripts\activate on Windows
pip install -r requirements.txt

# Set environment variables
export MQTT_BROKER=192.168.0.167
export MQTT_PORT=1883

python app.py
```

## Configuration

### Environment Variables (.env)
```bash
MQTT_BROKER=192.168.0.167    # Required: MQTT broker IP
MQTT_PORT=1883               # Default: 1883
MQTT_USERNAME=               # Optional
MQTT_PASSWORD=               # Optional
HOST=0.0.0.0                 # Web server host
PORT=5000                    # Web server port
DEBUG=False                  # Debug mode
```

### Settings Panel
- **Polling Interval**: Adjust MQTT message polling (1-60s, default 10s)
- **Device Visibility**: Show/hide devices from table
- **MQTT Broker**: Test & save broker connection settings

## Device Commands

Send via device drawer or programmatically:

```javascript
// Via Socket.IO client
socket.emit('send_command', {
  device: 'Spa',
  command: 'deepsleep 30'  // or 'restart', 'status', 'interval 60'
});
```

**Supported Commands**:
- `status` - Request device status
- `restart` - Restart device
- `deepsleep <seconds>` - Set deep sleep interval (0=disable, 1-3600=enable)
- `interval <seconds>` - Set sensor reading interval (5-3600)

**Confirmation Patterns**:
- Monitors MQTT events topic for success
- Auto-retries until confirmed or max attempts reached
- Pattern matching: "deep_sleep_config", "sensor_interval_config", "restarting"

## Architecture

**Backend**: Flask + Flask-SocketIO (eventlet)  
**MQTT**: Paho client with background thread + polling workaround  
**Frontend**: Vanilla JavaScript + Socket.IO client  
**Storage**: LocalStorage for user preferences  
**Docker**: Single container with health checks  

### Polling Workaround
- Flask-SocketIO eventlet mode doesn't emit events from MQTT background thread
- Solution: Client polls server every N seconds (configurable 1-60s)
- Server maintains last 100 messages and device states in memory

### API Endpoints

**REST API**:
- `GET /` - Web interface
- `GET /health` - Health check
- `GET /api/config` - Get MQTT config
- `POST /api/config` - Update MQTT config
- `GET /api/inventory` - Get device inventory

**WebSocket Events**:
- `connect` - Socket.IO connection established
- `mqtt_message` - New MQTT message received
- `mqtt_status` - MQTT broker connection status
- `device_update` - Device state changed
- `command_response` - Command execution result
- `send_command` - Send command to device (emit)
- `request_state` - Request full state update (emit)

## Troubleshooting

**Messages not updating**:
- Check Settings → Polling Interval (default 10s)
- Lower interval for faster updates (increases server load)
- Check Docker logs: `docker-compose logs -f`

**MQTT connection failed**:
- Verify broker IP in .env file
- Test connectivity: `mosquitto_sub -h BROKER_IP -t "#" -v`
- Check Settings → MQTT Broker Configuration → Test & Save

**Device shows offline**:
- Check device last_seen timestamp (must be < 5 minutes)
- Verify device is publishing to `esp-sensor-hub/{device}/status`
- Check MQTT log for device messages

**Commands not confirming**:
- Device may be in deep sleep (retry will keep trying)
- Check device drawer message log for events topic
- Verify device firmware handles command topic

## Development

```bash
# Install dependencies
pip install -r requirements.txt

# Run in debug mode
DEBUG=True python app.py

# Watch logs
docker-compose logs -f

# Rebuild after changes
docker-compose up -d --build
```

## Version History

### v2.0 (January 2026)
- Complete UI redesign with modern dark theme
- Sidebar navigation with 4 sections
- 900px slide-out device drawer
- Inline configuration controls (deep sleep, sensor interval)
- Command retry logic with confirmation
- Configurable polling interval
- Auto-dismiss toast notifications
- UTC timezone fixes for local time display
- Smart auto-scrolling MQTT log

### v1.0 (2025)
- Initial release with basic monitoring
- Simple device cards layout
- Basic MQTT message display

## License

Part of ESP Sensor Hub project. See main repository LICENSE file.
