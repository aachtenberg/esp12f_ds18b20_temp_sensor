#!/bin/bash
# Quick Docker start script for MQTT Admin Panel

set -e

echo "🐳 Starting ESP Sensor Hub Admin Panel in Docker..."

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "❌ Error: Docker is not installed"
    echo "   Install from: https://docs.docker.com/get-docker/"
    exit 1
fi

# Check if Docker Compose is available
if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    echo "❌ Error: Docker Compose is not installed"
    echo "   Install from: https://docs.docker.com/compose/install/"
    exit 1
fi

# Determine docker-compose command (v1 vs v2)
if command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE="docker-compose"
else
    DOCKER_COMPOSE="docker compose"
fi

# Check for .env file
if [ ! -f ".env" ]; then
    echo "⚠️  .env file not found, copying from .env.example"
    cp .env.example .env
    echo ""
    echo "📝 IMPORTANT: Please edit .env file with your MQTT broker settings"
    echo "   Especially update MQTT_BROKER to your broker's IP address"
    echo ""
    read -p "Press Enter to continue after editing .env file..."
fi

# Load environment variables
export $(grep -v '^#' .env | xargs)

# Check MQTT broker connectivity
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
echo "🔨 Building Docker image..."
$DOCKER_COMPOSE build

echo ""
echo "🚀 Starting container..."
$DOCKER_COMPOSE up -d

echo ""
echo "✨ Admin panel is starting..."
echo "📡 Access the dashboard at: http://localhost:5000"
echo ""
echo "📊 Useful commands:"
echo "   View logs:    $DOCKER_COMPOSE logs -f"
echo "   Stop panel:   $DOCKER_COMPOSE down"
echo "   Restart:      $DOCKER_COMPOSE restart"
echo "   Shell access: docker exec -it esp-sensor-hub-admin sh"
echo ""

# Show logs
echo "📋 Showing logs (Ctrl+C to exit, container will keep running)..."
$DOCKER_COMPOSE logs -f
