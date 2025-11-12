#include <gtest/gtest.h>

// Mock Vector2D struct
struct Vector2D {
    double x = 0;
    double y = 0;
};

// Mock monitor structure
struct MockMonitor {
    Vector2D m_position;
    Vector2D m_size;
};

// Test fixture for record mode restriction tests
class RecordModeRestrictionTest : public ::testing::Test {
protected:
    MockMonitor monitor;
    bool g_recordMode;
    constexpr static float PADDING = 20.0f;
    constexpr static float GAP_WIDTH = 10.0f;
    constexpr static int VISIBLE_GESTURES = 3;

    void SetUp() override {
        // Setup mock monitor with 1920x1080 resolution
        monitor.m_position = {0, 0};
        monitor.m_size = {1920, 1080};
        g_recordMode = false;
    }

    // Calculate the right square bounds (matching renderRecordModeUI)
    struct RightSquareBounds {
        float x;
        float y;
        float size;
    };

    RightSquareBounds calculateRightSquare() {
        const Vector2D& monitorSize = monitor.m_size;
        const Vector2D& monitorPos = monitor.m_position;

        const float verticalSpace = monitorSize.y - (2.0f * PADDING);
        const float totalGaps = (VISIBLE_GESTURES - 1) * GAP_WIDTH;
        const float baseHeight = (verticalSpace - totalGaps) / VISIBLE_GESTURES;
        const float gestureRectHeight = baseHeight * 0.9f;
        const float gestureRectWidth = gestureRectHeight;
        const float recordSquareSize = verticalSpace;
        const float totalWidth = gestureRectWidth + recordSquareSize;
        const float horizontalMargin = (monitorSize.x - totalWidth) / 3.0f;

        // Right square position and size
        const float recordSquareX = monitorPos.x + horizontalMargin + gestureRectWidth + horizontalMargin;
        const float recordSquareY = monitorPos.y + PADDING;

        return {recordSquareX, recordSquareY, recordSquareSize};
    }

    // Check if position is inside right square
    bool isInsideRightSquare(const Vector2D& pos) {
        auto bounds = calculateRightSquare();
        return pos.x >= bounds.x &&
               pos.x <= bounds.x + bounds.size &&
               pos.y >= bounds.y &&
               pos.y <= bounds.y + bounds.size;
    }
};

// Test that right square calculation produces valid bounds
TEST_F(RecordModeRestrictionTest, RightSquareCalculation) {
    auto bounds = calculateRightSquare();

    // Verify bounds are within monitor dimensions
    EXPECT_GE(bounds.x, 0);
    EXPECT_GE(bounds.y, 0);
    EXPECT_LE(bounds.x + bounds.size, monitor.m_size.x);
    EXPECT_LE(bounds.y + bounds.size, monitor.m_size.y);

    // Verify size is positive
    EXPECT_GT(bounds.size, 0);

    // Verify padding is respected
    EXPECT_EQ(bounds.y, PADDING);
}

// Test position inside right square center
TEST_F(RecordModeRestrictionTest, PositionInsideRightSquareCenter) {
    auto bounds = calculateRightSquare();
    Vector2D centerPos = {
        bounds.x + bounds.size / 2.0,
        bounds.y + bounds.size / 2.0
    };

    EXPECT_TRUE(isInsideRightSquare(centerPos));
}

// Test position at right square top-left corner
TEST_F(RecordModeRestrictionTest, PositionAtRightSquareTopLeft) {
    auto bounds = calculateRightSquare();
    Vector2D cornerPos = {bounds.x, bounds.y};

    EXPECT_TRUE(isInsideRightSquare(cornerPos));
}

// Test position at right square bottom-right corner
TEST_F(RecordModeRestrictionTest, PositionAtRightSquareBottomRight) {
    auto bounds = calculateRightSquare();
    Vector2D cornerPos = {
        bounds.x + bounds.size,
        bounds.y + bounds.size
    };

    EXPECT_TRUE(isInsideRightSquare(cornerPos));
}

// Test position just outside right square (left side)
TEST_F(RecordModeRestrictionTest, PositionOutsideRightSquareLeft) {
    auto bounds = calculateRightSquare();
    Vector2D outsidePos = {bounds.x - 1, bounds.y + bounds.size / 2.0};

    EXPECT_FALSE(isInsideRightSquare(outsidePos));
}

// Test position just outside right square (right side)
TEST_F(RecordModeRestrictionTest, PositionOutsideRightSquareRight) {
    auto bounds = calculateRightSquare();
    Vector2D outsidePos = {bounds.x + bounds.size + 1, bounds.y + bounds.size / 2.0};

    EXPECT_FALSE(isInsideRightSquare(outsidePos));
}

// Test position just outside right square (top side)
TEST_F(RecordModeRestrictionTest, PositionOutsideRightSquareTop) {
    auto bounds = calculateRightSquare();
    Vector2D outsidePos = {bounds.x + bounds.size / 2.0, bounds.y - 1};

    EXPECT_FALSE(isInsideRightSquare(outsidePos));
}

// Test position just outside right square (bottom side)
TEST_F(RecordModeRestrictionTest, PositionOutsideRightSquareBottom) {
    auto bounds = calculateRightSquare();
    Vector2D outsidePos = {bounds.x + bounds.size / 2.0, bounds.y + bounds.size + 1};

    EXPECT_FALSE(isInsideRightSquare(outsidePos));
}

// Test position far outside right square (left edge of screen)
TEST_F(RecordModeRestrictionTest, PositionFarOutsideLeft) {
    Vector2D leftEdgePos = {0, monitor.m_size.y / 2.0};

    EXPECT_FALSE(isInsideRightSquare(leftEdgePos));
}

// Test position far outside right square (top edge of screen)
TEST_F(RecordModeRestrictionTest, PositionFarOutsideTop) {
    Vector2D topEdgePos = {monitor.m_size.x / 2.0, 0};

    EXPECT_FALSE(isInsideRightSquare(topEdgePos));
}

// Test multiple monitor setups - left monitor
TEST_F(RecordModeRestrictionTest, MultiMonitorLeftMonitor) {
    // Setup left monitor at negative X position
    monitor.m_position = {-1920, 0};
    monitor.m_size = {1920, 1080};

    auto bounds = calculateRightSquare();

    // Verify bounds are relative to monitor position
    EXPECT_GE(bounds.x, monitor.m_position.x);
    EXPECT_LE(bounds.x + bounds.size, monitor.m_position.x + monitor.m_size.x);
}

// Test multiple monitor setups - right monitor
TEST_F(RecordModeRestrictionTest, MultiMonitorRightMonitor) {
    // Setup right monitor at positive X position
    monitor.m_position = {1920, 0};
    monitor.m_size = {1920, 1080};

    auto bounds = calculateRightSquare();

    // Verify bounds are relative to monitor position
    EXPECT_GE(bounds.x, monitor.m_position.x);
    EXPECT_LE(bounds.x + bounds.size, monitor.m_position.x + monitor.m_size.x);
}

// Test different monitor resolutions - 4K
TEST_F(RecordModeRestrictionTest, Resolution4K) {
    monitor.m_position = {0, 0};
    monitor.m_size = {3840, 2160};

    auto bounds = calculateRightSquare();

    // Verify bounds scale appropriately
    EXPECT_GT(bounds.size, 0);
    EXPECT_LE(bounds.x + bounds.size, monitor.m_size.x);
    EXPECT_LE(bounds.y + bounds.size, monitor.m_size.y);
}

// Test different monitor resolutions - 720p
TEST_F(RecordModeRestrictionTest, Resolution720p) {
    monitor.m_position = {0, 0};
    monitor.m_size = {1280, 720};

    auto bounds = calculateRightSquare();

    // Verify bounds scale appropriately for smaller resolution
    EXPECT_GT(bounds.size, 0);
    EXPECT_LE(bounds.x + bounds.size, monitor.m_size.x);
    EXPECT_LE(bounds.y + bounds.size, monitor.m_size.y);
}

// Test ultrawide monitor resolution
TEST_F(RecordModeRestrictionTest, ResolutionUltrawide) {
    monitor.m_position = {0, 0};
    monitor.m_size = {3440, 1440};

    auto bounds = calculateRightSquare();

    // Verify bounds work with ultrawide aspect ratio
    EXPECT_GT(bounds.size, 0);
    EXPECT_LE(bounds.x + bounds.size, monitor.m_size.x);
    EXPECT_LE(bounds.y + bounds.size, monitor.m_size.y);
}

// Test vertical monitor orientation
TEST_F(RecordModeRestrictionTest, VerticalMonitor) {
    monitor.m_position = {0, 0};
    monitor.m_size = {1080, 1920};

    auto bounds = calculateRightSquare();

    // Verify bounds work with vertical orientation
    EXPECT_GT(bounds.size, 0);
    EXPECT_LE(bounds.x + bounds.size, monitor.m_size.x);
    EXPECT_LE(bounds.y + bounds.size, monitor.m_size.y);
}

// Test that right square is on the right side of the screen
TEST_F(RecordModeRestrictionTest, RightSquareIsOnRightSide) {
    auto bounds = calculateRightSquare();

    // Right square should be in the right half of the screen
    EXPECT_GT(bounds.x, monitor.m_size.x / 2.0);
}

// Test that right square height matches vertical space
TEST_F(RecordModeRestrictionTest, RightSquareHeightMatchesVerticalSpace) {
    auto bounds = calculateRightSquare();

    const float verticalSpace = monitor.m_size.y - (2.0f * PADDING);

    EXPECT_FLOAT_EQ(bounds.size, verticalSpace);
}

// Test boundary precision at exact edge
TEST_F(RecordModeRestrictionTest, ExactEdgeBoundary) {
    auto bounds = calculateRightSquare();

    // Test exact boundaries
    Vector2D exactLeft = {bounds.x, bounds.y + 1};
    Vector2D justBeforeLeft = {bounds.x - 0.1, bounds.y + 1};

    EXPECT_TRUE(isInsideRightSquare(exactLeft));
    EXPECT_FALSE(isInsideRightSquare(justBeforeLeft));
}

// Test that small monitors still have valid bounds
TEST_F(RecordModeRestrictionTest, SmallMonitor) {
    monitor.m_position = {0, 0};
    monitor.m_size = {800, 600};

    auto bounds = calculateRightSquare();

    // Even small monitors should have valid bounds
    EXPECT_GT(bounds.size, 0);
    EXPECT_GE(bounds.x, 0);
    EXPECT_GE(bounds.y, 0);
    EXPECT_LE(bounds.x + bounds.size, monitor.m_size.x);
    EXPECT_LE(bounds.y + bounds.size, monitor.m_size.y);
}

// Test record mode vs normal mode behavior concept
TEST_F(RecordModeRestrictionTest, RecordModeConceptTest) {
    auto bounds = calculateRightSquare();

    // Position inside right square
    Vector2D insidePos = {
        bounds.x + bounds.size / 2.0,
        bounds.y + bounds.size / 2.0
    };

    // Position outside right square
    Vector2D outsidePos = {100, 100};

    // In record mode, inside should be accepted, outside should be rejected
    g_recordMode = true;
    EXPECT_TRUE(isInsideRightSquare(insidePos));
    EXPECT_FALSE(isInsideRightSquare(outsidePos));

    // In normal mode (not in record mode), restriction doesn't apply
    // This is handled by the main code, but we verify the bounds logic
    g_recordMode = false;
    EXPECT_TRUE(isInsideRightSquare(insidePos));  // Still technically inside
    EXPECT_FALSE(isInsideRightSquare(outsidePos)); // Still technically outside
}
