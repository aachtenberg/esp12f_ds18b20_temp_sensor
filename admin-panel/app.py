"""ESP Sensor Hub - MQTT Admin Panel"""
import logging
import time
import os
from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from mqtt_client import MQTTClient
from config import Config
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
    
    # Track which MQTT devices we've matched
    matched_mqtt_devices = set()
    
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
            matched_mqtt_devices.add(mqtt_name)
        elif device_name in device_states:
            device['mqtt_state'] = device_states[device_name]
            device['mqtt_name'] = device_name
            device['online'] = True
            enriched_devices.append(device)
            matched_mqtt_devices.add(device_name)
    
    # Add devices publishing to MQTT but not in inventory
    for mqtt_name, state in device_states.items():
        if mqtt_name not in matched_mqtt_devices:
            # Create device entry for unknown MQTT device
            enriched_devices.append({
                'name': mqtt_name.replace('-', ' '),  # Display with spaces
                'chip_id': 'Unknown',
                'platform': 'Unknown',
                'display': 'Unknown',
                'ip': 'Unknown',
                'status': '✅ Active',
                'mqtt_state': state,
                'mqtt_name': mqtt_name,
                'online': True
            })
    
    # Sort: online devices first, then by name
    enriched_devices.sort(key=lambda d: (not d.get('online', False), d['name']))
    
    return jsonify(enriched_devices)

@app.route('/api/messages')
def get_messages():
    """API endpoint to get recent MQTT messages"""
    messages = mqtt_client.get_recent_messages() if mqtt_client else []
    return jsonify(messages)

@app.route('/api/config', methods=['GET', 'POST'])
def handle_config():
    """API endpoint to get/set MQTT configuration"""
    if request.method == 'GET':
        # Return current MQTT configuration
        return jsonify({
            'broker': Config.MQTT_BROKER,
            'port': Config.MQTT_PORT,
            'username': Config.MQTT_USERNAME or '',
            'password': ''  # Don't expose password
        })
    
    elif request.method == 'POST':
        # Update MQTT configuration
        data = request.json
        broker = data.get('broker', '').strip()
        port = data.get('port', 1883)
        username = data.get('username', '').strip() or None
        password = data.get('password', '').strip() or None
        
        # Validate input
        if not broker:
            return jsonify({'success': False, 'error': 'Broker address required'}), 400
        
        try:
            port = int(port)
            if port < 1 or port > 65535:
                raise ValueError("Port must be between 1 and 65535")
        except (ValueError, TypeError):
            return jsonify({'success': False, 'error': 'Invalid port number'}), 400
        
        # Test connection to new broker
        try:
            logger.info(f"Testing MQTT connection to {broker}:{port}")
            import paho.mqtt.client as mqtt_test
            
            test_connected = {'success': False}
            
            def on_test_connect(client, userdata, flags, rc):
                if rc == 0:
                    test_connected['success'] = True
                client.disconnect()
            
            test_client = mqtt_test.Client(client_id="mqtt-admin-test")
            test_client.on_connect = on_test_connect
            
            if username:
                test_client.username_pw_set(username, password)
            
            # Try to connect with short timeout
            try:
                test_client.connect(broker, port, keepalive=5)
                test_client.loop_start()
                
                # Wait for connection
                import time
                timeout = 5
                start = time.time()
                while not test_connected['success'] and (time.time() - start) < timeout:
                    time.sleep(0.1)
                
                test_client.loop_stop()
                test_client.disconnect()
                
                if not test_connected['success']:
                    return jsonify({
                        'success': False, 
                        'error': 'Could not connect to MQTT broker'
                    }), 400
            except Exception as conn_err:
                return jsonify({
                    'success': False,
                    'error': f'Connection failed: {str(conn_err)}'
                }), 400
            
            # Update environment variables
            os.environ['MQTT_BROKER'] = broker
            os.environ['MQTT_PORT'] = str(port)
            os.environ['MQTT_USERNAME'] = username or ''
            os.environ['MQTT_PASSWORD'] = password or ''
            
            # Update Config class
            Config.MQTT_BROKER = broker
            Config.MQTT_PORT = port
            Config.MQTT_USERNAME = username or ''
            Config.MQTT_PASSWORD = password or ''
            
            # Update .env file
            env_file = os.path.join(os.path.dirname(__file__), '.env')
            update_env_file(env_file, {
                'MQTT_BROKER': broker,
                'MQTT_PORT': str(port),
                'MQTT_USERNAME': username or '',
                'MQTT_PASSWORD': password or ''
            })
            
            # Reconnect MQTT client with new settings
            global mqtt_client
            if mqtt_client:
                mqtt_client.disconnect()
            mqtt_client = MQTTClient(socketio)
            mqtt_client.connect()
            
            logger.info(f"✓ MQTT configuration updated: {broker}:{port}")
            return jsonify({'success': True})
            
        except Exception as e:
            logger.error(f"MQTT connection test failed: {e}")
            return jsonify({'success': False, 'error': str(e)}), 400

def update_env_file(env_file, updates):
    """Update .env file with new configuration"""
    lines = []
    updated_keys = set()
    
    # Read existing file and update matching keys
    if os.path.exists(env_file):
        with open(env_file, 'r') as f:
            for line in f:
                updated = False
                for key, value in updates.items():
                    if line.startswith(f"{key}="):
                        lines.append(f"{key}={value}\n")
                        updated_keys.add(key)
                        updated = True
                        break
                if not updated:
                    lines.append(line)
    
    # Add any new keys that weren't in the file
    for key, value in updates.items():
        if key not in updated_keys:
            lines.append(f"{key}={value}\n")
    
    # Write updated file
    with open(env_file, 'w') as f:
        f.writelines(lines)

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
    logger.info(f'WebSocket client connected, SID: {request.sid}')
    
    # Send current device states and MQTT connection status
    if mqtt_client:
        device_states = mqtt_client.get_device_states()
        emit('initial_state', {
            'devices': device_states,
            'messages': mqtt_client.get_recent_messages(),
            'mqtt_connected': mqtt_client.client.is_connected()
        })

@socketio.on('disconnect')
def handle_disconnect():
    """Handle WebSocket client disconnection"""
    logger.info('WebSocket client disconnected')

@socketio.on('request_state')
def handle_request_state():
    """Handle client request for fresh state (polling workaround)"""
    logger.info('Client requested fresh state')
    if mqtt_client:
        device_states = mqtt_client.get_device_states()
        emit('initial_state', {
            'devices': device_states,
            'messages': mqtt_client.get_recent_messages(),
            'mqtt_connected': mqtt_client.client.is_connected()
        })

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
