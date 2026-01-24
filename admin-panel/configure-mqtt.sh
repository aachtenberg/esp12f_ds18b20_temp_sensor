#!/bin/bash
# Configure MQTT broker for admin panel

echo "🔧 MQTT Broker Configuration"
echo ""

# Get current setting
CURRENT_BROKER=$(grep MQTT_BROKER .env 2>/dev/null | cut -d'=' -f2 || echo "not set")
echo "Current MQTT_BROKER: $CURRENT_BROKER"
echo ""

# Prompt for broker IP
read -p "Enter MQTT broker IP address (or press Enter to keep current): " NEW_BROKER

if [ -n "$NEW_BROKER" ]; then
    # Update .env file
    if [ -f ".env" ]; then
        sed -i "s/^MQTT_BROKER=.*/MQTT_BROKER=$NEW_BROKER/" .env
    else
        cp .env.example .env
        sed -i "s/^MQTT_BROKER=.*/MQTT_BROKER=$NEW_BROKER/" .env
    fi
    
    echo "✅ Updated MQTT_BROKER to: $NEW_BROKER"
    echo ""
    
    # Test connectivity
    echo "🔍 Testing connection to $NEW_BROKER..."
    if ping -c 1 -W 2 "$NEW_BROKER" &> /dev/null; then
        echo "✅ Broker is reachable"
    else
        echo "⚠️  Warning: Cannot reach broker at $NEW_BROKER"
    fi
    
    echo ""
    echo "📡 Restarting container with new settings..."
    docker compose restart
    
    echo ""
    echo "✨ Done! View logs: docker compose logs -f"
else
    echo "ℹ️  No changes made"
fi
