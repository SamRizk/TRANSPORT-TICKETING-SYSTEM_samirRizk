// src/tvm/main.cpp
#include <iostream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include <thread>
#include <chrono>

using json = nlohmann::json;

/**
 * @brief Ticket Vending Machine Service
 * 
 * Flow (as per requirements):
 * 1. Receives ticket info via MQTT message (ticket/sale/request)
 * 2. Sends request to Back-Office via REST API
 * 3. Back-Office creates ticket and responds with Base64 data
 * 4. Publishes result to MQTT (ticket/sale/response)
 */
class TVMService {
public:
    TVMService(const std::string& mqttBroker, const std::string& clientId, 
               const std::string& backOfficeUrl)
        : mqttClient_(mqttBroker, clientId),
          backOfficeUrl_(backOfficeUrl),
          running_(true) {
    }

    ~TVMService() {
        disconnect();
    }

    void start() {
        std::cout << "╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║    Ticket Vending Machine Service     ║" << std::endl;
        std::cout << "╚════════════════════════════════════════╝" << std::endl;
        std::cout << "MQTT Broker: " << mqttClient_.get_server_uri() << std::endl;
        std::cout << "Back-Office: " << backOfficeUrl_ << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        // Connect to MQTT broker
        connectMQTT();
        
        // Subscribe to sale request topic
        subscribe();
        
        // Start message loop
        consumeMessages();
    }

    void stop() {
        running_ = false;
        disconnect();
    }

private:
    mqtt::async_client mqttClient_;
    std::string backOfficeUrl_;
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
            const std::string TOPIC = "ticket/sale/request";
            const int QOS = 1;
            
            mqttClient_.subscribe(TOPIC, QOS)->wait();
            mqttClient_.start_consuming();
            
            std::cout << "✓ Subscribed to: " << TOPIC << std::endl;
            std::cout << "\nWaiting for sale requests...\n" << std::endl;
            
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
                
                handleSaleRequest(msg->to_string());
            }
        } catch (const mqtt::exception& exc) {
            std::cerr << "✗ Consume error: " << exc.what() << std::endl;
        }
    }

    void handleSaleRequest(const std::string& payload) {
        try {
            std::cout << "\n=== New Sale Request ===" << std::endl;
            std::cout << "Payload: " << payload << std::endl;
            
            // Parse MQTT message
            json request = json::parse(payload);
            int validityDays = request["validityDays"];
            int lineNumber = request["lineNumber"];
            
            std::cout << "Validity: " << validityDays << " days" << std::endl;
            std::cout << "Line: " << lineNumber << std::endl;
            
            // Create request for Back-Office
            json backOfficeRequest = {
                {"validityDays", validityDays},
                {"lineNumber", lineNumber}
            };
            
            std::cout << "Sending request to Back-Office..." << std::endl;
            
            // Send HTTP POST to Back-Office
            httplib::Client client(backOfficeUrl_);
            client.set_connection_timeout(5, 0);
            client.set_read_timeout(10, 0);
            
            auto res = client.Post("/api/tickets/create", 
                                  backOfficeRequest.dump(), 
                                  "application/json");
            
            if (!res) {
                std::cerr << "✗ Failed to connect to Back-Office" << std::endl;
                publishError("Back-Office unavailable");
                return;
            }
            
            if (res->status == 200) {
                json response = json::parse(res->body);
                
                std::cout << "\n✓ Ticket created successfully!" << std::endl;
                std::cout << "Ticket ID: " << response["ticketId"] << std::endl;
                std::cout << "Base64: " << response["ticketBase64"].get<std::string>().substr(0, 40) << "..." << std::endl;
                
                // Publish success response
                json ticketResponse = {
                    {"status", "success"},
                    {"ticketId", response["ticketId"]},
                    {"ticketBase64", response["ticketBase64"]}
                };
                
                publishResponse(ticketResponse.dump());
                
            } else {
                std::cerr << "✗ Back-Office error: " << res->status << " - " << res->body << std::endl;
                publishError("Ticket creation failed");
            }
            
        } catch (const std::exception& e) {
            std::cerr << "✗ Error handling sale request: " << e.what() << std::endl;
            publishError(std::string("Error: ") + e.what());
        }
    }

    void publishResponse(const std::string& payload) {
        try {
            const std::string TOPIC = "ticket/sale/response";
            const int QOS = 1;
            
            auto msg = mqtt::make_message(TOPIC, payload);
            msg->set_qos(QOS);
            mqttClient_.publish(msg)->wait();
            
            std::cout << "✓ Response published to: " << TOPIC << std::endl;
            
        } catch (const mqtt::exception& exc) {
            std::cerr << "✗ Publish error: " << exc.what() << std::endl;
        }
    }

    void publishError(const std::string& errorMsg) {
        json errorResponse = {
            {"status", "error"},
            {"message", errorMsg}
        };
        publishResponse(errorResponse.dump());
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
};

int main(int argc, char* argv[]) {
    std::string mqttBroker = "tcp://mosquitto:1883";
    std::string backOfficeUrl = "http://backoffice:8080";
    
    // Allow command line arguments
    if (argc > 1) mqttBroker = argv[1];
    if (argc > 2) backOfficeUrl = argv[2];
    
    try {
        TVMService tvm(mqttBroker, "TVM-001", backOfficeUrl);
        tvm.start();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
