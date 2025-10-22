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
