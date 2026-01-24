"""ESP Sensor Hub - MQTT Admin Panel"""
import logging
from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from mqtt_client import MQTTClient
from config import Config
import os
import re

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Initialize Flask app
app = Flask(__name__)
app.config.from_object(Config)

# Initialize SocketIO
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

# Initialize MQTT client
mqtt_client = None

def load_device_inventory():
    """Load device inventory from markdown file"""
    devices = []
    inventory_path = os.path.join(os.path.dirname(__file__), Config.DEVICE_INVENTORY_PATH)
    
    try:
        if not os.path.exists(inventory_path):
            logger.warning(f"Device inventory not found at {inventory_path}")
            return devices
        
        with open(inventory_path, 'r') as f:
            content = f.read()
            
        # Parse markdown table
        lines = content.split('\n')
        in_table = False
        
        for line in lines:
            if line.startswith('| Device Name'):
                in_table = True
                continue
            if in_table and line.startswith('|---'):
                continue
            if in_table and line.startswith('|'):
                parts = [p.strip() for p in line.split('|')[1:-1]]
                if len(parts) >= 5 and parts[0] and not parts[0].startswith('#'):
                    devices.append({
                        'name': parts[0],
                        'chip_id': parts[1],
                        'platform': parts[2],
                        'display': parts[3],
                        'ip': parts[4],
                        'status': parts[5] if len(parts) > 5 else 'Unknown',
                        'last_update': parts[6] if len(parts) > 6 else 'N/A'
                    })
            elif in_table and not line.startswith('|'):
                break
                
        logger.info(f"Loaded {len(devices)} devices from inventory")
        return devices
        
    except Exception as e:
        logger.error(f"Error loading device inventory: {e}")
        return devices

@app.route('/')
def index():
    """Main dashboard page"""
    devices = load_device_inventory()
    return render_template('index.html', 
                         devices=devices,
                         mqtt_broker=Config.MQTT_BROKER)

@app.route('/api/devices')
def get_devices():
    """API endpoint to get device list"""
    devices = load_device_inventory()
    device_states = mqtt_client.get_device_states() if mqtt_client else {}
    
    # Enrich devices with MQTT state and determine online status
    enriched_devices = []
    for device in devices:
        device_name = device['name']
        
        # Try exact match first, then try with hyphens (MQTT naming convention)
        mqtt_name = device_name
        if device_name not in device_states:
            # Try hyphenated version
            mqtt_name = device_name.replace(' ', '-')
        
        if mqtt_name in device_states:
            device['mqtt_state'] = device_states[mqtt_name]
            device['mqtt_name'] = mqtt_name
            device['online'] = True
            enriched_devices.append(device)
        elif device_name in device_states:
            device['mqtt_state'] = device_states[device_name]
            device['mqtt_name'] = device_name
            device['online'] = True
            enriched_devices.append(device)
        else:
            # Check if device is publishing under any name variation
            device['online'] = False
            # Only include devices that have published at least once
            # Don't show devices from inventory that never reported
    
    # Sort: online devices first, then by name
    enriched_devices.sort(key=lambda d: (not d.get('online', False), d['name']))
    
    return jsonify(enriched_devices)

@app.route('/api/messages')
def get_messages():
    """API endpoint to get recent MQTT messages"""
    messages = mqtt_client.get_recent_messages() if mqtt_client else []
    return jsonify(messages)

@app.route('/api/command', methods=['POST'])
def send_command():
    """API endpoint to send command to device"""
    data = request.json
    device = data.get('device')
    command = data.get('command')
    
    if not device or not command:
        return jsonify({'success': False, 'error': 'Device and command required'}), 400
    
    if mqtt_client:
        success = mqtt_client.publish_command(device, command)
        return jsonify({'success': success})
    else:
        return jsonify({'success': False, 'error': 'MQTT client not connected'}), 500

@socketio.on('connect')
def handle_connect():
    """Handle WebSocket client connection"""
    logger.info('WebSocket client connected')
    
    # Send current device states
    if mqtt_client:
        device_states = mqtt_client.get_device_states()
        emit('initial_state', {
            'devices': device_states,
            'messages': mqtt_client.get_recent_messages()
        })

@socketio.on('disconnect')
def handle_disconnect():
    """Handle WebSocket client disconnection"""
    logger.info('WebSocket client disconnected')

@socketio.on('send_command')
def handle_command(data):
    """Handle command from WebSocket client"""
    device = data.get('device')
    command = data.get('command')
    
    if mqtt_client and device and command:
        success = mqtt_client.publish_command(device, command)
        emit('command_response', {'success': success, 'device': device, 'command': command})

def main():
    """Main entry point"""
    global mqtt_client
    
    logger.info("Starting ESP Sensor Hub Admin Panel")
    logger.info(f"MQTT Broker: {Config.MQTT_BROKER}:{Config.MQTT_PORT}")
    logger.info(f"Web Server: http://{Config.HOST}:{Config.PORT}")
    
    # Initialize and connect MQTT client
    mqtt_client = MQTTClient(socketio)
    try:
        mqtt_client.connect()
        logger.info("✓ MQTT client connected successfully")
    except Exception as e:
        logger.warning(f"⚠ MQTT connection failed: {e}")
        logger.warning("Web interface will start but MQTT features unavailable")
        logger.warning("Check MQTT_BROKER setting in .env file")
        mqtt_client = None
    
    try:
        # Start Flask-SocketIO server
        socketio.run(app, 
                    host=Config.HOST, 
                    port=Config.PORT, 
                    debug=Config.DEBUG,
                    use_reloader=False)  # Disable reloader to prevent duplicate MQTT connections
    except KeyboardInterrupt:
        logger.info("Shutting down...")
    finally:
        if mqtt_client:
            mqtt_client.disconnect()

if __name__ == '__main__':
    main()
