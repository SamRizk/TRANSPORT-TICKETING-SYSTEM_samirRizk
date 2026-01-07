#!/bin/bash
# scripts/simulate_ticket_sale.sh - Simulates ticket purchase via MQTT

# Configuration
MQTT_BROKER="${MQTT_BROKER:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_TOPIC="ticket/sale/request"

# Parse arguments
VALIDITY_DAYS=${1:-7}
LINE_NUMBER=${2:-1}

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║      Ticket Sale Simulation            ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
echo -e "Validity: ${GREEN}${VALIDITY_DAYS}${NC} days"
echo -e "Line Number: ${GREEN}${LINE_NUMBER}${NC}"
echo -e "MQTT Broker: ${MQTT_BROKER}:${MQTT_PORT}"
echo ""

# Check if mosquitto_pub is available
if ! command -v mosquitto_pub &> /dev/null; then
    echo -e "${RED}✗ mosquitto_pub not found${NC}"
    echo "Install with: sudo apt-get install mosquitto-clients"
    exit 1
fi

# Create JSON payload
PAYLOAD=$(cat <<EOF
{
  "validityDays": ${VALIDITY_DAYS},
  "lineNumber": ${LINE_NUMBER}
}
EOF
)

echo -e "${YELLOW}Publishing to MQTT topic: ${MQTT_TOPIC}${NC}"
echo "Payload: $PAYLOAD"
echo ""

# Publish message
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$MQTT_TOPIC" -m "$PAYLOAD"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Message published successfully!${NC}"
    echo ""
    echo "Waiting for response on: ticket/sale/response"
    echo "(Timeout: 10 seconds)"
    echo ""
    
    # Subscribe to response with timeout
    timeout 10s mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
        -t "ticket/sale/response" -C 1 | while IFS= read -r line; do
        echo -e "${GREEN}=== Response Received ===${NC}"
        
        # Try to pretty-print JSON if jq is available
        if command -v jq &> /dev/null; then
            echo "$line" | jq '.'
        else
            echo "$line"
        fi
        
        # Extract ticket info if possible
        if command -v jq &> /dev/null; then
            TICKET_ID=$(echo "$line" | jq -r '.ticketId // empty')
            TICKET_BASE64=$(echo "$line" | jq -r '.ticketBase64 // empty')
            
            if [ -n "$TICKET_ID" ]; then
                echo ""
                echo -e "${GREEN}✓ Ticket Created!${NC}"
                echo "Ticket ID: $TICKET_ID"
                echo "Base64 (first 50 chars): ${TICKET_BASE64:0:50}..."
                echo ""
                echo "Save this Base64 for validation:"
                echo "$TICKET_BASE64"
            fi
        fi
    done
    
    if [ $? -eq 124 ]; then
        echo -e "${RED}✗ Timeout waiting for response${NC}"
        echo "Make sure TVM and Back-Office services are running"
    fi
else
    echo -e "${RED}✗ Failed to publish message${NC}"
    echo "Make sure Mosquitto broker is running"
    exit 1
fi
