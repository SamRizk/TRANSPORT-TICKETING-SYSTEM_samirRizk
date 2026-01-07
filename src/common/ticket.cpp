// src/common/ticket.cpp
#include "ticket.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <stdexcept>
#include <vector>

// Base64 encoding table
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// Default constructor
Ticket::Ticket() 
    : ticketId_(""), 
      creationDate_(getCurrentDateISO()), 
      validityDays_(0), 
      lineNumber_(0) {
}

// Parameterized constructor
Ticket::Ticket(const std::string& id, int validityDays, int lineNumber)
    : ticketId_(id), 
      creationDate_(getCurrentDateISO()),
      validityDays_(validityDays), 
      lineNumber_(lineNumber) {
}

// Check if ticket is valid (has ID, validity days > 0, and not expired)
bool Ticket::isValid() const {
    return !ticketId_.empty() && validityDays_ > 0 && !isExpired();
}

// Check if ticket has expired based on creation date and validity period
bool Ticket::isExpired() const {
    try {
        auto creationTime = getCreationTimePoint();
        auto now = std::chrono::system_clock::now();
        auto validUntil = creationTime + std::chrono::hours(24 * validityDays_);
        return now > validUntil;
    } catch (const std::exception&) {
        return true; // If date parsing fails, consider expired for safety
    }
}

// Serialize ticket to JSON string
std::string Ticket::toJson() const {
    json j = *this;
    return j.dump();
}

// Deserialize ticket from JSON string
Ticket Ticket::fromJson(const std::string& jsonStr) {
    json j = json::parse(jsonStr);
    return j.get<Ticket>();
}

// Serialize ticket to Base64 string (as required by task)
std::string Ticket::toBase64() const {
    std::string jsonStr = toJson();
    return base64Encode(jsonStr);
}

// Deserialize ticket from Base64 string (as required by task)
Ticket Ticket::fromBase64(const std::string& base64Str) {
    std::string jsonStr = base64Decode(base64Str);
    return fromJson(jsonStr);
}

// Parse creation date string to time_point
std::chrono::system_clock::time_point Ticket::getCreationTimePoint() const {
    std::tm tm = {};
    std::istringstream ss(creationDate_);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    if (ss.fail()) {
        throw std::runtime_error("Failed to parse creation date: " + creationDate_);
    }
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Get current date/time in ISO 8601 format
std::string Ticket::getCurrentDateISO() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// Base64 encode implementation
std::string Ticket::base64Encode(const std::string& input) {
    std::string output;
    int val = 0;
    int valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        output.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    // Add padding
    while (output.size() % 4) {
        output.push_back('=');
    }
    
    return output;
}

// Base64 decode implementation
std::string Ticket::base64Decode(const std::string& input) {
    std::string output;
    std::vector<int> T(256, -1);
    
    // Build decoding table
    for (int i = 0; i < 64; i++) {
        T[base64_chars[i]] = i;
    }
    
    int val = 0;
    int valb = -8;
    
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            output.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return output;
}

// JSON serialization (for nlohmann::json)
void to_json(json& j, const Ticket& t) {
    j = json{
        {"ticketId", t.ticketId_},
        {"creationDate", t.creationDate_},
        {"validityDays", t.validityDays_},
        {"lineNumber", t.lineNumber_}
    };
}

// JSON deserialization (for nlohmann::json)
void from_json(const json& j, Ticket& t) {
    j.at("ticketId").get_to(t.ticketId_);
    j.at("creationDate").get_to(t.creationDate_);
    j.at("validityDays").get_to(t.validityDays_);
    j.at("lineNumber").get_to(t.lineNumber_);
}
