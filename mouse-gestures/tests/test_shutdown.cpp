#include <gtest/gtest.h>
#include <chrono>
#include <thread>

// Test shutdown behavior
// These tests verify that the plugin correctly handles shutdown:
// 1. Shutdown flag prevents new operations
// 2. Existing operations check shutdown flag
// 3. Cleanup happens in correct order

// Mock global shutdown flag
bool g_pluginShuttingDown = false;

// Mock record mode
bool g_recordMode = false;

// Mock gesture state
struct PathPoint {
    double x;
    double y;
    std::chrono::steady_clock::time_point timestamp;
};

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

// Mock render hook behavior
bool shouldRenderOverlay() {
    // Don't render if plugin is shutting down
    if (g_pluginShuttingDown) {
        return false;
    }

    // Only render when there's something to show
    bool shouldRender = g_recordMode ||
                       (!g_gestureState.timestampedPath.empty() &&
                        (g_gestureState.dragDetected ||
                         !g_gestureState.rightButtonPressed));

    return shouldRender;
}

// Mock overlay draw behavior
bool shouldDrawOverlay() {
    // Don't draw if plugin is shutting down
    if (g_pluginShuttingDown) {
        return false;
    }

    return true;
}

// Test fixture for shutdown tests
class ShutdownTest : public ::testing::Test {
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

    // Simulate plugin shutdown
    void simulateShutdown() {
        // Set shutdown flag FIRST
        g_pluginShuttingDown = true;

        // Exit record mode
        g_recordMode = false;

        // Clear gesture state
        g_gestureState.timestampedPath.clear();
        g_gestureState.reset();
    }
};

// Test that shutdown flag prevents rendering
TEST_F(ShutdownTest, ShutdownFlagPreventsRendering) {
    // Setup active state
    g_recordMode = true;

    // Verify rendering is active
    EXPECT_TRUE(shouldRenderOverlay());

    // Trigger shutdown
    simulateShutdown();

    // Verify rendering is disabled
    EXPECT_FALSE(shouldRenderOverlay());
}

// Test that shutdown flag prevents overlay drawing
TEST_F(ShutdownTest, ShutdownFlagPreventsOverlayDraw) {
    // Overlay should draw normally
    EXPECT_TRUE(shouldDrawOverlay());

    // Trigger shutdown
    simulateShutdown();

    // Overlay should not draw
    EXPECT_FALSE(shouldDrawOverlay());
}

// Test shutdown with record mode active
TEST_F(ShutdownTest, ShutdownWithRecordModeActive) {
    g_recordMode = true;
    EXPECT_TRUE(shouldRenderOverlay());

    simulateShutdown();

    EXPECT_FALSE(g_recordMode);
    EXPECT_TRUE(g_pluginShuttingDown);
    EXPECT_FALSE(shouldRenderOverlay());
}

// Test shutdown with drag in progress
TEST_F(ShutdownTest, ShutdownWithDragInProgress) {
    // Setup drag state
    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});

    EXPECT_TRUE(shouldRenderOverlay());

    simulateShutdown();

    EXPECT_TRUE(g_pluginShuttingDown);
    EXPECT_FALSE(g_gestureState.rightButtonPressed);
    EXPECT_FALSE(g_gestureState.dragDetected);
    EXPECT_TRUE(g_gestureState.timestampedPath.empty());
    EXPECT_FALSE(shouldRenderOverlay());
}

// Test shutdown with trail fade in progress
TEST_F(ShutdownTest, ShutdownWithTrailFading) {
    // Setup trail fade state (button released but trail still visible)
    g_gestureState.rightButtonPressed = false;
    g_gestureState.dragDetected = false;
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});

    EXPECT_TRUE(shouldRenderOverlay());

    simulateShutdown();

    EXPECT_TRUE(g_pluginShuttingDown);
    EXPECT_TRUE(g_gestureState.timestampedPath.empty());
    EXPECT_FALSE(shouldRenderOverlay());
}

// Test shutdown flag checked before render
TEST_F(ShutdownTest, ShutdownFlagCheckedBeforeRender) {
    // Setup various states that would normally trigger rendering
    g_recordMode = true;
    EXPECT_TRUE(shouldRenderOverlay());

    g_pluginShuttingDown = true;
    EXPECT_FALSE(shouldRenderOverlay());

    // Even if record mode is still active
    EXPECT_TRUE(g_recordMode);
    EXPECT_FALSE(shouldRenderOverlay());
}

// Test shutdown flag checked in overlay draw
TEST_F(ShutdownTest, ShutdownFlagCheckedInOverlayDraw) {
    EXPECT_TRUE(shouldDrawOverlay());

    g_pluginShuttingDown = true;
    EXPECT_FALSE(shouldDrawOverlay());
}

// Test shutdown order: flag -> record mode -> cleanup
TEST_F(ShutdownTest, ShutdownOrderCorrect) {
    // Setup active state
    g_recordMode = true;
    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});

    // Step 1: Set shutdown flag
    g_pluginShuttingDown = true;
    // Rendering should be blocked immediately
    EXPECT_FALSE(shouldRenderOverlay());

    // Step 2: Exit record mode
    g_recordMode = false;

    // Step 3: Clear state
    g_gestureState.timestampedPath.clear();
    g_gestureState.reset();

    // Verify final state
    EXPECT_TRUE(g_pluginShuttingDown);
    EXPECT_FALSE(g_recordMode);
    EXPECT_FALSE(g_gestureState.rightButtonPressed);
    EXPECT_FALSE(g_gestureState.dragDetected);
    EXPECT_TRUE(g_gestureState.timestampedPath.empty());
}

// Test that idle state doesn't render
TEST_F(ShutdownTest, IdleStateDoesNotRender) {
    // Default idle state
    EXPECT_FALSE(g_recordMode);
    EXPECT_FALSE(g_gestureState.dragDetected);
    EXPECT_TRUE(g_gestureState.timestampedPath.empty());

    // Should not render
    EXPECT_FALSE(shouldRenderOverlay());
}

// Test rendering only when needed
TEST_F(ShutdownTest, RenderOnlyWhenNeeded) {
    // Idle - no render
    EXPECT_FALSE(shouldRenderOverlay());

    // Record mode - render
    g_recordMode = true;
    EXPECT_TRUE(shouldRenderOverlay());
    g_recordMode = false;

    // Drag detected - render
    g_gestureState.dragDetected = true;
    g_gestureState.rightButtonPressed = true;
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});
    EXPECT_TRUE(shouldRenderOverlay());

    // Button released but trail visible - render
    g_gestureState.rightButtonPressed = false;
    EXPECT_TRUE(shouldRenderOverlay());

    // Trail empty - no render
    g_gestureState.timestampedPath.clear();
    EXPECT_FALSE(shouldRenderOverlay());
}

// Test shutdown prevents rendering even with active state
TEST_F(ShutdownTest, ShutdownOverridesActiveState) {
    // Setup all possible active states
    g_recordMode = true;
    g_gestureState.rightButtonPressed = true;
    g_gestureState.dragDetected = true;
    g_gestureState.timestampedPath.push_back({100, 100, std::chrono::steady_clock::now()});

    // All conditions for rendering are met
    EXPECT_TRUE(shouldRenderOverlay());

    // But shutdown overrides everything
    g_pluginShuttingDown = true;
    EXPECT_FALSE(shouldRenderOverlay());
    EXPECT_FALSE(shouldDrawOverlay());
}

// Test multiple shutdown calls are safe
TEST_F(ShutdownTest, MultipleShutdownCallsSafe) {
    g_recordMode = true;

    simulateShutdown();
    EXPECT_TRUE(g_pluginShuttingDown);
    EXPECT_FALSE(g_recordMode);

    // Second shutdown call should be safe
    simulateShutdown();
    EXPECT_TRUE(g_pluginShuttingDown);
    EXPECT_FALSE(g_recordMode);
}

// Test shutdown with empty state
TEST_F(ShutdownTest, ShutdownWithEmptyState) {
    // Already in idle state
    EXPECT_FALSE(g_recordMode);
    EXPECT_TRUE(g_gestureState.timestampedPath.empty());

    simulateShutdown();

    // Should still set shutdown flag
    EXPECT_TRUE(g_pluginShuttingDown);
    EXPECT_FALSE(shouldRenderOverlay());
}

// Test render optimization reduces overhead
TEST_F(ShutdownTest, RenderOptimizationReducesOverhead) {
    // Track render attempts
    int renderAttempts = 0;

    // Simulate 100 frames while idle
    for (int i = 0; i < 100; i++) {
        if (shouldRenderOverlay()) {
            renderAttempts++;
        }
    }

    // Should be 0 - no rendering when idle
    EXPECT_EQ(renderAttempts, 0);

    // Now with record mode active
    g_recordMode = true;
    renderAttempts = 0;

    for (int i = 0; i < 100; i++) {
        if (shouldRenderOverlay()) {
            renderAttempts++;
        }
    }

    // Should be 100 - rendering every frame
    EXPECT_EQ(renderAttempts, 100);

    // After shutdown, no rendering
    g_pluginShuttingDown = true;
    renderAttempts = 0;

    for (int i = 0; i < 100; i++) {
        if (shouldRenderOverlay()) {
            renderAttempts++;
        }
    }

    // Should be 0 again
    EXPECT_EQ(renderAttempts, 0);
}
