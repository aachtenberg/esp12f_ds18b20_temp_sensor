#!/bin/bash

# ESP32 Device Control via MQTT
# Supports all available MQTT commands with automatic retry and confirmation

# Default configuration
MQTT_BROKER="${MQTT_BROKER:-192.168.0.167}"
DEVICE_NAME="${DEVICE_NAME:-Spa}"
RETRY_INTERVAL=2
MAX_ATTEMPTS=15

# MQTT Topics
COMMAND_TOPIC="esp-sensor-hub/${DEVICE_NAME}/command"
STATUS_TOPIC="esp-sensor-hub/${DEVICE_NAME}/status"
EVENTS_TOPIC="esp-sensor-hub/${DEVICE_NAME}/events"
TEMP_TOPIC="esp-sensor-hub/${DEVICE_NAME}/temperature"

# Color output (disabled for simplicity)
RED=''
GREEN=''
YELLOW=''
BLUE=''
NC=''

# Usage information
usage() {
    cat << EOF
${BLUE}ESP32 Device Control via MQTT${NC}

Usage: $0 <command> [arguments] [options]

${GREEN}Commands:${NC}
  deepsleep <seconds>    Configure deep sleep interval (0-3600 seconds)
                        0 = disable deep sleep
                        Example: $0 deepsleep 30

  disable-sleep         Shortcut to disable deep sleep (infinite retries until success)
  
  enable-sleep <secs>   Shortcut to enable deep sleep
                        Example: $0 enable-sleep 30

  status               Request device status update
  
  restart              Restart the device
  
  monitor              Monitor all device topics (temperature, status, events)

${GREEN}Options:${NC}
  -b, --broker <ip>    MQTT broker IP (default: $MQTT_BROKER)
  -d, --device <name>  Device name (default: $DEVICE_NAME)
  -r, --retry <count>  Maximum retry attempts (default: $MAX_ATTEMPTS)
                        Use 0 for infinite retries
  -i, --interval <sec> Retry interval in seconds (default: $RETRY_INTERVAL)
  -h, --help          Show this help message

${GREEN}Environment Variables:${NC}
  MQTT_BROKER         Default MQTT broker IP
  DEVICE_NAME         Default device name

${GREEN}Examples:${NC}
  # Enable deep sleep with 30 second interval
  $0 deepsleep 30

  # Disable deep sleep (continuous operation with infinite retries)
  $0 disable-sleep

  # Restart device with infinite retries
  $0 -r 0 restart

  # Enable deep sleep with infinite retries
  $0 -r 0 enable-sleep 60

  # Restart device
  $0 restart

  # Request status from different device
  $0 -d Greenhouse status

  # Monitor all topics
  $0 monitor

EOF
    exit 0
}

# Parse command line options
COMMAND=""
ARGS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            ;;
        -b|--broker)
            MQTT_BROKER="$2"
            shift 2
            ;;
        -d|--device)
            DEVICE_NAME="$2"
            # Update topics with new device name
            COMMAND_TOPIC="esp-sensor-hub/${DEVICE_NAME}/command"
            STATUS_TOPIC="esp-sensor-hub/${DEVICE_NAME}/status"
            EVENTS_TOPIC="esp-sensor-hub/${DEVICE_NAME}/events"
            TEMP_TOPIC="esp-sensor-hub/${DEVICE_NAME}/temperature"
            shift 2
            ;;
        -r|--retry)
            # Validate as integer to prevent command injection
            if ! [[ "$2" =~ ^[0-9]+$ ]]; then
                echo "ERROR: --retry must be a non-negative integer (0 for infinite)"
                exit 1
            fi
            MAX_ATTEMPTS="$2"
            shift 2
            ;;
        -i|--interval)
            # Validate as integer to prevent command injection
            if ! [[ "$2" =~ ^[0-9]+$ ]]; then
                echo "ERROR: --interval must be a positive integer"
                exit 1
            fi
            RETRY_INTERVAL="$2"
            shift 2
            ;;
        deepsleep|disable-sleep|enable-sleep|status|restart|monitor|config)
            COMMAND="$1"
            shift
            # Collect remaining args (non-option arguments)
            while [[ $# -gt 0 ]] && [[ ! "$1" =~ ^- ]]; do
                ARGS+=("$1")
                shift
            done
            # Continue processing remaining options instead of breaking
            ;;
        *)
            echo -e "${RED}Error: Unknown option: $1${NC}"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Check if command is provided
if [ -z "$COMMAND" ]; then
    echo -e "${RED}Error: No command specified${NC}"
    usage
fi

# Function to print header
print_header() {
    echo ""
    echo -e "${BLUE}==========================================${NC}"
    echo -e "${BLUE}  ESP32 Device Control via MQTT${NC}"
    echo -e "${BLUE}==========================================${NC}"
    echo -e "Device: ${GREEN}$DEVICE_NAME${NC}"
    echo -e "Broker: ${GREEN}$MQTT_BROKER${NC}"
    echo -e "Command: ${GREEN}$1${NC}"
    echo -e "${BLUE}==========================================${NC}"
    echo ""
}

# Function to monitor all topics
monitor_topics() {
    print_header "Monitor All Topics"
    echo -e "${YELLOW}Subscribing to all device topics...${NC}"
    echo -e "${YELLOW}Press Ctrl+C to stop${NC}"
    echo ""

    # The mosquitto_sub process will be cleaned up by pkill -P $$ in the trap
    mosquitto_sub -h "$MQTT_BROKER" -t "esp-sensor-hub/${DEVICE_NAME}/#" -v | while IFS= read -r line; do
        timestamp=$(date '+%H:%M:%S')
        topic=$(echo "$line" | cut -d' ' -f1)
        message=$(echo "$line" | cut -d' ' -f2-)
        
        # Color code by topic type
        if [[ "$topic" == *"/temperature" ]]; then
            echo -e "[$timestamp] ${GREEN}$topic${NC}"
            echo "$message" | jq '.' 2>/dev/null || echo "$message"
        elif [[ "$topic" == *"/status" ]]; then
            echo -e "[$timestamp] ${BLUE}$topic${NC}"
            echo "$message" | jq '.' 2>/dev/null || echo "$message"
        elif [[ "$topic" == *"/events" ]]; then
            echo -e "[$timestamp] ${YELLOW}$topic${NC}"
            echo "$message" | jq '.' 2>/dev/null || echo "$message"
        else
            echo -e "[$timestamp] $topic"
            echo "$message"
        fi
        echo ""
    done
}

# Function to send command with retry and confirmation
send_command() {
    local mqtt_message="$1"
    local success_pattern="$2"
    local success_message="$3"
    local monitor_topic="${4:-$EVENTS_TOPIC}"
    local infinite_retry="${5:-false}"

    print_header "$mqtt_message"

    if [ "$infinite_retry" = "true" ]; then
        echo "Sending command every ${RETRY_INTERVAL}s until success..."
        echo "Press Ctrl+C to stop"
    else
        echo "Sending command every ${RETRY_INTERVAL}s to catch wake window..."
        echo "Will retry up to $MAX_ATTEMPTS times - Press Ctrl+C to stop"
    fi
    echo ""

    # Simple loop: send command, check for response, repeat
    attempt=0
    while true; do
        ((attempt++))
        timestamp=$(date '+%H:%M:%S')

        if [ "$infinite_retry" = "true" ]; then
            echo "[$timestamp] [Attempt $attempt] Sending: $mqtt_message"
        else
            if [ "$attempt" -gt "$MAX_ATTEMPTS" ]; then
                echo ""
                echo "✗ No response after $MAX_ATTEMPTS attempts"
                echo "Device may be in deep sleep. Try again or increase retry attempts."
                exit 1
            fi
            echo "[$timestamp] [Attempt $attempt/$MAX_ATTEMPTS] Sending: $mqtt_message"
        fi

        # Send command
        mosquitto_pub -h "$MQTT_BROKER" -t "$COMMAND_TOPIC" -m "$mqtt_message" 2>/dev/null

        # Check for response (wait briefly)
        if response=$(timeout 1 mosquitto_sub -h "$MQTT_BROKER" -t "$monitor_topic" -C 1 -v 2>/dev/null); then
            echo "[$timestamp] RECEIVED: $response"

            if echo "$response" | grep -q "$success_pattern"; then
                echo ""
                echo "✓ SUCCESS! $success_message"
                echo ""
                exit 0
            fi
        fi

        sleep "$RETRY_INTERVAL"
    done
}

# Execute command
case "$COMMAND" in
    deepsleep)
        if [ ${#ARGS[@]} -eq 0 ]; then
            echo -e "${RED}Error: deepsleep requires seconds argument (0-3600)${NC}"
            echo "Example: $0 deepsleep 30"
            exit 1
        fi
        
        SECONDS="${ARGS[0]}"
        
        # Validate seconds
        if ! [[ "$SECONDS" =~ ^[0-9]+$ ]]; then
            echo -e "${RED}Error: seconds must be a number${NC}"
            exit 1
        fi
        
        if [ "$SECONDS" -gt 3600 ]; then
            echo -e "${RED}Error: seconds must be between 0-3600${NC}"
            exit 1
        fi
        
        # Determine if infinite retry mode
        INFINITE_MODE="false"
        if [ "$MAX_ATTEMPTS" -eq 0 ]; then
            INFINITE_MODE="true"
        fi

        if [ "$SECONDS" -eq 0 ]; then
            send_command "deepsleep 0" \
                         "deep_sleep_config.*disabled" \
                         "Deep sleep disabled" \
                         "$EVENTS_TOPIC" \
                         "$INFINITE_MODE"
        else
            send_command "deepsleep $SECONDS" \
                         "deep_sleep_config.*$SECONDS seconds" \
                         "Deep sleep enabled ($SECONDS seconds)" \
                         "$EVENTS_TOPIC" \
                         "$INFINITE_MODE"
        fi
        ;;
        
    disable-sleep)
        send_command "deepsleep 0" \
                     "deep_sleep_config.*disabled" \
                     "Deep sleep disabled" \
                     "$EVENTS_TOPIC" \
                     "true"
        ;;
        
    enable-sleep)
        if [ ${#ARGS[@]} -eq 0 ]; then
            echo -e "${RED}Error: enable-sleep requires seconds argument (1-3600)${NC}"
            echo "Example: $0 enable-sleep 30"
            exit 1
        fi
        
        SECONDS="${ARGS[0]}"
        
        if ! [[ "$SECONDS" =~ ^[0-9]+$ ]] || [ "$SECONDS" -lt 1 ] || [ "$SECONDS" -gt 3600 ]; then
            echo -e "${RED}Error: seconds must be between 1-3600${NC}"
            exit 1
        fi

        # Determine if infinite retry mode
        INFINITE_MODE="false"
        if [ "$MAX_ATTEMPTS" -eq 0 ]; then
            INFINITE_MODE="true"
        fi

        send_command "deepsleep $SECONDS" \
                     "deep_sleep_config.*$SECONDS seconds" \
                     "Deep sleep enabled ($SECONDS seconds)" \
                     "$EVENTS_TOPIC" \
                     "$INFINITE_MODE"
        ;;
        
    status)
        print_header "Request Status"
        echo "Requesting status update..."
        echo ""

        # Send status request
        mosquitto_pub -h "$MQTT_BROKER" -t "$COMMAND_TOPIC" -m "status"

        # Listen for response
        mosquitto_sub -h "$MQTT_BROKER" -t "$STATUS_TOPIC" -C 1 -v

        echo ""
        echo "✓ Status received"
        ;;
        
    restart)
        # Determine if infinite retry mode
        INFINITE_MODE="false"
        if [ "$MAX_ATTEMPTS" -eq 0 ]; then
            INFINITE_MODE="true"
        fi

        send_command "restart" \
                     "restart" \
                     "Device restarting" \
                     "$EVENTS_TOPIC" \
                     "$INFINITE_MODE"
        ;;

    config)
        # Determine if infinite retry mode
        INFINITE_MODE="false"
        if [ "$MAX_ATTEMPTS" -eq 0 ]; then
            INFINITE_MODE="true"
        fi

        send_command "config" \
                     "config" \
                     "Device resetting WiFI" \
                     "$EVENTS_TOPIC" \
                     "$INFINITE_MODE"
        ;;
        
    monitor)
        monitor_topics
        ;;
        
    *)
        echo -e "${RED}Error: Unknown command: $COMMAND${NC}"
        usage
        ;;
esac
