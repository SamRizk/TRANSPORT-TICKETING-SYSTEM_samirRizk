#!/bin/bash
# scripts/simulate_ticket_validation.sh - Simulates ticket validation at gate

# Configuration
MQTT_BROKER="${MQTT_BROKER:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_TOPIC="ticket/validation/request"
BACKOFFICE_URL="${BACKOFFICE_URL:-http://localhost:8080}"

# Ticket Base64 (from argument or create new)
TICKET_BASE64="$1"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   Ticket Validation Simulation         ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"

# Check dependencies
if ! command -v mosquitto_pub &> /dev/null; then
    echo -e "${RED}✗ mosquitto_pub not found${NC}"
    echo "Install with: sudo apt-get install mosquitto-clients"
    exit 1
fi

# If no ticket provided, create one first
if [ -z "$TICKET_BASE64" ]; then
    echo -e "${YELLOW}No ticket provided. Creating test ticket...${NC}"
    echo ""
    
    if ! command -v curl &> /dev/null; then
        echo -e "${RED}✗ curl not found${NC}"
        echo "Install with: sudo apt-get install curl"
        exit 1
    fi
    
    # Create a ticket via REST API
    RESPONSE=$(curl -s -X POST "${BACKOFFICE_URL}/api/tickets/create" \
        -H "Content-Type: application/json" \
        -d '{"validityDays": 7, "lineNumber": 1}')
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}✗ Failed to create test ticket${NC}"
        echo "Make sure Back-Office is running at: $BACKOFFICE_URL"
        exit 1
    fi
    
    # Extract Base64 ticket
    if command -v jq &> /dev/null; then
        TICKET_BASE64=$(echo "$RESPONSE" | jq -r '.ticketBase64 // empty')
        TICKET_ID=$(echo "$RESPONSE" | jq -r '.ticketId // empty')
    else
        echo -e "${YELLOW}⚠ jq not installed, parsing may be inaccurate${NC}"
        TICKET_BASE64=$(echo "$RESPONSE" | grep -o '"ticketBase64":"[^"]*"' | cut -d'"' -f4)
        TICKET_ID=$(echo "$RESPONSE" | grep -o '"ticketId":"[^"]*"' | cut -d'"' -f4)
    fi
    
    if [ -z "$TICKET_BASE64" ]; then
        echo -e "${RED}✗ Failed to extract ticket from response${NC}"
        echo "Response: $RESPONSE"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Test ticket created${NC}"
    echo "Ticket ID: $TICKET_ID"
    echo ""
fi

echo "Ticket (Base64): ${TICKET_BASE64:0:50}..."
echo -e "MQTT Broker: ${MQTT_BROKER}:${MQTT_PORT}"
echo ""

# Create JSON payload
PAYLOAD=$(cat <<EOF
{
  "ticketBase64": "$TICKET_BASE64"
}
EOF
)

echo -e "${YELLOW}Publishing validation request to: ${MQTT_TOPIC}${NC}"
echo ""

# Publish validation request
mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$MQTT_TOPIC" -m "$PAYLOAD"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Message published successfully!${NC}"
    echo ""
    echo "Waiting for validation response..."
    echo "(Timeout: 10 seconds)"
    echo ""
    
    # Subscribe to response
    timeout 10s mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
        -t "ticket/validation/response" -C 1 | while IFS= read -r line; do
        echo -e "${GREEN}=== Validation Response ===${NC}"
        
        # Pretty-print JSON if possible
        if command -v jq &> /dev/null; then
            echo "$line" | jq '.'
            
            # Extract validation result
            VALID=$(echo "$line" | jq -r '.valid // empty')
            GATE_ACTION=$(echo "$line" | jq -r '.gateAction // empty')
            MESSAGE=$(echo "$line" | jq -r '.message // empty')
            MODE=$(echo "$line" | jq -r '.validationMode // empty')
            
            echo ""
            if [ "$VALID" == "true" ]; then
                echo -e "${GREEN}✓ Ticket is VALID${NC}"
                echo -e "🚪 Gate: ${GREEN}${GATE_ACTION}${NC}"
            else
                echo -e "${RED}✗ Ticket is INVALID${NC}"
                echo -e "🚪 Gate: ${RED}${GATE_ACTION}${NC}"
            fi
            echo "Message: $MESSAGE"
            echo "Validation Mode: $MODE"
        else
            echo "$line"
        fi
    done
    
    if [ $? -eq 124 ]; then
        echo -e "${RED}✗ Timeout waiting for response${NC}"
        echo "Make sure Gate service is running"
    fi
else
    echo -e "${RED}✗ Failed to publish message${NC}"
    echo "Make sure Mosquitto broker is running"
    exit 1
fi
