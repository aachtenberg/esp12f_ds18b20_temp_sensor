# MQTT Admin Panel - Quick Start 🚀

## 🐳 Docker (Recommended)

### First Time Setup
```bash
cd admin-panel
cp .env.example .env
# Edit .env - set MQTT_BROKER to your broker IP
```

### Start the Panel
```bash
./run-docker.sh
```

Or manually:
```bash
docker compose up -d
```

### Access
Open browser: **http://localhost:5000**

### Useful Commands
```bash
# View logs
docker compose logs -f

# Stop
docker compose down

# Restart
docker compose restart

# Rebuild after changes
docker compose up -d --build
```

---

## 🐍 Python (Alternative)

### Setup
```bash
cd admin-panel
./start.sh
```

### Access
Open browser: **http://localhost:5000**

---

## ⚙️ Configuration (.env)

**Important:** The panel needs to connect to your MQTT broker to receive device updates!

### Quick Configure
```bash
cd admin-panel
./configure-mqtt.sh
# Enter your broker IP (e.g., 192.168.0.100)
```

### Manual Configuration
Edit `.env` file:
```bash
# MQTT Broker (REQUIRED for live updates)
MQTT_BROKER=192.168.0.100  # Change to your broker IP

# Optional Settings
MQTT_PORT=1883
MQTT_USERNAME=              # If auth enabled
MQTT_PASSWORD=              # If auth enabled
FLASK_PORT=5000
```

**Finding your MQTT broker IP:**
- Same machine: Use host's IP (not localhost if using Docker)
- WSL2: Use Windows IP from `ip route show default`
- Other machine: Use broker's LAN IP

---

## 🎮 Features

### Device Cards
- Real-time temperature display
- Online/offline status
- Last seen timestamp
- Quick action buttons

### Commands Available
- **📊 Status** - Request device status
- **🔄 Restart** - Reboot device
- **😴 Sleep** - Configure deep sleep (seconds)

### MQTT Monitor
- Live message stream
- Filter by type (temperature/status/events/commands)
- Auto-scroll option
- Color-coded messages

---

## 📡 MQTT Topics

The panel interacts with:

**Subscribe (listen):**
```
esp-sensor-hub/#
```

**Publish (commands):**
```
esp-sensor-hub/{device-name}/command
```

---

## 🔧 Troubleshooting

### Container won't start
```bash
docker compose logs
```

### MQTT not connecting
1. **Check `MQTT_BROKER` in `.env`** - Must be reachable IP (not `localhost` in Docker)
2. **Use helper script**: `./configure-mqtt.sh`
3. Verify broker is running: `systemctl status mosquitto`
4. Test connectivity: `ping {MQTT_BROKER}`
5. Check firewall allows port 1883
6. **For WSL2**: Use Windows host IP, not localhost

### Can't access web interface
1. Check container is running: `docker ps`
2. Verify port 5000 not in use: `netstat -tlnp | grep 5000`
3. Try: `http://localhost:5000` or `http://127.0.0.1:5000`

### Devices not showing as online
1. Wait 10-30 seconds for MQTT messages
2. Verify devices are publishing to correct topics
3. Check MQTT broker: `mosquitto_sub -h localhost -t "esp-sensor-hub/#" -v`

---

## 🎯 Quick Test

After starting, you should see:
- ✅ Container healthy: `docker ps`
- ✅ W**"MQTT: Disconnected" warning** = Broker not configured (see Configuration above)
- ✅ **"MQTT: Connected" (green dot)** = Live updates working!
- ✅ Devices show "Online" with temperature when MQTT connected
- ✅ Messages appear in real-time log when MQTT connected

**Not seeing temperatures?**
1. Configure MQTT broker: `./configure-mqtt.sh`
2. Restart: `docker compose restart`
3. Check connection: Look for green "MQTT: Connected" indicator
4. Wait 30 seconds for device publish cycle
- ⚠️ MQTT warning if broker not configured (expected on first run)
- ✅ Devices show "Online" when MQTT connected

---

## 📚 Full Documentation

See [admin-panel/README.md](README.md) for complete documentation.
