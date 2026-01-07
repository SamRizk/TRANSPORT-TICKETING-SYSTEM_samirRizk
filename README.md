# Transport Ticketing System

A comprehensive C++ transport ticketing system featuring ticket vending machines, gate validators, and a centralized back-office system with MQTT and REST API communication.

## 📋 Project Overview

This project implements a realistic transport ticketing system simulator as per the technical requirements, including:

- **Ticket Vending Machine (TVM)**: Issues tickets via MQTT → REST API flow
- **Gate Validator**: Validates tickets online/offline with automatic fallback
- **Back-Office Service**: Central system for ticket creation, validation, and reporting
- **MQTT Communication**: Real-time messaging using Eclipse Mosquitto
- **Docker Support**: Complete containerized deployment with docker-compose

## 🏗️ System Architecture

```
                    ┌─────────────────┐
                    │  MQTT Broker    │
                    │  (Mosquitto)    │
                    └────────┬────────┘
                             │
            ┌────────────────┼────────────────┐
            │                │                │
            ▼                ▼                ▼
    ┌───────────────┐  ┌───────────┐  ┌──────────────┐
    │     TVM       │  │   Gate    │  │  Back-Office │
    │   (MQTT)      │  │  (MQTT)   │  │   (REST)     │
    └───────┬───────┘  └─────┬─────┘  └──────┬───────┘
            │                │                │
            │    HTTP REST   │   HTTP REST    │
            └────────────────┴────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │  Tickets (CSV)  │
                    └─────────────────┘
```

## ✨ Features

### ✅ All Required Features
- [x] Ticket creation through TVM (MQTT → REST)
- [x] Online validation through Gate (REST API)
- [x] Offline validation (expiry check when Back-Office unavailable)
- [x] Base64 ticket encoding/decoding
- [x] MQTT communication (Mosquitto broker)
- [x] REST API endpoints for all operations
- [x] CSV ticket storage
- [x] XML transaction reporting from gates
- [x] Docker containerization (docker-compose)
- [x] Multiple gate support

### 🎁 Bonus Features
- [x] Health check endpoints
- [x] Simulated delayed responses and failures
- [x] Retry mechanisms (HTTP timeouts)
- [x] Dynamic gate scaling
- [x] Comprehensive error handling
- [x] Structured logging
- [x] Thread-safe operations

## 📋 Prerequisites

### For Docker Deployment (Recommended)
- Docker Engine 20.10+
- Docker Compose 1.29+
- Git

### For Local Development
- Ubuntu 22.04 or similar Linux distribution
- GCC/G++ 9.0+ with C++17 support
- CMake 3.15+
- OpenSSL development libraries
- Paho MQTT C++ library
- Mosquitto broker and clients

## 🚀 Quick Start

### Option 1: Docker (Easiest)

```bash
# 1. Clone the repository
git clone <[your-repo-url](https://github.com/SamRizk/TRANSPORT-TICKETING-SYSTEM_samirRizk.git)>
cd TicketingTask_SamirRizk

# 2. Start all services
docker-compose up --build

# 3. In another terminal, test the system
./scripts/simulate_ticket_sale.sh 7 1
./scripts/simulate_ticket_validation.sh
```

### Option 2: Manual Build

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    libssl-dev libpaho-mqtt-dev libpaho-mqttpp-dev \
    mosquitto mosquitto-clients \
    curl jq

# 2. Build the project
./scripts/build.sh

# 3. Start Mosquitto (Terminal 1)
mosquitto -c docker/mosquitto.conf

# 4. Start Back-Office (Terminal 2)
./build/bin/backoffice

# 5. Start TVM (Terminal 3)
./build/bin/tvm tcp://localhost:1883 http://localhost:8080

# 6. Start Gate (Terminal 4)
./build/bin/gate 001 tcp://localhost:1883 http://localhost:8080

# 7. Test (Terminal 5)
./scripts/simulate_ticket_sale.sh 7 1
./scripts/simulate_ticket_validation.sh
```

## 📁 Project Structure

```
TicketingTask_YourName/
├── src/
│   ├── backoffice/
│   │   └── main.cpp              # Back-Office service
│   ├── tvm/
│   │   └── main.cpp              # Ticket Vending Machine
│   ├── gate/
│   │   └── main.cpp              # Gate Validator
│   └── common/
│       └── ticket.cpp            # Ticket class implementation
├── include/
│   └── common/
│       └── ticket.h              # Ticket class header
├── tests/
│   └── unit/                     # Unit tests
├── scripts/
│   ├── build.sh                  # Build script
│   ├── simulate_ticket_sale.sh   # Sale simulation
│   └── simulate_ticket_validation.sh  # Validation simulation
├── docker/
│   ├── Dockerfile.backoffice     # Back-Office container
│   ├── Dockerfile.tvm            # TVM container
│   ├── Dockerfile.gate           # Gate container
│   └── mosquitto.conf            # MQTT broker config
├── data/
│   └── tickets.csv               # Ticket storage
├── docker-compose.yml            # Docker orchestration
├── CMakeLists.txt                # Root build config
└── README.md                     # This file
```

## 🎯 Usage Examples

### Example 1: Create and Validate Ticket

```bash
# Start services
docker-compose up -d

# Create a ticket (7 days validity, line 1)
./scripts/simulate_ticket_sale.sh 7 1

# The script will output a Base64 ticket
# Validate the ticket
./scripts/simulate_ticket_validation.sh
```

### Example 2: Test Offline Validation

```bash
# Stop Back-Office to simulate network failure
docker-compose stop backoffice

# Validate a ticket - gate will use offline mode (expiry check only)
./scripts/simulate_ticket_validation.sh

# Restart Back-Office
docker-compose start backoffice
```

### Example 3: Multiple Gates

```bash
# Scale to 5 gates
docker-compose up -d --scale gate1=5

# Send validation to specific gate
mosquitto_pub -h localhost -t "ticket/validation/request/003" \
  -m '{"ticketBase64": "<your-base64-ticket>"}'
```

### Example 4: Direct REST API

```bash
# Create ticket
curl -X POST http://localhost:8080/api/tickets/create \
  -H "Content-Type: application/json" \
  -d '{"validityDays": 30, "lineNumber": 5}'

# Validate ticket
curl -X POST http://localhost:8080/api/tickets/validate \
  -H "Content-Type: application/json" \
  -d '{"ticketBase64": "eyJ0aWNrZXRJZCI6..."}'

# List all tickets
curl http://localhost:8080/api/tickets | jq '.'

# Health check
curl http://localhost:8080/health
```

## 🔌 API Reference

### Back-Office REST API (Port 8080)

| Method | Endpoint | Description | Request Body |
|--------|----------|-------------|--------------|
| GET | `/health` | Health check | - |
| POST | `/api/tickets/create` | Create ticket | `{"validityDays": 7, "lineNumber": 1}` |
| POST | `/api/tickets/validate` | Validate ticket | `{"ticketBase64": "..."}` |
| POST | `/api/reports` | Submit gate report | XML data |
| GET | `/api/tickets` | List all tickets | - |

### MQTT Topics

| Topic | Direction | Purpose | Payload |
|-------|-----------|---------|---------|
| `ticket/sale/request` | → TVM | Ticket creation | `{"validityDays": 7, "lineNumber": 1}` |
| `ticket/sale/response` | TVM → | Creation result | `{"ticketId": "...", "ticketBase64": "..."}` |
| `ticket/validation/request` | → Gate | Validation request | `{"ticketBase64": "..."}` |
| `ticket/validation/request/{gateId}` | → Gate | Gate-specific validation | `{"ticketBase64": "..."}` |
| `ticket/validation/response` | Gate → | Validation result | `{"valid": true, "gateAction": "OPEN"}` |

## 🧪 Testing

### Run All Tests

```bash
cd build
ctest --verbose
```

### Test Scenarios

**Scenario 1: Happy Path**
```bash
./scripts/simulate_ticket_sale.sh 7 1
./scripts/simulate_ticket_validation.sh
# Expected: Gate OPEN
```

**Scenario 2: Expired Ticket**
```bash
# Create ticket with 0 days validity
curl -X POST http://localhost:8080/api/tickets/create \
  -d '{"validityDays": 0, "lineNumber": 1}'
# Validate immediately - Expected: Gate CLOSED
```

**Scenario 3: Offline Mode**
```bash
docker-compose stop backoffice
./scripts/simulate_ticket_validation.sh
# Expected: Offline validation (expiry check only)
```

## 📊 Monitoring

### View Logs

```bash
# All services
docker-compose logs -f

# Specific service
docker-compose logs -f backoffice
docker-compose logs -f gate1
```

### Check Service Status

```bash
docker-compose ps
```

### View Statistics

```bash
# Back-Office health
curl http://localhost:8080/health

# List all tickets
curl http://localhost:8080/api/tickets | jq '.[] | {id, validityDays, lineNumber}'
```

## 🔧 Configuration

### Environment Variables

**Back-Office:**
- `PORT`: HTTP server port (default: 8080)

**TVM:**
- `MQTT_BROKER`: MQTT broker URL (default: tcp://mosquitto:1883)
- `BACKOFFICE_URL`: Back-Office URL (default: http://backoffice:8080)

**Gate:**
- `GATE_ID`: Unique gate identifier
- `MQTT_BROKER`: MQTT broker URL
- `BACKOFFICE_URL`: Back-Office URL

## 🐛 Troubleshooting

### Services Won't Start

```bash
# Check ports
netstat -tuln | grep -E '8080|1883'

# Restart all services
docker-compose down
docker-compose up --build
```

### MQTT Connection Failed

```bash
# Check Mosquitto
docker-compose ps mosquitto

# Test MQTT
mosquitto_pub -h localhost -t test -m "hello"
mosquitto_sub -h localhost -t test
```

### Build Errors

```bash
# Clean rebuild
CLEAN_BUILD=true ./scripts/build.sh

# Check dependencies
sudo apt-get install libpaho-mqtt-dev libpaho-mqttpp-dev
```

## 📝 Design Decisions

### Base64 Encoding
- **Why**: Easy transmission over MQTT and HTTP without binary issues
- **Implementation**: Custom implementation in Ticket class

### MQTT + REST Hybrid
- **MQTT**: Event-driven requests (sale, validation) - pub/sub model
- **REST**: Critical operations (API calls to Back-Office) - request/response

### Offline Validation
- **Trade-off**: Less secure but maintains availability
- **Implementation**: Local expiry check only (as specified)

### CSV Storage
- **Why**: Simple, human-readable, easy to debug
- **Alternative**: Could use SQLite for production

## 👤 Author

**Your Name**
- GitHub: ([SamRizk](https://github.com/SamRizk))
- Email: samir9999life@gmail.com

## 📄 License

This project was created as part of a technical assessment for Hitachi Rail.

## 🙏 Acknowledgments

- Eclipse Paho MQTT C++ library
- cpp-httplib for HTTP server/client
- nlohmann/json for JSON parsing
- Eclipse Mosquitto MQTT broker
