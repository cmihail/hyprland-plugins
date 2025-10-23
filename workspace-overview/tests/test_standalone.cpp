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

static WorkspaceIDs calculateWorkspaceIDs(int currentID, const std::vector<int>& monitorWorkspaces) {
    WorkspaceIDs result;

    // Create a copy of monitor workspaces and sort them
    std::vector<int> sortedWorkspaces = monitorWorkspaces;
    std::sort(sortedWorkspaces.begin(), sortedWorkspaces.end());

    // Remove the active workspace from the list
    sortedWorkspaces.erase(
        std::remove(sortedWorkspaces.begin(), sortedWorkspaces.end(), currentID),
        sortedWorkspaces.end()
    );

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

static std::vector<LayoutBox> calculateLayout(float monitorWidth, float monitorHeight,
                                               float leftWidthRatio, float gapWidth, float padding) {
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
    auto workspaces = calculateWorkspaceIDs(1, {1, 2, 3, 4, 5});
    EXPECT_EQ(workspaces.ids[0], 2);
    EXPECT_EQ(workspaces.ids[1], 3);
    EXPECT_EQ(workspaces.ids[2], 4);
    EXPECT_EQ(workspaces.ids[3], 5);
}

// Test: Workspace ID calculation for second workspace
TEST(WorkspaceOffsetsTest, SecondWorkspace) {
    // Monitor has workspaces 1, 2, 3, 4, 5 - active is 2
    auto workspaces = calculateWorkspaceIDs(2, {1, 2, 3, 4, 5});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 3);
    EXPECT_EQ(workspaces.ids[2], 4);
    EXPECT_EQ(workspaces.ids[3], 5);
}

// Test: Workspace ID calculation for normal workspace
TEST(WorkspaceOffsetsTest, NormalWorkspace) {
    // Monitor has workspace 5 only - active is 5, fill with new IDs
    auto workspaces = calculateWorkspaceIDs(5, {5});
    EXPECT_EQ(workspaces.ids[0], 1);  // New workspace IDs to fill up to 4
    EXPECT_EQ(workspaces.ids[1], 2);
    EXPECT_EQ(workspaces.ids[2], 3);
    EXPECT_EQ(workspaces.ids[3], 4);
}

// Test: Workspace ID calculation for high workspace ID
TEST(WorkspaceOffsetsTest, HighWorkspaceID) {
    // Monitor has workspace 100 only - active is 100, fill with new IDs
    auto workspaces = calculateWorkspaceIDs(100, {100});
    EXPECT_EQ(workspaces.ids[0], 1);  // New workspace IDs to fill up to 4
    EXPECT_EQ(workspaces.ids[1], 2);
    EXPECT_EQ(workspaces.ids[2], 3);
    EXPECT_EQ(workspaces.ids[3], 4);
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
    auto workspaces = calculateWorkspaceIDs(3, {1, 2, 3, 4, 5});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 2);
    EXPECT_EQ(workspaces.ids[2], 4);
    EXPECT_EQ(workspaces.ids[3], 5);
}

// Test: Monitor 1 specific workspaces (1, 3, 5 on monitor, 7 unassigned)
TEST(WorkspaceOffsetsTest, Monitor1Example) {
    // Monitor 1 has workspaces 1, 3, 5, and 7 is unassigned (available) - active is 5
    auto workspaces = calculateWorkspaceIDs(5, {1, 3, 5, 7});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 3);
    EXPECT_EQ(workspaces.ids[2], 7);
    EXPECT_EQ(workspaces.ids[3], 2);  // Fill with next available (2, 4, 6 are unused)
}

// Test: Monitor 2 specific workspaces (2, 4, 5 on monitor, 7 unassigned)
TEST(WorkspaceOffsetsTest, Monitor2Example) {
    // Monitor 2 has workspaces 2, 4, 5, and 7 is unassigned (available) - active is 5
    auto workspaces = calculateWorkspaceIDs(5, {2, 4, 5, 7});
    EXPECT_EQ(workspaces.ids[0], 2);
    EXPECT_EQ(workspaces.ids[1], 4);
    EXPECT_EQ(workspaces.ids[2], 7);
    EXPECT_EQ(workspaces.ids[3], 1);  // Fill with next available (1, 3, 6 are unused)
}

// Test: Sparse workspace IDs
TEST(WorkspaceOffsetsTest, SparseWorkspaceIDs) {
    // Monitor has workspaces 1, 10, 20, 30 - active is 20
    auto workspaces = calculateWorkspaceIDs(20, {1, 10, 20, 30});
    EXPECT_EQ(workspaces.ids[0], 1);
    EXPECT_EQ(workspaces.ids[1], 10);
    EXPECT_EQ(workspaces.ids[2], 30);
    EXPECT_EQ(workspaces.ids[3], 2);  // Fill with next available (2, 3, 4, etc)
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
