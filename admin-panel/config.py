"""Configuration for MQTT Admin Panel"""
import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    # Flask Configuration
    SECRET_KEY = os.getenv('SECRET_KEY', 'dev-secret-key-change-in-production')
    DEBUG = os.getenv('FLASK_DEBUG', 'True').lower() == 'true'
    HOST = os.getenv('FLASK_HOST', '0.0.0.0')
    PORT = int(os.getenv('FLASK_PORT', '5000'))
    
    # MQTT Configuration
    MQTT_BROKER = os.getenv('MQTT_BROKER', 'localhost')
    MQTT_PORT = int(os.getenv('MQTT_PORT', '1883'))
    MQTT_USERNAME = os.getenv('MQTT_USERNAME', '')
    MQTT_PASSWORD = os.getenv('MQTT_PASSWORD', '')
    MQTT_KEEPALIVE = 60
    
    # MQTT Topics
    MQTT_BASE_TOPIC = 'esp-sensor-hub'
    MQTT_COMMAND_TOPIC = f'{MQTT_BASE_TOPIC}/{{device}}/command'
    MQTT_STATUS_TOPIC = f'{MQTT_BASE_TOPIC}/{{device}}/status'
    MQTT_TEMPERATURE_TOPIC = f'{MQTT_BASE_TOPIC}/{{device}}/temperature'
    MQTT_EVENTS_TOPIC = f'{MQTT_BASE_TOPIC}/{{device}}/events'
    
    # Device Inventory
    DEVICE_INVENTORY_PATH = os.getenv('DEVICE_INVENTORY_PATH', 
                                      '../temperature-sensor/docs/DEVICE_INVENTORY.md')
