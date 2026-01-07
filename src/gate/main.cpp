// src/gate/main.cpp
#include <iostream>
#include <vector>
#include <sstream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include "ticket.h"

using json = nlohmann::json;

struct ValidationRecord {
    std::string ticketId;
    std::string timestamp;
    bool valid;
    std::string validationMode; // "online" or "offline"
};

/**
 * @brief Gate Validator Service
 * 
 * Responsibilities (as per requirements):
 * - Receive ticket Base64 via MQTT
 * - Validate online through Back-Office (REST API)
 * - If Back-Office unavailable: offline validation (expiry date only)
 * - Open/Close gate based on validation
 * - Maintain XML transactions and send to Back-Office
 */
class GateService {
public:
    GateService(const std::string& gateId, const std::string& mqttBroker, 
                const std::string& backOfficeUrl)
        : gateId_(gateId),
          mqttClient_(mqttBroker, "GATE-" + gateId),
          backOfficeUrl_(backOfficeUrl),
          totalProcessed_(0),
          validCount_(0),
          invalidCount_(0),
          running_(true) {
    }

    ~GateService() {
        disconnect();
    }

    void start() {
        std::cout << "╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║       Gate Validator Service          ║" << std::endl;
        std::cout << "╚════════════════════════════════════════╝" << std::endl;
        std::cout << "Gate ID: " << gateId_ << std::endl;
        std::cout << "MQTT Broker: " << mqttClient_.get_server_uri() << std::endl;
        std::cout << "Back-Office: " << backOfficeUrl_ << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        // Connect and subscribe
        connectMQTT();
        subscribe();
        
        // Start message loop
        consumeMessages();
    }

    void stop() {
        running_ = false;
        disconnect();
    }

private:
    std::string gateId_;
    mqtt::async_client mqttClient_;
    std::string backOfficeUrl_;
    
    int totalProcessed_;
    int validCount_;
    int invalidCount_;
    std::vector<ValidationRecord> validationHistory_;
    bool running_;

    void connectMQTT() {
        try {
            mqtt::connect_options connOpts;
            connOpts.set_keep_alive_interval(20);
            connOpts.set_clean_session(true);
            connOpts.set_automatic_reconnect(true);
            
            std::cout << "Connecting to MQTT broker..." << std::endl;
            
            auto tok = mqttClient_.connect(connOpts);
            tok->wait();
            
            std::cout << "✓ Connected to MQTT broker" << std::endl;
            
        } catch (const mqtt::exception& exc) {
            std::cerr << "✗ MQTT Connection error: " << exc.what() << std::endl;
            throw;
        }
    }

    void subscribe() {
        try {
            const int QOS = 1;
            
            // Subscribe to general validation topic
            std::string topic1 = "ticket/validation/request";
            mqttClient_.subscribe(topic1, QOS)->wait();
            std::cout << "✓ Subscribed to: " << topic1 << std::endl;
            
            // Subscribe to gate-specific topic
            std::string topic2 = "ticket/validation/request/" + gateId_;
            mqttClient_.subscribe(topic2, QOS)->wait();
            std::cout << "✓ Subscribed to: " << topic2 << std::endl;
            
            mqttClient_.start_consuming();
            std::cout << "\nWaiting for validation requests...\n" << std::endl;
            
        } catch (const mqtt::exception& exc) {
            std::cerr << "✗ Subscribe error: " << exc.what() << std::endl;
            throw;
        }
    }

    void consumeMessages() {
        try {
            while (running_) {
                auto msg = mqttClient_.consume_message();
                
                if (!msg) {
                    if (!mqttClient_.is_connected()) {
                        std::cout << "Lost connection. Reconnecting..." << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }
                    break;
                }
                
                handleValidationRequest(msg->to_string());
            }
        } catch (const mqtt::exception& exc) {
            std::cerr << "✗ Consume error: " << exc.what() << std::endl;
        }
    }

    void handleValidationRequest(const std::string& payload) {
        try {
            std::cout << "\n=== Validation Request [Gate " << gateId_ << "] ===" << std::endl;
            
            // Parse MQTT message
            json request = json::parse(payload);
            std::string ticketBase64 = request["ticketBase64"];
            
            std::cout << "Ticket (Base64): " << ticketBase64.substr(0, 30) << "..." << std::endl;
            
            // Decode ticket
            Ticket ticket = Ticket::fromBase64(ticketBase64);
            std::cout << "Ticket ID: " << ticket.getId() << std::endl;
            std::cout << "Line Number: " << ticket.getLineNumber() << std::endl;
            std::cout << "Validity: " << ticket.getValidityDays() << " days" << std::endl;
            
            // Try online validation first
            bool valid = false;
            std::string validationMode = "online";
            std::string message;
            
            if (validateOnline(ticketBase64, valid, message)) {
                std::cout << "✓ Online validation successful" << std::endl;
            } else {
                std::cout << "⚠ Back-Office unavailable - Using offline validation" << std::endl;
                valid = validateOffline(ticket);
                validationMode = "offline";
                message = valid ? "Valid (offline check - expiry only)" : "Expired (offline check)";
            }
            
            // Record validation
            recordValidation(ticket.getId(), valid, validationMode);
            
            // Gate decision
            std::string gateAction = valid ? "OPEN" : "CLOSED";
            std::cout << "\n🚪 Gate Action: " << gateAction << std::endl;
            std::cout << "Message: " << message << std::endl;
            
            // Send report to Back-Office periodically
            if (totalProcessed_ % 10 == 0) {
                sendReport();
            }
            
            // Publish validation response
            json response = {
                {"gateId", gateId_},
                {"ticketId", ticket.getId()},
                {"valid", valid},
                {"gateAction", gateAction},
                {"validationMode", validationMode},
                {"message", message}
            };
            
            publishResponse(response.dump());
            
        } catch (const std::exception& e) {
            std::cerr << "✗ Error handling validation request: " << e.what() << std::endl;
        }
    }

    // Online validation via Back-Office REST API
    bool validateOnline(const std::string& ticketBase64, bool& valid, std::string& message) {
        try {
            httplib::Client client(backOfficeUrl_);
            client.set_connection_timeout(2, 0);
            client.set_read_timeout(5, 0);
            
            json request = {{"ticketBase64", ticketBase64}};
            
            auto res = client.Post("/api/tickets/validate", 
                                  request.dump(), 
                                  "application/json");
            
            if (!res || res->status != 200) {
                return false; // Back-Office unavailable
            }
            
            json response = json::parse(res->body);
            valid = response["valid"];
            message = response["message"];
            
            return true;
            
        } catch (const std::exception&) {
            return false;
        }
    }

    // Offline validation (only checks expiry date as per requirements)
    bool validateOffline(const Ticket& ticket) {
        return !ticket.isExpired();
    }

    // Record validation in history
    void recordValidation(const std::string& ticketId, bool valid, const std::string& mode) {
        totalProcessed_++;
        
        if (valid) {
            validCount_++;
        } else {
            invalidCount_++;
        }
        
        ValidationRecord record;
        record.ticketId = ticketId;
        record.timestamp = getCurrentTimestamp();
        record.valid = valid;
        record.validationMode = mode;
        
        validationHistory_.push_back(record);
        
        // Keep only last 100 records
        if (validationHistory_.size() > 100) {
            validationHistory_.erase(validationHistory_.begin());
        }
    }

    // Send XML report to Back-Office (as per requirements)
    void sendReport() {
        try {
            std::cout << "\nSending report to Back-Office..." << std::endl;
            
            // Create XML report
            std::stringstream xml;
            xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            xml << "<GateReport>\n";
            xml << "  <GateId>" << gateId_ << "</GateId>\n";
            xml << "  <Timestamp>" << getCurrentTimestamp() << "</Timestamp>\n";
            xml << "  <Statistics>\n";
            xml << "    <TotalProcessed>" << totalProcessed_ << "</TotalProcessed>\n";
            xml << "    <ValidCount>" << validCount_ << "</ValidCount>\n";
            xml << "    <InvalidCount>" << invalidCount_ << "</InvalidCount>\n";
            xml << "  </Statistics>\n";
            xml << "  <RecentValidations>\n";
            
            // Include last N validations
            int count = 0;
            for (auto it = validationHistory_.rbegin(); 
                 it != validationHistory_.rend() && count < 10; ++it, ++count) {
                xml << "    <Validation>\n";
                xml << "      <TicketId>" << it->ticketId << "</TicketId>\n";
                xml << "      <Timestamp>" << it->timestamp << "</Timestamp>\n";
                xml << "      <Valid>" << (it->valid ? "true" : "false") << "</Valid>\n";
                xml << "      <Mode>" << it->validationMode << "</Mode>\n";
                xml << "    </Validation>\n";
            }
            
            xml << "  </RecentValidations>\n";
            xml << "</GateReport>\n";
            
            // Send to Back-Office
            httplib::Client client(backOfficeUrl_);
            client.set_connection_timeout(2, 0);
            
            auto res = client.Post("/api/reports", xml.str(), "application/xml");
            
            if (res && res->status == 200) {
                std::cout << "✓ Report sent successfully" << std::endl;
            } else {
                std::cout << "⚠ Report send failed (Back-Office may be unavailable)" << std::endl;
            }
            
        } catch (const std::exception& e) {
            // Silently fail - reporting is not critical
            std::cout << "⚠ Report send error: " << e.what() << std::endl;
        }
    }

    void publishResponse(const std::string& payload) {
        try {
            const std::string TOPIC = "ticket/validation/response";
            const int QOS = 1;
            
            auto msg = mqtt::make_message(TOPIC, payload);
            msg->set_qos(QOS);
            mqttClient_.publish(msg)->wait();
            
            std::cout << "✓ Response published to: " << TOPIC << std::endl;
            
        } catch (const mqtt::exception& exc) {
            std::cerr << "✗ Publish error: " << exc.what() << std::endl;
        }
    }

    void disconnect() {
        try {
            if (mqttClient_.is_connected()) {
                mqttClient_.stop_consuming();
                mqttClient_.disconnect()->wait();
                std::cout << "Disconnected from MQTT broker" << std::endl;
            }
        } catch (const mqtt::exception& exc) {
            std::cerr << "Disconnect error: " << exc.what() << std::endl;
        }
    }

    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

int main(int argc, char* argv[]) {
    std::string gateId = "001";
    std::string mqttBroker = "tcp://mosquitto:1883";
    std::string backOfficeUrl = "http://backoffice:8080";
    
    if (argc > 1) gateId = argv[1];
    if (argc > 2) mqttBroker = argv[2];
    if (argc > 3) backOfficeUrl = argv[3];
    
    try {
        GateService gate(gateId, mqttBroker, backOfficeUrl);
        gate.start();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
