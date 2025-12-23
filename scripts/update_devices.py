#!/usr/bin/env python3
"""
Simple Device Database Updater
Updates PostgreSQL database on raspberrypi2 with device information from DEVICE_INVENTORY.md
"""

import re
import json
import psycopg2
import psycopg2.extras
import logging
import os
from pathlib import Path
from datetime import datetime

# Database config (use environment variables in production)
DB_CONFIG = {
    'host': os.getenv('CAMERA_DB_HOST', '127.0.0.1'),  # e.g., '192.168.0.146' or hostname
    'port': int(os.getenv('CAMERA_DB_PORT', '5432')),
    'database': os.getenv('CAMERA_DB_NAME', 'camera_db'),
    'user': os.getenv('CAMERA_DB_USER', 'camera'),
    'password': os.getenv('CAMERA_DB_PASSWORD')  # Required in production
}

# Build flags mapping by device type and platform
BUILD_FLAGS = {
    'temperature': {
        'ESP8266': {
            'OLED_ENABLED': 1,  # USB powered devices have OLED enabled
            'CPU_FREQ_MHZ': 80,
            'WIFI_PS_MODE': 'WIFI_PS_MIN_MODEM',  # Less aggressive than battery devices
            'API_ENDPOINTS_ONLY': True,
            'MQTT_MAX_PACKET_SIZE': 512,
            'FIRMWARE_VERSION_MAJOR': 1,
            'FIRMWARE_VERSION_MINOR': 0,
            'FIRMWARE_VERSION_PATCH': 3
        },
        'ESP32': {
            'OLED_ENABLED': 0,  # Battery powered devices disable OLED for power saving
            'BATTERY_POWERED': True,  # Battery powered
            'CPU_FREQ_MHZ': 80,
            'WIFI_PS_MODE': 'WIFI_PS_MIN_MODEM',
            'API_ENDPOINTS_ONLY': True,
            'MQTT_MAX_PACKET_SIZE': 2048,
            'FIRMWARE_VERSION_MAJOR': 1,
            'FIRMWARE_VERSION_MINOR': 0,
            'FIRMWARE_VERSION_PATCH': 3
        },
        'ESP32-S3': {
            'CPU_FREQ_MHZ': 80,
            'WIFI_PS_MODE': 'WIFI_PS_MIN_MODEM',
            'MQTT_MAX_PACKET_SIZE': 512,
            'FIRMWARE_VERSION_MAJOR': 1,
            'FIRMWARE_VERSION_MINOR': 0,
            'FIRMWARE_VERSION_PATCH': 3
        }
    },
    'surveillance': {
        'ESP32-CAM': {
            'CAMERA_MODEL_AI_THINKER': True,
            'MOTION_DETECTION_ENABLED': True,
            'MOTION_CHECK_INTERVAL': 3000,
            'MOTION_THRESHOLD': 25,
            'FLASH_PIN': 4,
            'PIR_PIN': 13,
            'WEB_SERVER_PORT': 80,
            'MQTT_MAX_PACKET_SIZE': 1024,
            'FIRMWARE_VERSION_MAJOR': 1,
            'FIRMWARE_VERSION_MINOR': 0,
            'FIRMWARE_VERSION_PATCH': 0
        },
        'ESP32-S3': {
            'CAMERA_MODEL_ESP32S3_EYE': True,
            'MOTION_DETECTION_ENABLED': True,
            'MOTION_CHECK_INTERVAL': 3000,
            'MOTION_THRESHOLD': 25,
            'FLASH_PIN': -1,
            'PIR_PIN': 14,
            'WEB_SERVER_PORT': 80,
            'MQTT_MAX_PACKET_SIZE': 1024,
            'FIRMWARE_VERSION_MAJOR': 1,
            'FIRMWARE_VERSION_MINOR': 0,
            'FIRMWARE_VERSION_PATCH': 0
        }
    },
    'solar': {
        'ESP32': {
            'CORE_DEBUG_LEVEL': 0,
            'ARDUINO_ESP32_DEV': True,
            'VICTRON_ENABLED': True,
            'OLED_ENABLED': 1,
            'MQTT_MAX_PACKET_SIZE': 1024
        }
    }
}

# Setup logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s')

def get_db_connection():
    """Get database connection"""
    return psycopg2.connect(**DB_CONFIG)

def init_database():
    """Create tables if they don't exist"""
    with get_db_connection() as conn:
        with conn.cursor() as cursor:
            # Devices table
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS devices (
                    id SERIAL PRIMARY KEY,
                    device_name VARCHAR(100) UNIQUE NOT NULL,
                    chip_id VARCHAR(20),
                    device_type VARCHAR(50),
                    platform VARCHAR(50),
                    ip_address VARCHAR(20),
                    location VARCHAR(200),
                    status VARCHAR(20) DEFAULT 'unknown',
                    config JSONB DEFAULT '{}',
                    metadata JSONB DEFAULT '{}',
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            ''')

            # Device groups
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS device_groups (
                    id SERIAL PRIMARY KEY,
                    name VARCHAR(100) UNIQUE NOT NULL,
                    description TEXT,
                    color VARCHAR(7)
                )
            ''')

            # Insert default groups
            cursor.execute('''
                INSERT INTO device_groups (name, description, color)
                VALUES
                    ('Temperature Sensors', 'Temperature monitoring', '#3498db'),
                    ('Surveillance Cameras', 'Security cameras', '#e74c3c'),
                    ('Solar Monitors', 'Solar power monitoring', '#f39c12')
                ON CONFLICT (name) DO NOTHING
            ''')

            conn.commit()
            logging.info("Database initialized")

def parse_inventory_file():
    """Parse DEVICE_INVENTORY.md and return device list"""
    devices = []

    try:
        SCRIPT_DIR = Path(__file__).resolve().parent
        REPO_ROOT = SCRIPT_DIR.parent
        inventory_path = REPO_ROOT / 'DEVICE_INVENTORY.md'
        with open(inventory_path, 'r') as f:
            content = f.read()
    except FileNotFoundError:
        logging.error("DEVICE_INVENTORY.md not found")
        return devices

    # Find device table
    table_match = re.search(r'\| Device Name \| Chip ID \| Platform \| IP Address \| Status \|.*?\n\|.*?\n((?:\|.*?\n)+)', content, re.MULTILINE | re.DOTALL)

    if not table_match:
        logging.error("Could not find device table in inventory")
        return devices

    for line in table_match.group(1).strip().split('\n'):
        if '|' in line and not line.startswith('|---'):
            parts = [part.strip() for part in line.split('|')[1:-1]]

            if len(parts) >= 5:
                device_name = parts[0]
                chip_id = parts[1] if parts[1] != 'Unknown' else None
                platform = parts[2] if parts[2] != 'Unknown' else None
                ip_address = parts[3] if parts[3] != 'Unknown' else None
                status = 'online' if '✅' in parts[4] else 'offline'

                # Determine device type
                device_type = 'unknown'
                name_lower = device_name.lower()

                if any(word in name_lower for word in ['camera', 'cam', 'surveillance']):
                    device_type = 'surveillance'
                elif any(word in name_lower for word in ['temp', 'temperature', 'sensor']):
                    device_type = 'temperature'
                elif any(word in name_lower for word in ['solar', 'monitor']):
                    device_type = 'solar'

                # Special cases
                if device_name in ['Pump House', 'Main Cottage', 'Big Garage', 'Spa', 'Sauna']:
                    device_type = 'temperature'
                elif device_name in ['Small Garage']:
                    device_type = 'surveillance'

                # Get build flags for this device type/platform
                build_flags = {}
                if device_type in BUILD_FLAGS:
                    if platform in BUILD_FLAGS[device_type]:
                        build_flags = BUILD_FLAGS[device_type][platform].copy()
                    elif platform == 'ESP32' and device_type == 'surveillance':
                        # Map ESP32 surveillance to ESP32-CAM flags
                        build_flags = BUILD_FLAGS[device_type]['ESP32-CAM'].copy()
                    elif platform is None and device_type == 'temperature':
                        # Default to ESP8266 for temperature sensors without platform
                        build_flags = BUILD_FLAGS[device_type]['ESP8266'].copy()
                    elif BUILD_FLAGS[device_type]:
                        # Use first available platform for this device type
                        first_platform = list(BUILD_FLAGS[device_type].keys())[0]
                        build_flags = BUILD_FLAGS[device_type][first_platform].copy()

                devices.append({
                    'device_name': device_name,
                    'chip_id': chip_id,
                    'device_type': device_type,
                    'platform': platform,
                    'ip_address': ip_address,
                    'status': status,
                    'config': {'build_flags': build_flags},
                    'metadata': {'source': 'DEVICE_INVENTORY.md', 'updated': str(datetime.now())}
                })

    return devices

def update_device(device_data):
    """Add or update a device in database"""
    with get_db_connection() as conn:
        with conn.cursor() as cursor:
            cursor.execute('''
                INSERT INTO devices
                (device_name, chip_id, device_type, platform, ip_address, status, config, metadata, updated_at)
                VALUES (%s, %s, %s, %s, %s, %s, %s, %s, CURRENT_TIMESTAMP)
                ON CONFLICT (device_name) DO UPDATE SET
                    chip_id = EXCLUDED.chip_id,
                    device_type = EXCLUDED.device_type,
                    platform = EXCLUDED.platform,
                    ip_address = EXCLUDED.ip_address,
                    status = EXCLUDED.status,
                    config = EXCLUDED.config,
                    metadata = EXCLUDED.metadata,
                    updated_at = CURRENT_TIMESTAMP
                RETURNING id
            ''', (
                device_data['device_name'],
                device_data['chip_id'],
                device_data['device_type'],
                device_data['platform'],
                device_data['ip_address'],
                device_data['status'],
                json.dumps(device_data.get('config', {})),
                json.dumps(device_data['metadata'])
            ))

            device_id = cursor.fetchone()[0]
            conn.commit()
            return device_id

def main():
    """Main function"""
    print("ESP Sensor Hub - Device Database Updater")
    print("=" * 50)

    try:
        # Initialize database
        init_database()

        # Parse inventory
        devices = parse_inventory_file()
        print(f"Found {len(devices)} devices in inventory")

        if not devices:
            print("No devices found!")
            return

        # Update database
        updated_count = 0
        for device in devices:
            try:
                device_id = update_device(device)
                print(f"✅ {device['device_name']} (ID: {device_id})")
                updated_count += 1
            except Exception as e:
                print(f"❌ {device['device_name']}: {e}")

        print(f"\n🎉 Successfully updated {updated_count} devices in database")

        # Show summary
        with get_db_connection() as conn:
            with conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor) as cursor:
                cursor.execute("SELECT device_type, COUNT(*) as count FROM devices GROUP BY device_type")
                summary = cursor.fetchall()

                print("\nDevice Summary:")
                for row in summary:
                    print(f"  {row['device_type']}: {row['count']}")

    except Exception as e:
        print(f"❌ Error: {e}")
        return 1

    return 0

if __name__ == "__main__":
    exit(main())