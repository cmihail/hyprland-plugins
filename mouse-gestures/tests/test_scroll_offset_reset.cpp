#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <unordered_map>

// Mock types for testing
struct MockMonitor {
    struct Size {
        float x;
        float y;
    } m_size;
};

using PHLMONITOR = std::shared_ptr<MockMonitor>;

// Mock global state
namespace {
std::unordered_map<PHLMONITOR, float> g_scrollOffsets;
std::unordered_map<PHLMONITOR, float> g_maxScrollOffsets;
bool g_recordMode = false;
}

class ScrollOffsetResetTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_scrollOffsets.clear();
        g_maxScrollOffsets.clear();
        g_recordMode = false;
    }

    void TearDown() override {
        g_scrollOffsets.clear();
        g_maxScrollOffsets.clear();
        g_recordMode = false;
    }

    PHLMONITOR createMockMonitor(float width, float height) {
        auto monitor = std::make_shared<MockMonitor>();
        monitor->m_size.x = width;
        monitor->m_size.y = height;
        return monitor;
    }

    // Simulate the mouseGesturesDispatch function behavior
    void simulateRecordModeToggle() {
        bool wasRecordMode = g_recordMode;
        g_recordMode = !g_recordMode;

        // If we're entering record mode, reset scroll offsets
        if (!wasRecordMode && g_recordMode) {
            g_scrollOffsets.clear();
            g_maxScrollOffsets.clear();
        }
    }
};

// Test 1: Scroll offset is reset when entering record mode
TEST_F(ScrollOffsetResetTest, ScrollOffsetResetOnEnteringRecordMode) {
    auto monitor1 = createMockMonitor(1920, 1080);
    auto monitor2 = createMockMonitor(2560, 1440);

    // Set scroll offsets (simulating previous record mode session)
    g_scrollOffsets[monitor1] = 150.0f;
    g_scrollOffsets[monitor2] = 200.0f;
    g_maxScrollOffsets[monitor1] = 300.0f;
    g_maxScrollOffsets[monitor2] = 400.0f;

    EXPECT_EQ(g_scrollOffsets[monitor1], 150.0f);
    EXPECT_EQ(g_scrollOffsets[monitor2], 200.0f);

    // Enter record mode
    simulateRecordModeToggle();

    // Scroll offsets should be cleared
    EXPECT_TRUE(g_scrollOffsets.empty());
    EXPECT_TRUE(g_maxScrollOffsets.empty());
    EXPECT_TRUE(g_recordMode);
}

// Test 2: Scroll offset NOT reset when exiting record mode
TEST_F(ScrollOffsetResetTest, ScrollOffsetNotResetOnExitingRecordMode) {
    auto monitor = createMockMonitor(1920, 1080);

    // Enter record mode first
    g_recordMode = true;

    // Set scroll offsets during record mode
    g_scrollOffsets[monitor] = 100.0f;
    g_maxScrollOffsets[monitor] = 200.0f;

    // Exit record mode
    simulateRecordModeToggle();

    // Scroll offsets should still exist (not cleared on exit)
    EXPECT_EQ(g_scrollOffsets[monitor], 100.0f);
    EXPECT_EQ(g_maxScrollOffsets[monitor], 200.0f);
    EXPECT_FALSE(g_recordMode);
}

// Test 3: Multiple monitors - all scroll offsets reset on enter
TEST_F(ScrollOffsetResetTest, MultipleMonitorsScrollOffsetReset) {
    auto monitor1 = createMockMonitor(1920, 1080);
    auto monitor2 = createMockMonitor(2560, 1440);
    auto monitor3 = createMockMonitor(3840, 2160);

    // Set scroll offsets for all monitors
    g_scrollOffsets[monitor1] = 50.0f;
    g_scrollOffsets[monitor2] = 100.0f;
    g_scrollOffsets[monitor3] = 150.0f;
    g_maxScrollOffsets[monitor1] = 100.0f;
    g_maxScrollOffsets[monitor2] = 200.0f;
    g_maxScrollOffsets[monitor3] = 300.0f;

    EXPECT_EQ(g_scrollOffsets.size(), 3);
    EXPECT_EQ(g_maxScrollOffsets.size(), 3);

    // Enter record mode
    simulateRecordModeToggle();

    // All scroll offsets should be cleared
    EXPECT_TRUE(g_scrollOffsets.empty());
    EXPECT_TRUE(g_maxScrollOffsets.empty());
}

// Test 4: Re-entering record mode resets again
TEST_F(ScrollOffsetResetTest, ReEnteringRecordModeResetsAgain) {
    auto monitor = createMockMonitor(1920, 1080);

    // First record mode session
    simulateRecordModeToggle();  // Enter
    g_scrollOffsets[monitor] = 50.0f;
    simulateRecordModeToggle();  // Exit

    EXPECT_EQ(g_scrollOffsets[monitor], 50.0f);

    // Second record mode session
    simulateRecordModeToggle();  // Enter again

    // Scroll offset should be reset
    EXPECT_TRUE(g_scrollOffsets.empty());
}

// Test 5: Starting from clean state
TEST_F(ScrollOffsetResetTest, CleanStateEnterRecordMode) {
    // No scroll offsets set
    EXPECT_TRUE(g_scrollOffsets.empty());
    EXPECT_TRUE(g_maxScrollOffsets.empty());

    // Enter record mode
    simulateRecordModeToggle();

    // Should still be empty (no-op clear)
    EXPECT_TRUE(g_scrollOffsets.empty());
    EXPECT_TRUE(g_maxScrollOffsets.empty());
    EXPECT_TRUE(g_recordMode);
}
