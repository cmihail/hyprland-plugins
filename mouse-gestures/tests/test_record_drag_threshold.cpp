#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Mock Vector2D struct
struct Vector2D {
    double x = 0;
    double y = 0;

    bool operator==(const Vector2D& other) const {
        return std::abs(x - other.x) < 0.001 && std::abs(y - other.y) < 0.001;
    }
};

// Mock MouseGestureState
struct MouseGestureState {
    bool rightButtonPressed = false;
    Vector2D mouseDownPos = {0, 0};
    bool dragDetected = false;
    std::vector<Vector2D> path;

    void reset() {
        rightButtonPressed = false;
        mouseDownPos = {0, 0};
        dragDetected = false;
        path.clear();
    }
};

// Constants
constexpr float DRAG_THRESHOLD_PX = 50.0f;

// Test fixture for record mode drag threshold behavior
class RecordModeDragThresholdTest : public ::testing::Test {
protected:
    MouseGestureState state;
    bool recordMode = false;
    float dragThreshold = DRAG_THRESHOLD_PX;

    void SetUp() override {
        state.reset();
        recordMode = false;
        dragThreshold = DRAG_THRESHOLD_PX;
    }

    // Helper to check drag threshold (mimics main.cpp logic)
    bool checkDragThresholdExceeded(const Vector2D& mousePos) {
        const float distanceX = std::abs(mousePos.x - state.mouseDownPos.x);
        const float distanceY = std::abs(mousePos.y - state.mouseDownPos.y);
        return (distanceX > dragThreshold || distanceY > dragThreshold);
    }

    // Helper to simulate mouse move (mimics main.cpp:960-998 logic)
    void simulateMouseMove(const Vector2D& mousePos) {
        if (!state.rightButtonPressed)
            return;

        // Check if drag threshold exceeded (if not yet detected)
        if (!state.dragDetected) {
            if (checkDragThresholdExceeded(mousePos)) {
                state.dragDetected = true;
            }
        }

        // Record path point for gesture matching
        // In record mode: always record all points from the start
        // In normal mode: only record after drag threshold is exceeded
        if (recordMode || state.dragDetected) {
            state.path.push_back(mousePos);
        }
    }

    // Helper to simulate button press
    void simulateButtonPress(const Vector2D& pos) {
        state.rightButtonPressed = true;
        state.mouseDownPos = pos;
        state.dragDetected = false;
        state.path.clear();
        state.path.push_back(pos);
    }
};

// Test that in normal mode, points before drag threshold are not recorded
TEST_F(RecordModeDragThresholdTest, NormalModeIgnoresPointsBeforeThreshold) {
    recordMode = false;

    // Simulate button press at origin
    simulateButtonPress({100.0, 100.0});

    // Initial press point should be recorded
    ASSERT_EQ(state.path.size(), 1);
    EXPECT_EQ(state.path[0], Vector2D({100.0, 100.0}));

    // Move within threshold (should NOT be recorded in normal mode)
    simulateMouseMove({110.0, 110.0});  // 10px movement
    EXPECT_EQ(state.path.size(), 1);  // Still only initial point

    simulateMouseMove({120.0, 120.0});  // 20px movement
    EXPECT_EQ(state.path.size(), 1);  // Still only initial point

    simulateMouseMove({140.0, 140.0});  // 40px movement
    EXPECT_EQ(state.path.size(), 1);  // Still only initial point
    EXPECT_FALSE(state.dragDetected);

    // Move beyond threshold (should trigger recording)
    simulateMouseMove({160.0, 160.0});  // 60px movement - exceeds threshold
    EXPECT_TRUE(state.dragDetected);
    EXPECT_EQ(state.path.size(), 2);  // Initial point + this point
    EXPECT_EQ(state.path[1], Vector2D({160.0, 160.0}));

    // Subsequent moves should be recorded
    simulateMouseMove({170.0, 170.0});
    EXPECT_EQ(state.path.size(), 3);
}

// Test that in record mode, ALL points are captured from the start
TEST_F(RecordModeDragThresholdTest, RecordModeCapturesAllPointsFromStart) {
    recordMode = true;

    // Simulate button press at origin
    simulateButtonPress({100.0, 100.0});

    // Initial press point should be recorded
    ASSERT_EQ(state.path.size(), 1);
    EXPECT_EQ(state.path[0], Vector2D({100.0, 100.0}));

    // Move within threshold (SHOULD be recorded in record mode)
    simulateMouseMove({110.0, 110.0});  // 10px movement
    EXPECT_EQ(state.path.size(), 2);
    EXPECT_EQ(state.path[1], Vector2D({110.0, 110.0}));

    simulateMouseMove({120.0, 120.0});  // 20px movement
    EXPECT_EQ(state.path.size(), 3);
    EXPECT_EQ(state.path[2], Vector2D({120.0, 120.0}));

    simulateMouseMove({140.0, 140.0});  // 40px movement
    EXPECT_EQ(state.path.size(), 4);
    EXPECT_EQ(state.path[3], Vector2D({140.0, 140.0}));

    // Even though we haven't exceeded threshold, all points are recorded
    EXPECT_FALSE(state.dragDetected);

    // Move beyond threshold (should still record, and set dragDetected)
    simulateMouseMove({160.0, 160.0});  // 60px movement - exceeds threshold
    EXPECT_TRUE(state.dragDetected);
    EXPECT_EQ(state.path.size(), 5);
    EXPECT_EQ(state.path[4], Vector2D({160.0, 160.0}));

    // Subsequent moves continue to be recorded
    simulateMouseMove({170.0, 170.0});
    EXPECT_EQ(state.path.size(), 6);
}

// Test very small movements in record mode
TEST_F(RecordModeDragThresholdTest, RecordModeCapturesToyMovements) {
    recordMode = true;

    simulateButtonPress({100.0, 100.0});
    ASSERT_EQ(state.path.size(), 1);

    // Move just 1 pixel at a time
    simulateMouseMove({101.0, 100.0});
    EXPECT_EQ(state.path.size(), 2);

    simulateMouseMove({102.0, 100.0});
    EXPECT_EQ(state.path.size(), 3);

    simulateMouseMove({103.0, 100.0});
    EXPECT_EQ(state.path.size(), 4);

    // All tiny movements should be captured in record mode
    EXPECT_FALSE(state.dragDetected);  // Still under threshold
}

// Test that normal mode doesn't record tiny movements
TEST_F(RecordModeDragThresholdTest, NormalModeIgnoresToyMovements) {
    recordMode = false;

    simulateButtonPress({100.0, 100.0});
    ASSERT_EQ(state.path.size(), 1);

    // Move just 1 pixel at a time (should not be recorded)
    simulateMouseMove({101.0, 100.0});
    EXPECT_EQ(state.path.size(), 1);

    simulateMouseMove({102.0, 100.0});
    EXPECT_EQ(state.path.size(), 1);

    simulateMouseMove({103.0, 100.0});
    EXPECT_EQ(state.path.size(), 1);

    // Tiny movements ignored in normal mode
    EXPECT_FALSE(state.dragDetected);
}

// Test exact threshold boundary in record mode
TEST_F(RecordModeDragThresholdTest, RecordModeExactThresholdBoundary) {
    recordMode = true;

    simulateButtonPress({100.0, 100.0});

    // Move exactly to threshold
    simulateMouseMove({150.0, 100.0});  // Exactly 50px
    EXPECT_EQ(state.path.size(), 2);
    EXPECT_FALSE(state.dragDetected);  // At threshold, not over

    // Move just over threshold
    simulateMouseMove({150.1, 100.0});  // 50.1px
    EXPECT_EQ(state.path.size(), 3);
    EXPECT_TRUE(state.dragDetected);  // Now over threshold
}

// Test that switching to record mode mid-gesture works correctly
TEST_F(RecordModeDragThresholdTest, SwitchingToRecordModeMidGesture) {
    recordMode = false;

    simulateButtonPress({100.0, 100.0});

    // Move within threshold (not recorded)
    simulateMouseMove({120.0, 120.0});
    EXPECT_EQ(state.path.size(), 1);

    // Switch to record mode
    recordMode = true;

    // Now all subsequent moves should be recorded
    simulateMouseMove({130.0, 130.0});
    EXPECT_EQ(state.path.size(), 2);

    simulateMouseMove({140.0, 140.0});
    EXPECT_EQ(state.path.size(), 3);
}

// Test complete gesture recording in record mode
TEST_F(RecordModeDragThresholdTest, RecordModeCompleteGestureCapture) {
    recordMode = true;

    simulateButtonPress({100.0, 100.0});

    // Simulate drawing an 'L' shape with small movements
    std::vector<Vector2D> expectedPath = {
        {100.0, 100.0},  // Start
        {105.0, 100.0},
        {110.0, 100.0},
        {115.0, 100.0},
        {120.0, 100.0},  // Horizontal line (all under threshold)
        {120.0, 105.0},
        {120.0, 110.0},
        {120.0, 115.0},
        {120.0, 120.0}   // Vertical line
    };

    for (size_t i = 1; i < expectedPath.size(); i++) {
        simulateMouseMove(expectedPath[i]);
    }

    // All points should be captured
    ASSERT_EQ(state.path.size(), expectedPath.size());
    for (size_t i = 0; i < expectedPath.size(); i++) {
        EXPECT_EQ(state.path[i], expectedPath[i]);
    }
}

// Test that normal mode only captures significant movements
TEST_F(RecordModeDragThresholdTest, NormalModeOnlySignificantMovements) {
    recordMode = false;

    simulateButtonPress({100.0, 100.0});

    // Make many small movements (all ignored)
    for (int i = 1; i <= 40; i++) {
        simulateMouseMove({100.0 + i, 100.0});
    }

    // Should still only have initial point (40px < 50px threshold)
    EXPECT_EQ(state.path.size(), 1);
    EXPECT_FALSE(state.dragDetected);

    // Move beyond threshold
    simulateMouseMove({160.0, 100.0});  // 60px total
    EXPECT_TRUE(state.dragDetected);
    EXPECT_EQ(state.path.size(), 2);  // Initial + this point
}

// Test diagonal movements in record mode
TEST_F(RecordModeDragThresholdTest, RecordModeDiagonalMovements) {
    recordMode = true;

    simulateButtonPress({100.0, 100.0});

    // Diagonal movement (both X and Y change)
    simulateMouseMove({110.0, 110.0});
    EXPECT_EQ(state.path.size(), 2);

    simulateMouseMove({120.0, 120.0});
    EXPECT_EQ(state.path.size(), 3);

    simulateMouseMove({130.0, 130.0});
    EXPECT_EQ(state.path.size(), 4);

    // All diagonal movements captured in record mode
    EXPECT_FALSE(state.dragDetected);  // ~42px diagonal, under 50px threshold
}

// Test Y-axis threshold in record mode
TEST_F(RecordModeDragThresholdTest, RecordModeYAxisThreshold) {
    recordMode = true;

    simulateButtonPress({100.0, 100.0});

    // Move vertically in small increments
    simulateMouseMove({100.0, 110.0});
    EXPECT_EQ(state.path.size(), 2);

    simulateMouseMove({100.0, 120.0});
    EXPECT_EQ(state.path.size(), 3);

    simulateMouseMove({100.0, 130.0});
    EXPECT_EQ(state.path.size(), 4);

    simulateMouseMove({100.0, 140.0});
    EXPECT_EQ(state.path.size(), 5);

    EXPECT_FALSE(state.dragDetected);  // 40px, under threshold

    simulateMouseMove({100.0, 160.0});  // 60px, over threshold
    EXPECT_EQ(state.path.size(), 6);
    EXPECT_TRUE(state.dragDetected);
}

// Test that path is preserved correctly in record mode
TEST_F(RecordModeDragThresholdTest, RecordModePathPreservation) {
    recordMode = true;

    simulateButtonPress({50.0, 50.0});

    // Create a zigzag pattern
    std::vector<Vector2D> moves = {
        {55.0, 50.0},
        {55.0, 55.0},
        {60.0, 55.0},
        {60.0, 60.0},
        {65.0, 60.0},
        {65.0, 65.0}
    };

    for (const auto& move : moves) {
        simulateMouseMove(move);
    }

    // Verify all points are in path in correct order
    ASSERT_EQ(state.path.size(), moves.size() + 1);  // +1 for initial press
    EXPECT_EQ(state.path[0], Vector2D({50.0, 50.0}));
    for (size_t i = 0; i < moves.size(); i++) {
        EXPECT_EQ(state.path[i + 1], moves[i]);
    }
}

// Test behavior when button is released before threshold
TEST_F(RecordModeDragThresholdTest, RecordModeReleaseBeforeThreshold) {
    recordMode = true;

    simulateButtonPress({100.0, 100.0});

    // Move within threshold
    simulateMouseMove({110.0, 110.0});
    simulateMouseMove({120.0, 120.0});

    // Should have captured these movements even though under threshold
    EXPECT_EQ(state.path.size(), 3);
    EXPECT_FALSE(state.dragDetected);

    // Release button
    state.reset();
    EXPECT_EQ(state.path.size(), 0);
}

// Test normal mode behavior when releasing before threshold
TEST_F(RecordModeDragThresholdTest, NormalModeReleaseBeforeThreshold) {
    recordMode = false;

    simulateButtonPress({100.0, 100.0});

    // Move within threshold
    simulateMouseMove({110.0, 110.0});
    simulateMouseMove({120.0, 120.0});

    // Should only have initial point in normal mode
    EXPECT_EQ(state.path.size(), 1);
    EXPECT_FALSE(state.dragDetected);
}

// Test that record mode captures initial micro-adjustments
TEST_F(RecordModeDragThresholdTest, RecordModeCapturesMicroAdjustments) {
    recordMode = true;

    simulateButtonPress({100.0, 100.0});

    // Simulate user making tiny initial adjustments
    simulateMouseMove({100.5, 100.2});
    simulateMouseMove({101.0, 100.8});
    simulateMouseMove({101.2, 101.0});

    // All micro-adjustments should be captured
    EXPECT_EQ(state.path.size(), 4);
    EXPECT_FALSE(state.dragDetected);
}

// Test the specific bug scenario: recording from initial drag point
TEST_F(RecordModeDragThresholdTest, BugScenarioRecordFromInitialPoint) {
    // This test verifies the specific bug fix:
    // "Points are recorded from initial drag point and from drag threshold"
    // We want ALL points from the initial drag point in record mode

    recordMode = true;

    // User starts drawing at 100,100
    simulateButtonPress({100.0, 100.0});
    EXPECT_EQ(state.path.size(), 1);

    // User draws a small curve before hitting threshold
    // These points were being LOST before the fix
    simulateMouseMove({105.0, 102.0});
    simulateMouseMove({110.0, 105.0});
    simulateMouseMove({115.0, 109.0});
    simulateMouseMove({120.0, 114.0});
    simulateMouseMove({125.0, 120.0});

    // All points before threshold should be captured (this is the fix!)
    EXPECT_EQ(state.path.size(), 6);
    EXPECT_FALSE(state.dragDetected);  // Still under 50px threshold

    // Continue to exceed threshold (need to move > 50px from start)
    simulateMouseMove({130.0, 135.0});
    simulateMouseMove({155.0, 145.0});  // 55px in X axis - exceeds 50px threshold!

    // Now drag is detected, but we didn't lose the initial points
    EXPECT_TRUE(state.dragDetected);
    EXPECT_EQ(state.path.size(), 8);

    // Verify the initial subtle movements are preserved
    EXPECT_EQ(state.path[0], Vector2D({100.0, 100.0}));
    EXPECT_EQ(state.path[1], Vector2D({105.0, 102.0}));
    EXPECT_EQ(state.path[2], Vector2D({110.0, 105.0}));
}
