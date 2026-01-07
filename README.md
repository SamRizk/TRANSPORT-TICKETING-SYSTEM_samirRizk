# Transport Ticketing System

A modular C++ transport ticketing system simulator featuring ticket vending machines, gate validators, and a centralized back-office system with MQTT communication.

## 🏗️ Architecture

```
┌─────────────────┐       MQTT        ┌──────────────────┐
│  Ticket Vending │◄─────────────────►│  MQTT Broker     │
│    Machine      │   sale/request    │  (Mosquitto)     │
└────────┬────────┘                   └────────┬─────────┘
         │                                     │
         │ HTTP REST API                       │ MQTT
         │                                     │ validation/request
         ▼                                     ▼
┌─────────────────┐                   ┌──────────────────┐
│   Back-Office   │◄──────────────────│  Gate Validator  │
│    Service      │   HTTP REST API   │   (Multiple)     │
└─────────────────┘   validation      └──────────────────┘
         │
         ▼
    ┌────────┐
    │ Tickets│
    │  CSV   │
    └────────┘
```

## ✨ Features

- **Ticket Vending Machine (TVM)**: Issues tickets through MQTT → REST API flow
- **Gate Validator**: Validates tickets online/offline with automatic fallback
- **Back-Office**: Centralized ticket creation, validation, and reporting
- **MQTT Communication**: Real-time messaging using Mosquitto broker
- **Docker Support**: Complete containerized deployment
- **Offline Mode**: Gates can validate expired tickets when back-office is unavailable
- **XML Reporting**: Gates send transaction reports to back-office
- **Multiple Gates**: Support for dynamic gate addition

## 📋 Prerequisites

### For Docker Deployment (Recommended)
- Docker Engine 20.10+
- Docker Compose 1.29+

### For Local Development
- Ubuntu 22.04 or similar Linux distribution
- GCC/G++ 9.0+ with C++17 support
- CMake 3.15+
- OpenSSL development libraries
- MQTT C++ (Paho) library
- Mosquitto broker

## 🚀 Quick Start

### Using Docker (Easiest Method)

1. **Clone the repository**
   ```bash
   git clone <[repository-url](https://github.com/SamRizk/TRANSPORT-TICKETING-SYSTEM_samirRizk.git)>
   cd TicketingSystem_samirRizk
   ```

2. **Build and start all services**
   ```bash
   docker-compose up --build
   ```

3. **Verify services are running**
   ```bash
   # Check Back-Office health
   curl http://localhost:8080/health
   
   # Check Docker containers
   docker-compose ps
   ```

4. **Simulate ticket purchase**
   ```bash
   ./scripts/simulate_ticket_sale.sh 7 1
   # Arguments: validity_days line_number
   ```

5. **Validate a ticket**
   ```bash
   # Option 1: Validation with auto-created ticket
   ./scripts/simulate_ticket_validation.sh
   
   # Option 2: Validation with specific ticket
   ./scripts/simulate_ticket_validation.sh <base64_ticket>
   ```

### Manual Build (Linux)

1. **Install dependencies**
   ```bash
   sudo apt-get update
   sudo apt-get install -y \
       build-essential cmake git \
       libssl-dev libpaho-mqtt-dev \
       mosquitto mosquitto-clients \
       curl jq
   ```

2. **Build the project**
   ```bash
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   make -j$(nproc)
   ```

3. **Start Mosquitto broker**
   ```bash
   mosquitto -c /etc/mosquitto/mosquitto.conf
   ```

4. **Run services** (in separate terminals)
   ```bash
   # Terminal 1: Back-Office
   ./build/bin/backoffice
   
   # Terminal 2: TVM
   ./build/bin/tvm tcp://localhost:1883 http://localhost:8080
   
   # Terminal 3: Gate
   ./build/bin/gate 001 tcp://localhost:1883 http://localhost:8080
   ```

## 📁 Project Structure

```
TicketingSystem_YourName/
├── src/
│   ├── backoffice/          # Back-Office service
│   ├── tvm/                 # Ticket Vending Machine
│   ├── gate/                # Gate Validator
│   └── common/              # Shared components (Ticket, MQTT, HTTP)
├── include/
│   ├── backoffice/          # Back-Office headers
│   ├── tvm/                 # TVM headers
│   ├── gate/                # Gate headers
│   └── common/              # Common headers
├── tests/
│   ├── unit/                # Unit tests
│   └── integration/         # Integration tests
├── scripts/
│   ├── simulate_ticket_sale.sh
│   ├── simulate_ticket_validation.sh
│   ├── run_services.sh
│   └── build.sh
├── docker/
│   ├── Dockerfile.backoffice
│   ├── Dockerfile.tvm
│   ├── Dockerfile.gate
│   └── mosquitto.conf
├── data/
│   └── tickets.csv          # Ticket storage
├── docker-compose.yml
├── CMakeLists.txt
└── README.md
```

## 🎯 Usage Examples

### Example 1: Complete Flow

```bash
# 1. Start all services
docker-compose up -d

# 2. Create a ticket (7 days validity, line 1)
./scripts/simulate_ticket_sale.sh 7 1

# 3. Get the ticket Base64 from response and validate it
TICKET_BASE64="<base64_from_response>"
./scripts/simulate_ticket_validation.sh $TICKET_BASE64

# 4. View Back-Office logs
docker-compose logs -f backoffice

# 5. View Gate logs
docker-compose logs -f gate1
```

### Example 2: Test Offline Validation

```bash
# Stop the back-office to simulate network failure
docker-compose stop backoffice

# Try validating a ticket - gate will use offline mode
./scripts/simulate_ticket_validation.sh

# Restart back-office
docker-compose start backoffice
```

### Example 3: Multiple Gates

```bash
# Scale gates to 5 instances
docker-compose up -d --scale gate1=5

# Send validation to specific gate
mosquitto_pub -h localhost -t "ticket/validation/request/003" \
  -m '{"ticketBase64": "..."}'
```

### Example 4: Direct API Testing

```bash
# Create ticket via REST API
curl -X POST http://localhost:8080/api/tickets/create \
  -H "Content-Type: application/json" \
  -d '{"validityDays": 30, "lineNumber": 5}'

# Validate ticket via REST API
curl -X POST http://localhost:8080/api/tickets/validate \
  -H "Content-Type: application/json" \
  -d '{"ticketBase64": "eyJ0aWNrZXRJZCI6IlRLVC0..."}'

# Get all tickets
curl http://localhost:8080/api/tickets | jq '.'
```

## 🧪 Testing

### Unit Tests
```bash
cd build
ctest --verbose
```

### Integration Tests
```bash
./scripts/run_integration_tests.sh
```

### Manual Testing with MQTT Explorer
1. Download [MQTT Explorer](http://mqtt-explorer.com/)
2. Connect to `localhost:1883`
3. Publish to topics:
   - `ticket/sale/request` - Create tickets
   - `ticket/validation/request` - Validate tickets
4. Subscribe to:
   - `ticket/sale/response` - Ticket creation responses
   - `ticket/validation/response` - Validation responses

## 📊 Monitoring

### View Service Logs
```bash
# All services
docker-compose logs -f

# Specific service
docker-compose logs -f backoffice
docker-compose logs -f tvm
docker-compose logs -f gate1
```

### Check Service Health
```bash
# Back-Office health
curl http://localhost:8080/health

# View all containers
docker-compose ps
```

## 🔧 Configuration

### Environment Variables

**Back-Office:**
- `PORT`: HTTP server port (default: 8080)
- `MQTT_BROKER`: MQTT broker address

**TVM:**
- `MQTT_BROKER`: MQTT broker address (default: tcp://mosquitto:1883)
- `BACKOFFICE_URL`: Back-Office URL (default: http://backoffice:8080)

**Gate:**
- `GATE_ID`: Unique gate identifier
- `MQTT_BROKER`: MQTT broker address
- `BACKOFFICE_URL`: Back-Office URL

### MQTT Topics
- `ticket/sale/request` - TVM listens for sale requests
- `ticket/sale/response` - TVM publishes ticket creation results
- `ticket/validation/request` - Gates listen for validation requests
- `ticket/validation/request/{gate_id}` - Gate-specific validation
- `ticket/validation/response` - Gates publish validation results

## 🛠️ Troubleshooting

### Issue: Services won't start
```bash
# Check if ports are already in use
netstat -tuln | grep -E '8080|1883'

# Restart all services
docker-compose down
docker-compose up --build
```

### Issue: MQTT connection fails
```bash
# Check Mosquitto is running
docker-compose ps mosquitto

# Test MQTT connection
mosquitto_pub -h localhost -t test -m "hello"
mosquitto_sub -h localhost -t test
```

### Issue: Back-Office not responding
```bash
# Check Back-Office logs
docker-compose logs backoffice

# Restart Back-Office
docker-compose restart backoffice
```

## 🎓 Design Decisions

### Ticket Format
- **JSON + Base64**: Tickets are stored as JSON and encoded in Base64 for transmission
- **Fields**: ID, creation date, validity days, line number
- **Why**: Easy to serialize, human-readable when decoded, compact for MQTT

### Communication Protocol
- **MQTT for Commands**: Asynchronous, pub/sub model ideal for vending/validation requests
- **HTTP REST for Services**: Synchronous, reliable for back-office communication
- **Why**: Best of both worlds - MQTT for real-time events, HTTP for critical operations

### Offline Validation
- Gates validate locally by checking ticket expiry when back-office is unavailable
- **Trade-off**: Less secure but maintains service availability
- **Why**: Real-world requirement - gates must function during network issues

### XML Reporting
- Gates send periodic XML reports to back-office
- **Format**: Statistics + last N validations
- **Why**: Structured data format suitable for archival and analysis

## 🚀 Advanced Features (Bonus)

### gRPC Support
To enable gRPC for transaction reporting:
```bash
cmake -DENABLE_GRPC=ON ..
make
```

### Retry Mechanisms
Automatic retry with exponential backoff for failed requests (implemented in HTTP client)

### Dynamic Gate Addition
Add gates at runtime by scaling Docker services:
```bash
docker-compose up -d --scale gate1=10
```

## 📝 License

This project is created for evaluation purposes as part of a technical assessment.

## 👤 Author

**Samir Rizk**
- GitHub: [SamRizk](https://github.com/SamRizk)
- Email: samir9999life@gmail.com

## 🙏 Acknowledgments

- Eclipse Paho MQTT C++ library
- cpp-httplib for HTTP server/client
- nlohmann/json for JSON parsing
- Eclipse Mosquitto MQTT broker
