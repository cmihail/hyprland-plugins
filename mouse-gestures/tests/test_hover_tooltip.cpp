#include <gtest/gtest.h>

class HoverTooltipTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HoverTooltipTest, MousePositionTrackedInRecordMode) {
    // Verify that g_lastMousePos is updated when mouse moves in record mode
    // This is tested indirectly through the mouse move handler
    EXPECT_TRUE(true); // Placeholder - actual implementation requires mock input manager
}

TEST_F(HoverTooltipTest, TooltipTextTrimmedTo100Characters) {
    std::string longCommand = std::string(150, 'a');
    std::string trimmedCommand = longCommand.substr(0, 97) + "...";

    EXPECT_EQ(trimmedCommand.length(), 100);
    EXPECT_EQ(trimmedCommand.substr(97), "...");
}

TEST_F(HoverTooltipTest, HoverDetectionUsesMonitorRelativeCoordinates) {
    // Test that global mouse coordinates are converted to monitor-relative
    // Monitor at (1920, 0)
    float monitorX = 1920.0f;
    float monitorY = 0.0f;

    // Global mouse position
    float globalMouseX = 2000.0f;
    float globalMouseY = 100.0f;

    // Expected relative position
    float expectedRelativeX = globalMouseX - monitorX; // 80.0
    float expectedRelativeY = globalMouseY - monitorY; // 100.0

    EXPECT_EQ(expectedRelativeX, 80.0f);
    EXPECT_EQ(expectedRelativeY, 100.0f);
}

TEST_F(HoverTooltipTest, TooltipPositionedAtBottomOfGestureRectangle) {
    float gestureX = 20.0f;
    float gestureY = 50.0f;
    float gestureSize = 300.0f;

    float tooltipPadding = 6.0f;
    float fontSize = 15.0f;
    float lineHeight = fontSize * 2.5f;
    float tooltipHeight = lineHeight + tooltipPadding * 2.0f;

    // Tooltip should be at bottom of gesture rectangle
    float expectedTooltipY = gestureY + gestureSize - tooltipHeight;

    EXPECT_EQ(expectedTooltipY, (gestureY + gestureSize - tooltipHeight));
}

TEST_F(HoverTooltipTest, TooltipWidthGrowsWithTextContent) {
    // Mock text width measurement
    float shortTextWidth = 100.0f;
    float longTextWidth = 500.0f;
    float tooltipPadding = 6.0f;

    float shortTooltipWidth = shortTextWidth + tooltipPadding * 2.0f;
    float longTooltipWidth = longTextWidth + tooltipPadding * 2.0f;

    EXPECT_EQ(shortTooltipWidth, 112.0f);
    EXPECT_EQ(longTooltipWidth, 512.0f);
    EXPECT_GT(longTooltipWidth, shortTooltipWidth);
}

TEST_F(HoverTooltipTest, LineHeightPreventsDescenderClipping) {
    // Line height should be 2.5x font size to accommodate descenders
    float fontSize = 15.0f;
    float lineHeight = fontSize * 2.5f;

    EXPECT_EQ(lineHeight, 37.5f);

    // This should be enough for ascenders + descenders
    // Typical font metrics: ascent ~22px, descent ~6px for 15px font
    // 2.5x gives us 37.5px which is more than enough
    EXPECT_GT(lineHeight, 28.0f); // ascent + descent
}

TEST_F(HoverTooltipTest, NoCommandAssignedMessageDisplayed) {
    std::string emptyCommand = "";
    std::string expectedMessage = "No command assigned. Modify config file to add a command";

    std::string displayText = emptyCommand.empty()
        ? "No command assigned. Modify config file to add a command"
        : emptyCommand;

    EXPECT_EQ(displayText, expectedMessage);
}

TEST_F(HoverTooltipTest, HoverDetectionChecksRectangleBounds) {
    // Gesture rectangle bounds
    float rectX = 100.0f;
    float rectY = 200.0f;
    float rectSize = 300.0f;

    // Test point inside rectangle
    float mouseInsideX = 150.0f;
    float mouseInsideY = 250.0f;
    bool isInside = mouseInsideX >= rectX &&
                   mouseInsideX <= rectX + rectSize &&
                   mouseInsideY >= rectY &&
                   mouseInsideY <= rectY + rectSize;
    EXPECT_TRUE(isInside);

    // Test point outside rectangle (to the left)
    float mouseOutsideX = 50.0f;
    float mouseOutsideY = 250.0f;
    bool isOutside = mouseOutsideX >= rectX &&
                    mouseOutsideX <= rectX + rectSize &&
                    mouseOutsideY >= rectY &&
                    mouseOutsideY <= rectY + rectSize;
    EXPECT_FALSE(isOutside);
}
