#!/bin/bash
# Quick start script for MQTT Admin Panel

set -e

echo "🚀 Starting ESP Sensor Hub Admin Panel..."

# Check if we're in the right directory
if [ ! -f "app.py" ]; then
    echo "❌ Error: Please run this script from the admin-panel directory"
    exit 1
fi

# Check for Python
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: Python 3 is not installed"
    exit 1
fi

# Check for .env file
if [ ! -f ".env" ]; then
    echo "⚠️  .env file not found, copying from .env.example"
    cp .env.example .env
    echo "📝 Please edit .env file with your MQTT broker settings"
    echo "   Especially update MQTT_BROKER to your broker's IP address"
fi

# Create virtual environment if it doesn't exist
if [ ! -d "venv" ]; then
    echo "📦 Creating virtual environment..."
    python3 -m venv venv
fi

# Activate virtual environment
echo "🔧 Activating virtual environment..."
source venv/bin/activate

# Install dependencies
echo "📥 Installing dependencies..."
pip install -q --upgrade pip
pip install -q -r requirements.txt

# Check MQTT broker connectivity
MQTT_BROKER=$(grep MQTT_BROKER .env | cut -d '=' -f2)
if [ -n "$MQTT_BROKER" ] && [ "$MQTT_BROKER" != "localhost" ]; then
    echo "🔍 Testing connection to MQTT broker at $MQTT_BROKER..."
    if ping -c 1 -W 2 "$MQTT_BROKER" &> /dev/null; then
        echo "✅ MQTT broker is reachable"
    else
        echo "⚠️  Warning: Cannot reach MQTT broker at $MQTT_BROKER"
        echo "   The panel will start but may not receive messages"
    fi
fi

echo ""
echo "✨ Starting admin panel..."
echo "📡 Access the dashboard at: http://localhost:5000"
echo "Press Ctrl+C to stop"
echo ""

# Start the application
python app.py
