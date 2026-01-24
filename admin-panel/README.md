# ESP Sensor Hub - MQTT Admin Panel

Web-based administration panel for monitoring and controlling ESP32/ESP8266 temperature sensor devices via MQTT.

## Features

- 📡 **Real-time Device Monitoring**: View all devices from inventory with live status updates
- 📊 **Temperature Display**: Real-time temperature readings from all sensors
- 🎛️ **Device Control**: Send commands (restart, deep sleep, status requests)
- 📨 **MQTT Message Viewer**: Monitor all MQTT messages in real-time
- 🔌 **WebSocket Integration**: Live updates without page refresh
- 🎨 **Dark Theme UI**: Modern, responsive interface

## Screenshots

### Dashboard
- Device cards showing status, temperature, and controls
- MQTT message log with filtering
- Real-time connection status

## Requirements

- Python 3.8+ OR Docker
- MQTT broker (Mosquitto recommended)
- Network access to ESP devices

## Quick Start (Docker - Recommended)

### 1. Configure Environment

```bash
cd admin-panel
cp .env.example .env
# Edit .env with your MQTT broker IP
```

### 2. Run with Docker

```bash
./run-docker.sh
```

Or manually:

```bash
docker-compose up -d
```

The admin panel will be available at: **http://localhost:5000**

### Docker Commands

```bash
# View logs
docker-compose logs -f

# Stop
docker-compose down

# Restart
docker-compose restart

# Rebuild after code changes
docker-compose up -d --build
```

## Manual Installation (Without Docker)

### 1. Install Dependencies

```bash
cd admin-panel
pip install -r requirements.txt
```

### 2. Configure Environment

Copy the example environment file:

```bash
cp .env.example .env
```

Edit `.env` with your settings:

```bash
# MQTT Broker
MQTT_BROKER=192.168.0.100  # Your MQTT broker IP
MQTT_PORT=1883
MQTT_USERNAME=              # Optional
MQTT_PASSWORD=              # Optional

# Flask
SECRET_KEY=your-random-secret-key
FLASK_PORT=5000
```

### 3. Run the Application

Using the start script:

```bash
./start.sh
```

Or manually:

```bash
source venv/bin/activate
python app.py
```

The admin panel will be available at: **http://localhost:5000**

## Usage

### Device Monitoring

All devices from `temperature-sensor/docs/DEVICE_INVENTORY.md` are automatically loaded and displayed as cards showing:
- Device name, platform, chip ID
- Current temperature
- Online/offline status
- Last seen timestamp

### Sending Commands

**Available Commands:**

1. **Status Request** - Get current device status
   ```
   Click "📊 Status" button
   ```

2. **Restart Device** - Reboot the device
   ```
   Click "🔄 Restart" button
   ```

3. **Configure Deep Sleep** - Set sleep duration
   ```
   Click "😴 Sleep" button
   Enter seconds (0 to disable)
   ```

### MQTT Message Log

- View all incoming MQTT messages in real-time
- Filter by message type (temperature, status, events, commands)
- Auto-scroll option
- Color-coded by message type

## Architecture

### Components

1. **Flask Web Server** (`app.py`)
   - Serves web interface
   - REST API endpoints
   - WebSocket server

2. **MQTT Client** (`mqtt_client.py`)
   - Connects to MQTT broker
   - Subscribes to all esp-sensor-hub topics
   - Publishes commands
   - Maintains device state

3. **Configuration** (`config.py`)
   - Environment variable management
   - MQTT topic patterns
   - Device inventory path

4. **Web Interface** (`templates/index.html`, `static/`)
   - Real-time dashboard
   - Device control interface
   - Message viewer

### MQTT Topics

The admin panel subscribes to:
```
esp-sensor-hub/#
```

And publishes commands to:
```
esp-sensor-hub/{device-name}/command
```

### WebSocket Events

**Client → Server:**
- `send_command`: Send command to device

**Server → Client:**
- `mqtt_status`: MQTT connection status
- `mqtt_message`: New MQTT message received
- `device_update`: Device state changed
- `initial_state`: Full state on connection

## API Endpoints

### GET `/`
Main dashboard page

### GET `/api/devices`
Returns JSON array of all devices with current states

### GET `/api/messages`
Returns JSON array of recent MQTT messages (last 100)

### POST `/api/command`
Send command to device

**Request Body:**
```json
{
  "device": "Pump House",
  "command": "restart"
}
```

**Response:**
```json
{
  "success": true
}
```

## Development

### Running in Development Mode

```bash
export FLASK_DEBUG=True
python app.py
```

### Adding New Features

1. **New Commands**: Add to command buttons in `templates/index.html`
2. **Message Filters**: Update filter options in `static/app.js`
3. **Device Actions**: Extend `mqtt_client.py` publish methods

## Troubleshooting

### MQTT Connection Failed

**Check:**
- MQTT broker is running: `systemctl status mosquitto`
- Network connectivity: `ping {MQTT_BROKER}`
- Firewall allows port 1883
- Username/password correct (if authentication enabled)

### Devices Not Appearing

**Check:**
- Device inventory file path in `.env`
- Devices are publishing to correct MQTT topics
- MQTT broker shows subscriptions: `mosquitto_sub -h localhost -t "esp-sensor-hub/#" -v`

### WebSocket Disconnects

**Check:**
- Browser console for errors
- Flask logs for connection issues
- Network stability

## Security Considerations

- Change `SECRET_KEY` in production
- Use MQTT authentication (username/password)
- Consider MQTT over TLS for remote access
- Restrict Flask `HOST` to localhost for local-only access
- Use reverse proxy (nginx) for production deployment

## Production Deployment

### Using systemd Service

Create `/etc/systemd/system/mqtt-admin-panel.service`:

```ini
[Unit]
Description=ESP Sensor Hub MQTT Admin Panel
After=network.target mosquitto.service

[Service]
Type=simple
User=your-user
WorkingDirectory=/path/to/esp-sensor-hub/admin-panel
Environment="PATH=/path/to/venv/bin"
ExecStart=/path/to/venv/bin/python app.py
Restart=always

[Install]
WantedBy=multi-u (Included)

The project includes full Docker support:

**Quick Start:**
```bash
./run-docker.sh
```

**Docker Compose:**
```bash
docker-compose up -d
```

**Features:**
- Auto-restart on failure
- Health checks
- Volume mount for live device inventory updates
- Configurable via .env file
- Optimized multi-stage build with caching

**Network Configuration:**

By default, uses bridge networking. If your MQTT broker is on the host machine:

1. Edit `docker-compose.yml`
2. Uncomment `network_mode: host`
3. Comment out `ports` and `networks` sections
EXPOSE 5000
CMD ["python", "app.py"]
```

Build and run:
```bash
docker build -t mqtt-admin-panel .
docker run -d -p 5000:5000 --env-file .env mqtt-admin-panel
```

## Contributing

1. Create feature branch: `git checkout -b feature/new-feature`
2. Make changes
3. Test thoroughly
4. Submit pull request

## License

Same as parent project (esp-sensor-hub)

## Related Documentation

- [Temperature Sensor Configuration](../docs/temperature-sensor/CONFIG.md)
- [Platform Guide](../docs/reference/PLATFORM_GUIDE.md)
- [Device Inventory](../temperature-sensor/docs/DEVICE_INVENTORY.md)
- [MQTT Device Control Script](../scripts/mqtt_device_control.sh)
