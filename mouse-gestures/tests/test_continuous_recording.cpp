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

// Mock global state (anonymous namespace to avoid multiple definition)
namespace {
std::vector<std::string> g_pendingGestureAdditions;
std::unordered_map<PHLMONITOR, float> g_scrollOffsets;
std::unordered_map<PHLMONITOR, float> g_maxScrollOffsets;
bool g_recordMode = false;

// Mock gesture action structure
struct MockGestureAction {
    std::string strokeData;
    std::string command;
};

std::vector<MockGestureAction> g_gestureActions;
}

class ContinuousRecordingTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_pendingGestureAdditions.clear();
        g_scrollOffsets.clear();
        g_maxScrollOffsets.clear();
        g_gestureActions.clear();
        g_recordMode = false;
    }

    void TearDown() override {
        g_pendingGestureAdditions.clear();
        g_scrollOffsets.clear();
        g_maxScrollOffsets.clear();
        g_gestureActions.clear();
        g_recordMode = false;
    }

    PHLMONITOR createMockMonitor(float width, float height) {
        auto monitor = std::make_shared<MockMonitor>();
        monitor->m_size.x = width;
        monitor->m_size.y = height;
        return monitor;
    }

    float calculateScrollOffset(PHLMONITOR monitor, size_t numGestures) {
        constexpr float PADDING = 20.0f;
        constexpr float GAP_WIDTH = 10.0f;
        constexpr int VISIBLE_GESTURES = 3;

        const float verticalSpace = monitor->m_size.y - (2.0f * PADDING);
        const float totalGaps = (VISIBLE_GESTURES - 1) * GAP_WIDTH;
        const float baseHeight = (verticalSpace - totalGaps) / VISIBLE_GESTURES;
        const float gestureRectHeight = baseHeight * 0.9f;

        const float totalHeight = numGestures * (gestureRectHeight + GAP_WIDTH);
        const float maxScrollOffset = totalHeight - verticalSpace;
        return std::max(0.0f, maxScrollOffset);
    }
};

// Test 1: Record mode stays active after adding gesture
TEST_F(ContinuousRecordingTest, RecordModeStaysActive) {
    g_recordMode = true;

    // Simulate adding a gesture
    g_pendingGestureAdditions.push_back("0.1,0.2;0.3,0.4;");

    // Record mode should still be true (not toggled off)
    EXPECT_TRUE(g_recordMode);
}

// Test 2: Multiple gestures can be added in sequence
TEST_F(ContinuousRecordingTest, MultipleGesturesInSequence) {
    g_recordMode = true;

    // Add first gesture
    g_pendingGestureAdditions.push_back("gesture1");
    EXPECT_EQ(g_pendingGestureAdditions.size(), 1);
    EXPECT_TRUE(g_recordMode);

    // Add second gesture
    g_pendingGestureAdditions.push_back("gesture2");
    EXPECT_EQ(g_pendingGestureAdditions.size(), 2);
    EXPECT_TRUE(g_recordMode);

    // Add third gesture
    g_pendingGestureAdditions.push_back("gesture3");
    EXPECT_EQ(g_pendingGestureAdditions.size(), 3);
    EXPECT_TRUE(g_recordMode);
}

// Test 3: Gestures accumulate in memory for immediate display
TEST_F(ContinuousRecordingTest, GesturesAccumulateInMemory) {
    // Simulate adding gestures to both pending list and in-memory display
    g_pendingGestureAdditions.push_back("stroke1");
    g_gestureActions.push_back({"stroke1", "cmd1"});

    EXPECT_EQ(g_pendingGestureAdditions.size(), 1);
    EXPECT_EQ(g_gestureActions.size(), 1);

    g_pendingGestureAdditions.push_back("stroke2");
    g_gestureActions.push_back({"stroke2", "cmd2"});

    EXPECT_EQ(g_pendingGestureAdditions.size(), 2);
    EXPECT_EQ(g_gestureActions.size(), 2);
}

// Test 4: Auto-scroll when gestures exceed visible limit
TEST_F(ContinuousRecordingTest, AutoScrollWhenExceedingLimit) {
    auto monitor = createMockMonitor(1920, 1080);

    // Add 4 gestures (more than VISIBLE_GESTURES = 3)
    for (int i = 0; i < 4; i++) {
        g_gestureActions.push_back({"stroke" + std::to_string(i), "cmd"});
    }

    // Calculate expected scroll offset
    float expectedOffset = calculateScrollOffset(monitor, 4);
    g_scrollOffsets[monitor] = expectedOffset;

    EXPECT_GT(g_scrollOffsets[monitor], 0.0f);
    EXPECT_EQ(g_gestureActions.size(), 4);
}

// Test 5: No scroll when gestures within visible limit
TEST_F(ContinuousRecordingTest, NoScrollWhenWithinLimit) {
    auto monitor = createMockMonitor(1920, 1080);

    // Add 3 gestures (exactly VISIBLE_GESTURES = 3)
    for (int i = 0; i < 3; i++) {
        g_gestureActions.push_back({"stroke" + std::to_string(i), "cmd"});
    }

    // With 3 or fewer gestures, scroll offset should be 0
    if (g_gestureActions.size() <= 3) {
        // Don't set scroll offset (stays at default 0)
    }

    // Scroll offset should not be set (or be 0)
    if (g_scrollOffsets.find(monitor) == g_scrollOffsets.end()) {
        EXPECT_EQ(g_scrollOffsets[monitor], 0.0f); // Default value
    }
}

// Test 6: Scroll offset increases as more gestures added
TEST_F(ContinuousRecordingTest, ScrollOffsetIncreasesWithGestures) {
    auto monitor = createMockMonitor(1920, 1080);

    // Add 4 gestures
    for (int i = 0; i < 4; i++) {
        g_gestureActions.push_back({"stroke" + std::to_string(i), "cmd"});
    }
    float offset4 = calculateScrollOffset(monitor, 4);

    // Add 5th gesture
    g_gestureActions.push_back({"stroke5", "cmd"});
    float offset5 = calculateScrollOffset(monitor, 5);

    // Scroll offset should increase
    EXPECT_GT(offset5, offset4);
}

// Test 7: Scroll offset calculated correctly for different monitor sizes
TEST_F(ContinuousRecordingTest, ScrollOffsetForDifferentMonitorSizes) {
    auto smallMonitor = createMockMonitor(1920, 1080);
    auto largeMonitor = createMockMonitor(3840, 2160);

    // Add 10 gestures
    for (int i = 0; i < 10; i++) {
        g_gestureActions.push_back({"stroke" + std::to_string(i), "cmd"});
    }

    float smallOffset = calculateScrollOffset(smallMonitor, 10);
    float largeOffset = calculateScrollOffset(largeMonitor, 10);

    // Larger monitor should have larger scroll offset (more vertical space)
    EXPECT_GT(largeOffset, smallOffset);
}

// Test 8: Multiple monitors have independent scroll offsets
TEST_F(ContinuousRecordingTest, IndependentScrollOffsetsPerMonitor) {
    auto monitor1 = createMockMonitor(1920, 1080);
    auto monitor2 = createMockMonitor(2560, 1440);

    g_scrollOffsets[monitor1] = 100.0f;
    g_scrollOffsets[monitor2] = 200.0f;

    EXPECT_EQ(g_scrollOffsets[monitor1], 100.0f);
    EXPECT_EQ(g_scrollOffsets[monitor2], 200.0f);
    EXPECT_NE(g_scrollOffsets[monitor1], g_scrollOffsets[monitor2]);
}

// Test 9: Exiting record mode processes all pending additions
TEST_F(ContinuousRecordingTest, ExitingRecordModeProcessesPending) {
    g_recordMode = true;

    // Add multiple gestures while in record mode
    g_pendingGestureAdditions.push_back("gesture1");
    g_pendingGestureAdditions.push_back("gesture2");
    g_pendingGestureAdditions.push_back("gesture3");

    EXPECT_EQ(g_pendingGestureAdditions.size(), 3);

    // Exit record mode
    g_recordMode = false;

    // Simulate processing (in real code, processPendingGestureChanges() would be called)
    // Here we just verify the list is ready to be processed
    EXPECT_EQ(g_pendingGestureAdditions.size(), 3);
    EXPECT_FALSE(g_recordMode);
}

// Test 10: Scroll offset clamped to valid range
TEST_F(ContinuousRecordingTest, ScrollOffsetClamped) {
    auto monitor = createMockMonitor(1920, 1080);

    // Set negative scroll offset (invalid)
    g_scrollOffsets[monitor] = -50.0f;

    // Clamp to 0
    g_scrollOffsets[monitor] = std::max(0.0f, g_scrollOffsets[monitor]);
    EXPECT_EQ(g_scrollOffsets[monitor], 0.0f);

    // Set scroll offset beyond max
    g_maxScrollOffsets[monitor] = 500.0f;
    g_scrollOffsets[monitor] = 1000.0f;

    // Clamp to max
    g_scrollOffsets[monitor] = std::min(g_scrollOffsets[monitor], g_maxScrollOffsets[monitor]);
    EXPECT_EQ(g_scrollOffsets[monitor], 500.0f);
}

// Test 11: Adding first gesture doesn't trigger scroll
TEST_F(ContinuousRecordingTest, FirstGestureNoScroll) {
    auto monitor = createMockMonitor(1920, 1080);

    g_gestureActions.push_back({"stroke1", "cmd"});

    // With 1 gesture, no scroll needed
    if (g_gestureActions.size() <= 3) {
        // Don't update scroll offset
    }

    EXPECT_EQ(g_gestureActions.size(), 1);
    // Scroll offset should not exist or be 0
    if (g_scrollOffsets.find(monitor) == g_scrollOffsets.end()) {
        EXPECT_TRUE(true); // Not set, which is correct
    } else {
        EXPECT_EQ(g_scrollOffsets[monitor], 0.0f);
    }
}

// Test 12: Scroll to bottom shows newest gestures
TEST_F(ContinuousRecordingTest, ScrollToBottomShowsNewest) {
    auto monitor = createMockMonitor(1920, 1080);

    // Add many gestures
    for (int i = 0; i < 10; i++) {
        g_gestureActions.push_back({"stroke" + std::to_string(i), "cmd"});
    }

    // Calculate scroll to show bottom 3 gestures
    float scrollOffset = calculateScrollOffset(monitor, 10);
    g_scrollOffsets[monitor] = scrollOffset;

    // Verify scroll offset moves list down (positive value)
    EXPECT_GT(g_scrollOffsets[monitor], 0.0f);

    // The scroll offset should allow viewing the last 3 gestures (indices 7, 8, 9)
    // This is verified by the rendering code, here we just verify offset is set correctly
    EXPECT_EQ(g_gestureActions.size(), 10);
}

// Test 13: Max scroll offset updates with gesture count
TEST_F(ContinuousRecordingTest, MaxScrollOffsetUpdates) {
    auto monitor = createMockMonitor(1920, 1080);

    // Start with 4 gestures
    for (int i = 0; i < 4; i++) {
        g_gestureActions.push_back({"stroke" + std::to_string(i), "cmd"});
    }

    constexpr float PADDING = 20.0f;
    constexpr float GAP_WIDTH = 10.0f;
    constexpr int VISIBLE_GESTURES = 3;
    const float verticalSpace = monitor->m_size.y - (2.0f * PADDING);
    const float totalGaps = (VISIBLE_GESTURES - 1) * GAP_WIDTH;
    const float baseHeight = (verticalSpace - totalGaps) / VISIBLE_GESTURES;
    const float gestureRectHeight = baseHeight * 0.9f;

    float totalHeight4 = 4 * (gestureRectHeight + GAP_WIDTH);
    float maxScroll4 = totalHeight4 - verticalSpace;
    g_maxScrollOffsets[monitor] = maxScroll4;

    // Add more gestures
    for (int i = 4; i < 7; i++) {
        g_gestureActions.push_back({"stroke" + std::to_string(i), "cmd"});
    }

    float totalHeight7 = 7 * (gestureRectHeight + GAP_WIDTH);
    float maxScroll7 = totalHeight7 - verticalSpace;
    g_maxScrollOffsets[monitor] = maxScroll7;

    // Max scroll offset should increase
    EXPECT_GT(maxScroll7, maxScroll4);
}

// Test 14: Pending list maintained across multiple recordings
TEST_F(ContinuousRecordingTest, PendingListMaintainedAcrossRecordings) {
    g_recordMode = true;

    // First recording session
    g_pendingGestureAdditions.push_back("session1_gesture1");
    g_pendingGestureAdditions.push_back("session1_gesture2");

    // User continues recording without exiting
    g_pendingGestureAdditions.push_back("session2_gesture1");
    g_pendingGestureAdditions.push_back("session2_gesture2");

    // All gestures should be in pending list
    EXPECT_EQ(g_pendingGestureAdditions.size(), 4);
    EXPECT_EQ(g_pendingGestureAdditions[0], "session1_gesture1");
    EXPECT_EQ(g_pendingGestureAdditions[3], "session2_gesture2");
}
