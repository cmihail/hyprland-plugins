#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <linux/input-event-codes.h>

// Mock Vector2D struct
struct Vector2D {
    double x = 0;
    double y = 0;
};

// Gesture directions
enum class Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    UP_RIGHT,
    UP_LEFT,
    DOWN_RIGHT,
    DOWN_LEFT,
    NONE
};

const char* directionToString(Direction dir) {
    switch (dir) {
        case Direction::UP: return "UP";
        case Direction::DOWN: return "DOWN";
        case Direction::LEFT: return "LEFT";
        case Direction::RIGHT: return "RIGHT";
        case Direction::UP_RIGHT: return "UP_RIGHT";
        case Direction::UP_LEFT: return "UP_LEFT";
        case Direction::DOWN_RIGHT: return "DOWN_RIGHT";
        case Direction::DOWN_LEFT: return "DOWN_LEFT";
        default: return "NONE";
    }
}

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

// Test fixture
class MouseGestureTest : public ::testing::Test {
protected:
    MouseGestureState state;
    float dragThreshold = DRAG_THRESHOLD_PX;
    uint32_t actionButton = BTN_RIGHT;

    void SetUp() override {
        state.reset();
        dragThreshold = DRAG_THRESHOLD_PX;
        actionButton = BTN_RIGHT;
    }

    // Helper to simulate button press
    void simulateButtonPress(double x, double y) {
        state.rightButtonPressed = true;
        state.mouseDownPos = {x, y};
        state.dragDetected = false;
    }

    // Helper to simulate right button press (legacy)
    void simulateRightButtonPress(double x, double y) {
        simulateButtonPress(x, y);
    }

    // Helper to simulate button release
    void simulateButtonRelease() {
        state.reset();
    }

    // Helper to simulate right button release (legacy)
    void simulateRightButtonRelease() {
        simulateButtonRelease();
    }

    // Helper to check drag threshold
    bool checkDragThreshold(Vector2D currentPos) {
        if (!state.rightButtonPressed || state.dragDetected)
            return false;

        const float distanceX = std::abs(currentPos.x - state.mouseDownPos.x);
        const float distanceY = std::abs(currentPos.y - state.mouseDownPos.y);

        if (distanceX > dragThreshold || distanceY > dragThreshold) {
            state.dragDetected = true;
            return true;
        }
        return false;
    }

    // Helper to check if button matches action button
    bool isActionButton(uint32_t button) {
        return button == actionButton;
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

// Constants for gesture detection
constexpr float MIN_SEGMENT_LENGTH = 10.0f;

// Calculate direction from two points
static Direction calculateDirection(const Vector2D& from, const Vector2D& to) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;

    float distance = std::sqrt(dx * dx + dy * dy);
    if (distance < MIN_SEGMENT_LENGTH) {
        return Direction::NONE;
    }

    float angle = std::atan2(dy, dx) * 180.0f / M_PI;

    if (angle < 0) {
        angle += 360.0f;
    }

    if (angle >= 337.5f || angle < 22.5f) {
        return Direction::RIGHT;
    } else if (angle >= 22.5f && angle < 67.5f) {
        return Direction::DOWN_RIGHT;
    } else if (angle >= 67.5f && angle < 112.5f) {
        return Direction::DOWN;
    } else if (angle >= 112.5f && angle < 157.5f) {
        return Direction::DOWN_LEFT;
    } else if (angle >= 157.5f && angle < 202.5f) {
        return Direction::LEFT;
    } else if (angle >= 202.5f && angle < 247.5f) {
        return Direction::UP_LEFT;
    } else if (angle >= 247.5f && angle < 292.5f) {
        return Direction::UP;
    } else {
        return Direction::UP_RIGHT;
    }
}

// Analyze gesture path and return sequence of directions
static std::vector<Direction> analyzeGesture(
    const std::vector<Vector2D>& path
) {
    std::vector<Direction> rawDirections;

    if (path.size() < 2) {
        return rawDirections;
    }

    for (size_t i = 1; i < path.size(); i++) {
        Direction dir = calculateDirection(path[i - 1], path[i]);
        if (dir != Direction::NONE) {
            rawDirections.push_back(dir);
        }
    }

    std::vector<Direction> gesture;
    if (!rawDirections.empty()) {
        gesture.push_back(rawDirections[0]);
        for (size_t i = 1; i < rawDirections.size(); i++) {
            if (rawDirections[i] != gesture.back()) {
                gesture.push_back(rawDirections[i]);
            }
        }
    }

    return gesture;
}

// Test fixture for gesture direction tests
class GestureDirectionTest : public ::testing::Test {
};

// Test RIGHT direction (0 degrees)
TEST_F(GestureDirectionTest, DirectionRight) {
    Vector2D from = {0, 0};
    Vector2D to = {100, 0};
    EXPECT_EQ(calculateDirection(from, to), Direction::RIGHT);
}

// Test DOWN_RIGHT direction (45 degrees)
TEST_F(GestureDirectionTest, DirectionDownRight) {
    Vector2D from = {0, 0};
    Vector2D to = {100, 100};
    EXPECT_EQ(calculateDirection(from, to), Direction::DOWN_RIGHT);
}

// Test DOWN direction (90 degrees)
TEST_F(GestureDirectionTest, DirectionDown) {
    Vector2D from = {0, 0};
    Vector2D to = {0, 100};
    EXPECT_EQ(calculateDirection(from, to), Direction::DOWN);
}

// Test DOWN_LEFT direction (135 degrees)
TEST_F(GestureDirectionTest, DirectionDownLeft) {
    Vector2D from = {0, 0};
    Vector2D to = {-100, 100};
    EXPECT_EQ(calculateDirection(from, to), Direction::DOWN_LEFT);
}

// Test LEFT direction (180 degrees)
TEST_F(GestureDirectionTest, DirectionLeft) {
    Vector2D from = {0, 0};
    Vector2D to = {-100, 0};
    EXPECT_EQ(calculateDirection(from, to), Direction::LEFT);
}

// Test UP_LEFT direction (225 degrees)
TEST_F(GestureDirectionTest, DirectionUpLeft) {
    Vector2D from = {0, 0};
    Vector2D to = {-100, -100};
    EXPECT_EQ(calculateDirection(from, to), Direction::UP_LEFT);
}

// Test UP direction (270 degrees)
TEST_F(GestureDirectionTest, DirectionUp) {
    Vector2D from = {0, 0};
    Vector2D to = {0, -100};
    EXPECT_EQ(calculateDirection(from, to), Direction::UP);
}

// Test UP_RIGHT direction (315 degrees)
TEST_F(GestureDirectionTest, DirectionUpRight) {
    Vector2D from = {0, 0};
    Vector2D to = {100, -100};
    EXPECT_EQ(calculateDirection(from, to), Direction::UP_RIGHT);
}

// Test movement below minimum segment length
TEST_F(GestureDirectionTest, BelowMinSegmentLength) {
    Vector2D from = {0, 0};
    Vector2D to = {5, 5};
    EXPECT_EQ(calculateDirection(from, to), Direction::NONE);
}

// Test exact minimum segment length boundary
TEST_F(GestureDirectionTest, ExactMinSegmentLength) {
    Vector2D from = {0, 0};
    Vector2D to = {10, 0};
    EXPECT_EQ(calculateDirection(from, to), Direction::RIGHT);
}

// Test boundary between RIGHT and DOWN_RIGHT (22.5 degrees)
TEST_F(GestureDirectionTest, BoundaryRightDownRight) {
    Vector2D from = {0, 0};
    Vector2D to = {100, 45};
    EXPECT_EQ(calculateDirection(from, to), Direction::DOWN_RIGHT);
}

// Test boundary between DOWN_RIGHT and DOWN (67.5 degrees)
TEST_F(GestureDirectionTest, BoundaryDownRightDown) {
    Vector2D from = {0, 0};
    Vector2D to = {40, 100};
    EXPECT_EQ(calculateDirection(from, to), Direction::DOWN);
}

// Test fixture for gesture analysis tests
class GestureAnalysisTest : public ::testing::Test {
};

// Test empty path
TEST_F(GestureAnalysisTest, EmptyPath) {
    std::vector<Vector2D> path;
    std::vector<Direction> result = analyzeGesture(path);
    EXPECT_TRUE(result.empty());
}

// Test single point path
TEST_F(GestureAnalysisTest, SinglePointPath) {
    std::vector<Vector2D> path = {{0, 0}};
    std::vector<Direction> result = analyzeGesture(path);
    EXPECT_TRUE(result.empty());
}

// Test simple horizontal right gesture
TEST_F(GestureAnalysisTest, SimpleRightGesture) {
    std::vector<Vector2D> path = {
        {0, 0},
        {20, 0},
        {40, 0},
        {60, 0}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], Direction::RIGHT);
}

// Test simple vertical down gesture
TEST_F(GestureAnalysisTest, SimpleDownGesture) {
    std::vector<Vector2D> path = {
        {0, 0},
        {0, 20},
        {0, 40},
        {0, 60}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], Direction::DOWN);
}

// Test L-shape gesture (RIGHT then DOWN)
TEST_F(GestureAnalysisTest, LShapeRightDown) {
    std::vector<Vector2D> path = {
        {0, 0},
        {30, 0},
        {60, 0},
        {60, 30},
        {60, 60}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], Direction::RIGHT);
    EXPECT_EQ(result[1], Direction::DOWN);
}

// Test reverse L-shape gesture (DOWN then RIGHT)
TEST_F(GestureAnalysisTest, LShapeDownRight) {
    std::vector<Vector2D> path = {
        {0, 0},
        {0, 30},
        {0, 60},
        {30, 60},
        {60, 60}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], Direction::DOWN);
    EXPECT_EQ(result[1], Direction::RIGHT);
}

// Test diagonal gesture
TEST_F(GestureAnalysisTest, DiagonalDownRight) {
    std::vector<Vector2D> path = {
        {0, 0},
        {20, 20},
        {40, 40},
        {60, 60}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], Direction::DOWN_RIGHT);
}

// Test Z-shape gesture (RIGHT, DOWN, RIGHT)
TEST_F(GestureAnalysisTest, ZShapeGesture) {
    std::vector<Vector2D> path = {
        {0, 0},
        {30, 0},
        {60, 0},
        {60, 30},
        {60, 60},
        {90, 60},
        {120, 60}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], Direction::RIGHT);
    EXPECT_EQ(result[1], Direction::DOWN);
    EXPECT_EQ(result[2], Direction::RIGHT);
}

// Test gesture with very small movements filtered out
TEST_F(GestureAnalysisTest, SmallMovementsFiltered) {
    std::vector<Vector2D> path = {
        {0, 0},
        {2, 1},
        {4, 2},
        {60, 0}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], Direction::RIGHT);
}

// Test circular gesture (RIGHT, DOWN, LEFT, UP)
TEST_F(GestureAnalysisTest, CircularGesture) {
    std::vector<Vector2D> path = {
        {0, 0},
        {50, 0},
        {50, 50},
        {0, 50},
        {0, 0}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], Direction::RIGHT);
    EXPECT_EQ(result[1], Direction::DOWN);
    EXPECT_EQ(result[2], Direction::LEFT);
    EXPECT_EQ(result[3], Direction::UP);
}

// Test UP gesture (negative Y)
TEST_F(GestureAnalysisTest, SimpleUpGesture) {
    std::vector<Vector2D> path = {
        {0, 100},
        {0, 70},
        {0, 40},
        {0, 10}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], Direction::UP);
}

// Test LEFT gesture (negative X)
TEST_F(GestureAnalysisTest, SimpleLeftGesture) {
    std::vector<Vector2D> path = {
        {100, 0},
        {70, 0},
        {40, 0},
        {10, 0}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], Direction::LEFT);
}

// Test path with duplicate consecutive directions merged
TEST_F(GestureAnalysisTest, ConsecutiveDuplicatesMerged) {
    std::vector<Vector2D> path = {
        {0, 0},
        {20, 0},
        {40, 0},
        {60, 0},
        {80, 0},
        {100, 0}
    };
    std::vector<Direction> result = analyzeGesture(path);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], Direction::RIGHT);
}

// ============================================================================
// Configuration Tests
// ============================================================================

// Test fixture for configuration tests
class ConfigurableGestureTest : public MouseGestureTest {
};

// Test custom drag threshold (25px)
TEST_F(ConfigurableGestureTest, CustomDragThreshold25) {
    dragThreshold = 25.0f;
    simulateButtonPress(100.0, 100.0);

    // Move 24px - should not trigger
    Vector2D pos1 = {124.0, 100.0};
    bool drag1 = checkDragThreshold(pos1);
    EXPECT_FALSE(drag1);
    EXPECT_FALSE(state.dragDetected);

    // Reset and try 26px - should trigger
    state.reset();
    simulateButtonPress(100.0, 100.0);
    Vector2D pos2 = {126.0, 100.0};
    bool drag2 = checkDragThreshold(pos2);
    EXPECT_TRUE(drag2);
    EXPECT_TRUE(state.dragDetected);
}

// Test custom drag threshold (100px)
TEST_F(ConfigurableGestureTest, CustomDragThreshold100) {
    dragThreshold = 100.0f;
    simulateButtonPress(100.0, 100.0);

    // Move 99px - should not trigger
    Vector2D pos1 = {199.0, 100.0};
    bool drag1 = checkDragThreshold(pos1);
    EXPECT_FALSE(drag1);
    EXPECT_FALSE(state.dragDetected);

    // Reset and try 101px - should trigger
    state.reset();
    simulateButtonPress(100.0, 100.0);
    Vector2D pos2 = {201.0, 100.0};
    bool drag2 = checkDragThreshold(pos2);
    EXPECT_TRUE(drag2);
    EXPECT_TRUE(state.dragDetected);
}

// Test very small drag threshold (5px)
TEST_F(ConfigurableGestureTest, SmallDragThreshold) {
    dragThreshold = 5.0f;
    simulateButtonPress(100.0, 100.0);

    // Move 6px - should trigger
    Vector2D pos = {106.0, 100.0};
    bool drag = checkDragThreshold(pos);
    EXPECT_TRUE(drag);
    EXPECT_TRUE(state.dragDetected);
}

// Test very large drag threshold (200px)
TEST_F(ConfigurableGestureTest, LargeDragThreshold) {
    dragThreshold = 200.0f;
    simulateButtonPress(100.0, 100.0);

    // Move 150px - should not trigger
    Vector2D pos1 = {250.0, 100.0};
    bool drag1 = checkDragThreshold(pos1);
    EXPECT_FALSE(drag1);
    EXPECT_FALSE(state.dragDetected);

    // Reset and try 201px - should trigger
    state.reset();
    simulateButtonPress(100.0, 100.0);
    Vector2D pos2 = {301.0, 100.0};
    bool drag2 = checkDragThreshold(pos2);
    EXPECT_TRUE(drag2);
    EXPECT_TRUE(state.dragDetected);
}

// Test action button configuration - right button (BTN_RIGHT = 273)
TEST_F(ConfigurableGestureTest, ActionButtonRight) {
    actionButton = BTN_RIGHT;
    EXPECT_TRUE(isActionButton(BTN_RIGHT));
    EXPECT_FALSE(isActionButton(BTN_LEFT));
    EXPECT_FALSE(isActionButton(BTN_MIDDLE));
}

// Test action button configuration - left button (BTN_LEFT = 272)
TEST_F(ConfigurableGestureTest, ActionButtonLeft) {
    actionButton = BTN_LEFT;
    EXPECT_TRUE(isActionButton(BTN_LEFT));
    EXPECT_FALSE(isActionButton(BTN_RIGHT));
    EXPECT_FALSE(isActionButton(BTN_MIDDLE));
}

// Test action button configuration - middle button (BTN_MIDDLE = 274)
TEST_F(ConfigurableGestureTest, ActionButtonMiddle) {
    actionButton = BTN_MIDDLE;
    EXPECT_TRUE(isActionButton(BTN_MIDDLE));
    EXPECT_FALSE(isActionButton(BTN_LEFT));
    EXPECT_FALSE(isActionButton(BTN_RIGHT));
}

// Test that BTN_LEFT constant has correct value
TEST(ConfigurationConstantTest, BTN_LEFT_Value) {
    EXPECT_EQ(BTN_LEFT, 272);
    EXPECT_EQ(BTN_LEFT, 0x110);
}

// Test that BTN_MIDDLE constant has correct value
TEST(ConfigurationConstantTest, BTN_MIDDLE_Value) {
    EXPECT_EQ(BTN_MIDDLE, 274);
    EXPECT_EQ(BTN_MIDDLE, 0x112);
}

// Test default configuration values
TEST(ConfigurationConstantTest, DefaultDragThreshold) {
    EXPECT_EQ(DRAG_THRESHOLD_PX, 50.0f);
}

TEST(ConfigurationConstantTest, DefaultActionButton) {
    EXPECT_EQ(BTN_RIGHT, 273);
}

// Test drag threshold with Y-axis movement
TEST_F(ConfigurableGestureTest, CustomDragThresholdYAxis) {
    dragThreshold = 30.0f;
    simulateButtonPress(100.0, 100.0);

    // Move 31px in Y - should trigger
    Vector2D pos = {100.0, 131.0};
    bool drag = checkDragThreshold(pos);
    EXPECT_TRUE(drag);
    EXPECT_TRUE(state.dragDetected);
}

// Test drag threshold with diagonal movement
TEST_F(ConfigurableGestureTest, CustomDragThresholdDiagonal) {
    dragThreshold = 40.0f;
    simulateButtonPress(100.0, 100.0);

    // Move 41px in X, 30px in Y - should trigger (X exceeds threshold)
    Vector2D pos = {141.0, 130.0};
    bool drag = checkDragThreshold(pos);
    EXPECT_TRUE(drag);
    EXPECT_TRUE(state.dragDetected);
}

// Test zero drag threshold (edge case)
TEST_F(ConfigurableGestureTest, ZeroDragThreshold) {
    dragThreshold = 0.0f;
    simulateButtonPress(100.0, 100.0);

    // Any movement should trigger
    Vector2D pos = {100.1, 100.0};
    bool drag = checkDragThreshold(pos);
    EXPECT_TRUE(drag);
    EXPECT_TRUE(state.dragDetected);
}

// Test action button filtering - ignore non-action buttons
TEST_F(ConfigurableGestureTest, ActionButtonFiltering) {
    actionButton = BTN_RIGHT;

    // Simulate different button presses
    EXPECT_TRUE(isActionButton(BTN_RIGHT));
    EXPECT_FALSE(isActionButton(BTN_LEFT));
    EXPECT_FALSE(isActionButton(BTN_MIDDLE));
    EXPECT_FALSE(isActionButton(275)); // BTN_SIDE
    EXPECT_FALSE(isActionButton(276)); // BTN_EXTRA
}
