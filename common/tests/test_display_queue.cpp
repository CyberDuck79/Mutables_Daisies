// Test for Display message queue functionality
// Note: We test the logic without actual hardware rendering.

#include "test_framework.h"
#include <cstring>
#include <cstdint>

// ============================================================================
// Mock System time for testing
// ============================================================================

namespace test_display {

static uint32_t mock_time_ms = 0;

uint32_t GetMockTime() {
    return mock_time_ms;
}

void SetMockTime(uint32_t ms) {
    mock_time_ms = ms;
}

void AdvanceTime(uint32_t ms) {
    mock_time_ms += ms;
}

// ============================================================================
// Simplified Display message queue for testing without hardware
// ============================================================================

class TestDisplayMessageQueue {
public:
    TestDisplayMessageQueue() : pending_message_expire_(0) {
        pending_message_.title[0] = '\0';
        pending_message_.message[0] = '\0';
        pending_message_.success = true;
    }
    
    void QueueMessage(const char* title, const char* message, 
                     uint32_t duration_ms = 1500, bool success = true) {
        strncpy(pending_message_.title, title, sizeof(pending_message_.title) - 1);
        pending_message_.title[sizeof(pending_message_.title) - 1] = '\0';
        strncpy(pending_message_.message, message, sizeof(pending_message_.message) - 1);
        pending_message_.message[sizeof(pending_message_.message) - 1] = '\0';
        pending_message_.success = success;
        pending_message_expire_ = GetMockTime() + duration_ms;
    }
    
    bool HasPendingMessage() const {
        return pending_message_expire_ > 0 && 
               GetMockTime() < pending_message_expire_;
    }
    
    // Returns true if message was displayed
    bool UpdatePendingMessages() {
        uint32_t now = GetMockTime();
        if (pending_message_expire_ > 0 && now < pending_message_expire_) {
            return true;  // Would render message
        }
        
        // Clear expired message
        if (pending_message_expire_ > 0 && now >= pending_message_expire_) {
            pending_message_expire_ = 0;
            pending_message_.title[0] = '\0';
            pending_message_.message[0] = '\0';
        }
        
        return false;
    }
    
    // Test helpers
    const char* GetPendingTitle() const { return pending_message_.title; }
    const char* GetPendingMessage() const { return pending_message_.message; }
    bool GetPendingSuccess() const { return pending_message_.success; }
    
private:
    struct PendingMessage {
        char title[16];
        char message[32];
        bool success;
    };
    PendingMessage pending_message_;
    uint32_t pending_message_expire_;
};

} // namespace test_display

using namespace test_display;

// ============================================================================
// Display Message Queue Tests
// ============================================================================

TEST(DisplayMessageQueue, InitiallyNoPendingMessage) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    EXPECT_FALSE(display.HasPendingMessage());
}

TEST(DisplayMessageQueue, QueueMessageCreatesPending) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    display.QueueMessage("Test", "Hello World", 1000);
    
    EXPECT_TRUE(display.HasPendingMessage());
    EXPECT_STREQ(display.GetPendingTitle(), "Test");
    EXPECT_STREQ(display.GetPendingMessage(), "Hello World");
}

TEST(DisplayMessageQueue, MessageExpiresAfterDuration) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    display.QueueMessage("Test", "Message", 1000);
    
    // At t=500, still showing
    SetMockTime(500);
    EXPECT_TRUE(display.HasPendingMessage());
    
    // At t=999, still showing
    SetMockTime(999);
    EXPECT_TRUE(display.HasPendingMessage());
    
    // At t=1000, expired
    SetMockTime(1000);
    EXPECT_FALSE(display.HasPendingMessage());
}

TEST(DisplayMessageQueue, UpdateClearsExpiredMessage) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    display.QueueMessage("Test", "Message", 500);
    
    // Before expiry, update returns true
    SetMockTime(400);
    EXPECT_TRUE(display.UpdatePendingMessages());
    
    // After expiry, update returns false and clears
    SetMockTime(600);
    EXPECT_FALSE(display.UpdatePendingMessages());
    
    // Title should be cleared
    EXPECT_STREQ(display.GetPendingTitle(), "");
}

TEST(DisplayMessageQueue, NewMessageOverwritesOld) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    display.QueueMessage("First", "First Message", 1000);
    
    SetMockTime(200);
    display.QueueMessage("Second", "Second Message", 1000);
    
    EXPECT_STREQ(display.GetPendingTitle(), "Second");
    EXPECT_STREQ(display.GetPendingMessage(), "Second Message");
    
    // New message should expire at t=1200
    SetMockTime(1100);
    EXPECT_TRUE(display.HasPendingMessage());
    
    SetMockTime(1200);
    EXPECT_FALSE(display.HasPendingMessage());
}

TEST(DisplayMessageQueue, SuccessFlagIsStored) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    display.QueueMessage("Error", "Failed!", 1000, false);
    
    EXPECT_FALSE(display.GetPendingSuccess());
    
    display.QueueMessage("OK", "Saved!", 1000, true);
    
    EXPECT_TRUE(display.GetPendingSuccess());
}

TEST(DisplayMessageQueue, LongTitleIsTruncated) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    display.QueueMessage("This Is A Very Long Title That Should Be Truncated", "Msg", 1000);
    
    // Title buffer is 16 chars, so it should be truncated
    EXPECT_EQ(strlen(display.GetPendingTitle()), 15);
}

TEST(DisplayMessageQueue, LongMessageIsTruncated) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    display.QueueMessage("Title", 
        "This is a very long message that definitely exceeds the buffer size", 1000);
    
    // Message buffer is 32 chars, so it should be truncated
    EXPECT_EQ(strlen(display.GetPendingMessage()), 31);
}

TEST(DisplayMessageQueue, ZeroDurationExpiresImmediately) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    display.QueueMessage("Test", "Instant", 0);
    
    // Should already be expired
    EXPECT_FALSE(display.HasPendingMessage());
}

TEST(DisplayMessageQueue, DefaultDuration) {
    SetMockTime(0);
    TestDisplayMessageQueue display;
    
    // Use default duration (1500ms)
    display.QueueMessage("Test", "Default");
    
    SetMockTime(1400);
    EXPECT_TRUE(display.HasPendingMessage());
    
    SetMockTime(1500);
    EXPECT_FALSE(display.HasPendingMessage());
}
