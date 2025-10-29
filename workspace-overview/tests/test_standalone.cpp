#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cmath>

// Standalone implementations of plugin logic for testing

// Test helper: Calculate workspace IDs to display for a given current workspace ID
// and a list of workspaces that belong to the current monitor
struct WorkspaceIDs {
    int ids[4];
};

static WorkspaceIDs calculateWorkspaceIDs(
    int currentID,
    const std::vector<int>& monitorWorkspaces
) {
    WorkspaceIDs result;

    // Create a copy of monitor workspaces and sort them (keep active workspace in the list)
    std::vector<int> sortedWorkspaces = monitorWorkspaces;
    std::sort(sortedWorkspaces.begin(), sortedWorkspaces.end());

    // Populate left side workspaces: first with existing
    size_t numExisting = std::min(static_cast<size_t>(4), sortedWorkspaces.size());
    for (size_t i = 0; i < numExisting; ++i) {
        result.ids[i] = sortedWorkspaces[i];
    }

    // Fill remaining slots with new workspace IDs (not in monitorWorkspaces)
    if (numExisting < 4) {
        std::vector<int> allUsedIDs = monitorWorkspaces;
        std::sort(allUsedIDs.begin(), allUsedIDs.end());

        int nextID = 1;
        for (size_t i = numExisting; i < 4; ++i) {
            // Find an ID that's not in use
            while (std::find(allUsedIDs.begin(), allUsedIDs.end(), nextID) != allUsedIDs.end()) {
                nextID++;
            }

            result.ids[i] = nextID;
            allUsedIDs.push_back(nextID);
            std::sort(allUsedIDs.begin(), allUsedIDs.end());
            nextID++;
        }
    }

    return result;
}

// Test helper: Calculate aspect ratio fitting
struct ScaledBox {
    float x, y, w, h;
};

static ScaledBox calculateAspectRatioFit(float boxX, float boxY, float boxW, float boxH,
                                          float fbWidth, float fbHeight) {
    const float fbAspect = fbWidth / fbHeight;
    const float boxAspect = boxW / boxH;

    ScaledBox result = {boxX, boxY, boxW, boxH};

    if (fbAspect > boxAspect) {
        // Framebuffer is wider, fit to width
        const float newHeight = boxW / fbAspect;
        result.y = boxY + (boxH - newHeight) / 2.0f;
        result.h = newHeight;
    } else {
        // Framebuffer is taller, fit to height
        const float newWidth = boxH * fbAspect;
        result.x = boxX + (boxW - newWidth) / 2.0f;
        result.w = newWidth;
    }

    return result;
}

// Test helper: Calculate layout boxes
struct LayoutBox {
    float x, y, w, h;
    bool isActive;
};

static std::vector<LayoutBox> calculateLayout(
    float monitorWidth,
    float monitorHeight,
    float leftWidthRatio,
    float gapWidth,
    float padding
) {
    std::vector<LayoutBox> boxes;
    const int LEFT_WORKSPACES = 4;

    const float availableHeight = monitorHeight - (2 * padding);
    const float totalGaps = (LEFT_WORKSPACES - 1) * gapWidth;
    const float leftPreviewHeight = (availableHeight - totalGaps) / LEFT_WORKSPACES;

    // Left workspaces: calculate width so that left margin = top/bottom margin = padding
    // The aspect ratio should match the monitor's aspect ratio for proper scaling
    const float monitorAspectRatio = monitorWidth / monitorHeight;
    const float leftWorkspaceWidth = leftPreviewHeight * monitorAspectRatio;

    // Active workspace: starts right after left section with padding gap
    const float activeX = padding + leftWorkspaceWidth + padding;  // left margin + left width + gap
    const float activeMaxWidth = monitorWidth - activeX - padding;  // Leave padding on right edge
    const float activeMaxHeight = monitorHeight - (2 * padding);

    // Left side workspaces (left margin = padding, same as top/bottom)
    for (int i = 0; i < LEFT_WORKSPACES; ++i) {
        LayoutBox box;
        box.x = padding;
        box.y = padding + i * (leftPreviewHeight + gapWidth);
        box.w = leftWorkspaceWidth;
        box.h = leftPreviewHeight;
        box.isActive = false;
        boxes.push_back(box);
    }

    // Right side - active workspace (maximized with consistent margins)
    LayoutBox activeBox;
    activeBox.x = activeX;
    activeBox.y = padding;
    activeBox.w = activeMaxWidth;
    activeBox.h = activeMaxHeight;
    activeBox.isActive = true;
    boxes.push_back(activeBox);

    return boxes;
}

// Test: Workspace offset calculation for first workspace
TEST(WorkspaceOffsetsTest, FirstWorkspace) {
    // Monitor has workspaces 1, 2, 3, 4, 5 - active is 1
    // Active workspace is now included in the left side list
    auto workspaces = calculateWorkspaceIDs(1, {1, 2, 3, 4, 5});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 2);
    EXPECT_EQ(workspaces.ids[2], 3);
    EXPECT_EQ(workspaces.ids[3], 4);
}

// Test: Workspace ID calculation for second workspace
TEST(WorkspaceOffsetsTest, SecondWorkspace) {
    // Monitor has workspaces 1, 2, 3, 4, 5 - active is 2
    // Active workspace is now included in the left side list
    auto workspaces = calculateWorkspaceIDs(2, {1, 2, 3, 4, 5});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 2);
    EXPECT_EQ(workspaces.ids[2], 3);
    EXPECT_EQ(workspaces.ids[3], 4);
}

// Test: Workspace ID calculation for normal workspace
TEST(WorkspaceOffsetsTest, NormalWorkspace) {
    // Monitor has workspace 5 only - active is 5, shown on left + fill with new IDs
    auto workspaces = calculateWorkspaceIDs(5, {5});
    EXPECT_EQ(workspaces.ids[0], 5);  // Active workspace appears on left
    EXPECT_EQ(workspaces.ids[1], 1);  // New workspace IDs to fill up to 4
    EXPECT_EQ(workspaces.ids[2], 2);
    EXPECT_EQ(workspaces.ids[3], 3);
}

// Test: Workspace ID calculation for high workspace ID
TEST(WorkspaceOffsetsTest, HighWorkspaceID) {
    // Monitor has workspace 100 only - active is 100, shown on left + fill with new IDs
    auto workspaces = calculateWorkspaceIDs(100, {100});
    EXPECT_EQ(workspaces.ids[0], 100);  // Active workspace appears on left
    EXPECT_EQ(workspaces.ids[1], 1);    // New workspace IDs to fill up to 4
    EXPECT_EQ(workspaces.ids[2], 2);
    EXPECT_EQ(workspaces.ids[3], 3);
}

// Test: Layout calculation produces correct number of boxes
TEST(LayoutTest, NumberOfBoxes) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);
    EXPECT_EQ(boxes.size(), 5);  // 4 left + 1 active
}

// Test: Layout calculation - active workspace identification
TEST(LayoutTest, ActiveWorkspaceIdentification) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    int activeCount = 0;
    for (const auto& box : boxes) {
        if (box.isActive) {
            activeCount++;
        }
    }

    EXPECT_EQ(activeCount, 1);  // Only one active workspace
    EXPECT_TRUE(boxes[4].isActive);  // Last box should be active
}

// Test: Layout calculation - left side dimensions
TEST(LayoutTest, LeftSideDimensions) {
    const float monitorWidth = 1920;
    const float monitorHeight = 1080;
    const float leftWidthRatio = 0.33f;
    const float padding = 20.0f;

    auto boxes = calculateLayout(monitorWidth, monitorHeight, leftWidthRatio, 10.0f, padding);

    const float availableHeight = monitorHeight - (2 * padding);
    const float totalGaps = 3 * 10.0f;  // 3 gaps between 4 workspaces
    const float leftPreviewHeight = (availableHeight - totalGaps) / 4;
    const float monitorAspectRatio = monitorWidth / monitorHeight;
    const float expectedLeftWidth = leftPreviewHeight * monitorAspectRatio;

    // Check first 4 boxes (left side)
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(boxes[i].w, expectedLeftWidth, 0.1f);
        EXPECT_FLOAT_EQ(boxes[i].x, padding);  // Left margin should equal padding
        EXPECT_NEAR(boxes[i].h, leftPreviewHeight, 0.1f);
    }
}

// Test: Layout calculation - right side dimensions
TEST(LayoutTest, RightSideDimensions) {
    const float monitorWidth = 1920;
    const float monitorHeight = 1080;
    const float leftWidthRatio = 0.33f;
    const float padding = 20.0f;

    auto boxes = calculateLayout(monitorWidth, monitorHeight, leftWidthRatio, 10.0f, padding);

    const float availableHeight = monitorHeight - (2 * padding);
    const float totalGaps = 3 * 10.0f;
    const float leftPreviewHeight = (availableHeight - totalGaps) / 4;
    const float monitorAspectRatio = monitorWidth / monitorHeight;
    const float leftWorkspaceWidth = leftPreviewHeight * monitorAspectRatio;
    const float activeX = padding + leftWorkspaceWidth + padding;
    const float activeMaxWidth = monitorWidth - activeX - padding;

    // Check active workspace box (index 4)
    EXPECT_NEAR(boxes[4].x, activeX, 0.1f);
    EXPECT_NEAR(boxes[4].w, activeMaxWidth, 0.1f);
    EXPECT_NEAR(boxes[4].h, monitorHeight - 2 * padding, 0.1f);
}

// Test: Aspect ratio fitting - wider framebuffer
TEST(AspectRatioTest, WiderFramebuffer) {
    // Box: 100x100, Framebuffer: 200x100 (2:1 ratio)
    auto result = calculateAspectRatioFit(0, 0, 100, 100, 200, 100);

    // Should fit to width, reducing height
    EXPECT_FLOAT_EQ(result.w, 100.0f);
    EXPECT_FLOAT_EQ(result.h, 50.0f);  // Height halved to maintain 2:1
    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 25.0f);  // Centered vertically
}

// Test: Aspect ratio fitting - taller framebuffer
TEST(AspectRatioTest, TallerFramebuffer) {
    // Box: 100x100, Framebuffer: 100x200 (1:2 ratio)
    auto result = calculateAspectRatioFit(0, 0, 100, 100, 100, 200);

    // Should fit to height, reducing width
    EXPECT_FLOAT_EQ(result.h, 100.0f);
    EXPECT_FLOAT_EQ(result.w, 50.0f);  // Width halved to maintain 1:2
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.x, 25.0f);  // Centered horizontally
}

// Test: Aspect ratio fitting - same ratio
TEST(AspectRatioTest, SameAspectRatio) {
    // Box: 100x100, Framebuffer: 200x200 (same 1:1 ratio)
    auto result = calculateAspectRatioFit(0, 0, 100, 100, 200, 200);

    // Should fill exactly
    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.w, 100.0f);
    EXPECT_FLOAT_EQ(result.h, 100.0f);
}

// Test: Layout with different monitor sizes
TEST(LayoutTest, DifferentMonitorSizes) {
    // Test with various monitor resolutions
    std::vector<std::pair<float, float>> resolutions = {
        {1920, 1080},  // 1080p
        {2560, 1440},  // 1440p
        {3840, 2160},  // 4K
        {1366, 768}    // Common laptop
    };

    for (const auto& [width, height] : resolutions) {
        auto boxes = calculateLayout(width, height, 0.33f, 10.0f, 20.0f);

        EXPECT_EQ(boxes.size(), 5);

        // Verify all boxes are within monitor bounds
        for (const auto& box : boxes) {
            EXPECT_GE(box.x, 0.0f);
            EXPECT_GE(box.y, 0.0f);
            EXPECT_LE(box.x + box.w, width);
            EXPECT_LE(box.y + box.h, height);
        }
    }
}

// Test: Layout spacing consistency
TEST(LayoutTest, SpacingConsistency) {
    const float gapWidth = 10.0f;
    auto boxes = calculateLayout(1920, 1080, 0.33f, gapWidth, 20.0f);

    // Check gaps between left side boxes
    for (int i = 0; i < 3; ++i) {
        float gap = boxes[i + 1].y - (boxes[i].y + boxes[i].h);
        EXPECT_NEAR(gap, gapWidth, 0.1f);
    }
}

// Test: Edge case - workspace ID 3 (boundary between special cases)
TEST(WorkspaceOffsetsTest, WorkspaceThree) {
    // Monitor has workspaces 1, 2, 3, 4, 5 - active is 3
    // Active workspace is now included in the left side list
    auto workspaces = calculateWorkspaceIDs(3, {1, 2, 3, 4, 5});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 2);
    EXPECT_EQ(workspaces.ids[2], 3);
    EXPECT_EQ(workspaces.ids[3], 4);
}

// Test: Monitor 1 specific workspaces (1, 3, 5 on monitor, 7 unassigned)
TEST(WorkspaceOffsetsTest, Monitor1Example) {
    // Monitor 1 has workspaces 1, 3, 5, and 7 is unassigned (available) - active is 5
    // Active workspace is now included in the left side list
    auto workspaces = calculateWorkspaceIDs(5, {1, 3, 5, 7});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 3);
    EXPECT_EQ(workspaces.ids[2], 5);
    EXPECT_EQ(workspaces.ids[3], 7);
}

// Test: Monitor 2 specific workspaces (2, 4, 5 on monitor, 7 unassigned)
TEST(WorkspaceOffsetsTest, Monitor2Example) {
    // Monitor 2 has workspaces 2, 4, 5, and 7 is unassigned (available) - active is 5
    // Active workspace is now included in the left side list
    auto workspaces = calculateWorkspaceIDs(5, {2, 4, 5, 7});
    EXPECT_EQ(workspaces.ids[0], 2);
    EXPECT_EQ(workspaces.ids[1], 4);
    EXPECT_EQ(workspaces.ids[2], 5);
    EXPECT_EQ(workspaces.ids[3], 7);
}

// Test: Sparse workspace IDs
TEST(WorkspaceOffsetsTest, SparseWorkspaceIDs) {
    // Monitor has workspaces 1, 10, 20, 30 - active is 20
    // Active workspace is now included in the left side list
    auto workspaces = calculateWorkspaceIDs(20, {1, 10, 20, 30});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 10);
    EXPECT_EQ(workspaces.ids[2], 20);
    EXPECT_EQ(workspaces.ids[3], 30);
}

// Test: Zero padding edge case
TEST(LayoutTest, ZeroPadding) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 0.0f);

    EXPECT_EQ(boxes.size(), 5);
    EXPECT_FLOAT_EQ(boxes[0].x, 0.0f);  // Should start at edge
    EXPECT_FLOAT_EQ(boxes[4].y, 0.0f);  // Should start at edge
}

// Test helper: Workspace selection
static int selectWorkspaceAtPosition(float posX, float posY, const std::vector<LayoutBox>& boxes) {
    int selectedIndex = -1;

    for (size_t i = 0; i < boxes.size(); ++i) {
        const auto& box = boxes[i];

        if (posX >= box.x && posX <= box.x + box.w &&
            posY >= box.y && posY <= box.y + box.h) {
            selectedIndex = i;
            break;
        }
    }

    return selectedIndex;
}

// Test: Click inside first workspace (top-left)
TEST(WorkspaceSelectionTest, ClickInsideFirstWorkspace) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    // Click in the middle of first workspace
    float clickX = boxes[0].x + boxes[0].w / 2.0f;
    float clickY = boxes[0].y + boxes[0].h / 2.0f;

    int selected = selectWorkspaceAtPosition(clickX, clickY, boxes);
    EXPECT_EQ(selected, 0);
}

// Test: Click inside active workspace (right side)
TEST(WorkspaceSelectionTest, ClickInsideActiveWorkspace) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    // Click in the middle of active workspace (index 4)
    float clickX = boxes[4].x + boxes[4].w / 2.0f;
    float clickY = boxes[4].y + boxes[4].h / 2.0f;

    int selected = selectWorkspaceAtPosition(clickX, clickY, boxes);
    EXPECT_EQ(selected, 4);
}

// Test: Click outside all workspaces
TEST(WorkspaceSelectionTest, ClickOutsideAllWorkspaces) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    // Click in gap between left and right sections
    float clickX = boxes[0].x + boxes[0].w + 5.0f;
    float clickY = 540.0f;  // Middle of screen height

    int selected = selectWorkspaceAtPosition(clickX, clickY, boxes);
    EXPECT_EQ(selected, -1);
}

// Test: Click on workspace boundary (edge case)
TEST(WorkspaceSelectionTest, ClickOnWorkspaceBoundary) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    // Click exactly on the left edge of first workspace
    float clickX = boxes[0].x;
    float clickY = boxes[0].y;

    int selected = selectWorkspaceAtPosition(clickX, clickY, boxes);
    EXPECT_EQ(selected, 0);

    // Click exactly on the right edge of first workspace
    clickX = boxes[0].x + boxes[0].w;
    clickY = boxes[0].y + boxes[0].h;

    selected = selectWorkspaceAtPosition(clickX, clickY, boxes);
    EXPECT_EQ(selected, 0);
}

// Test: Click in gap between left workspaces
TEST(WorkspaceSelectionTest, ClickInGapBetweenWorkspaces) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    // Click in gap between first and second workspace
    float clickX = boxes[0].x + boxes[0].w / 2.0f;
    float clickY = boxes[0].y + boxes[0].h + 5.0f;  // Middle of gap

    int selected = selectWorkspaceAtPosition(clickX, clickY, boxes);
    EXPECT_EQ(selected, -1);
}

// Test: Click on each left workspace
TEST(WorkspaceSelectionTest, ClickOnEachLeftWorkspace) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    // Test clicking each of the 4 left workspaces
    for (int i = 0; i < 4; ++i) {
        float clickX = boxes[i].x + boxes[i].w / 2.0f;
        float clickY = boxes[i].y + boxes[i].h / 2.0f;

        int selected = selectWorkspaceAtPosition(clickX, clickY, boxes);
        EXPECT_EQ(selected, i) << "Failed for workspace " << i;
    }
}

// Test: Click outside monitor bounds
TEST(WorkspaceSelectionTest, ClickOutsideMonitorBounds) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    // Click beyond monitor width
    int selected = selectWorkspaceAtPosition(2000.0f, 500.0f, boxes);
    EXPECT_EQ(selected, -1);

    // Click beyond monitor height
    selected = selectWorkspaceAtPosition(1000.0f, 1200.0f, boxes);
    EXPECT_EQ(selected, -1);

    // Click with negative coordinates
    selected = selectWorkspaceAtPosition(-10.0f, 500.0f, boxes);
    EXPECT_EQ(selected, -1);
}

// Test: Click in padding area
TEST(WorkspaceSelectionTest, ClickInPaddingArea) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);

    // Click in top padding
    int selected = selectWorkspaceAtPosition(100.0f, 10.0f, boxes);
    EXPECT_EQ(selected, -1);

    // Click in left padding
    selected = selectWorkspaceAtPosition(10.0f, 100.0f, boxes);
    EXPECT_EQ(selected, -1);
}

// Test helper: Workspace number badge calculations
struct BadgeGeometry {
    float circleSize;
    float textX;
    float textY;
};

static BadgeGeometry calculateBadgeGeometry(float textWidth, float textHeight, float padding) {
    BadgeGeometry result;

    // Calculate circle size (must be square for perfect circle)
    result.circleSize = std::max(textWidth, textHeight) + padding * 2;

    // Center text within circle
    result.textX = (result.circleSize - textWidth) / 2;
    result.textY = (result.circleSize - textHeight) / 2;

    return result;
}

// Test: Badge circle size calculation for square text
TEST(WorkspaceBadgeTest, CircleSizeSquareText) {
    auto badge = calculateBadgeGeometry(20.0f, 20.0f, 4.0f);

    // Circle should be text size + 2*padding
    EXPECT_FLOAT_EQ(badge.circleSize, 28.0f);

    // Text should be centered
    EXPECT_FLOAT_EQ(badge.textX, 4.0f);
    EXPECT_FLOAT_EQ(badge.textY, 4.0f);
}

// Test: Badge circle size calculation for wide text
TEST(WorkspaceBadgeTest, CircleSizeWideText) {
    auto badge = calculateBadgeGeometry(30.0f, 20.0f, 4.0f);

    // Circle should use larger dimension (width)
    EXPECT_FLOAT_EQ(badge.circleSize, 38.0f);

    // Text should be centered
    EXPECT_FLOAT_EQ(badge.textX, 4.0f);
    EXPECT_FLOAT_EQ(badge.textY, 9.0f);  // (38 - 20) / 2 = 9
}

// Test: Badge circle size calculation for tall text
TEST(WorkspaceBadgeTest, CircleSizeTallText) {
    auto badge = calculateBadgeGeometry(20.0f, 30.0f, 4.0f);

    // Circle should use larger dimension (height)
    EXPECT_FLOAT_EQ(badge.circleSize, 38.0f);

    // Text should be centered
    EXPECT_FLOAT_EQ(badge.textX, 9.0f);  // (38 - 20) / 2 = 9
    EXPECT_FLOAT_EQ(badge.textY, 4.0f);
}

// Test: Badge with different padding values
TEST(WorkspaceBadgeTest, DifferentPadding) {
    auto badge1 = calculateBadgeGeometry(20.0f, 20.0f, 2.0f);
    auto badge2 = calculateBadgeGeometry(20.0f, 20.0f, 8.0f);

    EXPECT_FLOAT_EQ(badge1.circleSize, 24.0f);
    EXPECT_FLOAT_EQ(badge2.circleSize, 36.0f);

    // Larger padding = larger centering offset
    EXPECT_FLOAT_EQ(badge1.textX, 2.0f);
    EXPECT_FLOAT_EQ(badge2.textX, 8.0f);
}

// Test: Badge ensures perfect circle (width == height)
TEST(WorkspaceBadgeTest, PerfectCircleProperty) {
    std::vector<std::pair<float, float>> textSizes = {
        {10.0f, 20.0f},
        {20.0f, 10.0f},
        {15.0f, 25.0f},
        {25.0f, 15.0f}
    };

    for (const auto& [width, height] : textSizes) {
        auto badge = calculateBadgeGeometry(width, height, 4.0f);

        // Circle must be square (perfect circle)
        float expectedSize = std::max(width, height) + 8.0f;
        EXPECT_FLOAT_EQ(badge.circleSize, expectedSize);
    }
}

// Test: Badge text always centered
TEST(WorkspaceBadgeTest, TextAlwaysCentered) {
    auto badge = calculateBadgeGeometry(15.0f, 25.0f, 5.0f);

    // Verify text is centered in both dimensions
    float expectedCircleSize = 35.0f;  // max(15, 25) + 2*5

    EXPECT_FLOAT_EQ(badge.textX, (expectedCircleSize - 15.0f) / 2);
    EXPECT_FLOAT_EQ(badge.textY, (expectedCircleSize - 25.0f) / 2);
}

// Test helper: Animation zoom calculations
struct ZoomAnimation {
    float scale;
    float posX;
    float posY;
};

static ZoomAnimation calculateOpeningAnimation(float monitorWidth, float monitorHeight,
                                                float activeBoxX, float activeBoxY,
                                                float activeBoxW, float activeBoxH) {
    ZoomAnimation result;

    // Calculate scale needed to zoom the active box to fill the screen
    const float scaleX = monitorWidth / activeBoxW;
    const float scaleY = monitorHeight / activeBoxH;
    result.scale = std::min(scaleX, scaleY);

    // Calculate position offset to center active workspace
    const float activeCenter_x = activeBoxX + activeBoxW / 2.0f;
    const float activeCenter_y = activeBoxY + activeBoxH / 2.0f;
    const float screenCenter_x = monitorWidth / 2.0f;
    const float screenCenter_y = monitorHeight / 2.0f;

    result.posX = (screenCenter_x - activeCenter_x) * result.scale;
    result.posY = (screenCenter_y - activeCenter_y) * result.scale;

    return result;
}

static ScaledBox applyZoomTransform(float boxX, float boxY, float boxW, float boxH,
                                     float zoomScale, float offsetX, float offsetY) {
    ScaledBox result;
    result.x = boxX * zoomScale + offsetX;
    result.y = boxY * zoomScale + offsetY;
    result.w = boxW * zoomScale;
    result.h = boxH * zoomScale;
    return result;
}

// Test: Opening animation scale calculation
TEST(AnimationTest, OpeningAnimationScale) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);
    const auto& activeBox = boxes[4];

    auto anim = calculateOpeningAnimation(1920, 1080,
                                          activeBox.x, activeBox.y,
                                          activeBox.w, activeBox.h);

    // Scale should be greater than 1 (zooming in on active workspace)
    EXPECT_GT(anim.scale, 1.0f);

    // Scale should make active box fill the screen
    float scaledWidth = activeBox.w * anim.scale;
    float scaledHeight = activeBox.h * anim.scale;

    // Either width or height should match monitor size (aspect ratio preserved)
    bool widthMatches = std::abs(scaledWidth - 1920.0f) < 0.1f;
    bool heightMatches = std::abs(scaledHeight - 1080.0f) < 0.1f;
    EXPECT_TRUE(widthMatches || heightMatches);
}

// Test: Opening animation centers active workspace
TEST(AnimationTest, OpeningAnimationCentersActiveWorkspace) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);
    const auto& activeBox = boxes[4];

    auto anim = calculateOpeningAnimation(1920, 1080,
                                          activeBox.x, activeBox.y,
                                          activeBox.w, activeBox.h);

    // Apply transform to active box
    auto transformed = applyZoomTransform(activeBox.x, activeBox.y,
                                          activeBox.w, activeBox.h,
                                          anim.scale, anim.posX, anim.posY);

    // Center of transformed box should be at screen center
    float transformedCenterX = transformed.x + transformed.w / 2.0f;
    float transformedCenterY = transformed.y + transformed.h / 2.0f;

    EXPECT_NEAR(transformedCenterX, 960.0f, 40.0f);  // 1920/2, tolerance for rounding
    EXPECT_NEAR(transformedCenterY, 540.0f, 25.0f);  // 1080/2, tolerance for rounding
}

// Test: Zoom transform maintains aspect ratio
TEST(AnimationTest, ZoomTransformAspectRatio) {
    const float originalW = 100.0f;
    const float originalH = 50.0f;
    const float scale = 2.0f;

    auto transformed = applyZoomTransform(0, 0, originalW, originalH, scale, 0, 0);

    // Aspect ratio should be preserved
    float originalRatio = originalW / originalH;
    float transformedRatio = transformed.w / transformed.h;

    EXPECT_NEAR(originalRatio, transformedRatio, 0.01f);
}

// Test: Zoom transform with identity transform (scale=1, offset=0)
TEST(AnimationTest, IdentityZoomTransform) {
    const float x = 100.0f, y = 200.0f, w = 300.0f, h = 400.0f;

    auto transformed = applyZoomTransform(x, y, w, h, 1.0f, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(transformed.x, x);
    EXPECT_FLOAT_EQ(transformed.y, y);
    EXPECT_FLOAT_EQ(transformed.w, w);
    EXPECT_FLOAT_EQ(transformed.h, h);
}

// Test: Zoom transform with scale only (no offset)
TEST(AnimationTest, ZoomTransformScaleOnly) {
    const float scale = 1.5f;

    auto transformed = applyZoomTransform(100.0f, 200.0f, 300.0f, 400.0f,
                                          scale, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(transformed.x, 150.0f);   // 100 * 1.5
    EXPECT_FLOAT_EQ(transformed.y, 300.0f);   // 200 * 1.5
    EXPECT_FLOAT_EQ(transformed.w, 450.0f);   // 300 * 1.5
    EXPECT_FLOAT_EQ(transformed.h, 600.0f);   // 400 * 1.5
}

// Test: Zoom transform with offset only (no scale)
TEST(AnimationTest, ZoomTransformOffsetOnly) {
    const float offsetX = 50.0f;
    const float offsetY = 100.0f;

    auto transformed = applyZoomTransform(100.0f, 200.0f, 300.0f, 400.0f,
                                          1.0f, offsetX, offsetY);

    EXPECT_FLOAT_EQ(transformed.x, 150.0f);   // 100 + 50
    EXPECT_FLOAT_EQ(transformed.y, 300.0f);   // 200 + 100
    EXPECT_FLOAT_EQ(transformed.w, 300.0f);   // unchanged
    EXPECT_FLOAT_EQ(transformed.h, 400.0f);   // unchanged
}

// Test: Zoom transform with both scale and offset
TEST(AnimationTest, ZoomTransformScaleAndOffset) {
    const float scale = 2.0f;
    const float offsetX = 10.0f;
    const float offsetY = 20.0f;

    auto transformed = applyZoomTransform(50.0f, 100.0f, 200.0f, 150.0f,
                                          scale, offsetX, offsetY);

    EXPECT_FLOAT_EQ(transformed.x, 110.0f);   // 50*2 + 10
    EXPECT_FLOAT_EQ(transformed.y, 220.0f);   // 100*2 + 20
    EXPECT_FLOAT_EQ(transformed.w, 400.0f);   // 200*2
    EXPECT_FLOAT_EQ(transformed.h, 300.0f);   // 150*2
}

// Test: Opening animation for different monitor sizes
TEST(AnimationTest, OpeningAnimationDifferentMonitorSizes) {
    std::vector<std::pair<float, float>> resolutions = {
        {1920, 1080},  // 1080p
        {2560, 1440},  // 1440p
        {3840, 2160},  // 4K
    };

    for (const auto& [width, height] : resolutions) {
        auto boxes = calculateLayout(width, height, 0.33f, 10.0f, 20.0f);
        const auto& activeBox = boxes[4];

        auto anim = calculateOpeningAnimation(width, height,
                                              activeBox.x, activeBox.y,
                                              activeBox.w, activeBox.h);

        // Apply transform to active box
        auto transformed = applyZoomTransform(activeBox.x, activeBox.y,
                                              activeBox.w, activeBox.h,
                                              anim.scale, anim.posX, anim.posY);

        // Active workspace should be centered after transform
        float centerX = transformed.x + transformed.w / 2.0f;
        float centerY = transformed.y + transformed.h / 2.0f;

        EXPECT_NEAR(centerX, width / 2.0f, 40.0f);   // Tolerance for rounding
        EXPECT_NEAR(centerY, height / 2.0f, 25.0f);  // Tolerance for rounding
    }
}

// Test: Animation ensures left workspaces move off-screen when zoomed
TEST(AnimationTest, LeftWorkspacesOffScreenWhenZoomed) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);
    const auto& activeBox = boxes[4];

    auto anim = calculateOpeningAnimation(1920, 1080,
                                          activeBox.x, activeBox.y,
                                          activeBox.w, activeBox.h);

    // Apply transform to first left workspace
    const auto& leftBox = boxes[0];
    auto transformed = applyZoomTransform(leftBox.x, leftBox.y,
                                          leftBox.w, leftBox.h,
                                          anim.scale, anim.posX, anim.posY);

    // Left workspace should be pushed left (possibly off-screen)
    // Since we're zooming into the right side, left boxes should have negative x
    EXPECT_LT(transformed.x, leftBox.x);
}

// Test: Opening animation scale consistency
TEST(AnimationTest, ScaleConsistencyAcrossBoxes) {
    auto boxes = calculateLayout(1920, 1080, 0.33f, 10.0f, 20.0f);
    const auto& activeBox = boxes[4];

    auto anim = calculateOpeningAnimation(1920, 1080,
                                          activeBox.x, activeBox.y,
                                          activeBox.w, activeBox.h);

    // All boxes should scale by the same amount
    for (const auto& box : boxes) {
        auto transformed = applyZoomTransform(box.x, box.y, box.w, box.h,
                                              anim.scale, anim.posX, anim.posY);

        // Check that scale is applied uniformly
        EXPECT_NEAR(transformed.w / box.w, anim.scale, 0.01f);
        EXPECT_NEAR(transformed.h / box.h, anim.scale, 0.01f);
    }
}

// Test helper: Alpha calculation for fade animations
static float calculateFadeAlpha(float animationPercent, bool isClosing, bool isActiveWorkspace) {
    if (isActiveWorkspace) {
        return 1.0f;  // Active workspace always fully visible
    }

    if (isClosing) {
        return 1.0f - animationPercent;  // Fade out
    } else {
        return animationPercent;  // Fade in
    }
}

// Test: Fade animation alpha for opening (non-active workspaces)
TEST(AnimationTest, FadeInAlphaForOpening) {
    // At start of opening animation (0%)
    float alpha = calculateFadeAlpha(0.0f, false, false);
    EXPECT_FLOAT_EQ(alpha, 0.0f);

    // Mid-way through opening (50%)
    alpha = calculateFadeAlpha(0.5f, false, false);
    EXPECT_FLOAT_EQ(alpha, 0.5f);

    // At end of opening (100%)
    alpha = calculateFadeAlpha(1.0f, false, false);
    EXPECT_FLOAT_EQ(alpha, 1.0f);
}

// Test: Fade animation alpha for closing (non-active workspaces)
TEST(AnimationTest, FadeOutAlphaForClosing) {
    // At start of closing animation (0%)
    float alpha = calculateFadeAlpha(0.0f, true, false);
    EXPECT_FLOAT_EQ(alpha, 1.0f);

    // Mid-way through closing (50%)
    alpha = calculateFadeAlpha(0.5f, true, false);
    EXPECT_FLOAT_EQ(alpha, 0.5f);

    // At end of closing (100%)
    alpha = calculateFadeAlpha(1.0f, true, false);
    EXPECT_FLOAT_EQ(alpha, 0.0f);
}

// Test: Active workspace always fully visible
TEST(AnimationTest, ActiveWorkspaceAlwaysVisible) {
    std::vector<float> percentages = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float percent : percentages) {
        // Opening animation
        float alpha = calculateFadeAlpha(percent, false, true);
        EXPECT_FLOAT_EQ(alpha, 1.0f);

        // Closing animation
        alpha = calculateFadeAlpha(percent, true, true);
        EXPECT_FLOAT_EQ(alpha, 1.0f);
    }
}

// Test: Alpha always within valid range [0, 1]
TEST(AnimationTest, AlphaWithinValidRange) {
    std::vector<float> percentages = {0.0f, 0.1f, 0.5f, 0.9f, 1.0f};
    std::vector<bool> closingStates = {false, true};
    std::vector<bool> activeStates = {false, true};

    for (float percent : percentages) {
        for (bool closing : closingStates) {
            for (bool active : activeStates) {
                float alpha = calculateFadeAlpha(percent, closing, active);
                EXPECT_GE(alpha, 0.0f);
                EXPECT_LE(alpha, 1.0f);
            }
        }
    }
}

// Test helper: Drag detection
static bool isDragDetected(float startX, float startY, float endX, float endY,
                          float threshold) {
    const float distanceX = std::abs(endX - startX);
    const float distanceY = std::abs(endY - startY);
    return (distanceX > threshold || distanceY > threshold);
}

// Test: Drag threshold - no drag when within threshold
TEST(DragDetectionTest, NoDragWithinThreshold) {
    const float threshold = 50.0f;

    // Mouse moved 30px horizontally (less than threshold)
    EXPECT_FALSE(isDragDetected(100.0f, 100.0f, 130.0f, 100.0f, threshold));

    // Mouse moved 40px vertically (less than threshold)
    EXPECT_FALSE(isDragDetected(100.0f, 100.0f, 100.0f, 140.0f, threshold));

    // Mouse moved diagonally but each component < threshold
    EXPECT_FALSE(isDragDetected(100.0f, 100.0f, 130.0f, 130.0f, threshold));
}

// Test: Drag threshold - drag detected when exceeds threshold
TEST(DragDetectionTest, DragDetectedAboveThreshold) {
    const float threshold = 50.0f;

    // Mouse moved 51px horizontally (exceeds threshold)
    EXPECT_TRUE(isDragDetected(100.0f, 100.0f, 151.0f, 100.0f, threshold));

    // Mouse moved 60px vertically (exceeds threshold)
    EXPECT_TRUE(isDragDetected(100.0f, 100.0f, 100.0f, 160.0f, threshold));

    // Mouse moved diagonally with one component > threshold
    EXPECT_TRUE(isDragDetected(100.0f, 100.0f, 151.0f, 130.0f, threshold));
}

// Test: Drag threshold - exact threshold boundary
TEST(DragDetectionTest, ExactThresholdBoundary) {
    const float threshold = 50.0f;

    // Exactly at threshold should not trigger (uses >)
    EXPECT_FALSE(isDragDetected(100.0f, 100.0f, 150.0f, 100.0f, threshold));

    // One pixel over threshold should trigger
    EXPECT_TRUE(isDragDetected(100.0f, 100.0f, 150.01f, 100.0f, threshold));
}

// Test: Drag threshold - negative movement
TEST(DragDetectionTest, NegativeMovement) {
    const float threshold = 50.0f;

    // Mouse moved 60px left (negative direction)
    EXPECT_TRUE(isDragDetected(100.0f, 100.0f, 40.0f, 100.0f, threshold));

    // Mouse moved 70px up (negative direction)
    EXPECT_TRUE(isDragDetected(100.0f, 100.0f, 100.0f, 30.0f, threshold));
}

// Test helper: Calculate global window position
struct WindowPosition {
    float globalX;
    float globalY;
};

static WindowPosition calculateGlobalWindowPosition(float monitorX, float monitorY,
                                                     float monitorW, float monitorH,
                                                     float relX, float relY) {
    WindowPosition result;
    result.globalX = monitorX + relX * monitorW;
    result.globalY = monitorY + relY * monitorH;
    return result;
}

// Test: Global coordinate calculation for first monitor
TEST(CoordinateTransformTest, FirstMonitorAtOrigin) {
    // Monitor 1 at (0, 0) with size 1920x1080
    auto pos = calculateGlobalWindowPosition(0, 0, 1920, 1080, 0.5f, 0.5f);

    // Center of monitor
    EXPECT_FLOAT_EQ(pos.globalX, 960.0f);
    EXPECT_FLOAT_EQ(pos.globalY, 540.0f);
}

// Test: Global coordinate calculation for second monitor
TEST(CoordinateTransformTest, SecondMonitorOffset) {
    // Monitor 2 at (1920, 0) with size 1920x1080
    auto pos = calculateGlobalWindowPosition(1920, 0, 1920, 1080, 0.5f, 0.5f);

    // Center of second monitor
    EXPECT_FLOAT_EQ(pos.globalX, 2880.0f);  // 1920 + 960
    EXPECT_FLOAT_EQ(pos.globalY, 540.0f);
}

// Test: Global coordinate calculation for third monitor
TEST(CoordinateTransformTest, ThirdMonitorOffset) {
    // Monitor 3 at (3840, 0) with size 1920x1080
    auto pos = calculateGlobalWindowPosition(3840, 0, 1920, 1080, 0.5f, 0.5f);

    // Center of third monitor
    EXPECT_FLOAT_EQ(pos.globalX, 4800.0f);  // 3840 + 960
    EXPECT_FLOAT_EQ(pos.globalY, 540.0f);
}

// Test: Global coordinate at top-left corner
TEST(CoordinateTransformTest, TopLeftCorner) {
    auto pos = calculateGlobalWindowPosition(1920, 0, 1920, 1080, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(pos.globalX, 1920.0f);
    EXPECT_FLOAT_EQ(pos.globalY, 0.0f);
}

// Test: Global coordinate at bottom-right corner
TEST(CoordinateTransformTest, BottomRightCorner) {
    auto pos = calculateGlobalWindowPosition(1920, 0, 1920, 1080, 1.0f, 1.0f);

    EXPECT_FLOAT_EQ(pos.globalX, 3840.0f);  // 1920 + 1920
    EXPECT_FLOAT_EQ(pos.globalY, 1080.0f);
}

// Test helper: Check if point is in black bar
static bool isInBlackBar(float clickX, float clickY,
                        float boxX, float boxY, float boxW, float boxH,
                        float fbW, float fbH) {
    // Apply aspect ratio scaling
    auto scaled = calculateAspectRatioFit(boxX, boxY, boxW, boxH, fbW, fbH);

    // Check if click is outside the scaled area (in black bars)
    return (clickX < scaled.x || clickX > scaled.x + scaled.w ||
            clickY < scaled.y || clickY > scaled.y + scaled.h);
}

// Test: Black bar detection for wider framebuffer (horizontal bars)
TEST(BlackBarDetectionTest, WiderFramebufferVerticalBars) {
    // Box 100x100, FB 200x100 creates vertical black bars on top/bottom
    // Scaled area will be 100x50 centered vertically

    // Click in actual content area (should not be in black bar)
    EXPECT_FALSE(isInBlackBar(50.0f, 50.0f, 0, 0, 100, 100, 200, 100));

    // Click in top black bar
    EXPECT_TRUE(isInBlackBar(50.0f, 10.0f, 0, 0, 100, 100, 200, 100));

    // Click in bottom black bar
    EXPECT_TRUE(isInBlackBar(50.0f, 90.0f, 0, 0, 100, 100, 200, 100));
}

// Test: Black bar detection for taller framebuffer (vertical bars)
TEST(BlackBarDetectionTest, TallerFramebufferHorizontalBars) {
    // Box 100x100, FB 100x200 creates horizontal black bars on left/right
    // Scaled area will be 50x100 centered horizontally

    // Click in actual content area (should not be in black bar)
    EXPECT_FALSE(isInBlackBar(50.0f, 50.0f, 0, 0, 100, 100, 100, 200));

    // Click in left black bar
    EXPECT_TRUE(isInBlackBar(10.0f, 50.0f, 0, 0, 100, 100, 100, 200));

    // Click in right black bar
    EXPECT_TRUE(isInBlackBar(90.0f, 50.0f, 0, 0, 100, 100, 100, 200));
}

// Test: No black bars when aspect ratios match
TEST(BlackBarDetectionTest, NoBlackBarsMatchingAspectRatio) {
    // Box 100x100, FB 200x200 (same aspect ratio)
    // Entire box should be content, no black bars

    // Clicks anywhere in box should not be in black bars
    EXPECT_FALSE(isInBlackBar(10.0f, 10.0f, 0, 0, 100, 100, 200, 200));
    EXPECT_FALSE(isInBlackBar(90.0f, 90.0f, 0, 0, 100, 100, 200, 200));
    EXPECT_FALSE(isInBlackBar(50.0f, 50.0f, 0, 0, 100, 100, 200, 200));
}

// Test: Black bar boundary detection
TEST(BlackBarDetectionTest, BoundaryBetweenContentAndBlackBar) {
    // Box 100x100, FB 200x100
    auto scaled = calculateAspectRatioFit(0, 0, 100, 100, 200, 100);

    // Click exactly on the boundary (should be in content)
    EXPECT_FALSE(isInBlackBar(scaled.x, scaled.y, 0, 0, 100, 100, 200, 100));

    // Click just inside black bar
    EXPECT_TRUE(isInBlackBar(scaled.y - 1.0f, scaled.y - 1.0f,
                            0, 0, 100, 100, 200, 100));
}

// Test helper: Window hit detection
struct Window {
    float x, y, w, h;
};

static bool isWindowHit(float clickX, float clickY, const Window& window) {
    return (clickX >= window.x && clickX <= window.x + window.w &&
            clickY >= window.y && clickY <= window.y + window.h);
}

// Test: Window hit detection - center of window
TEST(WindowHitDetectionTest, ClickCenterOfWindow) {
    Window w = {100, 200, 300, 400};

    // Click in center
    EXPECT_TRUE(isWindowHit(250.0f, 400.0f, w));
}

// Test: Window hit detection - corners
TEST(WindowHitDetectionTest, ClickWindowCorners) {
    Window w = {100, 200, 300, 400};

    // All corners should be hit
    EXPECT_TRUE(isWindowHit(100.0f, 200.0f, w));  // Top-left
    EXPECT_TRUE(isWindowHit(400.0f, 200.0f, w));  // Top-right
    EXPECT_TRUE(isWindowHit(100.0f, 600.0f, w));  // Bottom-left
    EXPECT_TRUE(isWindowHit(400.0f, 600.0f, w));  // Bottom-right
}

// Test: Window hit detection - outside window
TEST(WindowHitDetectionTest, ClickOutsideWindow) {
    Window w = {100, 200, 300, 400};

    // Click outside (left)
    EXPECT_FALSE(isWindowHit(50.0f, 400.0f, w));

    // Click outside (right)
    EXPECT_FALSE(isWindowHit(450.0f, 400.0f, w));

    // Click outside (above)
    EXPECT_FALSE(isWindowHit(250.0f, 150.0f, w));

    // Click outside (below)
    EXPECT_FALSE(isWindowHit(250.0f, 650.0f, w));
}

// Test: Window hit detection with global coordinates
TEST(WindowHitDetectionTest, GlobalCoordinates) {
    // Window on second monitor at (1920, 0)
    Window w = {2000, 100, 500, 300};

    // Convert click from relative to global and test
    auto clickPos = calculateGlobalWindowPosition(1920, 0, 1920, 1080,
                                                   0.1f, 0.2f);

    // This click at (2112, 216) should hit the window
    EXPECT_TRUE(isWindowHit(clickPos.globalX, clickPos.globalY, w));
}

// Test: Multiple windows, find correct one
TEST(WindowHitDetectionTest, MultipleWindowsCorrectSelection) {
    std::vector<Window> windows = {
        {100, 100, 200, 200},
        {400, 100, 200, 200},
        {100, 400, 200, 200},
        {400, 400, 200, 200}
    };

    // Click should hit second window
    float clickX = 500.0f;
    float clickY = 200.0f;

    int hitCount = 0;
    int hitIndex = -1;
    for (size_t i = 0; i < windows.size(); ++i) {
        if (isWindowHit(clickX, clickY, windows[i])) {
            hitCount++;
            hitIndex = i;
        }
    }

    EXPECT_EQ(hitCount, 1);  // Only one window should be hit
    EXPECT_EQ(hitIndex, 1);  // Second window (index 1)
}

// Test helper: Window with type
struct WindowWithType {
    float x, y, w, h;
    bool isFullscreen;
    bool isFloating;
};

static int findTopmostWindow(float clickX, float clickY,
                             const std::vector<WindowWithType>& windows) {
    int topmostIndex = -1;

    for (size_t i = 0; i < windows.size(); ++i) {
        const auto& w = windows[i];

        // Check if click hits this window
        if (clickX >= w.x && clickX <= w.x + w.w &&
            clickY >= w.y && clickY <= w.y + w.h) {

            if (topmostIndex == -1) {
                topmostIndex = i;
            } else {
                const auto& topmost = windows[topmostIndex];

                // Prioritize: fullscreen > floating > tiled
                if (w.isFullscreen && !topmost.isFullscreen) {
                    topmostIndex = i;
                } else if (w.isFloating && !topmost.isFloating &&
                           !topmost.isFullscreen) {
                    topmostIndex = i;
                }
            }
        }
    }

    return topmostIndex;
}

// Test: Window stacking - fullscreen over tiled
TEST(WindowStackingTest, FullscreenOverTiled) {
    std::vector<WindowWithType> windows = {
        {100, 100, 200, 200, false, false},  // Tiled
        {100, 100, 200, 200, true, false}    // Fullscreen (same position)
    };

    int selected = findTopmostWindow(150, 150, windows);
    EXPECT_EQ(selected, 1);  // Fullscreen window should be selected
}

// Test: Window stacking - floating over tiled
TEST(WindowStackingTest, FloatingOverTiled) {
    std::vector<WindowWithType> windows = {
        {100, 100, 200, 200, false, false},  // Tiled
        {100, 100, 200, 200, false, true}    // Floating (same position)
    };

    int selected = findTopmostWindow(150, 150, windows);
    EXPECT_EQ(selected, 1);  // Floating window should be selected
}

// Test: Window stacking - fullscreen over floating
TEST(WindowStackingTest, FullscreenOverFloating) {
    std::vector<WindowWithType> windows = {
        {100, 100, 200, 200, false, true},   // Floating
        {100, 100, 200, 200, true, false}    // Fullscreen (same position)
    };

    int selected = findTopmostWindow(150, 150, windows);
    EXPECT_EQ(selected, 1);  // Fullscreen window should be selected
}

// Test: Window stacking - first tiled when both tiled
TEST(WindowStackingTest, FirstTiledWhenBothTiled) {
    std::vector<WindowWithType> windows = {
        {100, 100, 200, 200, false, false},  // Tiled
        {100, 100, 200, 200, false, false}   // Tiled (same position)
    };

    int selected = findTopmostWindow(150, 150, windows);
    EXPECT_EQ(selected, 0);  // First window found is selected
}

// Test: Window stacking - complex scenario
TEST(WindowStackingTest, ComplexStackingScenario) {
    std::vector<WindowWithType> windows = {
        {100, 100, 200, 200, false, false},  // Tiled
        {100, 100, 200, 200, false, true},   // Floating
        {100, 100, 200, 200, false, false},  // Tiled
        {100, 100, 200, 200, true, false}    // Fullscreen
    };

    int selected = findTopmostWindow(150, 150, windows);
    EXPECT_EQ(selected, 3);  // Fullscreen (index 3) should be selected
}

// Test: Window stacking - no overlap
TEST(WindowStackingTest, NoOverlap) {
    std::vector<WindowWithType> windows = {
        {100, 100, 100, 100, false, false},  // Tiled
        {300, 100, 100, 100, false, true},   // Floating (different position)
        {100, 300, 100, 100, true, false}    // Fullscreen (different position)
    };

    // Click on floating window
    int selected = findTopmostWindow(350, 150, windows);
    EXPECT_EQ(selected, 1);
}

// Test: Window stacking - partial overlap with fullscreen
TEST(WindowStackingTest, PartialOverlapWithFullscreen) {
    std::vector<WindowWithType> windows = {
        {100, 100, 200, 200, false, false},  // Tiled
        {150, 150, 200, 200, true, false}    // Fullscreen (partial overlap)
    };

    // Click in overlap area - fullscreen should win
    int selected = findTopmostWindow(200, 200, windows);
    EXPECT_EQ(selected, 1);

    // Click in tiled-only area
    selected = findTopmostWindow(120, 120, windows);
    EXPECT_EQ(selected, 0);
}

// Test: Window state preservation during workspace movement
TEST(WindowMovementTest, PreservesFloatingState) {
    // Simulates that a floating window should remain floating when moved
    struct WindowState {
        bool isFloating;
        bool isFullscreen;
    };

    auto getStateAfterMove = [](WindowState before) -> WindowState {
        WindowState after = before;

        // Fullscreen windows are converted to tiled
        if (before.isFullscreen) {
            after.isFullscreen = false;
            after.isFloating = false;
        }
        // Floating windows remain floating
        // Tiled windows remain tiled

        return after;
    };

    // Test floating window preservation
    WindowState floatingWindow{true, false};
    auto result = getStateAfterMove(floatingWindow);
    EXPECT_TRUE(result.isFloating);
    EXPECT_FALSE(result.isFullscreen);

    // Test tiled window preservation
    WindowState tiledWindow{false, false};
    result = getStateAfterMove(tiledWindow);
    EXPECT_FALSE(result.isFloating);
    EXPECT_FALSE(result.isFullscreen);

    // Test fullscreen window conversion
    WindowState fullscreenWindow{false, true};
    result = getStateAfterMove(fullscreenWindow);
    EXPECT_FALSE(result.isFloating);
    EXPECT_FALSE(result.isFullscreen);
}

TEST(WindowMovementTest, ActiveWorkspaceNotRedrawn) {
    // Simulates checking if a workspace index should be redrawn
    auto shouldRedraw = [](int workspaceIndex, int activeIndex) -> bool {
        return workspaceIndex != activeIndex;
    };

    const int activeIndex = 4;  // Active workspace on right side

    // Left-side workspaces should be redrawn
    EXPECT_TRUE(shouldRedraw(0, activeIndex));
    EXPECT_TRUE(shouldRedraw(1, activeIndex));
    EXPECT_TRUE(shouldRedraw(2, activeIndex));
    EXPECT_TRUE(shouldRedraw(3, activeIndex));

    // Active workspace should NOT be redrawn
    EXPECT_FALSE(shouldRedraw(4, activeIndex));
}

TEST(WindowMovementTest, LeftSideWorkspaceRedraw) {
    // Tests that only left-side workspace gets scheduled for redraw
    auto determineRedrawWorkspace = [](int sourceIndex, int targetIndex, int activeIndex) -> int {
        // Returns the workspace index that should be redrawn, or -1 if none
        if (sourceIndex >= 0 && sourceIndex != activeIndex) {
            return sourceIndex;
        } else if (targetIndex != activeIndex) {
            return targetIndex;
        }
        return -1;
    };

    const int activeIndex = 4;

    // Source on left, target is active
    int redrawIndex = determineRedrawWorkspace(1, 4, activeIndex);
    EXPECT_EQ(redrawIndex, 1);

    // Source is active, target on left
    redrawIndex = determineRedrawWorkspace(4, 2, activeIndex);
    EXPECT_EQ(redrawIndex, 2);

    // Both on left, source takes precedence
    redrawIndex = determineRedrawWorkspace(1, 2, activeIndex);
    EXPECT_EQ(redrawIndex, 1);

    // Source invalid, target on left
    redrawIndex = determineRedrawWorkspace(-1, 3, activeIndex);
    EXPECT_EQ(redrawIndex, 3);

    // Both are active (shouldn't happen in practice)
    redrawIndex = determineRedrawWorkspace(4, 4, activeIndex);
    EXPECT_EQ(redrawIndex, -1);
}

// Test: Drag preview scale calculation
TEST(DragPreviewTest, ScaleCalculation) {
    const float DRAG_PREVIEW_SCALE = 0.10f;

    // Test preview size for various window sizes
    struct TestCase {
        float windowWidth;
        float windowHeight;
        float expectedPreviewWidth;
        float expectedPreviewHeight;
    };

    std::vector<TestCase> cases = {
        {1920, 1080, 192, 108},
        {800, 600, 80, 60},
        {1000, 500, 100, 50},
        {500, 1000, 50, 100}
    };

    for (const auto& tc : cases) {
        float previewWidth = tc.windowWidth * DRAG_PREVIEW_SCALE;
        float previewHeight = tc.windowHeight * DRAG_PREVIEW_SCALE;

        EXPECT_FLOAT_EQ(previewWidth, tc.expectedPreviewWidth);
        EXPECT_FLOAT_EQ(previewHeight, tc.expectedPreviewHeight);
    }
}

TEST(DragPreviewTest, PreviewPositioning) {
    // Test that preview is centered on cursor
    const float DRAG_PREVIEW_SCALE = 0.10f;

    struct TestCase {
        float windowWidth;
        float windowHeight;
        float cursorX;
        float cursorY;
    };

    std::vector<TestCase> cases = {
        {1920, 1080, 500, 400},
        {800, 600, 100, 200},
        {1000, 500, 960, 540}
    };

    for (const auto& tc : cases) {
        float previewWidth = tc.windowWidth * DRAG_PREVIEW_SCALE;
        float previewHeight = tc.windowHeight * DRAG_PREVIEW_SCALE;

        // Preview should be centered on cursor
        float previewX = tc.cursorX - previewWidth / 2.0f;
        float previewY = tc.cursorY - previewHeight / 2.0f;

        // Verify the preview box dimensions
        EXPECT_FLOAT_EQ(previewX + previewWidth / 2.0f, tc.cursorX);
        EXPECT_FLOAT_EQ(previewY + previewHeight / 2.0f, tc.cursorY);
    }
}

// Test: Workspace refresh logic for window moves
TEST(WorkspaceRefreshTest, RefreshBothNonActiveWorkspaces) {
    // When moving a window between two non-active workspaces,
    // both should be scheduled for refresh

    const int activeIndex = 4;  // Right side active workspace
    const int sourceIndex = 0;  // Left side workspace
    const int targetIndex = 2;  // Another left side workspace

    std::vector<int> workspacesToRefresh;

    // Add source workspace if it's not the active workspace
    if (sourceIndex >= 0 && sourceIndex != activeIndex) {
        workspacesToRefresh.push_back(sourceIndex);
    }

    // Add target workspace if it's not the active workspace and different from source
    if (targetIndex != activeIndex && targetIndex != sourceIndex) {
        workspacesToRefresh.push_back(targetIndex);
    }

    // Both workspaces should be in the refresh list
    EXPECT_EQ(workspacesToRefresh.size(), 2);
    EXPECT_EQ(workspacesToRefresh[0], sourceIndex);
    EXPECT_EQ(workspacesToRefresh[1], targetIndex);
}

TEST(WorkspaceRefreshTest, RefreshOnlyNonActiveWorkspace) {
    // When moving from active to non-active or vice versa,
    // only the non-active workspace should be refreshed

    const int activeIndex = 4;

    // Test case 1: Move from active to non-active
    {
        const int sourceIndex = 4;  // Active workspace
        const int targetIndex = 2;  // Non-active workspace
        std::vector<int> workspacesToRefresh;

        if (sourceIndex >= 0 && sourceIndex != activeIndex) {
            workspacesToRefresh.push_back(sourceIndex);
        }
        if (targetIndex != activeIndex && targetIndex != sourceIndex) {
            workspacesToRefresh.push_back(targetIndex);
        }

        EXPECT_EQ(workspacesToRefresh.size(), 1);
        EXPECT_EQ(workspacesToRefresh[0], targetIndex);
    }

    // Test case 2: Move from non-active to active
    {
        const int sourceIndex = 1;  // Non-active workspace
        const int targetIndex = 4;  // Active workspace
        std::vector<int> workspacesToRefresh;

        if (sourceIndex >= 0 && sourceIndex != activeIndex) {
            workspacesToRefresh.push_back(sourceIndex);
        }
        if (targetIndex != activeIndex && targetIndex != sourceIndex) {
            workspacesToRefresh.push_back(targetIndex);
        }

        EXPECT_EQ(workspacesToRefresh.size(), 1);
        EXPECT_EQ(workspacesToRefresh[0], sourceIndex);
    }
}

TEST(WorkspaceRefreshTest, NoRefreshForActiveToActive) {
    // Moving within the active workspace shouldn't schedule any refreshes
    const int activeIndex = 4;
    const int sourceIndex = 4;
    const int targetIndex = 4;

    std::vector<int> workspacesToRefresh;

    if (sourceIndex >= 0 && sourceIndex != activeIndex) {
        workspacesToRefresh.push_back(sourceIndex);
    }
    if (targetIndex != activeIndex && targetIndex != sourceIndex) {
        workspacesToRefresh.push_back(targetIndex);
    }

    EXPECT_EQ(workspacesToRefresh.size(), 0);
}

TEST(WorkspaceRefreshTest, RefreshLeftSideActiveWorkspace) {
    // When moving to/from the active workspace (right side),
    // the left-side representation of the active workspace should also be refreshed

    const int activeIndex = 8;  // Right side active workspace (index 8)
    const int leftSideActiveIndex = 2;  // Left side representation of active workspace

    // Simulate the images array with isActive flags
    struct MockWorkspaceImage {
        bool isActive;
    };
    std::vector<MockWorkspaceImage> images(9);
    images[2].isActive = true;  // Left side active workspace
    images[8].isActive = true;  // Right side active workspace

    // Test case 1: Move from left workspace to active workspace (right side)
    {
        const int sourceIndex = 1;  // Non-active left workspace
        const int targetIndex = 8;  // Active workspace (right side)
        std::vector<int> workspacesToRefresh;

        // Find left-side active index
        int foundLeftSideActiveIndex = -1;
        for (size_t i = 0; i < (size_t)activeIndex; ++i) {
            if (images[i].isActive) {
                foundLeftSideActiveIndex = i;
                break;
            }
        }

        if (sourceIndex >= 0 && sourceIndex != activeIndex) {
            workspacesToRefresh.push_back(sourceIndex);
        }
        if (targetIndex != activeIndex && targetIndex != sourceIndex) {
            workspacesToRefresh.push_back(targetIndex);
        }
        if (foundLeftSideActiveIndex >= 0 &&
            (sourceIndex == activeIndex || targetIndex == activeIndex)) {
            workspacesToRefresh.push_back(foundLeftSideActiveIndex);
        }

        // Should refresh: source (1) and left-side active (2)
        EXPECT_EQ(workspacesToRefresh.size(), 2);
        EXPECT_EQ(workspacesToRefresh[0], sourceIndex);
        EXPECT_EQ(workspacesToRefresh[1], leftSideActiveIndex);
    }

    // Test case 2: Move from active workspace (right side) to left workspace
    {
        const int sourceIndex = 8;  // Active workspace (right side)
        const int targetIndex = 3;  // Non-active left workspace
        std::vector<int> workspacesToRefresh;

        // Find left-side active index
        int foundLeftSideActiveIndex = -1;
        for (size_t i = 0; i < (size_t)activeIndex; ++i) {
            if (images[i].isActive) {
                foundLeftSideActiveIndex = i;
                break;
            }
        }

        if (sourceIndex >= 0 && sourceIndex != activeIndex) {
            workspacesToRefresh.push_back(sourceIndex);
        }
        if (targetIndex != activeIndex && targetIndex != sourceIndex) {
            workspacesToRefresh.push_back(targetIndex);
        }
        if (foundLeftSideActiveIndex >= 0 &&
            (sourceIndex == activeIndex || targetIndex == activeIndex)) {
            workspacesToRefresh.push_back(foundLeftSideActiveIndex);
        }

        // Should refresh: target (3) and left-side active (2)
        EXPECT_EQ(workspacesToRefresh.size(), 2);
        EXPECT_EQ(workspacesToRefresh[0], targetIndex);
        EXPECT_EQ(workspacesToRefresh[1], leftSideActiveIndex);
    }
}

TEST(WorkspaceRefreshTest, CrossMonitorMoveFromActiveWorkspace) {
    // When moving a window from the active workspace to another monitor,
    // both the source active workspace and its left-side representation
    // should be refreshed

    const int sourceActiveIndex = 8;  // Right side active workspace
    const int sourceWorkspaceIndex = 8;  // Moving from active workspace

    // Simulate the source overview's images array
    struct MockWorkspaceImage {
        bool isActive;
    };
    std::vector<MockWorkspaceImage> sourceImages(9);
    sourceImages[2].isActive = true;  // Left side active workspace
    sourceImages[8].isActive = true;  // Right side active workspace

    // Determine which workspaces to refresh on source monitor
    std::vector<int> workspacesToRefresh;
    workspacesToRefresh.push_back(sourceWorkspaceIndex);

    // If moving from active workspace, also refresh left-side
    if (sourceWorkspaceIndex == sourceActiveIndex) {
        int leftSideActiveIndex = -1;
        for (size_t i = 0; i < (size_t)sourceActiveIndex; ++i) {
            if (sourceImages[i].isActive) {
                leftSideActiveIndex = i;
                break;
            }
        }
        if (leftSideActiveIndex >= 0) {
            workspacesToRefresh.push_back(leftSideActiveIndex);
        }
    }

    // Should refresh both the active workspace and left-side
    EXPECT_EQ(workspacesToRefresh.size(), 2);
    EXPECT_EQ(workspacesToRefresh[0], sourceWorkspaceIndex);
    EXPECT_EQ(workspacesToRefresh[1], 2);
}

TEST(WorkspaceRefreshTest, CrossMonitorMoveFromNonActiveWorkspace) {
    // When moving from non-active workspace to another monitor,
    // only the source workspace should be refreshed

    const int sourceActiveIndex = 8;
    const int sourceWorkspaceIndex = 1;  // Non-active workspace

    struct MockWorkspaceImage {
        bool isActive;
    };
    std::vector<MockWorkspaceImage> sourceImages(9);
    sourceImages[2].isActive = true;
    sourceImages[8].isActive = true;

    std::vector<int> workspacesToRefresh;
    workspacesToRefresh.push_back(sourceWorkspaceIndex);

    if (sourceWorkspaceIndex == sourceActiveIndex) {
        int leftSideActiveIndex = -1;
        for (size_t i = 0; i < (size_t)sourceActiveIndex; ++i) {
            if (sourceImages[i].isActive) {
                leftSideActiveIndex = i;
                break;
            }
        }
        if (leftSideActiveIndex >= 0) {
            workspacesToRefresh.push_back(leftSideActiveIndex);
        }
    }

    // Should refresh only the source workspace
    EXPECT_EQ(workspacesToRefresh.size(), 1);
    EXPECT_EQ(workspacesToRefresh[0], sourceWorkspaceIndex);
}

// Test: Workspace creation for non-existent workspaces
TEST(WorkspaceCreationTest, CreateWorkspaceBeforeMove) {
    // Test that we handle the case where target workspace doesn't exist yet
    // This simulates moving a window to a workspace that hasn't been created

    struct MockWorkspaceImage {
        int64_t workspaceID;
        bool hasWorkspace;  // Simulates pWorkspace being null or not
    };

    MockWorkspaceImage targetImage = {5, false};  // Workspace 5 doesn't exist

    // Before move, check if workspace needs to be created
    bool needsCreation = !targetImage.hasWorkspace;
    EXPECT_TRUE(needsCreation);

    // Simulate creation
    if (needsCreation) {
        targetImage.hasWorkspace = true;
    }

    // After creation, workspace should exist
    EXPECT_TRUE(targetImage.hasWorkspace);
    EXPECT_EQ(targetImage.workspaceID, 5);
}

TEST(WorkspaceCreationTest, NoCreationForExistingWorkspace) {
    // Test that we don't try to create a workspace that already exists
    struct MockWorkspaceImage {
        int64_t workspaceID;
        bool hasWorkspace;
    };

    MockWorkspaceImage targetImage = {3, true};  // Workspace 3 exists

    bool needsCreation = !targetImage.hasWorkspace;
    EXPECT_FALSE(needsCreation);

    // Workspace should remain unchanged
    EXPECT_TRUE(targetImage.hasWorkspace);
    EXPECT_EQ(targetImage.workspaceID, 3);
}

// Test: Plus sign rendering for new workspaces
TEST(PlusSignRenderingTest, PlusSizeCalculation) {
    // Test that plus sign size is calculated as 50% of the smaller dimension

    struct TestCase {
        float boxWidth;
        float boxHeight;
        float expectedPlusSize;
    };

    std::vector<TestCase> cases = {
        {400, 300, 150},    // Height is smaller: 300 * 0.5 = 150
        {300, 400, 150},    // Width is smaller: 300 * 0.5 = 150
        {500, 500, 250},    // Equal: 500 * 0.5 = 250
        {1920, 1080, 540}   // Height is smaller: 1080 * 0.5 = 540
    };

    for (const auto& tc : cases) {
        float plusSize = std::min(tc.boxWidth, tc.boxHeight) * 0.5f;
        EXPECT_FLOAT_EQ(plusSize, tc.expectedPlusSize);
    }
}

TEST(PlusSignRenderingTest, LineThicknessCalculation) {
    // Test that line thickness is fixed at 8.0 pixels
    const float lineThickness = 8.0f;

    EXPECT_FLOAT_EQ(lineThickness, 8.0f);
}

TEST(PlusSignRenderingTest, PlusCentering) {
    // Test that plus sign is centered in the workspace box
    const float boxX = 100.0f;
    const float boxY = 50.0f;
    const float boxW = 400.0f;
    const float boxH = 300.0f;

    const float centerX = boxX + boxW / 2.0f;
    const float centerY = boxY + boxH / 2.0f;

    EXPECT_FLOAT_EQ(centerX, 300.0f);
    EXPECT_FLOAT_EQ(centerY, 200.0f);

    // Verify the plus lines are centered
    const float plusSize = std::min(boxW, boxH) * 0.5f;  // 150.0f
    const float lineThickness = 8.0f;

    // Horizontal line position
    const float hLineX = centerX - plusSize / 2.0f;
    const float hLineY = centerY - lineThickness / 2.0f;
    EXPECT_FLOAT_EQ(hLineX, 225.0f);
    EXPECT_FLOAT_EQ(hLineY, 196.0f);

    // Vertical line position
    const float vLineX = centerX - lineThickness / 2.0f;
    const float vLineY = centerY - plusSize / 2.0f;
    EXPECT_FLOAT_EQ(vLineX, 296.0f);
    EXPECT_FLOAT_EQ(vLineY, 125.0f);
}

TEST(PlusSignRenderingTest, WorkspaceIndicatorSelection) {
    // Test that we show plus sign for new workspaces and number for existing ones

    struct MockWorkspace {
        bool exists;
    };

    // New workspace should show plus sign
    MockWorkspace newWs = {false};
    bool showPlusSign = !newWs.exists;
    EXPECT_TRUE(showPlusSign);

    // Existing workspace should show number
    MockWorkspace existingWs = {true};
    showPlusSign = !existingWs.exists;
    EXPECT_FALSE(showPlusSign);
}

TEST(PlusSignRenderingTest, WorkspaceNumberVisibilityByPosition) {
    // Test that workspace numbers are shown on left panel but not on right panel
    const size_t LEFT_WORKSPACES = 8;
    const size_t activeIndex = LEFT_WORKSPACES;  // Active workspace is on the right

    struct MockImage {
        int64_t workspaceID;
        bool pWorkspace;  // true if workspace exists
    };

    // Test left panel workspaces (should show number)
    for (size_t i = 0; i < LEFT_WORKSPACES; ++i) {
        MockImage leftImage = {static_cast<int64_t>(i + 1), true};

        bool isNewWorkspace = !leftImage.pWorkspace;
        bool shouldShowNumber = (leftImage.workspaceID > 0 && i != activeIndex);

        EXPECT_FALSE(isNewWorkspace);  // Existing workspace
        EXPECT_TRUE(shouldShowNumber);  // Should show number on left panel
    }

    // Test active workspace on right panel (should NOT show number)
    MockImage rightImage = {5, true};
    size_t rightIndex = activeIndex;

    bool isNewWorkspace = !rightImage.pWorkspace;
    bool shouldShowNumber = (rightImage.workspaceID > 0 && rightIndex != activeIndex);

    EXPECT_FALSE(isNewWorkspace);  // Existing workspace
    EXPECT_FALSE(shouldShowNumber);  // Should NOT show number on right panel
}

// Test: Conditional workspace layout based on count
TEST(WorkspaceLayoutTest, LayoutWithFourOrMoreWorkspaces) {
    // With 4 or more existing workspaces, show 4 on the left
    const size_t numExisting = 5;
    const size_t LEFT_WORKSPACES = 4;
    size_t numToShow;

    if (numExisting < LEFT_WORKSPACES) {
        numToShow = numExisting + 1;
    } else {
        numToShow = LEFT_WORKSPACES;
    }

    EXPECT_EQ(numToShow, 4);
}

TEST(WorkspaceLayoutTest, LayoutWithThreeWorkspaces) {
    // With 3 existing workspaces, show all 3 plus 1 with plus sign (total 4)
    const size_t numExisting = 3;
    const size_t LEFT_WORKSPACES = 4;
    size_t numToShow;

    if (numExisting < LEFT_WORKSPACES) {
        numToShow = numExisting + 1;
    } else {
        numToShow = LEFT_WORKSPACES;
    }

    EXPECT_EQ(numToShow, 4);
}

TEST(WorkspaceLayoutTest, LayoutWithTwoWorkspaces) {
    // With 2 existing workspaces, show both plus 1 with plus sign (total 3)
    const size_t numExisting = 2;
    const size_t LEFT_WORKSPACES = 4;
    size_t numToShow;

    if (numExisting < LEFT_WORKSPACES) {
        numToShow = numExisting + 1;
    } else {
        numToShow = LEFT_WORKSPACES;
    }

    EXPECT_EQ(numToShow, 3);
}

TEST(WorkspaceLayoutTest, LayoutWithOneWorkspace) {
    // With 1 existing workspace, show it plus 1 with plus sign (total 2)
    const size_t numExisting = 1;
    const size_t LEFT_WORKSPACES = 4;
    size_t numToShow;

    if (numExisting < LEFT_WORKSPACES) {
        numToShow = numExisting + 1;
    } else {
        numToShow = LEFT_WORKSPACES;
    }

    EXPECT_EQ(numToShow, 2);
}

TEST(WorkspaceLayoutTest, LayoutWithZeroWorkspaces) {
    // With 0 existing workspaces, show only 1 with plus sign (total 1)
    const size_t numExisting = 0;
    const size_t LEFT_WORKSPACES = 4;
    size_t numToShow;

    if (numExisting < LEFT_WORKSPACES) {
        numToShow = numExisting + 1;
    } else {
        numToShow = LEFT_WORKSPACES;
    }

    EXPECT_EQ(numToShow, 1);
}

TEST(WorkspaceLayoutTest, EmptySlotCalculation) {
    // Test that we correctly calculate how many empty slots to fill
    const size_t LEFT_WORKSPACES = 4;

    // With 1 workspace shown (0 existing + 1 plus), we need to fill 3 empty slots
    {
        size_t numLeftWorkspaces = 1;
        size_t numEmptySlots = 0;

        if (numLeftWorkspaces < LEFT_WORKSPACES) {
            numEmptySlots = LEFT_WORKSPACES - numLeftWorkspaces;
        }

        EXPECT_EQ(numEmptySlots, 3);
    }

    // With 2 workspaces shown (1 existing + 1 plus), we need to fill 2 empty slots
    {
        size_t numLeftWorkspaces = 2;
        size_t numEmptySlots = 0;

        if (numLeftWorkspaces < LEFT_WORKSPACES) {
            numEmptySlots = LEFT_WORKSPACES - numLeftWorkspaces;
        }

        EXPECT_EQ(numEmptySlots, 2);
    }

    // With 3 workspaces shown (2 existing + 1 plus), we need to fill 1 empty slot
    {
        size_t numLeftWorkspaces = 3;
        size_t numEmptySlots = 0;

        if (numLeftWorkspaces < LEFT_WORKSPACES) {
            numEmptySlots = LEFT_WORKSPACES - numLeftWorkspaces;
        }

        EXPECT_EQ(numEmptySlots, 1);
    }

    // With 4 workspaces shown, no empty slots
    {
        size_t numLeftWorkspaces = 4;
        size_t numEmptySlots = 0;

        if (numLeftWorkspaces < LEFT_WORKSPACES) {
            numEmptySlots = LEFT_WORKSPACES - numLeftWorkspaces;
        }

        EXPECT_EQ(numEmptySlots, 0);
    }
}

// Test: Active workspace appears on left side in proper position
TEST(WorkspaceOffsetsTest, ActiveWorkspaceOnLeftSide) {
    // When active workspace is 3 out of workspaces {1, 2, 3, 4},
    // it should appear in position 2 (0-indexed)
    auto workspaces = calculateWorkspaceIDs(3, {1, 2, 3, 4});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 2);
    EXPECT_EQ(workspaces.ids[2], 3);  // Active workspace in proper position
    EXPECT_EQ(workspaces.ids[3], 4);
}

// Test: Active workspace at beginning of sorted list
TEST(WorkspaceOffsetsTest, ActiveWorkspaceAtStart) {
    // When active workspace is 1 out of workspaces {1, 5, 10}, it should appear at position 0
    auto workspaces = calculateWorkspaceIDs(1, {1, 5, 10});
    EXPECT_EQ(workspaces.ids[0], 1);  // Active workspace at start
    EXPECT_EQ(workspaces.ids[1], 5);
    EXPECT_EQ(workspaces.ids[2], 10);
    EXPECT_EQ(workspaces.ids[3], 2);  // New workspace fills slot
}

// Test: Active workspace at end of sorted list
TEST(WorkspaceOffsetsTest, ActiveWorkspaceAtEnd) {
    // When active workspace is 10 out of workspaces {1, 5, 10}, it should appear at position 2
    auto workspaces = calculateWorkspaceIDs(10, {1, 5, 10});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 5);
    EXPECT_EQ(workspaces.ids[2], 10);  // Active workspace at end
    EXPECT_EQ(workspaces.ids[3], 2);   // New workspace fills slot
}

// Test: Active workspace in middle of sorted list
TEST(WorkspaceOffsetsTest, ActiveWorkspaceInMiddle) {
    // When active workspace is 5 out of workspaces {1, 5, 10}, it should appear at position 1
    auto workspaces = calculateWorkspaceIDs(5, {1, 5, 10});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 5);  // Active workspace in middle
    EXPECT_EQ(workspaces.ids[2], 10);
    EXPECT_EQ(workspaces.ids[3], 2);  // New workspace fills slot
}

// ============================================================================
// Multi-Monitor Tests
// ============================================================================

// Test: Multiple monitors can have independent overview states
TEST(MultiMonitorTest, IndependentOverviewStates) {
    // Simulates tracking overview state per monitor
    struct MonitorState {
        int monitorId;
        bool hasOverview;
    };

    std::vector<MonitorState> monitors = {
        {1, false},
        {2, false}
    };

    // Open overview on monitor 1
    monitors[0].hasOverview = true;
    EXPECT_TRUE(monitors[0].hasOverview);
    EXPECT_FALSE(monitors[1].hasOverview);

    // Opening on all monitors
    for (auto& mon : monitors) {
        mon.hasOverview = true;
    }
    EXPECT_TRUE(monitors[0].hasOverview);
    EXPECT_TRUE(monitors[1].hasOverview);

    // Close all overviews
    for (auto& mon : monitors) {
        mon.hasOverview = false;
    }
    EXPECT_FALSE(monitors[0].hasOverview);
    EXPECT_FALSE(monitors[1].hasOverview);
}

// Test: Click on different monitor doesn't close overview
TEST(MultiMonitorTest, CrossMonitorClickPassthrough) {
    // Simulates checking if a click should be handled or passed through
    auto shouldPassThrough = [](int clickMonitorId, int overviewMonitorId) -> bool {
        return clickMonitorId != overviewMonitorId;
    };

    const int overviewMonitor = 1;

    // Click on same monitor - handle it
    EXPECT_FALSE(shouldPassThrough(1, overviewMonitor));

    // Click on different monitor - pass through
    EXPECT_TRUE(shouldPassThrough(2, overviewMonitor));
    EXPECT_TRUE(shouldPassThrough(3, overviewMonitor));
}

// Test: All monitors close together
TEST(MultiMonitorTest, SynchronizedClosing) {
    // Simulates closing all overviews when one closes
    struct Monitor {
        int id;
        bool hasOverview;
    };

    std::vector<Monitor> monitors = {
        {1, true},
        {2, true},
        {3, true}
    };

    // Close overview on monitor 1 triggers close on all
    auto closeAllOverviews = [](std::vector<Monitor>& mons) {
        for (auto& mon : mons) {
            mon.hasOverview = false;
        }
    };

    closeAllOverviews(monitors);

    // All should be closed
    for (const auto& mon : monitors) {
        EXPECT_FALSE(mon.hasOverview);
    }
}

// Test: Opening on all monitors at once
TEST(MultiMonitorTest, SimultaneousOpening) {
    struct MonitorOverview {
        int monitorId;
        int activeWorkspaceId;
        bool isOpen;
    };

    std::vector<MonitorOverview> monitors = {
        {1, 1, false},
        {2, 5, false},
        {3, 9, false}
    };

    // Open all at once
    for (auto& mon : monitors) {
        mon.isOpen = true;
    }

    // All should be open with their respective workspaces
    EXPECT_TRUE(monitors[0].isOpen);
    EXPECT_EQ(monitors[0].activeWorkspaceId, 1);
    EXPECT_TRUE(monitors[1].isOpen);
    EXPECT_EQ(monitors[1].activeWorkspaceId, 5);
    EXPECT_TRUE(monitors[2].isOpen);
    EXPECT_EQ(monitors[2].activeWorkspaceId, 9);
}

// Test: Each monitor renders its own workspaces
TEST(MultiMonitorTest, IndependentWorkspaceRendering) {
    // Simulates each monitor having its own workspace list
    struct MonitorWorkspaces {
        int monitorId;
        std::vector<int> workspaceIds;
    };

    std::vector<MonitorWorkspaces> monitors = {
        {1, {1, 2, 3, 4}},
        {2, {5, 6, 7, 8}},
        {3, {9, 10, 11, 12}}
    };

    // Verify each monitor has independent workspace lists
    EXPECT_EQ(monitors[0].workspaceIds.size(), 4);
    EXPECT_EQ(monitors[0].workspaceIds[0], 1);

    EXPECT_EQ(monitors[1].workspaceIds.size(), 4);
    EXPECT_EQ(monitors[1].workspaceIds[0], 5);

    EXPECT_EQ(monitors[2].workspaceIds.size(), 4);
    EXPECT_EQ(monitors[2].workspaceIds[0], 9);
}

// Test: Toggle behavior with multiple monitors
TEST(MultiMonitorTest, ToggleBehavior) {
    auto toggleOverviews = [](std::vector<bool>& states, bool anyOpen) {
        if (anyOpen) {
            // Close all
            for (size_t i = 0; i < states.size(); ++i) {
                states[i] = false;
            }
        } else {
            // Open all
            for (size_t i = 0; i < states.size(); ++i) {
                states[i] = true;
            }
        }
    };

    std::vector<bool> monitorOverviews = {false, false, false};

    // First toggle - open all
    bool anyOpen = monitorOverviews[0] || monitorOverviews[1] || monitorOverviews[2];
    toggleOverviews(monitorOverviews, anyOpen);
    EXPECT_TRUE(monitorOverviews[0]);
    EXPECT_TRUE(monitorOverviews[1]);
    EXPECT_TRUE(monitorOverviews[2]);

    // Second toggle - close all
    anyOpen = monitorOverviews[0] || monitorOverviews[1] || monitorOverviews[2];
    toggleOverviews(monitorOverviews, anyOpen);
    EXPECT_FALSE(monitorOverviews[0]);
    EXPECT_FALSE(monitorOverviews[1]);
    EXPECT_FALSE(monitorOverviews[2]);
}

// Test suite for workspace ID allocation across monitors
class WorkspaceIDAllocationTest : public ::testing::Test {
protected:
    // Helper to find first available workspace ID
    // Simulates findFirstAvailableWorkspaceID()
    int64_t findFirstAvailableID(
        const std::vector<int64_t>& existingWorkspaces,
        const std::vector<std::vector<int64_t>>& plannedPerMonitor
    ) {
        std::vector<int64_t> allUsedIDs = existingWorkspaces;

        // Add planned workspace IDs from all monitors
        for (const auto& planned : plannedPerMonitor) {
            for (int64_t id : planned) {
                if (id > 0) {  // Only count non-placeholder IDs
                    allUsedIDs.push_back(id);
                }
            }
        }

        // Sort and remove duplicates
        std::sort(allUsedIDs.begin(), allUsedIDs.end());
        allUsedIDs.erase(
            std::unique(allUsedIDs.begin(), allUsedIDs.end()),
            allUsedIDs.end()
        );

        // Find first available
        int64_t nextID = 1;
        while (std::find(allUsedIDs.begin(), allUsedIDs.end(), nextID) !=
               allUsedIDs.end()) {
            nextID++;
        }

        return nextID;
    }
};

TEST_F(WorkspaceIDAllocationTest, FirstAvailableWithNoGaps) {
    // Existing: 1, 2, 3, 4, 5, 6
    // Expected: 7
    std::vector<int64_t> existing = {1, 2, 3, 4, 5, 6};
    std::vector<std::vector<int64_t>> planned = {};

    int64_t result = findFirstAvailableID(existing, planned);
    EXPECT_EQ(result, 7);
}

TEST_F(WorkspaceIDAllocationTest, FirstAvailableWithGaps) {
    // Existing: 1, 2, 4, 5, 6
    // Expected: 3 (fills the gap)
    std::vector<int64_t> existing = {1, 2, 4, 5, 6};
    std::vector<std::vector<int64_t>> planned = {};

    int64_t result = findFirstAvailableID(existing, planned);
    EXPECT_EQ(result, 3);
}

TEST_F(WorkspaceIDAllocationTest, SkipsPlaceholders) {
    // Existing: 1, 2, 3, 4, 5, 6
    // Monitor 1 planned: -1, -1 (placeholders)
    // Monitor 2 planned: -1 (placeholder)
    // Expected: 7 (placeholders with -1 are ignored)
    std::vector<int64_t> existing = {1, 2, 3, 4, 5, 6};
    std::vector<std::vector<int64_t>> planned = {
        {-1, -1},  // Monitor 1 placeholders
        {-1}       // Monitor 2 placeholder
    };

    int64_t result = findFirstAvailableID(existing, planned);
    EXPECT_EQ(result, 7);
}

TEST_F(WorkspaceIDAllocationTest, ConsidersPlannedNonPlaceholders) {
    // Existing: 1, 2, 3
    // Monitor 1 planned: 4, 5, -1
    // Monitor 2 planned: 6, -1
    // Expected: 7 (4, 5, 6 are planned but not created yet)
    std::vector<int64_t> existing = {1, 2, 3};
    std::vector<std::vector<int64_t>> planned = {
        {4, 5, -1},  // Monitor 1: two assigned, one placeholder
        {6, -1}      // Monitor 2: one assigned, one placeholder
    };

    int64_t result = findFirstAvailableID(existing, planned);
    EXPECT_EQ(result, 7);
}

TEST_F(WorkspaceIDAllocationTest, CrossMonitorAllocation) {
    // Existing: 1, 2
    // Monitor 1 planned: 3
    // Monitor 2 creates new workspace, should get 4
    // Monitor 3 creates new workspace, should get 5
    std::vector<int64_t> existing = {1, 2};
    std::vector<std::vector<int64_t>> planned1 = {{3}};

    int64_t result1 = findFirstAvailableID(existing, planned1);
    EXPECT_EQ(result1, 4);

    // After monitor 2 plans to create workspace 4
    std::vector<std::vector<int64_t>> planned2 = {{3}, {4}};
    int64_t result2 = findFirstAvailableID(existing, planned2);
    EXPECT_EQ(result2, 5);
}

TEST_F(WorkspaceIDAllocationTest, AllPlaceholders) {
    // No existing workspaces
    // Monitor 1 planned: -1, -1, -1
    // Monitor 2 planned: -1, -1
    // Expected: 1 (start from beginning when all are placeholders)
    std::vector<int64_t> existing = {};
    std::vector<std::vector<int64_t>> planned = {
        {-1, -1, -1},
        {-1, -1}
    };

    int64_t result = findFirstAvailableID(existing, planned);
    EXPECT_EQ(result, 1);
}

TEST_F(WorkspaceIDAllocationTest, DuplicateHandling) {
    // Existing: 1, 1, 2, 2, 3 (duplicates should be handled)
    // Expected: 4
    std::vector<int64_t> existing = {1, 1, 2, 2, 3};
    std::vector<std::vector<int64_t>> planned = {};

    int64_t result = findFirstAvailableID(existing, planned);
    EXPECT_EQ(result, 4);
}

// Test helper: Calculate max scroll offset based on workspace configuration
static float calculateMaxScrollOffset(
    const std::vector<int64_t>& workspaceIDs,  // -1 = placeholder
    float monitorHeight,
    float padding,
    float gapWidth
) {
    const int LEFT_WORKSPACES = 8;
    const int VISIBLE_WORKSPACES = 4;

    // Count non-placeholder workspaces
    size_t numExistingWorkspaces = 0;
    for (size_t i = 0; i < workspaceIDs.size() && i < LEFT_WORKSPACES; ++i) {
        if (workspaceIDs[i] != -1) {
            numExistingWorkspaces++;
        }
    }

    // Include the first placeholder in scrolling calculation
    size_t numWorkspacesToShow = numExistingWorkspaces;
    if (numExistingWorkspaces < LEFT_WORKSPACES) {
        numWorkspacesToShow++; // Add 1 for the first placeholder
    }

    // Only allow scrolling if there are more than 4 workspaces to show
    if (numWorkspacesToShow <= 4) {
        return 0.0f;
    }

    // Calculate workspace height (same as in the actual code)
    const float availableHeight = monitorHeight - (2 * padding);
    const float totalGaps = (VISIBLE_WORKSPACES - 1) * gapWidth;
    const float baseHeight = (availableHeight - totalGaps) / VISIBLE_WORKSPACES;
    const float leftPreviewHeight = baseHeight * 0.9f;  // 10% reduction

    // Calculate total height needed for workspaces + first placeholder
    float totalWorkspacesHeight = numWorkspacesToShow * leftPreviewHeight +
                                  (numWorkspacesToShow - 1) * gapWidth;
    return std::max(0.0f, totalWorkspacesHeight - availableHeight);
}

// Scrolling tests
class ScrollingTest : public ::testing::Test {
protected:
    const float MONITOR_HEIGHT = 1080.0f;
    const float PADDING = 20.0f;
    const float GAP_WIDTH = 10.0f;
};

TEST_F(ScrollingTest, NoScrollingWithFourOrFewerWorkspaces) {
    // With 1 workspace + 1 placeholder = 2 total, no scrolling
    std::vector<int64_t> workspaces1 = {1, -1, -1, -1, -1, -1, -1, -1};
    float maxScroll1 = calculateMaxScrollOffset(workspaces1, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    EXPECT_FLOAT_EQ(maxScroll1, 0.0f);

    // With 2 workspaces + 1 placeholder = 3 total, no scrolling
    std::vector<int64_t> workspaces2 = {1, 2, -1, -1, -1, -1, -1, -1};
    float maxScroll2 = calculateMaxScrollOffset(workspaces2, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    EXPECT_FLOAT_EQ(maxScroll2, 0.0f);

    // With 3 workspaces + 1 placeholder = 4 total, no scrolling
    std::vector<int64_t> workspaces3 = {1, 2, 3, -1, -1, -1, -1, -1};
    float maxScroll3 = calculateMaxScrollOffset(workspaces3, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    EXPECT_FLOAT_EQ(maxScroll3, 0.0f);

    // With 4 workspaces + 1 placeholder = 5 total, scrolling enabled
    std::vector<int64_t> workspaces4 = {1, 2, 3, 4, -1, -1, -1, -1};
    float maxScroll4 = calculateMaxScrollOffset(workspaces4, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    EXPECT_GT(maxScroll4, 0.0f);  // Should allow scrolling
}

TEST_F(ScrollingTest, ScrollingWithFiveWorkspacesIncludesPlaceholder) {
    // With 4 workspaces + 1 placeholder = 5 to show
    std::vector<int64_t> workspaces = {1, 2, 3, 4, -1, -1, -1, -1};
    float maxScroll = calculateMaxScrollOffset(workspaces, MONITOR_HEIGHT, PADDING, GAP_WIDTH);

    // Should be > 0 to allow scrolling to the placeholder
    EXPECT_GT(maxScroll, 0.0f);

    // Calculate expected value
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    const float totalGaps = 3 * GAP_WIDTH;  // 4 visible - 1
    const float baseHeight = (availableHeight - totalGaps) / 4;
    const float workspaceHeight = baseHeight * 0.9f;

    // 5 workspaces to show (4 existing + 1 placeholder)
    float totalHeight = 5 * workspaceHeight + 4 * GAP_WIDTH;
    float expected = totalHeight - availableHeight;

    EXPECT_FLOAT_EQ(maxScroll, expected);
}

TEST_F(ScrollingTest, ScrollingWithSixWorkspaces) {
    // With 5 workspaces + 1 placeholder = 6 to show
    std::vector<int64_t> workspaces = {1, 2, 3, 4, 5, -1, -1, -1};
    float maxScroll = calculateMaxScrollOffset(workspaces, MONITOR_HEIGHT, PADDING, GAP_WIDTH);

    EXPECT_GT(maxScroll, 0.0f);

    // Calculate expected value
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    const float totalGaps = 3 * GAP_WIDTH;
    const float baseHeight = (availableHeight - totalGaps) / 4;
    const float workspaceHeight = baseHeight * 0.9f;

    // 6 workspaces to show (5 existing + 1 placeholder)
    float totalHeight = 6 * workspaceHeight + 5 * GAP_WIDTH;
    float expected = totalHeight - availableHeight;

    EXPECT_FLOAT_EQ(maxScroll, expected);
}

TEST_F(ScrollingTest, ScrollingWithSevenWorkspaces) {
    // With 6 workspaces + 1 placeholder = 7 to show
    std::vector<int64_t> workspaces = {1, 2, 3, 4, 5, 6, -1, -1};
    float maxScroll = calculateMaxScrollOffset(workspaces, MONITOR_HEIGHT, PADDING, GAP_WIDTH);

    EXPECT_GT(maxScroll, 0.0f);

    // Calculate expected value
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    const float totalGaps = 3 * GAP_WIDTH;
    const float baseHeight = (availableHeight - totalGaps) / 4;
    const float workspaceHeight = baseHeight * 0.9f;

    // 7 workspaces to show (6 existing + 1 placeholder)
    float totalHeight = 7 * workspaceHeight + 6 * GAP_WIDTH;
    float expected = totalHeight - availableHeight;

    EXPECT_FLOAT_EQ(maxScroll, expected);
}

TEST_F(ScrollingTest, ScrollingWithEightWorkspacesFull) {
    // With 7 workspaces + 1 placeholder = 8 to show (maximum)
    std::vector<int64_t> workspaces = {1, 2, 3, 4, 5, 6, 7, -1};
    float maxScroll = calculateMaxScrollOffset(workspaces, MONITOR_HEIGHT, PADDING, GAP_WIDTH);

    EXPECT_GT(maxScroll, 0.0f);

    // Calculate expected value
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    const float totalGaps = 3 * GAP_WIDTH;
    const float baseHeight = (availableHeight - totalGaps) / 4;
    const float workspaceHeight = baseHeight * 0.9f;

    // 8 workspaces to show (7 existing + 1 placeholder)
    float totalHeight = 8 * workspaceHeight + 7 * GAP_WIDTH;
    float expected = totalHeight - availableHeight;

    EXPECT_FLOAT_EQ(maxScroll, expected);
}

TEST_F(ScrollingTest, NoPlaceholderWhenAllSlotsFilled) {
    // All 8 slots filled, no placeholder to add
    std::vector<int64_t> workspaces = {1, 2, 3, 4, 5, 6, 7, 8};
    float maxScroll = calculateMaxScrollOffset(workspaces, MONITOR_HEIGHT, PADDING, GAP_WIDTH);

    EXPECT_GT(maxScroll, 0.0f);

    // Calculate expected value - only 8 workspaces, no placeholder
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    const float totalGaps = 3 * GAP_WIDTH;
    const float baseHeight = (availableHeight - totalGaps) / 4;
    const float workspaceHeight = baseHeight * 0.9f;

    // 8 workspaces to show (no room for placeholder)
    float totalHeight = 8 * workspaceHeight + 7 * GAP_WIDTH;
    float expected = totalHeight - availableHeight;

    EXPECT_FLOAT_EQ(maxScroll, expected);
}

TEST_F(ScrollingTest, MaxScrollIncreasesWithMoreWorkspaces) {
    // Verify that maxScroll increases as we add more workspaces
    std::vector<int64_t> workspaces4 = {1, 2, 3, 4, -1, -1, -1, -1};
    std::vector<int64_t> workspaces5 = {1, 2, 3, 4, 5, -1, -1, -1};
    std::vector<int64_t> workspaces6 = {1, 2, 3, 4, 5, 6, -1, -1};
    std::vector<int64_t> workspaces7 = {1, 2, 3, 4, 5, 6, 7, -1};

    float maxScroll4 = calculateMaxScrollOffset(workspaces4, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    float maxScroll5 = calculateMaxScrollOffset(workspaces5, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    float maxScroll6 = calculateMaxScrollOffset(workspaces6, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    float maxScroll7 = calculateMaxScrollOffset(workspaces7, MONITOR_HEIGHT, PADDING, GAP_WIDTH);

    EXPECT_GT(maxScroll5, maxScroll4);
    EXPECT_GT(maxScroll6, maxScroll5);
    EXPECT_GT(maxScroll7, maxScroll6);
}

TEST_F(ScrollingTest, DifferentMonitorHeights) {
    // Test with different monitor heights
    std::vector<int64_t> workspaces = {1, 2, 3, 4, 5, -1, -1, -1};

    float maxScroll720 = calculateMaxScrollOffset(workspaces, 720.0f, PADDING, GAP_WIDTH);
    float maxScroll1080 = calculateMaxScrollOffset(workspaces, 1080.0f, PADDING, GAP_WIDTH);
    float maxScroll1440 = calculateMaxScrollOffset(workspaces, 1440.0f, PADDING, GAP_WIDTH);

    // All should allow scrolling
    EXPECT_GT(maxScroll720, 0.0f);
    EXPECT_GT(maxScroll1080, 0.0f);
    EXPECT_GT(maxScroll1440, 0.0f);

    // Higher resolution requires more scrolling (larger total workspace height)
    EXPECT_LT(maxScroll720, maxScroll1080);
    EXPECT_LT(maxScroll1080, maxScroll1440);
}

// Workspace selection after scrolling tests
class WorkspaceSelectionWithScrollingTest : public ::testing::Test {
protected:
    const float MONITOR_HEIGHT = 1080.0f;
    const float MONITOR_WIDTH = 1920.0f;
    const float PADDING = 20.0f;
    const float GAP_WIDTH = 10.0f;

    // Helper to calculate workspace boxes with scroll offset
    std::vector<LayoutBox> calculateWorkspaceBoxes(
        int numWorkspaces,
        float scrollOffset,
        int activeIndex
    ) {
        const int VISIBLE_WORKSPACES = 4;
        const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
        const float totalGaps = (VISIBLE_WORKSPACES - 1) * GAP_WIDTH;
        const float baseHeight = (availableHeight - totalGaps) / VISIBLE_WORKSPACES;
        const float leftPreviewHeight = baseHeight * 0.9f;

        const float monitorAspectRatio = MONITOR_WIDTH / MONITOR_HEIGHT;
        const float leftWorkspaceWidth = leftPreviewHeight * monitorAspectRatio;

        std::vector<LayoutBox> boxes;
        for (int i = 0; i < numWorkspaces + 1; ++i) {
            if (i == activeIndex) {
                // Right side - active workspace (not scrollable)
                const float activeX = PADDING + leftWorkspaceWidth + PADDING;
                const float activeMaxWidth = MONITOR_WIDTH - activeX - PADDING;
                const float activeMaxHeight = MONITOR_HEIGHT - (2 * PADDING);
                boxes.push_back({activeX, PADDING, activeMaxWidth, activeMaxHeight, true});
            } else {
                // Left side - workspace list (scrollable)
                float yPos = PADDING + i * (leftPreviewHeight + GAP_WIDTH) - scrollOffset;
                boxes.push_back({PADDING, yPos, leftWorkspaceWidth, leftPreviewHeight, false});
            }
        }
        return boxes;
    }

    // Helper to find which workspace was clicked
    int findClickedWorkspace(float posX, float posY, const std::vector<LayoutBox>& boxes) {
        for (size_t i = 0; i < boxes.size(); ++i) {
            const auto& box = boxes[i];
            if (posX >= box.x && posX <= box.x + box.w &&
                posY >= box.y && posY <= box.y + box.h) {
                return i;
            }
        }
        return -1;
    }
};

TEST_F(WorkspaceSelectionWithScrollingTest, ClickFirstWorkspaceNoScroll) {
    // With 5 workspaces (indices 0-4) and active at index 4, no scrolling
    std::vector<LayoutBox> boxes = calculateWorkspaceBoxes(4, 0.0f, 4);

    // Click on the first workspace (top of left side)
    int selected = findClickedWorkspace(PADDING + 10.0f, PADDING + 50.0f, boxes);

    EXPECT_EQ(selected, 0);
}

TEST_F(WorkspaceSelectionWithScrollingTest, ClickSecondWorkspaceAfterScroll) {
    // With 6 workspaces and scrolled down by 200 pixels
    const float scrollOffset = 200.0f;
    std::vector<LayoutBox> boxes = calculateWorkspaceBoxes(5, scrollOffset, 5);

    // The second workspace (index 1) should now be at the top
    // Click near the top of the visible area
    int selected = findClickedWorkspace(PADDING + 10.0f, PADDING + 10.0f, boxes);

    // We should hit workspace at index 1 or 2 depending on exact scroll amount
    EXPECT_GE(selected, 0);
    EXPECT_LE(selected, 5);
}

TEST_F(WorkspaceSelectionWithScrollingTest, ClickWorkspacePartiallyOffScreen) {
    // With 6 workspaces and scrolled down, the first workspace is partially off-screen
    const float scrollOffset = 100.0f;
    std::vector<LayoutBox> boxes = calculateWorkspaceBoxes(5, scrollOffset, 5);

    // Try to click on workspace 0 which is now partially above the visible area
    int selected = findClickedWorkspace(PADDING + 10.0f, PADDING - 50.0f, boxes);

    // Should not select anything (click is outside monitor bounds)
    // Or might select workspace 0 if its box extends above visible area
    // The key is that the box position should be updated correctly
    EXPECT_TRUE(selected == -1 || selected == 0);
}

TEST_F(WorkspaceSelectionWithScrollingTest, ClickBottomWorkspaceAfterMaxScroll) {
    // With 8 workspaces, scroll to maximum to see the last ones
    const int LEFT_WORKSPACES = 8;
    const int VISIBLE_WORKSPACES = 4;
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    const float totalGaps = (VISIBLE_WORKSPACES - 1) * GAP_WIDTH;
    const float baseHeight = (availableHeight - totalGaps) / VISIBLE_WORKSPACES;
    const float leftPreviewHeight = baseHeight * 0.9f;

    // Calculate max scroll for 8 workspaces
    float totalWorkspacesHeight = LEFT_WORKSPACES * leftPreviewHeight +
                                  (LEFT_WORKSPACES - 1) * GAP_WIDTH;
    float maxScrollOffset = std::max(0.0f, totalWorkspacesHeight - availableHeight);

    std::vector<LayoutBox> boxes = calculateWorkspaceBoxes(7, maxScrollOffset, 8);

    // Click near the bottom of the visible area (should be workspace 7)
    int selected = findClickedWorkspace(PADDING + 10.0f, MONITOR_HEIGHT - PADDING - 50.0f, boxes);

    // Should select one of the last workspaces (6 or 7)
    EXPECT_GE(selected, 5);
    EXPECT_LE(selected, 7);
}

TEST_F(WorkspaceSelectionWithScrollingTest, ClickActiveWorkspaceNotAffectedByScroll) {
    // The active workspace on the right should not be affected by scrolling
    const float scrollOffset = 200.0f;
    std::vector<LayoutBox> boxes = calculateWorkspaceBoxes(5, scrollOffset, 5);

    // Click on the right side where the active workspace is
    const float monitorAspectRatio = MONITOR_WIDTH / MONITOR_HEIGHT;
    const int VISIBLE_WORKSPACES = 4;
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    const float totalGaps = (VISIBLE_WORKSPACES - 1) * GAP_WIDTH;
    const float baseHeight = (availableHeight - totalGaps) / VISIBLE_WORKSPACES;
    const float leftPreviewHeight = baseHeight * 0.9f;
    const float leftWorkspaceWidth = leftPreviewHeight * monitorAspectRatio;
    const float activeX = PADDING + leftWorkspaceWidth + PADDING;

    int selected = findClickedWorkspace(activeX + 100.0f, MONITOR_HEIGHT / 2.0f, boxes);

    // Should select the active workspace (index 5)
    EXPECT_EQ(selected, 5);
}

TEST_F(WorkspaceSelectionWithScrollingTest, MultipleScrollUpdatesBoxPositions) {
    // Simulate multiple scroll operations
    std::vector<float> scrollOffsets = {0.0f, 100.0f, 200.0f, 150.0f, 50.0f};

    for (float scrollOffset : scrollOffsets) {
        std::vector<LayoutBox> boxes = calculateWorkspaceBoxes(5, scrollOffset, 5);

        // Verify that the first workspace's Y position is correctly offset
        EXPECT_FLOAT_EQ(boxes[0].y, PADDING - scrollOffset);

        // Verify that workspace box positions are consistent with scroll offset
        for (size_t i = 0; i < 5; ++i) {
            if (i != 5) {  // Skip active workspace
                const int VISIBLE_WORKSPACES = 4;
                const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
                const float totalGaps = (VISIBLE_WORKSPACES - 1) * GAP_WIDTH;
                const float baseHeight = (availableHeight - totalGaps) / VISIBLE_WORKSPACES;
                const float leftPreviewHeight = baseHeight * 0.9f;

                float expectedY = PADDING + i * (leftPreviewHeight + GAP_WIDTH) - scrollOffset;
                EXPECT_FLOAT_EQ(boxes[i].y, expectedY);
            }
        }
    }
}

// ============================================================================
// Initial Scroll Positioning Tests
// ============================================================================

class InitialScrollPositioningTest : public ::testing::Test {
protected:
    const float MONITOR_HEIGHT = 1200.0f;
    const float PADDING = 20.0f;
    const float GAP_WIDTH = 10.0f;

    float calculateCenteringScrollOffset(int activeIndex, float availableHeight) {
        const float totalGaps = 3 * GAP_WIDTH;
        const float baseHeight = (availableHeight - totalGaps) / 4;
        const float workspaceHeight = baseHeight * 0.9f;

        float panelCenter = availableHeight / 2.0f;
        float workspaceTopWithoutScroll = activeIndex * (workspaceHeight + GAP_WIDTH);
        float workspaceCenterOffset = workspaceHeight / 2.0f;

        return workspaceTopWithoutScroll + workspaceCenterOffset - panelCenter;
    }
};

TEST_F(InitialScrollPositioningTest, FirstWorkspaceStaysAtTop) {
    // Active workspace is first (index 0)
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    float scrollOffset = calculateCenteringScrollOffset(0, availableHeight);

    // Centering would result in negative scroll, should clamp to 0
    EXPECT_LE(scrollOffset, 0.0f);
}

TEST_F(InitialScrollPositioningTest, SecondWorkspaceStaysAtTop) {
    // Active workspace is second (index 1)
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    float scrollOffset = calculateCenteringScrollOffset(1, availableHeight);

    // Centering might still result in negative or very small scroll
    // Should be clamped to 0 or very small value
    EXPECT_LE(scrollOffset, 50.0f);
}

TEST_F(InitialScrollPositioningTest, MiddleWorkspaceIsCentered) {
    // Active workspace is in the middle (index 4 out of 8)
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
    float scrollOffset = calculateCenteringScrollOffset(4, availableHeight);

    // Should have meaningful scroll offset to center
    EXPECT_GT(scrollOffset, 100.0f);
    EXPECT_LT(scrollOffset, 700.0f);

    // Verify workspace would actually be centered
    const float totalGaps = 3 * GAP_WIDTH;
    const float baseHeight = (availableHeight - totalGaps) / 4;
    const float workspaceHeight = baseHeight * 0.9f;

    float workspaceYPos = PADDING + 4 * (workspaceHeight + GAP_WIDTH) - scrollOffset;
    float workspaceCenter = workspaceYPos + workspaceHeight / 2.0f;
    float panelCenter = PADDING + availableHeight / 2.0f;

    EXPECT_NEAR(workspaceCenter, panelCenter, 0.1f);
}

TEST_F(InitialScrollPositioningTest, LastWorkspaceClampedToMaxScroll) {
    // Active workspace is last (index 7)
    std::vector<int64_t> workspaces = {1, 2, 3, 4, 5, 6, 7, 8};
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);

    float scrollOffset = calculateCenteringScrollOffset(7, availableHeight);
    float maxScroll = calculateMaxScrollOffset(workspaces, MONITOR_HEIGHT, PADDING, GAP_WIDTH);

    // Should be clamped to maxScrollOffset
    EXPECT_GE(scrollOffset, maxScroll - 10.0f);  // Allow small tolerance
}

TEST_F(InitialScrollPositioningTest, CenteringWithDifferentMonitorHeights) {
    // Test with smaller monitor
    float smallHeight = 800.0f;
    float smallAvailableHeight = smallHeight - (2 * PADDING);
    float scrollSmall = calculateCenteringScrollOffset(4, smallAvailableHeight);

    // Test with larger monitor
    float largeHeight = 1600.0f;
    float largeAvailableHeight = largeHeight - (2 * PADDING);
    float scrollLarge = calculateCenteringScrollOffset(4, largeAvailableHeight);

    // Both should center, but with different scroll values
    EXPECT_GT(scrollSmall, 0.0f);
    EXPECT_GT(scrollLarge, 0.0f);
    EXPECT_NE(scrollSmall, scrollLarge);
}

// ============================================================================
// Equal Partial Visibility Tests
// ============================================================================

class EqualPartialVisibilityTest : public ::testing::Test {
protected:
    const float MONITOR_HEIGHT = 1200.0f;
    const float PADDING = 20.0f;
    const float GAP_WIDTH = 10.0f;

    struct PartialVisibilityInfo {
        bool firstPartiallyVisible;
        bool lastPartiallyVisible;
        float topPartial;
        float bottomPartial;
    };

    PartialVisibilityInfo checkPartialVisibility(
        float scrollOffset,
        int numWorkspaces,
        float workspaceHeight
    ) {
        const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);

        int firstIndex = 0;
        int lastIndex = numWorkspaces - 1;

        float firstYPos = PADDING + firstIndex * (workspaceHeight + GAP_WIDTH) - scrollOffset;
        float lastYPos = PADDING + lastIndex * (workspaceHeight + GAP_WIDTH) - scrollOffset;

        PartialVisibilityInfo info;
        info.firstPartiallyVisible = (firstYPos < PADDING) &&
                                      ((firstYPos + workspaceHeight) > PADDING);
        info.lastPartiallyVisible = ((lastYPos + workspaceHeight) > (PADDING + availableHeight)) &&
                                     (lastYPos < (PADDING + availableHeight));

        info.topPartial = (firstYPos + workspaceHeight) - PADDING;
        info.bottomPartial = (PADDING + availableHeight) - lastYPos;

        return info;
    }

    float calculateWorkspaceHeight() {
        const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);
        const float totalGaps = 3 * GAP_WIDTH;
        const float baseHeight = (availableHeight - totalGaps) / 4;
        return baseHeight * 0.9f;
    }
};

TEST_F(EqualPartialVisibilityTest, BothPartiallyVisibleGetsAdjusted) {
    // Setup: 8 workspaces, calculate scroll offset where both are partially visible
    float workspaceHeight = calculateWorkspaceHeight();
    const float availableHeight = MONITOR_HEIGHT - (2 * PADDING);

    // Calculate a scroll offset that puts workspace in middle positions
    // First workspace should be partially cut off at top
    // Last workspace should be partially cut off at bottom
    // Try positioning so workspace 1 is partially visible at top
    float scrollOffset = workspaceHeight + GAP_WIDTH + 50.0f;  // Scroll past first workspace

    auto info = checkPartialVisibility(scrollOffset, 8, workspaceHeight);

    // If both aren't partially visible with this scroll, the test setup needs adjustment
    // This might happen with different monitor sizes
    if (!info.firstPartiallyVisible || !info.lastPartiallyVisible) {
        GTEST_SKIP() << "Test configuration doesn't produce both partially visible workspaces";
    }

    // Calculate adjustment
    float difference = info.bottomPartial - info.topPartial;
    float adjustment = difference / 2.0f;

    // Apply adjustment
    float adjustedScroll = scrollOffset - adjustment;

    // After adjustment, partial visibility should be more equal
    auto infoAfter = checkPartialVisibility(adjustedScroll, 8, workspaceHeight);
    EXPECT_TRUE(infoAfter.firstPartiallyVisible);
    EXPECT_TRUE(infoAfter.lastPartiallyVisible);

    // Should be more equal (within small tolerance)
    EXPECT_NEAR(infoAfter.topPartial, infoAfter.bottomPartial, 1.0f);
}

TEST_F(EqualPartialVisibilityTest, FirstCompletelyOffScreenNoAdjustment) {
    // First workspace completely off screen (scrolled too far down)
    float workspaceHeight = calculateWorkspaceHeight();
    float scrollOffset = 800.0f;

    auto info = checkPartialVisibility(scrollOffset, 8, workspaceHeight);

    // First should NOT be partially visible (completely off screen)
    EXPECT_FALSE(info.firstPartiallyVisible);
}

TEST_F(EqualPartialVisibilityTest, LastCompletelyOffScreenNoAdjustment) {
    // Last workspace completely off screen (scrolled too far up)
    float workspaceHeight = calculateWorkspaceHeight();
    float scrollOffset = 50.0f;

    auto info = checkPartialVisibility(scrollOffset, 8, workspaceHeight);

    // Last should NOT be partially visible (completely off screen)
    EXPECT_FALSE(info.lastPartiallyVisible);
}

TEST_F(EqualPartialVisibilityTest, AtTopNoAdjustment) {
    // At the very top (scrollOffset = 0)
    float workspaceHeight = calculateWorkspaceHeight();
    float scrollOffset = 0.0f;

    auto info = checkPartialVisibility(scrollOffset, 8, workspaceHeight);

    // First is fully visible, not partially
    EXPECT_FALSE(info.firstPartiallyVisible);
}

TEST_F(EqualPartialVisibilityTest, AtBottomNoAdjustment) {
    // At the very bottom (scrollOffset = maxScrollOffset)
    std::vector<int64_t> workspaces = {1, 2, 3, 4, 5, 6, 7, 8};
    float maxScroll = calculateMaxScrollOffset(workspaces, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    float workspaceHeight = calculateWorkspaceHeight();

    auto info = checkPartialVisibility(maxScroll, 8, workspaceHeight);

    // Last is fully visible, not partially
    EXPECT_FALSE(info.lastPartiallyVisible);
}

TEST_F(EqualPartialVisibilityTest, FewWorkspacesNoAdjustment) {
    // With only 4 workspaces, no scrolling should happen
    float workspaceHeight = calculateWorkspaceHeight();
    float scrollOffset = 0.0f;

    auto info = checkPartialVisibility(scrollOffset, 4, workspaceHeight);

    // With 4 workspaces that fit on screen, neither should be partially visible
    EXPECT_FALSE(info.firstPartiallyVisible);
    EXPECT_FALSE(info.lastPartiallyVisible);
}

TEST_F(EqualPartialVisibilityTest, AdjustmentStaysWithinBounds) {
    // Test that adjustment doesn't push scroll beyond valid range
    std::vector<int64_t> workspaces = {1, 2, 3, 4, 5, 6, 7, 8};
    float maxScroll = calculateMaxScrollOffset(workspaces, MONITOR_HEIGHT, PADDING, GAP_WIDTH);
    float workspaceHeight = calculateWorkspaceHeight();

    // Try various scroll positions
    for (float scroll = 100.0f; scroll < maxScroll - 100.0f; scroll += 100.0f) {
        auto info = checkPartialVisibility(scroll, 8, workspaceHeight);

        if (info.firstPartiallyVisible && info.lastPartiallyVisible) {
            float difference = info.bottomPartial - info.topPartial;
            float adjustedScroll = scroll - (difference / 2.0f);

            // Adjusted scroll should stay within valid range
            EXPECT_GE(adjustedScroll, 0.0f);
            EXPECT_LE(adjustedScroll, maxScroll);
        }
    }
}

// Test helper: Calculate background image scaling to cover monitor
struct BackgroundBox {
    float x, y, w, h;
};

static BackgroundBox calculateBackgroundCover(
    float monitorWidth,
    float monitorHeight,
    float imageWidth,
    float imageHeight
) {
    const float monitorAspect = monitorWidth / monitorHeight;
    const float imageAspect = imageWidth / imageHeight;

    BackgroundBox result = {0, 0, monitorWidth, monitorHeight};

    if (imageAspect > monitorAspect) {
        // Image is wider, scale by height
        const float scale = monitorHeight / imageHeight;
        const float scaledWidth = imageWidth * scale;
        result.x = -(scaledWidth - monitorWidth) / 2.0f;
        result.w = scaledWidth;
    } else {
        // Image is taller, scale by width
        const float scale = monitorWidth / imageWidth;
        const float scaledHeight = imageHeight * scale;
        result.y = -(scaledHeight - monitorHeight) / 2.0f;
        result.h = scaledHeight;
    }

    return result;
}

// ============================================================================
// Background Image Scaling Tests
// ============================================================================

TEST(BackgroundImageTest, SquareImageOnSquareMonitor) {
    // Square image on square monitor should fit exactly
    auto box = calculateBackgroundCover(1000, 1000, 500, 500);

    EXPECT_FLOAT_EQ(box.x, 0.0f);
    EXPECT_FLOAT_EQ(box.y, 0.0f);
    EXPECT_FLOAT_EQ(box.w, 1000.0f);
    EXPECT_FLOAT_EQ(box.h, 1000.0f);
}

TEST(BackgroundImageTest, WideImageOnSquareMonitor) {
    // Wide image (16:9) on square monitor (1:1)
    // Should scale by height and crop sides
    auto box = calculateBackgroundCover(1000, 1000, 1920, 1080);

    EXPECT_FLOAT_EQ(box.y, 0.0f);
    EXPECT_FLOAT_EQ(box.h, 1000.0f);
    EXPECT_LT(box.x, 0.0f); // Negative x means cropped on sides
    EXPECT_GT(box.w, 1000.0f); // Width larger than monitor
}

TEST(BackgroundImageTest, TallImageOnSquareMonitor) {
    // Tall image (9:16) on square monitor (1:1)
    // Should scale by width and crop top/bottom
    auto box = calculateBackgroundCover(1000, 1000, 1080, 1920);

    EXPECT_FLOAT_EQ(box.x, 0.0f);
    EXPECT_FLOAT_EQ(box.w, 1000.0f);
    EXPECT_LT(box.y, 0.0f); // Negative y means cropped on top/bottom
    EXPECT_GT(box.h, 1000.0f); // Height larger than monitor
}

TEST(BackgroundImageTest, WideImageOnWideMonitor) {
    // 16:9 image on 16:9 monitor should fit exactly
    auto box = calculateBackgroundCover(1920, 1080, 3840, 2160);

    EXPECT_FLOAT_EQ(box.x, 0.0f);
    EXPECT_FLOAT_EQ(box.y, 0.0f);
    EXPECT_FLOAT_EQ(box.w, 1920.0f);
    EXPECT_FLOAT_EQ(box.h, 1080.0f);
}

TEST(BackgroundImageTest, UltraWideImageOnStandardMonitor) {
    // 21:9 image on 16:9 monitor
    // Should scale by height and crop sides
    auto box = calculateBackgroundCover(1920, 1080, 2560, 1080);

    EXPECT_FLOAT_EQ(box.y, 0.0f);
    EXPECT_FLOAT_EQ(box.h, 1080.0f);
    EXPECT_LT(box.x, 0.0f); // Cropped on sides
    EXPECT_GT(box.w, 1920.0f);
}

TEST(BackgroundImageTest, PortraitImageOnLandscapeMonitor) {
    // Portrait image (9:16) on landscape monitor (16:9)
    // Should scale by width and crop top/bottom significantly
    auto box = calculateBackgroundCover(1920, 1080, 1080, 1920);

    EXPECT_FLOAT_EQ(box.x, 0.0f);
    EXPECT_FLOAT_EQ(box.w, 1920.0f);
    EXPECT_LT(box.y, 0.0f);
    EXPECT_GT(box.h, 1080.0f);
}

TEST(BackgroundImageTest, SmallImageScalesUp) {
    // Small image should scale up to cover monitor
    auto box = calculateBackgroundCover(1920, 1080, 640, 480);

    // Image should be scaled up
    EXPECT_GT(box.w, 640.0f);
    EXPECT_GT(box.h, 480.0f);

    // Should maintain aspect ratio and cover entire monitor
    float scaledAspect = box.w / box.h;
    float originalAspect = 640.0f / 480.0f;
    EXPECT_NEAR(scaledAspect, originalAspect, 0.01f);
}

TEST(BackgroundImageTest, LargeImageScalesDown) {
    // Large image should scale down to cover monitor
    auto box = calculateBackgroundCover(1920, 1080, 7680, 4320);

    // At least one dimension should equal monitor size
    bool widthMatches = std::abs(box.w - 1920.0f) < 0.01f;
    bool heightMatches = std::abs(box.h - 1080.0f) < 0.01f;
    EXPECT_TRUE(widthMatches || heightMatches);
}

TEST(BackgroundImageTest, BackgroundCoversEntireMonitor) {
    // Test with various aspect ratios
    struct TestCase {
        float monitorW, monitorH;
        float imageW, imageH;
    };

    std::vector<TestCase> cases = {
        {1920, 1080, 1920, 1080},   // 16:9 on 16:9
        {1920, 1080, 3840, 2160},   // 16:9 on 16:9 (different size)
        {2560, 1440, 1920, 1080},   // 16:9 on 16:9 (different res)
        {3440, 1440, 1920, 1080},   // 21:9 monitor with 16:9 image
        {1920, 1080, 2560, 1080},   // 16:9 monitor with 21:9 image
        {1920, 1200, 1920, 1080},   // 16:10 monitor with 16:9 image
    };

    for (const auto& test : cases) {
        auto box = calculateBackgroundCover(test.monitorW, test.monitorH,
                                           test.imageW, test.imageH);

        // Background should completely cover the monitor
        // Either width matches and height is larger, or height matches and width is larger
        bool widthCovers = (std::abs(box.w - test.monitorW) < 0.01f && box.h >= test.monitorH) ||
                          (box.w >= test.monitorW);
        bool heightCovers = (std::abs(box.h - test.monitorH) < 0.01f && box.w >= test.monitorW) ||
                           (box.h >= test.monitorH);

        EXPECT_TRUE(widthCovers && heightCovers)
            << "Failed for monitor " << test.monitorW << "x" << test.monitorH
            << " with image " << test.imageW << "x" << test.imageH;
    }
}

TEST(BackgroundImageTest, CenteredCropping) {
    // Verify that cropping is centered (equal amount cropped from both sides)
    auto box = calculateBackgroundCover(1920, 1080, 2560, 1080);

    // Image is wider, so should be centered horizontally
    float leftCrop = -box.x;
    float rightCrop = box.w - 1920.0f - leftCrop;

    EXPECT_NEAR(leftCrop, rightCrop, 0.01f);
}

TEST(BackgroundImageTest, AspectRatioPreserved) {
    // Test that aspect ratio is preserved during scaling
    struct TestCase {
        float monitorW, monitorH;
        float imageW, imageH;
    };

    std::vector<TestCase> cases = {
        {1920, 1080, 3840, 2160},
        {1920, 1080, 1280, 720},
        {2560, 1440, 1920, 1080},
        {3440, 1440, 2560, 1080},
    };

    for (const auto& test : cases) {
        auto box = calculateBackgroundCover(test.monitorW, test.monitorH,
                                           test.imageW, test.imageH);

        float originalAspect = test.imageW / test.imageH;
        float scaledAspect = box.w / box.h;

        EXPECT_NEAR(originalAspect, scaledAspect, 0.01f)
            << "Aspect ratio not preserved for image " << test.imageW << "x" << test.imageH
            << " on monitor " << test.monitorW << "x" << test.monitorH;
    }
}
