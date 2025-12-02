#include <gtest/gtest.h>
#include <cmath>

// Test fixture for overlay layout calculations
class OverlayLayoutTest : public ::testing::Test {
protected:
    // Helper function to calculate layout with equal margins
    struct LayoutResult {
        float gestureRectWidth;
        float gestureRectHeight;
        float recordSquareSize;
        float horizontalMargin;
        float verticalPadding;
    };

    LayoutResult calculateLayout(float monitorWidth, float monitorHeight,
                                 int visibleGestures) {
        constexpr float PADDING = 20.0f;
        constexpr float GAP_WIDTH = 10.0f;

        const float verticalSpace = monitorHeight - (2.0f * PADDING);
        const float totalGaps = (visibleGestures - 1) * GAP_WIDTH;
        const float baseHeight = (verticalSpace - totalGaps) / visibleGestures;
        const float gestureRectHeight = baseHeight * 0.9f;
        const float gestureRectWidth = gestureRectHeight;
        const float recordSquareSize = verticalSpace;
        const float totalWidth = gestureRectWidth + recordSquareSize;
        const float horizontalMargin = (monitorWidth - totalWidth) / 3.0f;

        return {
            gestureRectWidth,
            gestureRectHeight,
            recordSquareSize,
            horizontalMargin,
            PADDING
        };
    }
};

// Test that gesture rectangles are squares
TEST_F(OverlayLayoutTest, GestureRectanglesAreSquares) {
    auto layout = calculateLayout(1920.0f, 1080.0f, 3);
    EXPECT_FLOAT_EQ(layout.gestureRectWidth, layout.gestureRectHeight);
}

// Test that record square matches vertical space
TEST_F(OverlayLayoutTest, RecordSquareMatchesVerticalSpace) {
    constexpr float PADDING = 20.0f;
    float monitorHeight = 1080.0f;
    float expectedVerticalSpace = monitorHeight - (2.0f * PADDING);

    auto layout = calculateLayout(1920.0f, monitorHeight, 3);
    EXPECT_FLOAT_EQ(layout.recordSquareSize, expectedVerticalSpace);
}

// Test equal margins calculation
TEST_F(OverlayLayoutTest, EqualMarginsCalculation) {
    float monitorWidth = 1920.0f;
    auto layout = calculateLayout(monitorWidth, 1080.0f, 3);

    // Total width should be gesture rect + record square
    float totalWidth = layout.gestureRectWidth + layout.recordSquareSize;

    // Remaining space divided by 3 should give equal margins
    float expectedMargin = (monitorWidth - totalWidth) / 3.0f;
    EXPECT_FLOAT_EQ(layout.horizontalMargin, expectedMargin);

    // Verify all three margins are equal by checking total width
    float reconstructedWidth = layout.horizontalMargin * 3 + totalWidth;
    EXPECT_NEAR(reconstructedWidth, monitorWidth, 0.01f);
}

// Test layout with different monitor sizes
TEST_F(OverlayLayoutTest, DifferentMonitorSizes) {
    // 1080p
    auto layout1080 = calculateLayout(1920.0f, 1080.0f, 3);
    EXPECT_GT(layout1080.gestureRectWidth, 0.0f);
    EXPECT_GT(layout1080.horizontalMargin, 0.0f);

    // 1440p
    auto layout1440 = calculateLayout(2560.0f, 1440.0f, 3);
    EXPECT_GT(layout1440.gestureRectWidth, layout1080.gestureRectWidth);
    EXPECT_GT(layout1440.recordSquareSize, layout1080.recordSquareSize);

    // 4K
    auto layout4k = calculateLayout(3840.0f, 2160.0f, 3);
    EXPECT_GT(layout4k.gestureRectWidth, layout1440.gestureRectWidth);
    EXPECT_GT(layout4k.recordSquareSize, layout1440.recordSquareSize);
}

// Test that gesture rectangles are 90% of base height
TEST_F(OverlayLayoutTest, GestureRectanglesSizeReduction) {
    constexpr float PADDING = 20.0f;
    constexpr float GAP_WIDTH = 10.0f;
    constexpr int VISIBLE_GESTURES = 3;

    float monitorHeight = 1080.0f;
    float verticalSpace = monitorHeight - (2.0f * PADDING);
    float totalGaps = (VISIBLE_GESTURES - 1) * GAP_WIDTH;
    float baseHeight = (verticalSpace - totalGaps) / VISIBLE_GESTURES;
    float expectedGestureHeight = baseHeight * 0.9f;

    auto layout = calculateLayout(1920.0f, monitorHeight, VISIBLE_GESTURES);
    EXPECT_FLOAT_EQ(layout.gestureRectHeight, expectedGestureHeight);
}

// Test vertical positioning with scroll offset
TEST_F(OverlayLayoutTest, VerticalPositioningWithScroll) {
    constexpr float PADDING = 20.0f;
    constexpr float GAP_WIDTH = 10.0f;
    float gestureRectHeight = 300.0f;
    float scrollOffset = 50.0f;

    // Calculate positions for first 3 gestures
    std::vector<float> positions;
    for (size_t i = 0; i < 3; ++i) {
        float yPos = PADDING + i * (gestureRectHeight + GAP_WIDTH) -
                    scrollOffset;
        positions.push_back(yPos);
    }

    // First gesture should be above its unscrolled position
    EXPECT_LT(positions[0], PADDING);

    // Each gesture should be GAP_WIDTH + gestureRectHeight apart
    EXPECT_FLOAT_EQ(positions[1] - positions[0],
                   gestureRectHeight + GAP_WIDTH);
    EXPECT_FLOAT_EQ(positions[2] - positions[1],
                   gestureRectHeight + GAP_WIDTH);
}

// Test scroll offset clamping
TEST_F(OverlayLayoutTest, ScrollOffsetClamping) {
    constexpr float PADDING = 20.0f;
    constexpr float GAP_WIDTH = 10.0f;
    constexpr int VISIBLE_GESTURES = 3;
    constexpr int TOTAL_GESTURES = 10;

    float monitorHeight = 1080.0f;
    auto layout = calculateLayout(1920.0f, monitorHeight, VISIBLE_GESTURES);

    float verticalSpace = monitorHeight - (2.0f * PADDING);
    float totalHeight = TOTAL_GESTURES *
                       (layout.gestureRectHeight + GAP_WIDTH);
    float maxScrollOffset = totalHeight - verticalSpace;

    EXPECT_GT(maxScrollOffset, 0.0f);

    // Test clamping at minimum
    float scrollOffset = -10.0f;
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);
    EXPECT_FLOAT_EQ(scrollOffset, 0.0f);

    // Test clamping at maximum
    scrollOffset = maxScrollOffset + 100.0f;
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);
    EXPECT_FLOAT_EQ(scrollOffset, maxScrollOffset);

    // Test value in range
    scrollOffset = maxScrollOffset / 2.0f;
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);
    EXPECT_FLOAT_EQ(scrollOffset, maxScrollOffset / 2.0f);
}

// Test visibility culling for gestures
TEST_F(OverlayLayoutTest, VisibilityCulling) {
    constexpr float PADDING = 20.0f;
    constexpr float GAP_WIDTH = 10.0f;
    float monitorHeight = 1080.0f;
    float gestureRectHeight = 300.0f;
    float scrollOffset = 400.0f;

    for (size_t i = 0; i < 10; ++i) {
        float yPos = PADDING + i * (gestureRectHeight + GAP_WIDTH) -
                    scrollOffset;

        // Check if gesture is visible
        bool isVisible = !(yPos + gestureRectHeight < 0 ||
                          yPos > monitorHeight);

        // Gesture 0 should be completely above viewport
        if (i == 0) {
            EXPECT_FALSE(isVisible);
        }
        // Gestures 1-4 should be visible (1 is partially visible)
        else if (i >= 1 && i <= 4) {
            EXPECT_TRUE(isVisible);
        }
    }
}

// Test aspect ratio preservation for background images
TEST_F(OverlayLayoutTest, BackgroundImageAspectRatio) {
    float monitorWidth = 1920.0f;
    float monitorHeight = 1080.0f;
    float texWidth = 3840.0f;
    float texHeight = 2160.0f;

    float monitorAspect = monitorWidth / monitorHeight;
    float textureAspect = texWidth / texHeight;

    EXPECT_FLOAT_EQ(monitorAspect, textureAspect);

    // Test with different aspect ratios
    float wideTexWidth = 2560.0f;
    float wideTexHeight = 1080.0f;
    float wideTextureAspect = wideTexWidth / wideTexHeight;

    EXPECT_GT(wideTextureAspect, monitorAspect);

    // When texture is wider, scale by height
    float scale = monitorHeight / wideTexHeight;
    float scaledWidth = wideTexWidth * scale;
    EXPECT_GT(scaledWidth, monitorWidth);

    // X offset should center the image
    float expectedXOffset = -(scaledWidth - monitorWidth) / 2.0f;
    EXPECT_LT(expectedXOffset, 0.0f);
}

// Test gesture pattern point scaling
TEST_F(OverlayLayoutTest, GesturePatternScaling) {
    constexpr float INNER_PADDING = 10.0f;
    float squareSize = 300.0f;
    float squareX = 100.0f;
    float squareY = 200.0f;

    float drawWidth = squareSize - 2 * INNER_PADDING;
    float drawHeight = squareSize - 2 * INNER_PADDING;

    // Test point at (0.5, 0.5) should be in center
    float normalizedX = 0.5f;
    float normalizedY = 0.5f;

    float px = squareX + INNER_PADDING + normalizedX * drawWidth;
    float py = squareY + INNER_PADDING + normalizedY * drawHeight;

    EXPECT_FLOAT_EQ(px, squareX + squareSize / 2.0f);
    EXPECT_FLOAT_EQ(py, squareY + squareSize / 2.0f);

    // Test point at (0, 0) should be at top-left corner of draw area
    px = squareX + INNER_PADDING + 0.0f * drawWidth;
    py = squareY + INNER_PADDING + 0.0f * drawHeight;

    EXPECT_FLOAT_EQ(px, squareX + INNER_PADDING);
    EXPECT_FLOAT_EQ(py, squareY + INNER_PADDING);

    // Test point at (1, 1) should be at bottom-right corner of draw area
    px = squareX + INNER_PADDING + 1.0f * drawWidth;
    py = squareY + INNER_PADDING + 1.0f * drawHeight;

    EXPECT_FLOAT_EQ(px, squareX + squareSize - INNER_PADDING);
    EXPECT_FLOAT_EQ(py, squareY + squareSize - INNER_PADDING);
}
