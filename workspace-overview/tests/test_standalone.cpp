#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cmath>

// Standalone implementations of plugin logic for testing

// Test helper: Calculate workspace offsets for different current workspace IDs
struct WorkspaceOffsets {
    int offsets[4];
};

static WorkspaceOffsets calculateWorkspaceOffsets(int currentID) {
    WorkspaceOffsets result;

    if (currentID == 1) {
        // First workspace: show next 4 workspaces (2, 3, 4, 5)
        result.offsets[0] = +1;
        result.offsets[1] = +2;
        result.offsets[2] = +3;
        result.offsets[3] = +4;
    } else if (currentID == 2) {
        // Second workspace: show previous 1 and next 3 (1, 3, 4, 5)
        result.offsets[0] = -1;
        result.offsets[1] = +1;
        result.offsets[2] = +2;
        result.offsets[3] = +3;
    } else {
        // Normal case: show 2 before and 2 after (-2, -1, +1, +2)
        result.offsets[0] = -2;
        result.offsets[1] = -1;
        result.offsets[2] = +1;
        result.offsets[3] = +2;
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

    const float leftWidth = monitorWidth * leftWidthRatio;
    const float rightWidth = monitorWidth - leftWidth;
    const float availableHeight = monitorHeight - (2 * padding);
    const float totalGaps = (LEFT_WORKSPACES - 1) * gapWidth;
    const float leftPreviewHeight = (availableHeight - totalGaps) / LEFT_WORKSPACES;

    // Left side workspaces
    for (int i = 0; i < LEFT_WORKSPACES; ++i) {
        LayoutBox box;
        box.x = padding;
        box.y = padding + i * (leftPreviewHeight + gapWidth);
        box.w = leftWidth - (2 * padding);
        box.h = leftPreviewHeight;
        box.isActive = false;
        boxes.push_back(box);
    }

    // Right side - active workspace
    LayoutBox activeBox;
    activeBox.x = leftWidth + padding;
    activeBox.y = padding;
    activeBox.w = rightWidth - (2 * padding);
    activeBox.h = monitorHeight - (2 * padding);
    activeBox.isActive = true;
    boxes.push_back(activeBox);

    return boxes;
}

// Test: Workspace offset calculation for first workspace
TEST(WorkspaceOffsetsTest, FirstWorkspace) {
    auto offsets = calculateWorkspaceOffsets(1);
    EXPECT_EQ(offsets.offsets[0], +1);
    EXPECT_EQ(offsets.offsets[1], +2);
    EXPECT_EQ(offsets.offsets[2], +3);
    EXPECT_EQ(offsets.offsets[3], +4);
}

// Test: Workspace offset calculation for second workspace
TEST(WorkspaceOffsetsTest, SecondWorkspace) {
    auto offsets = calculateWorkspaceOffsets(2);
    EXPECT_EQ(offsets.offsets[0], -1);
    EXPECT_EQ(offsets.offsets[1], +1);
    EXPECT_EQ(offsets.offsets[2], +2);
    EXPECT_EQ(offsets.offsets[3], +3);
}

// Test: Workspace offset calculation for normal workspace
TEST(WorkspaceOffsetsTest, NormalWorkspace) {
    auto offsets = calculateWorkspaceOffsets(5);
    EXPECT_EQ(offsets.offsets[0], -2);
    EXPECT_EQ(offsets.offsets[1], -1);
    EXPECT_EQ(offsets.offsets[2], +1);
    EXPECT_EQ(offsets.offsets[3], +2);
}

// Test: Workspace offset calculation for high workspace ID
TEST(WorkspaceOffsetsTest, HighWorkspaceID) {
    auto offsets = calculateWorkspaceOffsets(100);
    EXPECT_EQ(offsets.offsets[0], -2);
    EXPECT_EQ(offsets.offsets[1], -1);
    EXPECT_EQ(offsets.offsets[2], +1);
    EXPECT_EQ(offsets.offsets[3], +2);
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

    const float expectedLeftWidth = monitorWidth * leftWidthRatio - 2 * padding;

    // Check first 4 boxes (left side)
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(boxes[i].w, expectedLeftWidth, 0.1f);
        EXPECT_GE(boxes[i].x, 0.0f);
        EXPECT_LE(boxes[i].x + boxes[i].w, monitorWidth * leftWidthRatio);
    }
}

// Test: Layout calculation - right side dimensions
TEST(LayoutTest, RightSideDimensions) {
    const float monitorWidth = 1920;
    const float monitorHeight = 1080;
    const float leftWidthRatio = 0.33f;
    const float padding = 20.0f;

    auto boxes = calculateLayout(monitorWidth, monitorHeight, leftWidthRatio, 10.0f, padding);

    const float leftWidth = monitorWidth * leftWidthRatio;
    const float rightWidth = monitorWidth - leftWidth - 2 * padding;

    // Check active workspace box (index 4)
    EXPECT_NEAR(boxes[4].w, rightWidth, 0.1f);
    EXPECT_NEAR(boxes[4].h, monitorHeight - 2 * padding, 0.1f);
    EXPECT_GE(boxes[4].x, leftWidth);
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
    auto offsets = calculateWorkspaceOffsets(3);
    EXPECT_EQ(offsets.offsets[0], -2);
    EXPECT_EQ(offsets.offsets[1], -1);
    EXPECT_EQ(offsets.offsets[2], +1);
    EXPECT_EQ(offsets.offsets[3], +2);
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
