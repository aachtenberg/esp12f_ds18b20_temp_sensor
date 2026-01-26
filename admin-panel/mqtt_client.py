"""MQTT Client Handler for Admin Panel"""
import json
import logging
from datetime import datetime, timezone
import paho.mqtt.client as mqtt
from config import Config

logger = logging.getLogger(__name__)

class MQTTClient:
    def __init__(self, socketio):
        self.socketio = socketio
        self.client = mqtt.Client(client_id="mqtt-admin-panel")
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        # Message storage (last 100 messages)
        self.messages = []
        self.max_messages = 100
        
        # Device state tracking
        self.device_states = {}
        
        if Config.MQTT_USERNAME and Config.MQTT_PASSWORD:
            self.client.username_pw_set(Config.MQTT_USERNAME, Config.MQTT_PASSWORD)
    
    def on_connect(self, client, userdata, flags, rc):
        """Callback when connected to MQTT broker"""
        if rc == 0:
            logger.info(f"Connected to MQTT broker at {Config.MQTT_BROKER}:{Config.MQTT_PORT}")
            # Subscribe to all esp-sensor-hub topics
            client.subscribe(f"{Config.MQTT_BASE_TOPIC}/#")
            self.socketio.emit('mqtt_status', {
                'connected': True,
                'broker': Config.MQTT_BROKER,
                'timestamp': datetime.now(timezone.utc).isoformat()
            })
        else:
            logger.error(f"Failed to connect to MQTT broker, return code {rc}")
            self.socketio.emit('mqtt_status', {
                'connected': False,
                'error': f'Connection failed with code {rc}',
                'timestamp': datetime.now(timezone.utc).isoformat()
            })
    
    def on_disconnect(self, client, userdata, rc):
        """Callback when disconnected from MQTT broker"""
        logger.warning(f"Disconnected from MQTT broker, return code {rc}")
        self.socketio.emit('mqtt_status', {
            'connected': False,
            'timestamp': datetime.now(timezone.utc).isoformat()
        })
    
    def on_message(self, client, userdata, msg):
        """Callback when MQTT message received"""
        logger.info(f"[MQTT] Received message on topic: {msg.topic}")
        try:
            topic = msg.topic
            payload = msg.payload.decode('utf-8')
            logger.debug(f"[MQTT] Payload: {payload[:100]}...")  # Log first 100 chars
            
            # Try to parse as JSON
            try:
                payload_json = json.loads(payload)
            except json.JSONDecodeError:
                payload_json = payload
            
            # Extract device name from topic
            topic_parts = topic.split('/')
            device_name = topic_parts[1] if len(topic_parts) > 1 else 'unknown'
            message_type = topic_parts[2] if len(topic_parts) > 2 else 'unknown'
            
            # Store message
            message_data = {
                'topic': topic,
                'payload': payload_json,
                'timestamp': datetime.now(timezone.utc).isoformat(),
                'device': device_name,
                'type': message_type
            }
            
            self.messages.append(message_data)
            if len(self.messages) > self.max_messages:
                self.messages.pop(0)
            
            # Update device state
            if device_name not in self.device_states:
                self.device_states[device_name] = {}
            
            # Use device's timestamp from payload if available, otherwise use current time
            device_timestamp = datetime.now(timezone.utc).isoformat()
            if isinstance(payload_json, dict):
                # Check for timestamp in payload (ESP devices send this)
                if 'timestamp' in payload_json:
                    try:
                        # Convert milliseconds/seconds timestamp to ISO format
                        ts_value = payload_json['timestamp']
                        if ts_value > 1000000000000:  # Milliseconds
                            device_timestamp = datetime.fromtimestamp(ts_value / 1000, tz=timezone.utc).isoformat()
                        elif ts_value > 0:  # Seconds since boot - mark as stale
                            # This is uptime, not real time - use current time but mark it
                            device_timestamp = datetime.now(timezone.utc).isoformat()
                    except (ValueError, OSError):
                        pass
            
            self.device_states[device_name][message_type] = {
                'payload': payload_json,
                'timestamp': device_timestamp
            }
            
            # Always update last_seen when any message is received
            # This tracks when the device last communicated in any way
            self.device_states[device_name]['last_seen'] = device_timestamp
            
            # Emit to web clients
            # Use socketio.sleep(0) to ensure eventlet context switch
            logger.info(f"[MQTT] Emitting mqtt_message event for device: {device_name}, topic: {message_type}")
            logger.info(f"[MQTT] Event data: device={device_name}, topic={topic}, timestamp={message_data['timestamp']}")
            
            # Import eventlet for proper async handling
            import eventlet
            eventlet.sleep(0)  # Yield to eventlet
            
            self.socketio.emit('mqtt_message', message_data, namespace='/')
            eventlet.sleep(0)
            
            logger.info(f"[MQTT] Emitting device_update event for device: {device_name}")
            self.socketio.emit('device_update', {
                'device': device_name,
                'state': self.device_states[device_name]
            }, namespace='/')
            eventlet.sleep(0)
            
            logger.info(f"[MQTT] Events emitted successfully")
            
        except Exception as e:
            logger.error(f"Error processing MQTT message: {e}")
            import traceback
            logger.error(traceback.format_exc())
    
    def connect(self):
        """Connect to MQTT broker"""
        try:
            self.client.connect(Config.MQTT_BROKER, Config.MQTT_PORT, Config.MQTT_KEEPALIVE)
            self.client.loop_start()
            logger.info("MQTT client loop started")
        except Exception as e:
            logger.error(f"Failed to connect to MQTT broker: {e}")
            raise
    
    def disconnect(self):
        """Disconnect from MQTT broker"""
        self.client.loop_stop()
        self.client.disconnect()
        logger.info("Disconnected from MQTT broker")
    
    def publish_command(self, device, command):
        """Publish command to device"""
        topic = Config.MQTT_COMMAND_TOPIC.format(device=device)
        result = self.client.publish(topic, command)
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            logger.info(f"Published command '{command}' to {device}")
            return True
        else:
            logger.error(f"Failed to publish command to {device}")
            return False
    
    def get_recent_messages(self):
        """Get recent MQTT messages"""
        return self.messages
    
    def get_device_states(self):
        """Get current device states"""
        return self.device_states
