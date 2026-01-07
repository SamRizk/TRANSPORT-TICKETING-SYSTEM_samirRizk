// src/backoffice/main.cpp
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <random>
#include <thread>
#include <chrono>
#include "ticket.h"

using json = nlohmann::json;

/**
 * @brief Back-Office Service
 * 
 * Responsibilities (as per requirements):
 * - Sale: Generate ticket ID, create tickets, store in CSV
 * - Validation: Validate tickets against database
 * - Transactions: Receive and store reports from gates
 */
class BackOfficeService {
public:
    BackOfficeService(const std::string& host, int port, const std::string& stockFile)
        : host_(host), port_(port), stockFile_(stockFile), ticketCounter_(0) {
        loadTickets();
    }

    void start() {
        httplib::Server server;
        
        // Health check endpoint
        server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("OK", "text/plain");
        });

        // Ticket creation endpoint (SALE)
        server.Post("/api/tickets/create", [this](const httplib::Request& req, httplib::Response& res) {
            handleTicketCreation(req, res);
        });

        // Ticket validation endpoint
        server.Post("/api/tickets/validate", [this](const httplib::Request& req, httplib::Response& res) {
            handleTicketValidation(req, res);
        });

        // Report endpoint (from gates)
        server.Post("/api/reports", [this](const httplib::Request& req, httplib::Response& res) {
            handleReport(req, res);
        });

        // Get all tickets (for debugging/testing)
        server.Get("/api/tickets", [this](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(ticketMutex_);
            json j = json::array();
            for (const auto& ticket : tickets_) {
                j.push_back(json::parse(ticket.toJson()));
            }
            res.set_content(j.dump(2), "application/json");
        });

        std::cout << "╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║   Back-Office Service Starting...     ║" << std::endl;
        std::cout << "╚════════════════════════════════════════╝" << std::endl;
        std::cout << "Host: " << host_ << std::endl;
        std::cout << "Port: " << port_ << std::endl;
        std::cout << "Stock File: " << stockFile_ << std::endl;
        std::cout << "Loaded Tickets: " << tickets_.size() << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        server.listen(host_.c_str(), port_);
    }

private:
    std::string host_;
    int port_;
    std::string stockFile_;
    std::vector<Ticket> tickets_;
    std::mutex ticketMutex_;
    int ticketCounter_;
    std::vector<std::string> reports_;

    // Generate unique ticket ID
    std::string generateTicketId() {
        auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        return "TKT-" + std::to_string(++ticketCounter_) + "-" + std::to_string(timestamp);
    }

    // Load tickets from CSV file
    void loadTickets() {
        std::ifstream file(stockFile_);
        if (!file.is_open()) {
            std::cout << "⚠ Stock file not found. Starting with empty database." << std::endl;
            return;
        }

        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::stringstream ss(line);
            std::string id, date, validity, lineNum;
            
            std::getline(ss, id, ',');
            std::getline(ss, date, ',');
            std::getline(ss, validity, ',');
            std::getline(ss, lineNum, ',');
            
            if (!id.empty()) {
                Ticket ticket(id, std::stoi(validity), std::stoi(lineNum));
                ticket.setCreationDate(date);
                tickets_.push_back(ticket);
                
                // Update counter to avoid ID collision
                size_t pos = id.find('-');
                if (pos != std::string::npos) {
                    try {
                        int num = std::stoi(id.substr(pos + 1));
                        if (num > ticketCounter_) {
                            ticketCounter_ = num;
                        }
                    } catch (...) {}
                }
            }
        }
        
        file.close();
    }

    // Save tickets to CSV file
    void saveTickets() {
        std::ofstream file(stockFile_);
        file << "TicketID,CreationDate,ValidityDays,LineNumber\n";
        
        for (const auto& ticket : tickets_) {
            file << ticket.getId() << ","
                 << ticket.getCreationDate() << ","
                 << ticket.getValidityDays() << ","
                 << ticket.getLineNumber() << "\n";
        }
        
        file.close();
    }

    // Handle ticket creation request (SALE)
    void handleTicketCreation(const httplib::Request& req, httplib::Response& res) {
        try {
            std::cout << "\n=== Ticket Creation Request ===" << std::endl;
            
            json requestData = json::parse(req.body);
            
            int validityDays = requestData["validityDays"];
            int lineNumber = requestData["lineNumber"];
            
            std::cout << "Validity Days: " << validityDays << std::endl;
            std::cout << "Line Number: " << lineNumber << std::endl;
            
            // Generate unique ticket ID
            std::string ticketId = generateTicketId();
            Ticket ticket(ticketId, validityDays, lineNumber);
            
            // Simulate processing delay (realistic scenario)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            {
                std::lock_guard<std::mutex> lock(ticketMutex_);
                tickets_.push_back(ticket);
                saveTickets();
            }
            
            // Prepare response with Base64 ticket
            json response = {
                {"success", true},
                {"ticketId", ticket.getId()},
                {"ticket", ticket.toJson()},
                {"ticketBase64", ticket.toBase64()}
            };
            
            std::cout << "✓ Ticket Created: " << ticket.getId() << std::endl;
            std::cout << "  Base64: " << ticket.toBase64().substr(0, 30) << "..." << std::endl;
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            std::cerr << "✗ Error: " << e.what() << std::endl;
            json error = {{"success", false}, {"error", e.what()}};
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        }
    }

    // Handle ticket validation request
    void handleTicketValidation(const httplib::Request& req, httplib::Response& res) {
        try {
            std::cout << "\n=== Ticket Validation Request ===" << std::endl;
            
            json requestData = json::parse(req.body);
            std::string ticketBase64 = requestData["ticketBase64"];
            
            // Simulate occasional failures (10% chance) for retry testing
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 10);
            
            if (dis(gen) == 1) {
                throw std::runtime_error("Simulated validation service failure");
            }
            
            // Simulate processing delay
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            Ticket ticket = Ticket::fromBase64(ticketBase64);
            
            std::cout << "Ticket ID: " << ticket.getId() << std::endl;
            std::cout << "Line Number: " << ticket.getLineNumber() << std::endl;
            
            bool exists = false;
            bool isValid = false;
            std::string message;
            
            {
                std::lock_guard<std::mutex> lock(ticketMutex_);
                for (const auto& t : tickets_) {
                    if (t.getId() == ticket.getId()) {
                        exists = true;
                        break;
                    }
                }
            }
            
            if (!exists) {
                message = "Ticket not found in database";
            } else if (ticket.isExpired()) {
                message = "Ticket expired";
            } else {
                isValid = true;
                message = "Ticket is valid";
            }
            
            json response = {
                {"success", true},
                {"valid", isValid},
                {"message", message},
                {"ticketId", ticket.getId()},
                {"lineNumber", ticket.getLineNumber()}
            };
            
            std::cout << "Result: " << (isValid ? "✓ VALID" : "✗ INVALID") << std::endl;
            std::cout << "Message: " << message << std::endl;
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            std::cerr << "✗ Validation Error: " << e.what() << std::endl;
            json error = {{"success", false}, {"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    }

    // Handle report from gate (XML transactions)
    void handleReport(const httplib::Request& req, httplib::Response& res) {
        try {
            std::cout << "\n=== Report Received ===" << std::endl;
            
            reports_.push_back(req.body);
            
            // Log first few lines of report
            std::istringstream iss(req.body);
            std::string line;
            int lineCount = 0;
            while (std::getline(iss, line) && lineCount++ < 5) {
                std::cout << line << std::endl;
            }
            if (lineCount >= 5) std::cout << "..." << std::endl;
            
            json response = {{"success", true}, {"message", "Report received"}};
            res.set_content(response.dump(), "application/json");
            
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"error", e.what()}};
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        }
    }
};

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    int port = 8080;
    std::string stockFile = "../data/tickets.csv";
    
    // Allow command line arguments
    if (argc > 1) port = std::atoi(argv[1]);
    if (argc > 2) stockFile = argv[2];
    
    BackOfficeService service(host, port, stockFile);
    service.start();
    
    return 0;
}
