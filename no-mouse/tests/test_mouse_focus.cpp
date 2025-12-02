#include <gtest/gtest.h>

// Mock mouse movement and focus state to test the movement and focus logic
class MouseFocusState {
private:
    static constexpr int GRID_SIZE = 26;
    static constexpr int SUB_GRID_ROWS = 3;
    static constexpr int SUB_GRID_COLS = 6;

    float monitorWidth;
    float monitorHeight;
    float cursorX;
    float cursorY;
    bool cursorWarped;
    bool mouseSimulated;
    bool windowFocused;
    std::string focusedWindow;

public:
    MouseFocusState(float width = 1920.0f, float height = 1080.0f)
        : monitorWidth(width), monitorHeight(height),
          cursorX(0.0f), cursorY(0.0f),
          cursorWarped(false), mouseSimulated(false),
          windowFocused(false), focusedWindow("") {}

    // Simulate moving to a cell (with optional sub-column)
    void moveToCell(int row, int col, int subColumn = -1) {
        if (row < 0 || row >= GRID_SIZE || col < 0 || col >= GRID_SIZE) {
            return; // Invalid cell
        }

        const float cellWidth = monitorWidth / (float)GRID_SIZE;
        const float cellHeight = monitorHeight / (float)GRID_SIZE;

        float targetX;
        float targetY;

        if (subColumn >= 0 && subColumn < SUB_GRID_ROWS * SUB_GRID_COLS) {
            // Sub-cell refinement
            const float subCellWidth = cellWidth / (float)SUB_GRID_COLS;
            const float subCellHeight = cellHeight / (float)SUB_GRID_ROWS;

            const int subRow = subColumn / SUB_GRID_COLS;
            const int subCol = subColumn % SUB_GRID_COLS;

            targetX = col * cellWidth + subCol * subCellWidth + subCellWidth / 2.0f;
            targetY = row * cellHeight + subRow * subCellHeight + subCellHeight / 2.0f;
        } else {
            // Cell center
            targetX = col * cellWidth + cellWidth / 2.0f;
            targetY = row * cellHeight + cellHeight / 2.0f;
        }

        // Step 1: Warp cursor
        cursorX = targetX;
        cursorY = targetY;
        cursorWarped = true;

        // Step 2: Simulate mouse movement (for hover states)
        mouseSimulated = true;

        // Step 3: Focus window under cursor
        focusWindowAtPosition(targetX, targetY);
    }

    void focusWindowAtPosition(float x, float y) {
        // Simulate finding a window at the position
        if (x >= 0 && x <= monitorWidth && y >= 0 && y <= monitorHeight) {
            windowFocused = true;
            focusedWindow = "Window at (" + std::to_string((int)x) + "," + std::to_string((int)y) + ")";
        }
    }

    // Test helpers
    float getCursorX() const { return cursorX; }
    float getCursorY() const { return cursorY; }
    bool wasCursorWarped() const { return cursorWarped; }
    bool wasMouseSimulated() const { return mouseSimulated; }
    bool isWindowFocused() const { return windowFocused; }
    std::string getFocusedWindow() const { return focusedWindow; }

    void reset() {
        cursorX = 0.0f;
        cursorY = 0.0f;
        cursorWarped = false;
        mouseSimulated = false;
        windowFocused = false;
        focusedWindow = "";
    }
};

class MouseFocusTest : public ::testing::Test {
protected:
    MouseFocusState state;

    void SetUp() override {
        state.reset();
    }
};

// Test that cursor warp happens when moving to a cell
TEST_F(MouseFocusTest, CursorWarpOccurs) {
    state.moveToCell(5, 10);
    EXPECT_TRUE(state.wasCursorWarped());
}

// Test that mouse simulation happens after cursor warp
TEST_F(MouseFocusTest, MouseSimulationOccurs) {
    state.moveToCell(5, 10);
    EXPECT_TRUE(state.wasMouseSimulated());
}

// Test that window gets focused after movement
TEST_F(MouseFocusTest, WindowFocusOccurs) {
    state.moveToCell(5, 10);
    EXPECT_TRUE(state.isWindowFocused());
}

// Test complete movement sequence: warp -> simulate -> focus
TEST_F(MouseFocusTest, CompleteMovementSequence) {
    state.moveToCell(12, 12); // Middle of screen

    EXPECT_TRUE(state.wasCursorWarped());
    EXPECT_TRUE(state.wasMouseSimulated());
    EXPECT_TRUE(state.isWindowFocused());
    EXPECT_FALSE(state.getFocusedWindow().empty());
}

// Test movement to top-left cell (AA)
TEST_F(MouseFocusTest, MovementToTopLeft) {
    state.moveToCell(0, 0);

    EXPECT_TRUE(state.wasCursorWarped());
    EXPECT_TRUE(state.wasMouseSimulated());
    EXPECT_TRUE(state.isWindowFocused());

    // Cursor should be at center of first cell
    EXPECT_GT(state.getCursorX(), 0.0f);
    EXPECT_GT(state.getCursorY(), 0.0f);
}

// Test movement to bottom-right cell (ZZ)
TEST_F(MouseFocusTest, MovementToBottomRight) {
    state.moveToCell(25, 25);

    EXPECT_TRUE(state.wasCursorWarped());
    EXPECT_TRUE(state.wasMouseSimulated());
    EXPECT_TRUE(state.isWindowFocused());

    // Cursor should be near bottom-right
    EXPECT_GT(state.getCursorX(), 1800.0f); // Near right edge of 1920px
    EXPECT_GT(state.getCursorY(), 1000.0f); // Near bottom edge of 1080px
}

// Test movement with sub-column refinement
TEST_F(MouseFocusTest, MovementWithSubColumn) {
    state.moveToCell(10, 10, 5); // Cell JJ, sub-column F (index 5)

    EXPECT_TRUE(state.wasCursorWarped());
    EXPECT_TRUE(state.wasMouseSimulated());
    EXPECT_TRUE(state.isWindowFocused());
}

// Test that sub-column provides more precision than cell center
TEST_F(MouseFocusTest, SubColumnPrecision) {
    state.moveToCell(10, 10); // Cell center
    const float centerX = state.getCursorX();
    const float centerY = state.getCursorY();

    state.reset();

    state.moveToCell(10, 10, 0); // Sub-column A (top-left of cell)
    const float subColX = state.getCursorX();
    const float subColY = state.getCursorY();

    // Sub-column position should be different from cell center
    EXPECT_NE(centerX, subColX);
    EXPECT_NE(centerY, subColY);
}

// Test invalid cell coordinates
TEST_F(MouseFocusTest, InvalidCellCoordinates_Negative) {
    state.moveToCell(-1, 5);

    // Movement should not occur for invalid coordinates
    EXPECT_FALSE(state.wasCursorWarped());
    EXPECT_FALSE(state.wasMouseSimulated());
}

TEST_F(MouseFocusTest, InvalidCellCoordinates_OutOfBounds) {
    state.moveToCell(30, 30); // Beyond 26x26 grid

    EXPECT_FALSE(state.wasCursorWarped());
    EXPECT_FALSE(state.wasMouseSimulated());
}

// Test that all sub-columns (A-R, 0-17) are valid for 3x6 grid
TEST_F(MouseFocusTest, AllSubColumnsValid) {
    for (int subCol = 0; subCol < 18; subCol++) {
        state.reset();
        state.moveToCell(13, 13, subCol); // Middle cell, each sub-column

        EXPECT_TRUE(state.wasCursorWarped()) << "Sub-column " << subCol << " failed";
        EXPECT_TRUE(state.wasMouseSimulated()) << "Sub-column " << subCol << " failed";
        EXPECT_TRUE(state.isWindowFocused()) << "Sub-column " << subCol << " failed";
    }
}

// Test that invalid sub-columns are handled
TEST_F(MouseFocusTest, InvalidSubColumn) {
    state.moveToCell(13, 13, 20); // Sub-column 20 is invalid (only 0-17 for 3x6 grid)

    // Movement should still occur but without sub-column refinement
    EXPECT_TRUE(state.wasCursorWarped());
}

// Test cursor position calculation for specific cells
TEST_F(MouseFocusTest, CursorPositionCalculation_1920x1080) {
    const float cellWidth = 1920.0f / 26.0f;  // ~73.846px
    const float cellHeight = 1080.0f / 26.0f; // ~41.538px

    // Cell BM = row 1, col 12
    state.moveToCell(1, 12);

    const float expectedX = 12 * cellWidth + cellWidth / 2.0f;
    const float expectedY = 1 * cellHeight + cellHeight / 2.0f;

    EXPECT_FLOAT_EQ(state.getCursorX(), expectedX);
    EXPECT_FLOAT_EQ(state.getCursorY(), expectedY);
}

// Test sub-column position calculation
TEST_F(MouseFocusTest, SubColumnPositionCalculation) {
    const float cellWidth = 1920.0f / 26.0f;
    const float cellHeight = 1080.0f / 26.0f;
    const float subCellWidth = cellWidth / 6.0f;
    const float subCellHeight = cellHeight / 3.0f;

    // Cell BM, sub-column H (index 7)
    // In 3x6 grid: H is at row 1, col 1 (A B C D E F = row 0, G H I J K L = row 1)
    state.moveToCell(1, 12, 7);

    const int subRow = 7 / 6; // 1
    const int subCol = 7 % 6; // 1

    const float expectedX = 12 * cellWidth + subCol * subCellWidth + subCellWidth / 2.0f;
    const float expectedY = 1 * cellHeight + subRow * subCellHeight + subCellHeight / 2.0f;

    EXPECT_FLOAT_EQ(state.getCursorX(), expectedX);
    EXPECT_FLOAT_EQ(state.getCursorY(), expectedY);
}

// Test different monitor resolutions
TEST_F(MouseFocusTest, DifferentResolution_2560x1440) {
    MouseFocusState state2560(2560.0f, 1440.0f);

    state2560.moveToCell(13, 13);

    EXPECT_TRUE(state2560.wasCursorWarped());
    EXPECT_TRUE(state2560.wasMouseSimulated());
    EXPECT_TRUE(state2560.isWindowFocused());

    // Cursor should be roughly in middle of screen
    EXPECT_GT(state2560.getCursorX(), 1200.0f);
    EXPECT_LT(state2560.getCursorX(), 1400.0f);
    EXPECT_GT(state2560.getCursorY(), 670.0f);
    EXPECT_LT(state2560.getCursorY(), 770.0f);
}

// Test focus window name is set correctly
TEST_F(MouseFocusTest, FocusWindowNameSet) {
    state.moveToCell(5, 5);

    EXPECT_FALSE(state.getFocusedWindow().empty());
    EXPECT_NE(state.getFocusedWindow().find("Window at"), std::string::npos);
}

// Test multiple movements update state correctly
TEST_F(MouseFocusTest, MultipleMovements) {
    state.moveToCell(0, 0); // Top-left
    EXPECT_TRUE(state.isWindowFocused());

    state.reset();

    state.moveToCell(25, 25); // Bottom-right
    EXPECT_TRUE(state.isWindowFocused());

    state.reset();

    state.moveToCell(12, 12); // Center
    EXPECT_TRUE(state.isWindowFocused());
}
