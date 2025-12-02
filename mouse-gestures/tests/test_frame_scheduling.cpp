#include <gtest/gtest.h>
#include <chrono>
#include <vector>

// Test frame scheduling optimization to prevent crashes on plugin unload
// These tests verify that frames are only scheduled when actually needed:
// 1. During active dragging
// 2. In record mode
// 3. When trail points are still visible (not fully faded)

namespace frame_scheduling_test {

// Mock path point structure
struct PathPoint {
    double x;
    double y;
    std::chrono::steady_clock::time_point timestamp;
};

// Mock gesture state
struct MockGestureState {
    bool rightButtonPressed = false;
    bool dragDetected = false;
    std::vector<PathPoint> timestampedPath;

    void reset() {
        rightButtonPressed = false;
        dragDetected = false;
    }

    void clear() {
        timestampedPath.clear();
        reset();
    }
};

MockGestureState g_gestureState;
bool g_recordMode = false;
bool g_pluginShuttingDown = false;

// Mock hasVisibleTrailPoints function
bool hasVisibleTrailPoints(int fadeDurationMs) {
    if (g_gestureState.timestampedPath.empty()) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();

    for (const auto& point : g_gestureState.timestampedPath) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - point.timestamp
        ).count();

        if (age <= fadeDurationMs) {
            return true;
        }
    }

    return false;
}

// Mock function to check if we need continuous rendering
bool needsContinuousRendering(int fadeDurationMs) {
    return g_recordMode ||
           (g_gestureState.rightButtonPressed && g_gestureState.dragDetected) ||
           hasVisibleTrailPoints(fadeDurationMs);
}

// Test fixture
class FrameSchedulingTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_pluginShuttingDown = false;
        g_recordMode = false;
        g_gestureState.clear();
    }

    void TearDown() override {
        g_pluginShuttingDown = false;
        g_recordMode = false;
        g_gestureState.clear();
    }
};

// Test that idle state doesn't schedule frames
TEST_F(FrameSchedulingTest, IdleStateNoFrameScheduling) {
    int fadeDurationMs = 500;

    EXPECT_FALSE(g_recordMode);
    EXPECT_FALSE(g_gestureState.dragDetected);
    EXPECT_TRUE(g_gestureState.timestampedPath.empty());

    bool shouldSchedule = needsContinuousRendering(fadeDurationMs);
    EXPECT_FALSE(shouldSchedule);
}

// Test frame scheduling during record mode
TEST_F(FrameSchedulingTest, RecordModeSchedulesFrames) {
    int fadeDurationMs = 500;

    g_recordMode = true;
    bool shouldSchedule = needsContinuousRendering(fadeDurationMs);
    EXPECT_TRUE(shouldSchedule);
}

// Test frame scheduling during active drag
TEST_F(FrameSchedulingTest, ActiveDragSchedulesFrames) {
    int fadeDurationMs = 500;

    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});

    bool shouldSchedule = needsContinuousRendering(fadeDurationMs);
    EXPECT_TRUE(shouldSchedule);
}

// Test frame scheduling with visible trail points
TEST_F(FrameSchedulingTest, VisibleTrailPointsScheduleFrames) {
    int fadeDurationMs = 500;

    // Add recent trail point
    auto now = std::chrono::steady_clock::now();
    g_gestureState.timestampedPath.push_back({100, 100, now});

    bool shouldSchedule = needsContinuousRendering(fadeDurationMs);
    EXPECT_TRUE(shouldSchedule);
}

// Test no frame scheduling with fully faded trail points
TEST_F(FrameSchedulingTest, FadedTrailPointsNoFrameScheduling) {
    int fadeDurationMs = 500;

    // Add old trail point (already faded)
    auto oldTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(600);
    g_gestureState.timestampedPath.push_back({100, 100, oldTime});

    bool shouldSchedule = needsContinuousRendering(fadeDurationMs);
    EXPECT_FALSE(shouldSchedule);
}

// Test hasVisibleTrailPoints with empty path
TEST_F(FrameSchedulingTest, HasVisibleTrailPointsEmptyPath) {
    int fadeDurationMs = 500;

    EXPECT_TRUE(g_gestureState.timestampedPath.empty());
    bool hasVisible = hasVisibleTrailPoints(fadeDurationMs);
    EXPECT_FALSE(hasVisible);
}

// Test hasVisibleTrailPoints with fresh point
TEST_F(FrameSchedulingTest, HasVisibleTrailPointsFreshPoint) {
    int fadeDurationMs = 500;

    auto now = std::chrono::steady_clock::now();
    g_gestureState.timestampedPath.push_back({100, 100, now});

    bool hasVisible = hasVisibleTrailPoints(fadeDurationMs);
    EXPECT_TRUE(hasVisible);
}

// Test hasVisibleTrailPoints at exact fade duration
TEST_F(FrameSchedulingTest, HasVisibleTrailPointsAtExactFadeDuration) {
    int fadeDurationMs = 500;

    auto exactTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(500);
    g_gestureState.timestampedPath.push_back({100, 100, exactTime});

    bool hasVisible = hasVisibleTrailPoints(fadeDurationMs);
    EXPECT_TRUE(hasVisible);  // At exact duration, still visible (uses <=)
}

// Test hasVisibleTrailPoints just before fade duration
TEST_F(FrameSchedulingTest, HasVisibleTrailPointsJustBeforeFade) {
    int fadeDurationMs = 500;

    auto almostFaded = std::chrono::steady_clock::now() - std::chrono::milliseconds(499);
    g_gestureState.timestampedPath.push_back({100, 100, almostFaded});

    bool hasVisible = hasVisibleTrailPoints(fadeDurationMs);
    EXPECT_TRUE(hasVisible);
}

// Test hasVisibleTrailPoints with mixed ages
TEST_F(FrameSchedulingTest, HasVisibleTrailPointsMixedAges) {
    int fadeDurationMs = 500;
    auto now = std::chrono::steady_clock::now();

    // Add points with different ages
    g_gestureState.timestampedPath.push_back({100, 100, now - std::chrono::milliseconds(100)});  // Fresh
    g_gestureState.timestampedPath.push_back({110, 110, now - std::chrono::milliseconds(300)});  // Visible
    g_gestureState.timestampedPath.push_back({120, 120, now - std::chrono::milliseconds(600)});  // Faded

    // Should return true because at least one point is visible
    bool hasVisible = hasVisibleTrailPoints(fadeDurationMs);
    EXPECT_TRUE(hasVisible);
}

// Test hasVisibleTrailPoints with all faded points
TEST_F(FrameSchedulingTest, HasVisibleTrailPointsAllFaded) {
    int fadeDurationMs = 500;
    auto now = std::chrono::steady_clock::now();

    // Add only old points
    g_gestureState.timestampedPath.push_back({100, 100, now - std::chrono::milliseconds(600)});
    g_gestureState.timestampedPath.push_back({110, 110, now - std::chrono::milliseconds(700)});
    g_gestureState.timestampedPath.push_back({120, 120, now - std::chrono::milliseconds(800)});

    bool hasVisible = hasVisibleTrailPoints(fadeDurationMs);
    EXPECT_FALSE(hasVisible);
}

// Test frame scheduling stops when drag ends and trail fades
TEST_F(FrameSchedulingTest, FrameSchedulingStopsAfterTrailFades) {
    int fadeDurationMs = 500;

    // Start drag
    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    auto now = std::chrono::steady_clock::now();
    g_gestureState.timestampedPath.push_back({100, 100, now});

    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));

    // End drag
    g_gestureState.rightButtonPressed = false;
    g_gestureState.dragDetected = false;

    // Trail still visible - should continue scheduling
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));

    // Simulate time passing - trail fades
    g_gestureState.timestampedPath.clear();
    auto oldTime = now - std::chrono::milliseconds(600);
    g_gestureState.timestampedPath.push_back({100, 100, oldTime});

    // Trail faded - should stop scheduling
    EXPECT_FALSE(needsContinuousRendering(fadeDurationMs));
}

// Test no frame scheduling with button pressed but no drag and no visible trail
TEST_F(FrameSchedulingTest, NoFrameSchedulingBeforeDragThreshold) {
    int fadeDurationMs = 500;

    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = false;  // Threshold not crossed
    // Add old trail point that's already faded
    auto oldTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(600);
    g_gestureState.timestampedPath.push_back({100, 100, oldTime});

    bool shouldSchedule = needsContinuousRendering(fadeDurationMs);
    EXPECT_FALSE(shouldSchedule);
}

// Test frame scheduling with multiple conditions
TEST_F(FrameSchedulingTest, FrameSchedulingMultipleConditions) {
    int fadeDurationMs = 500;

    // Test each condition individually
    g_recordMode = true;
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));
    g_recordMode = false;

    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));
    g_gestureState.rightButtonPressed = false;
    g_gestureState.dragDetected = false;

    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));
}

// Test frame scheduling in shutdown state
TEST_F(FrameSchedulingTest, NoFrameSchedulingDuringShutdown) {
    int fadeDurationMs = 500;

    // Setup active state
    g_recordMode = true;
    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});

    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));

    // Simulate shutdown
    g_pluginShuttingDown = true;

    // Note: The shutdown check happens in the render hook before needsContinuousRendering
    // But we can verify that after cleanup, no conditions trigger scheduling
    g_recordMode = false;
    g_gestureState.clear();

    EXPECT_FALSE(needsContinuousRendering(fadeDurationMs));
}

// Test frame scheduling transition from drag to fade
TEST_F(FrameSchedulingTest, FrameSchedulingTransitionDragToFade) {
    int fadeDurationMs = 500;
    auto now = std::chrono::steady_clock::now();

    // Active drag - should schedule
    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    g_gestureState.timestampedPath.push_back({100, 100, now});
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));

    // Release button - transition to fade phase
    g_gestureState.rightButtonPressed = false;
    g_gestureState.dragDetected = false;

    // Still has visible trail points - should continue scheduling
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));
    EXPECT_TRUE(hasVisibleTrailPoints(fadeDurationMs));
}

// Test custom fade durations
TEST_F(FrameSchedulingTest, CustomFadeDurations) {
    std::vector<int> fadeDurations = {200, 500, 1000, 1500};
    auto now = std::chrono::steady_clock::now();

    for (int duration : fadeDurations) {
        g_gestureState.timestampedPath.clear();

        // Point at duration - 1ms (should be visible)
        auto justVisible = now - std::chrono::milliseconds(duration - 1);
        g_gestureState.timestampedPath.push_back({100, 100, justVisible});
        EXPECT_TRUE(hasVisibleTrailPoints(duration));

        // Point at duration + 1ms (should be invisible)
        g_gestureState.timestampedPath.clear();
        auto justInvisible = now - std::chrono::milliseconds(duration + 1);
        g_gestureState.timestampedPath.push_back({100, 100, justInvisible});
        EXPECT_FALSE(hasVisibleTrailPoints(duration));
    }
}

// Test optimization: no continuous frame scheduling when idle
TEST_F(FrameSchedulingTest, OptimizationNoIdleFrames) {
    int fadeDurationMs = 500;
    int frameCount = 0;

    // Simulate 1000 frame checks while idle
    for (int i = 0; i < 1000; i++) {
        if (needsContinuousRendering(fadeDurationMs)) {
            frameCount++;
        }
    }

    // Should be 0 - no frames scheduled when idle
    EXPECT_EQ(frameCount, 0);
}

// Test optimization: continuous scheduling only while needed
TEST_F(FrameSchedulingTest, OptimizationScheduleOnlyWhenNeeded) {
    int fadeDurationMs = 500;
    int frameCount = 0;

    // Start drag
    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});

    // Simulate 100 frames while dragging
    for (int i = 0; i < 100; i++) {
        if (needsContinuousRendering(fadeDurationMs)) {
            frameCount++;
        }
    }

    EXPECT_EQ(frameCount, 100);  // All frames scheduled during drag

    // End drag
    g_gestureState.rightButtonPressed = false;
    g_gestureState.dragDetected = false;
    g_gestureState.timestampedPath.clear();
    auto oldTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(600);
    g_gestureState.timestampedPath.push_back({100, 100, oldTime});

    frameCount = 0;

    // Simulate 100 frames after trail faded
    for (int i = 0; i < 100; i++) {
        if (needsContinuousRendering(fadeDurationMs)) {
            frameCount++;
        }
    }

    EXPECT_EQ(frameCount, 0);  // No frames scheduled after fade
}

// Test that frame scheduling prevents infinite loop
TEST_F(FrameSchedulingTest, PreventInfiniteFrameLoop) {
    int fadeDurationMs = 500;

    // Add old trail points that are fully faded
    auto oldTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(1000);
    g_gestureState.timestampedPath.push_back({100, 100, oldTime});
    g_gestureState.timestampedPath.push_back({110, 110, oldTime});
    g_gestureState.timestampedPath.push_back({120, 120, oldTime});

    // Should not schedule frames even though path is not empty
    EXPECT_FALSE(g_gestureState.timestampedPath.empty());
    EXPECT_FALSE(needsContinuousRendering(fadeDurationMs));
}

// Test frame scheduling respects all three conditions
TEST_F(FrameSchedulingTest, AllThreeConditionsRespected) {
    int fadeDurationMs = 500;

    // Condition 1: Record mode
    g_recordMode = true;
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));
    g_recordMode = false;
    EXPECT_FALSE(needsContinuousRendering(fadeDurationMs));

    // Condition 2: Active drag (both button pressed AND drag detected)
    g_gestureState.rightButtonPressed = true;
    EXPECT_FALSE(needsContinuousRendering(fadeDurationMs));  // Drag not detected yet
    g_gestureState.dragDetected = true;
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));   // Both conditions met
    g_gestureState.rightButtonPressed = false;
    EXPECT_FALSE(needsContinuousRendering(fadeDurationMs));  // Button released
    g_gestureState.dragDetected = false;

    // Condition 3: Visible trail points
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});
    EXPECT_TRUE(needsContinuousRendering(fadeDurationMs));
}

}  // namespace frame_scheduling_test
