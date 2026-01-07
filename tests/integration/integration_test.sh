#!/bin/bash
# tests/integration/integration_test.sh
# Integration tests for the complete ticketing system

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
BACKOFFICE_URL="${BACKOFFICE_URL:-http://localhost:8080}"
MQTT_BROKER="${MQTT_BROKER:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
print_test() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════${NC}"
    echo -e "${BLUE}TEST $1: $2${NC}"
    echo -e "${BLUE}═══════════════════════════════════════${NC}"
}

pass_test() {
    echo -e "${GREEN}✓ PASS: $1${NC}"
    ((TESTS_PASSED++))
    ((TESTS_RUN++))
}

fail_test() {
    echo -e "${RED}✗ FAIL: $1${NC}"
    echo -e "${RED}  Reason: $2${NC}"
    ((TESTS_FAILED++))
    ((TESTS_RUN++))
}

wait_for_service() {
    local url=$1
    local max_attempts=30
    local attempt=1
    
    echo "Waiting for service at $url..."
    
    while [ $attempt -le $max_attempts ]; do
        if curl -s -f "$url" > /dev/null 2>&1; then
            echo -e "${GREEN}✓ Service is ready${NC}"
            return 0
        fi
        echo -n "."
        sleep 1
        ((attempt++))
    done
    
    echo -e "${RED}✗ Service failed to start${NC}"
    return 1
}

# Main test suite
echo -e "${BLUE}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   Integration Tests - Ticketing System       ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================================
# TEST 1: Health Check
# ============================================================================
print_test "1" "Back-Office Health Check"

if wait_for_service "$BACKOFFICE_URL/health"; then
    RESPONSE=$(curl -s "$BACKOFFICE_URL/health")
    if [ "$RESPONSE" == "OK" ]; then
        pass_test "Back-Office is healthy"
    else
        fail_test "Health check" "Expected 'OK', got '$RESPONSE'"
    fi
else
    fail_test "Health check" "Service not responding"
fi

# ============================================================================
# TEST 2: Ticket Creation via REST API
# ============================================================================
print_test "2" "Direct Ticket Creation (REST API)"

RESPONSE=$(curl -s -X POST "$BACKOFFICE_URL/api/tickets/create" \
    -H "Content-Type: application/json" \
    -d '{"validityDays": 7, "lineNumber": 1}')

if echo "$RESPONSE" | grep -q "success.*true"; then
    TICKET_ID=$(echo "$RESPONSE" | jq -r '.ticketId // empty')
    TICKET_BASE64=$(echo "$RESPONSE" | jq -r '.ticketBase64 // empty')
    
    if [ -n "$TICKET_ID" ] && [ -n "$TICKET_BASE64" ]; then
        pass_test "Ticket created: $TICKET_ID"
        echo "  Base64: ${TICKET_BASE64:0:40}..."
        
        # Save for later tests
        echo "$TICKET_BASE64" > /tmp/test_ticket_base64.txt
    else
        fail_test "Ticket creation" "Missing ticketId or ticketBase64 in response"
    fi
else
    fail_test "Ticket creation" "Response: $RESPONSE"
fi

# ============================================================================
# TEST 3: Ticket Validation via REST API
# ============================================================================
print_test "3" "Direct Ticket Validation (REST API)"

if [ -f /tmp/test_ticket_base64.txt ]; then
    TICKET_BASE64=$(cat /tmp/test_ticket_base64.txt)
    
    RESPONSE=$(curl -s -X POST "$BACKOFFICE_URL/api/tickets/validate" \
        -H "Content-Type: application/json" \
        -d "{\"ticketBase64\": \"$TICKET_BASE64\"}")
    
    if echo "$RESPONSE" | grep -q "valid.*true"; then
        pass_test "Ticket validation successful"
        MESSAGE=$(echo "$RESPONSE" | jq -r '.message // empty')
        echo "  Message: $MESSAGE"
    else
        fail_test "Ticket validation" "Expected valid=true, Response: $RESPONSE"
    fi
else
    fail_test "Ticket validation" "No ticket available from previous test"
fi

# ============================================================================
# TEST 4: List All Tickets
# ============================================================================
print_test "4" "List All Tickets"

RESPONSE=$(curl -s "$BACKOFFICE_URL/api/tickets")

if echo "$RESPONSE" | jq . > /dev/null 2>&1; then
    TICKET_COUNT=$(echo "$RESPONSE" | jq '. | length')
    if [ "$TICKET_COUNT" -gt 0 ]; then
        pass_test "Retrieved $TICKET_COUNT ticket(s)"
    else
        fail_test "List tickets" "No tickets found"
    fi
else
    fail_test "List tickets" "Invalid JSON response"
fi

# ============================================================================
# TEST 5: Expired Ticket Handling
# ============================================================================
print_test "5" "Expired Ticket Validation"

# Create ticket with 0 days validity
RESPONSE=$(curl -s -X POST "$BACKOFFICE_URL/api/tickets/create" \
    -H "Content-Type: application/json" \
    -d '{"validityDays": 0, "lineNumber": 1}')

if echo "$RESPONSE" | grep -q "success.*true"; then
    EXPIRED_TICKET=$(echo "$RESPONSE" | jq -r '.ticketBase64 // empty')
    
    # Wait a moment
    sleep 1
    
    # Try to validate
    RESPONSE=$(curl -s -X POST "$BACKOFFICE_URL/api/tickets/validate" \
        -H "Content-Type: application/json" \
        -d "{\"ticketBase64\": \"$EXPIRED_TICKET\"}")
    
    if echo "$RESPONSE" | grep -q "valid.*false"; then
        pass_test "Expired ticket correctly rejected"
    else
        fail_test "Expired ticket handling" "Expected valid=false"
    fi
else
    fail_test "Expired ticket test" "Failed to create expired ticket"
fi

# ============================================================================
# TEST 6: Invalid Ticket Handling
# ============================================================================
print_test "6" "Invalid Ticket Data Handling"

INVALID_BASE64="aW52YWxpZCB0aWNrZXQgZGF0YQ=="

RESPONSE=$(curl -s -X POST "$BACKOFFICE_URL/api/tickets/validate" \
    -H "Content-Type: application/json" \
    -d "{\"ticketBase64\": \"$INVALID_BASE64\"}" 2>&1)

# Should fail or return error
if echo "$RESPONSE" | grep -qE "(success.*false|error|500)"; then
    pass_test "Invalid ticket correctly rejected"
else
    # If it somehow validates as false, that's also acceptable
    if echo "$RESPONSE" | grep -q "valid.*false"; then
        pass_test "Invalid ticket correctly identified"
    else
        fail_test "Invalid ticket handling" "Expected error or valid=false"
    fi
fi

# ============================================================================
# TEST 7: MQTT Ticket Sale Flow (if MQTT is available)
# ============================================================================
print_test "7" "Complete Ticket Sale Flow via MQTT"

if command -v mosquitto_pub &> /dev/null && command -v mosquitto_sub &> /dev/null; then
    # Test MQTT connectivity
    if mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "test" -m "ping" 2>/dev/null; then
        
        # Publish sale request
        mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
            -t "ticket/sale/request" \
            -m '{"validityDays": 14, "lineNumber": 2}' &
        
        # Subscribe to response (timeout 10 seconds)
        MQTT_RESPONSE=$(timeout 10s mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
            -t "ticket/sale/response" -C 1 2>/dev/null || echo "")
        
        if [ -n "$MQTT_RESPONSE" ] && echo "$MQTT_RESPONSE" | grep -q "ticketId"; then
            pass_test "MQTT sale flow completed"
            echo "  Response: ${MQTT_RESPONSE:0:100}..."
        else
            fail_test "MQTT sale flow" "No response or TVM not running"
        fi
    else
        fail_test "MQTT sale flow" "Cannot connect to MQTT broker"
    fi
else
    echo -e "${YELLOW}⊘ SKIP: MQTT clients not installed${NC}"
fi

# ============================================================================
# TEST 8: MQTT Validation Flow (if MQTT is available)
# ============================================================================
print_test "8" "Complete Validation Flow via MQTT"

if command -v mosquitto_pub &> /dev/null && [ -f /tmp/test_ticket_base64.txt ]; then
    TICKET_BASE64=$(cat /tmp/test_ticket_base64.txt)
    
    # Publish validation request
    mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
        -t "ticket/validation/request" \
        -m "{\"ticketBase64\": \"$TICKET_BASE64\"}" &
    
    # Subscribe to response
    MQTT_RESPONSE=$(timeout 10s mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" \
        -t "ticket/validation/response" -C 1 2>/dev/null || echo "")
    
    if [ -n "$MQTT_RESPONSE" ] && echo "$MQTT_RESPONSE" | grep -q "gateAction"; then
        if echo "$MQTT_RESPONSE" | grep -q "valid.*true"; then
            pass_test "MQTT validation flow - ticket validated"
        else
            pass_test "MQTT validation flow - ticket rejected"
        fi
        echo "  Response: ${MQTT_RESPONSE:0:100}..."
    else
        fail_test "MQTT validation flow" "No response or Gate not running"
    fi
else
    echo -e "${YELLOW}⊘ SKIP: MQTT clients not installed or no ticket available${NC}"
fi

# ============================================================================
# TEST 9: Multiple Tickets with Different Line Numbers
# ============================================================================
print_test "9" "Multiple Tickets with Different Line Numbers"

LINES=(1 2 5 10 99)
SUCCESS_COUNT=0

for LINE in "${LINES[@]}"; do
    RESPONSE=$(curl -s -X POST "$BACKOFFICE_URL/api/tickets/create" \
        -H "Content-Type: application/json" \
        -d "{\"validityDays\": 7, \"lineNumber\": $LINE}")
    
    if echo "$RESPONSE" | grep -q "success.*true"; then
        ((SUCCESS_COUNT++))
    fi
done

if [ $SUCCESS_COUNT -eq ${#LINES[@]} ]; then
    pass_test "Created tickets for ${#LINES[@]} different lines"
else
    fail_test "Multiple line numbers" "Only $SUCCESS_COUNT/${#LINES[@]} succeeded"
fi

# ============================================================================
# TEST 10: Concurrent Ticket Creation
# ============================================================================
print_test "10" "Concurrent Ticket Creation"

# Create 10 tickets concurrently
for i in {1..10}; do
    curl -s -X POST "$BACKOFFICE_URL/api/tickets/create" \
        -H "Content-Type: application/json" \
        -d '{"validityDays": 7, "lineNumber": 1}' > /tmp/concurrent_$i.json &
done

# Wait for all to complete
wait

# Check results
SUCCESS_COUNT=0
for i in {1..10}; do
    if [ -f /tmp/concurrent_$i.json ] && grep -q "success.*true" /tmp/concurrent_$i.json; then
        ((SUCCESS_COUNT++))
    fi
    rm -f /tmp/concurrent_$i.json
done

if [ $SUCCESS_COUNT -eq 10 ]; then
    pass_test "All 10 concurrent tickets created successfully"
else
    fail_test "Concurrent creation" "Only $SUCCESS_COUNT/10 succeeded"
fi

# ============================================================================
# TEST 11: Error Handling - Malformed Request
# ============================================================================
print_test "11" "Error Handling - Malformed Request"

RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "$BACKOFFICE_URL/api/tickets/create" \
    -H "Content-Type: application/json" \
    -d '{"invalid": "data"}' 2>&1)

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)

if [ "$HTTP_CODE" -eq 400 ] || [ "$HTTP_CODE" -eq 500 ]; then
    pass_test "Malformed request properly rejected (HTTP $HTTP_CODE)"
else
    fail_test "Error handling" "Expected HTTP 400/500, got $HTTP_CODE"
fi

# ============================================================================
# TEST 12: Report Submission
# ============================================================================
print_test "12" "Gate Report Submission"

XML_REPORT='<?xml version="1.0" encoding="UTF-8"?>
<GateReport>
  <GateId>TEST-001</GateId>
  <Timestamp>2024-01-07 12:00:00</Timestamp>
  <Statistics>
    <TotalProcessed>100</TotalProcessed>
    <ValidCount>95</ValidCount>
    <InvalidCount>5</InvalidCount>
  </Statistics>
</GateReport>'

RESPONSE=$(curl -s -X POST "$BACKOFFICE_URL/api/reports" \
    -H "Content-Type: application/xml" \
    -d "$XML_REPORT")

if echo "$RESPONSE" | grep -q "success.*true"; then
    pass_test "Gate report successfully submitted"
else
    fail_test "Report submission" "Response: $RESPONSE"
fi

# ============================================================================
# TEST SUMMARY
# ============================================================================
echo ""
echo -e "${BLUE}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║              TEST SUMMARY                     ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════╝${NC}"
echo ""
echo "Total Tests Run:    $TESTS_RUN"
echo -e "${GREEN}Tests Passed:       $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Tests Failed:       $TESTS_FAILED${NC}"
else
    echo -e "${GREEN}Tests Failed:       $TESTS_FAILED${NC}"
fi
echo ""

# Calculate pass rate
if [ $TESTS_RUN -gt 0 ]; then
    PASS_RATE=$((TESTS_PASSED * 100 / TESTS_RUN))
    echo "Pass Rate: $PASS_RATE%"
    echo ""
    
    if [ $PASS_RATE -eq 100 ]; then
        echo -e "${GREEN}╔═══════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║          ALL TESTS PASSED! ✓                  ║${NC}"
        echo -e "${GREEN}╚═══════════════════════════════════════════════╝${NC}"
        exit 0
    elif [ $PASS_RATE -ge 80 ]; then
        echo -e "${YELLOW}Most tests passed, but some issues detected${NC}"
        exit 1
    else
        echo -e "${RED}Multiple test failures detected${NC}"
        exit 1
    fi
else
    echo -e "${RED}No tests were executed${NC}"
    exit 1
fi
