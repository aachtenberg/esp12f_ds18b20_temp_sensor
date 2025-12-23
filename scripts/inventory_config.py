# PostgreSQL Database Configuration example
# Copy this to inventory_config.py (or set environment variables) and update with your credentials

DB_CONFIG = {
    'host': 'YOUR_DB_HOST',       # e.g., '127.0.0.1' or hostname
    'port': 5432,
    'database': 'YOUR_DB_NAME',   # e.g., 'camera_db'
    'user': 'YOUR_DB_USER',       # e.g., 'camera'
    'password': 'YOUR_DB_PASSWORD'  # Set to your actual database password
}

# MQTT Configuration example (for inventory_sync.py)
MQTT_CONFIG = {
    'broker': 'YOUR_MQTT_BROKER_IP',  # e.g., 'localhost' or MQTT broker hostname/IP
    'port': 1883,
    'topics': [
        'esp-sensor-hub/#',
        'surveillance/#',
        'solar-monitor/#'
    ]
}