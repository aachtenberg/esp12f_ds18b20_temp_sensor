#!/bin/bash
# Script to validate secrets.h configuration before flashing

set -e

SECRETS_FILE="include/secrets.h"
EXAMPLE_FILE="include/secrets.h.example"

echo "🔍 Validating secrets configuration..."
echo ""

# Check if secrets.h exists
if [ ! -f "$SECRETS_FILE" ]; then
    echo "❌ Error: $SECRETS_FILE not found"
    echo ""
    echo "Please create it from the template:"
    echo "  cp $EXAMPLE_FILE $SECRETS_FILE"
    echo "  vim $SECRETS_FILE"
    echo ""
    exit 1
fi

echo "✅ $SECRETS_FILE exists"

# Check if secrets.h is in .gitignore
if git check-ignore -q "$SECRETS_FILE" 2>/dev/null; then
    echo "✅ $SECRETS_FILE is properly gitignored"
else
    echo "⚠️  Warning: $SECRETS_FILE is NOT gitignored (may be committed!)"
fi

# Check for placeholder values
PLACEHOLDERS=$(grep -o 'YOUR_[A-Z_]*\|YOUR-[A-Z-]*' "$SECRETS_FILE" 2>/dev/null || true)
if [ -n "$PLACEHOLDERS" ]; then
    echo "❌ Found placeholder values that need to be replaced:"
    echo "$PLACEHOLDERS" | sort -u | sed 's/^/   - /'
    echo ""
    echo "Please edit $SECRETS_FILE and replace all YOUR_* placeholders"
    exit 1
fi

echo "✅ No placeholder values found (YOUR_*, YOUR-*)"

# Count WiFi networks configured
WIFI_COUNT=$(grep -c '{".*", ".*"}' "$SECRETS_FILE" || echo "0")
echo "✅ WiFi networks configured: $WIFI_COUNT"

if [ "$WIFI_COUNT" -eq 0 ]; then
    echo "⚠️  Warning: No WiFi networks found. Check wifi_networks array."
fi

# Check InfluxDB URL
if grep -q 'INFLUXDB_URL.*"http' "$SECRETS_FILE"; then
    INFLUX_URL=$(grep 'INFLUXDB_URL' "$SECRETS_FILE" | grep -o '"[^"]*"' | head -1)
    echo "✅ InfluxDB URL configured: $INFLUX_URL"
else
    echo "❌ InfluxDB URL not configured or invalid format"
    exit 1
fi

# Check InfluxDB token length (should be ~88 chars for valid tokens)
TOKEN_LENGTH=$(grep 'INFLUXDB_TOKEN' "$SECRETS_FILE" | grep -o '"[^"]*"' | head -1 | tr -d '"' | wc -c)
if [ "$TOKEN_LENGTH" -gt 20 ]; then
    echo "✅ InfluxDB token length: $TOKEN_LENGTH characters (looks valid)"
else
    echo "⚠️  InfluxDB token seems too short ($TOKEN_LENGTH chars). Verify it's correct."
fi

# Check for common mistakes
if grep -q 'example\|password123\|changeme' "$SECRETS_FILE"; then
    echo "⚠️  Warning: Found common example values. Make sure these are your actual credentials."
fi

# Check InfluxDB org ID format
if grep -q 'INFLUXDB_ORG.*"[0-9a-f]\{16\}"' "$SECRETS_FILE"; then
    echo "✅ InfluxDB Organization ID format looks valid"
else
    echo "⚠️  InfluxDB Organization ID may be incorrect (should be 16-char hex)"
fi

echo ""
echo "✅ Configuration looks good!"
echo ""
echo "Next steps:"
echo "  1. Build: platformio run -e esp8266 (or -e esp32dev)"
echo "  2. Flash: platformio run -e esp8266 --target upload"
echo "  3. Monitor: platformio device monitor -b 115200"
