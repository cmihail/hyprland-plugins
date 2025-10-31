#include <gtest/gtest.h>
#include <cmath>
#include <linux/input-event-codes.h>

// Mock Vector2D struct
struct Vector2D {
    double x = 0;
    double y = 0;
};

// Mock MouseGestureState
struct MouseGestureState {
    bool rightButtonPressed = false;
    Vector2D mouseDownPos = {0, 0};
    bool dragDetected = false;

    void reset() {
        rightButtonPressed = false;
        mouseDownPos = {0, 0};
        dragDetected = false;
    }
};

// Constants
constexpr float DRAG_THRESHOLD_PX = 50.0f;

// Test fixture
class MouseGestureTest : public ::testing::Test {
protected:
    MouseGestureState state;

    void SetUp() override {
        state.reset();
    }

    // Helper to simulate right button press
    void simulateRightButtonPress(double x, double y) {
        state.rightButtonPressed = true;
        state.mouseDownPos = {x, y};
        state.dragDetected = false;
    }

    // Helper to simulate right button release
    void simulateRightButtonRelease() {
        state.reset();
    }

    // Helper to check drag threshold
    bool checkDragThreshold(Vector2D currentPos) {
        if (!state.rightButtonPressed || state.dragDetected)
            return false;

        const float distanceX = std::abs(currentPos.x - state.mouseDownPos.x);
        const float distanceY = std::abs(currentPos.y - state.mouseDownPos.y);

        if (distanceX > DRAG_THRESHOLD_PX || distanceY > DRAG_THRESHOLD_PX) {
            state.dragDetected = true;
            return true;
        }
        return false;
    }
};

// Test right-click press records position
TEST_F(MouseGestureTest, RightClickPressRecordsPosition) {
    simulateRightButtonPress(100.0, 200.0);

    EXPECT_TRUE(state.rightButtonPressed);
    EXPECT_EQ(state.mouseDownPos.x, 100.0);
    EXPECT_EQ(state.mouseDownPos.y, 200.0);
    EXPECT_FALSE(state.dragDetected);
}

// Test right-click release resets state
TEST_F(MouseGestureTest, RightClickReleaseResetsState) {
    simulateRightButtonPress(100.0, 200.0);
    state.dragDetected = true;

    simulateRightButtonRelease();

    EXPECT_FALSE(state.rightButtonPressed);
    EXPECT_EQ(state.mouseDownPos.x, 0.0);
    EXPECT_EQ(state.mouseDownPos.y, 0.0);
    EXPECT_FALSE(state.dragDetected);
}

// Test drag not detected when movement is below threshold
TEST_F(MouseGestureTest, NoDragBelowThreshold) {
    simulateRightButtonPress(100.0, 100.0);

    // Move 49px in X direction (below threshold)
    Vector2D currentPos = {149.0, 100.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_FALSE(dragDetected);
    EXPECT_FALSE(state.dragDetected);
}

// Test drag detected when X movement exceeds threshold
TEST_F(MouseGestureTest, DragDetectedXThreshold) {
    simulateRightButtonPress(100.0, 100.0);

    // Move 51px in X direction (above threshold)
    Vector2D currentPos = {151.0, 100.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_TRUE(dragDetected);
    EXPECT_TRUE(state.dragDetected);
}

// Test drag detected when Y movement exceeds threshold
TEST_F(MouseGestureTest, DragDetectedYThreshold) {
    simulateRightButtonPress(100.0, 100.0);

    // Move 51px in Y direction (above threshold)
    Vector2D currentPos = {100.0, 151.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_TRUE(dragDetected);
    EXPECT_TRUE(state.dragDetected);
}

// Test drag detected with negative X movement
TEST_F(MouseGestureTest, DragDetectedNegativeX) {
    simulateRightButtonPress(100.0, 100.0);

    // Move -51px in X direction
    Vector2D currentPos = {49.0, 100.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_TRUE(dragDetected);
    EXPECT_TRUE(state.dragDetected);
}

// Test drag detected with negative Y movement
TEST_F(MouseGestureTest, DragDetectedNegativeY) {
    simulateRightButtonPress(100.0, 100.0);

    // Move -51px in Y direction
    Vector2D currentPos = {100.0, 49.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_TRUE(dragDetected);
    EXPECT_TRUE(state.dragDetected);
}

// Test drag detected with diagonal movement
TEST_F(MouseGestureTest, DragDetectedDiagonal) {
    simulateRightButtonPress(100.0, 100.0);

    // Move diagonally (40px X, 40px Y) - individually below threshold
    // but one axis should exceed threshold when checked separately
    Vector2D currentPos = {140.0, 140.0};
    bool dragDetected = checkDragThreshold(currentPos);

    // Neither axis exceeds 50px threshold
    EXPECT_FALSE(dragDetected);
    EXPECT_FALSE(state.dragDetected);

    // Now move to exceed threshold in one axis
    currentPos = {151.0, 140.0};
    dragDetected = checkDragThreshold(currentPos);
    EXPECT_TRUE(dragDetected);
    EXPECT_TRUE(state.dragDetected);
}

// Test exact threshold boundary (50px should not trigger)
TEST_F(MouseGestureTest, ExactThresholdBoundary) {
    simulateRightButtonPress(100.0, 100.0);

    // Move exactly 50px (should not trigger as we use > not >=)
    Vector2D currentPos = {150.0, 100.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_FALSE(dragDetected);
    EXPECT_FALSE(state.dragDetected);
}

// Test just over threshold boundary (50.1px should trigger)
TEST_F(MouseGestureTest, JustOverThresholdBoundary) {
    simulateRightButtonPress(100.0, 100.0);

    // Move 50.1px (should trigger)
    Vector2D currentPos = {150.1, 100.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_TRUE(dragDetected);
    EXPECT_TRUE(state.dragDetected);
}

// Test drag not checked when button not pressed
TEST_F(MouseGestureTest, NoDragCheckWhenButtonNotPressed) {
    // Don't press button
    EXPECT_FALSE(state.rightButtonPressed);

    Vector2D currentPos = {200.0, 200.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_FALSE(dragDetected);
    EXPECT_FALSE(state.dragDetected);
}

// Test drag not checked again after already detected
TEST_F(MouseGestureTest, NoDragCheckAfterAlreadyDetected) {
    simulateRightButtonPress(100.0, 100.0);

    // First drag detection
    Vector2D currentPos = {151.0, 100.0};
    bool dragDetected = checkDragThreshold(currentPos);
    EXPECT_TRUE(dragDetected);
    EXPECT_TRUE(state.dragDetected);

    // Try to check again - should return false (not trigger again)
    currentPos = {200.0, 100.0};
    dragDetected = checkDragThreshold(currentPos);
    EXPECT_FALSE(dragDetected);  // Already detected, so returns false
    EXPECT_TRUE(state.dragDetected);  // State remains true
}

// Test state reset clears all fields
TEST_F(MouseGestureTest, ResetClearsAllFields) {
    state.rightButtonPressed = true;
    state.mouseDownPos = {123.45, 678.90};
    state.dragDetected = true;

    state.reset();

    EXPECT_FALSE(state.rightButtonPressed);
    EXPECT_EQ(state.mouseDownPos.x, 0.0);
    EXPECT_EQ(state.mouseDownPos.y, 0.0);
    EXPECT_FALSE(state.dragDetected);
}

// Test multiple press/release cycles
TEST_F(MouseGestureTest, MultiplePressReleaseCycles) {
    // First cycle
    simulateRightButtonPress(100.0, 100.0);
    Vector2D pos1 = {151.0, 100.0};
    bool drag1 = checkDragThreshold(pos1);
    EXPECT_TRUE(drag1);
    simulateRightButtonRelease();

    // Second cycle - should start fresh
    simulateRightButtonPress(200.0, 200.0);
    EXPECT_FALSE(state.dragDetected);  // Reset from previous cycle
    Vector2D pos2 = {251.0, 200.0};
    bool drag2 = checkDragThreshold(pos2);
    EXPECT_TRUE(drag2);
    simulateRightButtonRelease();

    EXPECT_FALSE(state.rightButtonPressed);
    EXPECT_FALSE(state.dragDetected);
}

// Test that BTN_RIGHT constant has correct value
TEST(MouseGestureConstantTest, BTN_RIGHT_Value) {
    EXPECT_EQ(BTN_RIGHT, 273);
    EXPECT_EQ(BTN_RIGHT, 0x111);
}

// Test drag threshold constant
TEST(MouseGestureConstantTest, DragThreshold) {
    EXPECT_EQ(DRAG_THRESHOLD_PX, 50.0f);
}

// Test zero movement does not trigger drag
TEST_F(MouseGestureTest, ZeroMovementNoDrag) {
    simulateRightButtonPress(100.0, 100.0);

    // No movement
    Vector2D currentPos = {100.0, 100.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_FALSE(dragDetected);
    EXPECT_FALSE(state.dragDetected);
}

// Test very small movement (1px) does not trigger drag
TEST_F(MouseGestureTest, TinyMovementNoDrag) {
    simulateRightButtonPress(100.0, 100.0);

    // 1px movement
    Vector2D currentPos = {101.0, 100.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_FALSE(dragDetected);
    EXPECT_FALSE(state.dragDetected);
}

// Test large movement in both axes
TEST_F(MouseGestureTest, LargeMovementBothAxes) {
    simulateRightButtonPress(100.0, 100.0);

    // Large movement in both directions
    Vector2D currentPos = {200.0, 250.0};
    bool dragDetected = checkDragThreshold(currentPos);

    EXPECT_TRUE(dragDetected);
    EXPECT_TRUE(state.dragDetected);
}
