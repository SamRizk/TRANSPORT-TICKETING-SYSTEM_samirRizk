// tests/unit/test_ticket.cpp
// Unit tests for Ticket class using Google Test framework

#include <gtest/gtest.h>
#include "ticket.h"
#include <thread>
#include <chrono>

/**
 * Test fixture for Ticket class
 */
class TicketTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code before each test
    }

    void TearDown() override {
        // Cleanup code after each test
    }
};

// ============================================================================
// CONSTRUCTION TESTS
// ============================================================================

TEST_F(TicketTest, DefaultConstructor) {
    Ticket ticket;
    
    EXPECT_EQ(ticket.getId(), "");
    EXPECT_EQ(ticket.getValidityDays(), 0);
    EXPECT_EQ(ticket.getLineNumber(), 0);
    EXPECT_FALSE(ticket.getCreationDate().empty());
}

TEST_F(TicketTest, ParameterizedConstructor) {
    Ticket ticket("TKT-001", 7, 1);
    
    EXPECT_EQ(ticket.getId(), "TKT-001");
    EXPECT_EQ(ticket.getValidityDays(), 7);
    EXPECT_EQ(ticket.getLineNumber(), 1);
    EXPECT_FALSE(ticket.getCreationDate().empty());
}

// ============================================================================
// VALIDATION TESTS
// ============================================================================

TEST_F(TicketTest, ValidTicket) {
    Ticket ticket("TKT-002", 7, 1);
    
    EXPECT_TRUE(ticket.isValid());
    EXPECT_FALSE(ticket.isExpired());
}

TEST_F(TicketTest, ExpiredTicketZeroDays) {
    Ticket ticket("TKT-003", 0, 1);
    
    // Wait a moment to ensure expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(ticket.isExpired());
    EXPECT_FALSE(ticket.isValid());
}

TEST_F(TicketTest, InvalidEmptyId) {
    Ticket ticket("", 7, 1);
    
    EXPECT_FALSE(ticket.isValid());
}

TEST_F(TicketTest, InvalidNegativeValidity) {
    Ticket ticket("TKT-004", -1, 1);
    
    EXPECT_FALSE(ticket.isValid());
}

TEST_F(TicketTest, ValidLongDuration) {
    Ticket ticket("TKT-005", 365, 1);
    
    EXPECT_TRUE(ticket.isValid());
    EXPECT_FALSE(ticket.isExpired());
}

// ============================================================================
// JSON SERIALIZATION TESTS
// ============================================================================

TEST_F(TicketTest, JsonSerialization) {
    Ticket original("TKT-006", 7, 1);
    
    std::string json = original.toJson();
    
    // Check JSON contains required fields
    EXPECT_NE(json.find("ticketId"), std::string::npos);
    EXPECT_NE(json.find("creationDate"), std::string::npos);
    EXPECT_NE(json.find("validityDays"), std::string::npos);
    EXPECT_NE(json.find("lineNumber"), std::string::npos);
    EXPECT_NE(json.find("TKT-006"), std::string::npos);
}

TEST_F(TicketTest, JsonDeserialization) {
    std::string jsonStr = R"({
        "ticketId": "TKT-007",
        "creationDate": "2024-01-07T10:30:00",
        "validityDays": 7,
        "lineNumber": 1
    })";
    
    Ticket ticket = Ticket::fromJson(jsonStr);
    
    EXPECT_EQ(ticket.getId(), "TKT-007");
    EXPECT_EQ(ticket.getCreationDate(), "2024-01-07T10:30:00");
    EXPECT_EQ(ticket.getValidityDays(), 7);
    EXPECT_EQ(ticket.getLineNumber(), 1);
}

TEST_F(TicketTest, JsonRoundTrip) {
    Ticket original("TKT-008", 30, 5);
    
    std::string json = original.toJson();
    Ticket decoded = Ticket::fromJson(json);
    
    EXPECT_EQ(decoded.getId(), original.getId());
    EXPECT_EQ(decoded.getCreationDate(), original.getCreationDate());
    EXPECT_EQ(decoded.getValidityDays(), original.getValidityDays());
    EXPECT_EQ(decoded.getLineNumber(), original.getLineNumber());
}

TEST_F(TicketTest, InvalidJsonHandling) {
    std::string invalidJson = "this is not valid JSON";
    
    EXPECT_THROW(Ticket::fromJson(invalidJson), std::exception);
}

// ============================================================================
// BASE64 ENCODING/DECODING TESTS (Critical for requirements)
// ============================================================================

TEST_F(TicketTest, Base64Encoding) {
    Ticket ticket("TKT-009", 7, 1);
    
    std::string base64 = ticket.toBase64();
    
    EXPECT_FALSE(base64.empty());
    EXPECT_GT(base64.length(), 50);
    
    // Base64 should only contain valid characters (A-Z, a-z, 0-9, +, /, =)
    for (char c : base64) {
        EXPECT_TRUE(std::isalnum(c) || c == '+' || c == '/' || c == '=');
    }
}

TEST_F(TicketTest, Base64Decoding) {
    Ticket original("TKT-010", 7, 1);
    
    std::string base64 = original.toBase64();
    Ticket decoded = Ticket::fromBase64(base64);
    
    EXPECT_EQ(decoded.getId(), original.getId());
    EXPECT_EQ(decoded.getValidityDays(), original.getValidityDays());
    EXPECT_EQ(decoded.getLineNumber(), original.getLineNumber());
    EXPECT_EQ(decoded.getCreationDate(), original.getCreationDate());
}

TEST_F(TicketTest, Base64RoundTrip) {
    Ticket original("TKT-011", 14, 3);
    
    std::string base64_1 = original.toBase64();
    Ticket decoded = Ticket::fromBase64(base64_1);
    std::string base64_2 = decoded.toBase64();
    
    EXPECT_EQ(base64_1, base64_2);
}

TEST_F(TicketTest, Base64WithSpecialCharacters) {
    Ticket ticket("TKT-012-SPECIAL", 7, 99);
    
    std::string base64 = ticket.toBase64();
    Ticket decoded = Ticket::fromBase64(base64);
    
    EXPECT_EQ(decoded.getId(), "TKT-012-SPECIAL");
    EXPECT_EQ(decoded.getLineNumber(), 99);
}

TEST_F(TicketTest, InvalidBase64Handling) {
    std::string invalidBase64 = "!!!invalid base64!!!";
    
    EXPECT_THROW(Ticket::fromBase64(invalidBase64), std::exception);
}

// ============================================================================
// DATE PARSING AND EXPIRY TESTS
// ============================================================================

TEST_F(TicketTest, CreationDateFormat) {
    Ticket ticket("TKT-013", 7, 1);
    
    std::string date = ticket.getCreationDate();
    
    // Should be in ISO 8601 format: YYYY-MM-DDTHH:MM:SS
    EXPECT_EQ(date.length(), 19);
    EXPECT_EQ(date[4], '-');
    EXPECT_EQ(date[7], '-');
    EXPECT_EQ(date[10], 'T');
    EXPECT_EQ(date[13], ':');
    EXPECT_EQ(date[16], ':');
}

TEST_F(TicketTest, ExpiryCalculation) {
    Ticket ticket("TKT-014", 1, 1);
    
    // Ticket should not be expired immediately
    EXPECT_FALSE(ticket.isExpired());
    
    // Create ticket with past date (manipulate via JSON)
    std::string pastJson = R"({
        "ticketId": "TKT-015",
        "creationDate": "2020-01-01T00:00:00",
        "validityDays": 1,
        "lineNumber": 1
    })";
    
    Ticket pastTicket = Ticket::fromJson(pastJson);
    EXPECT_TRUE(pastTicket.isExpired());
}

// ============================================================================
// SETTERS TESTS
// ============================================================================

TEST_F(TicketTest, SettersWork) {
    Ticket ticket;
    
    ticket.setId("TKT-016");
    ticket.setValidityDays(30);
    ticket.setLineNumber(5);
    ticket.setCreationDate("2024-01-07T12:00:00");
    
    EXPECT_EQ(ticket.getId(), "TKT-016");
    EXPECT_EQ(ticket.getValidityDays(), 30);
    EXPECT_EQ(ticket.getLineNumber(), 5);
    EXPECT_EQ(ticket.getCreationDate(), "2024-01-07T12:00:00");
}

// ============================================================================
// EDGE CASES
// ============================================================================

TEST_F(TicketTest, VeryLargeLineNumber) {
    Ticket ticket("TKT-017", 7, 9999);
    
    std::string base64 = ticket.toBase64();
    Ticket decoded = Ticket::fromBase64(base64);
    
    EXPECT_EQ(decoded.getLineNumber(), 9999);
}

TEST_F(TicketTest, VeryLongValidityPeriod) {
    Ticket ticket("TKT-018", 3650, 1); // 10 years
    
    EXPECT_TRUE(ticket.isValid());
    EXPECT_FALSE(ticket.isExpired());
}

TEST_F(TicketTest, MultipleTicketsIndependent) {
    Ticket ticket1("TKT-019-A", 7, 1);
    Ticket ticket2("TKT-019-B", 14, 2);
    Ticket ticket3("TKT-019-C", 30, 3);
    
    EXPECT_NE(ticket1.getId(), ticket2.getId());
    EXPECT_NE(ticket2.getId(), ticket3.getId());
    
    EXPECT_EQ(ticket1.getValidityDays(), 7);
    EXPECT_EQ(ticket2.getValidityDays(), 14);
    EXPECT_EQ(ticket3.getValidityDays(), 30);
}

TEST_F(TicketTest, CopySemantics) {
    Ticket original("TKT-020", 7, 1);
    Ticket copy = original;
    
    EXPECT_EQ(copy.getId(), original.getId());
    EXPECT_EQ(copy.getValidityDays(), original.getValidityDays());
    EXPECT_EQ(copy.getLineNumber(), original.getLineNumber());
    EXPECT_EQ(copy.getCreationDate(), original.getCreationDate());
    
    // Modify copy shouldn't affect original
    copy.setId("TKT-021");
    EXPECT_NE(copy.getId(), original.getId());
}

// ============================================================================
// PERSISTENCE TESTS
// ============================================================================

TEST_F(TicketTest, PersistenceThroughBase64) {
    Ticket original("TKT-022", 7, 1);
    
    // Simulate saving to storage
    std::string stored = original.toBase64();
    
    // Simulate loading from storage
    Ticket loaded = Ticket::fromBase64(stored);
    
    // Should be identical
    EXPECT_EQ(loaded.getId(), original.getId());
    EXPECT_EQ(loaded.isValid(), original.isValid());
    EXPECT_EQ(loaded.isExpired(), original.isExpired());
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
