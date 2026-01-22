#include <gtest/gtest.h>

// Sub-cell refinement class to test the 3-letter logic with 3x6 grid (NEW BEHAVIOR: no auto-exit)
class SubColumnState {
private:
    static constexpr int SUB_GRID_ROWS = 3; // 3x6 sub-grid (18 positions, A-R)
    static constexpr int SUB_GRID_COLS = 6;
    bool hasPending;
    int row;
    int col;
    bool hasSubColumn;
    int subColumn; // 0-17 for A-R
    std::string letterSequence;
    bool overlayActive;
    int mouseRow;
    int mouseCol;

public:
    SubColumnState() : hasPending(false), row(-1), col(-1),
        hasSubColumn(false), subColumn(-1), letterSequence(""),
        overlayActive(false), mouseRow(-1), mouseCol(-1) {}

    void activateOverlay() {
        overlayActive = true;
    }

    // Add a letter
    void addLetter(char letter) {
        // Ensure uppercase
        if (letter >= 'a' && letter <= 'z') {
            letter = letter - 'a' + 'A';
        }

        if (letter >= 'A' && letter <= 'Z') {
            // If we already have a pending cell, this letter selects a sub-cell (3x6 grid)
            if (hasPending && letterSequence.length() == 2) {
                int subIndex = letter - 'A';
                // Only accept A-R (0-17) for 3x6 grid
                if (subIndex >= 0 && subIndex < SUB_GRID_ROWS * SUB_GRID_COLS) {
                    hasSubColumn = true;
                    subColumn = subIndex;

                    // NEW BEHAVIOR: Move to sub-cell immediately but DON'T exit overlay
                    mouseRow = row;
                    mouseCol = col;
                    // (in real implementation, sub-cell coordinates would be calculated here)

                    // Clear selection state so user can select another cell
                    hasPending = false;
                    row = -1;
                    col = -1;
                    hasSubColumn = false;
                    subColumn = -1;
                    letterSequence.clear();
                    // NOTE: overlayActive stays true!
                }
                // If S-Z is typed, ignore it (only A-R supported)
            } else {
                // Add to sequence for cell selection
                letterSequence += letter;

                // Limit to 2 characters
                if (letterSequence.length() > 2) {
                    letterSequence = letterSequence.substr(letterSequence.length() - 2);
                }

                // Clear sub-column if changing cell
                hasSubColumn = false;
                subColumn = -1;

                // If we have 2 letters, set pending cell AND move mouse immediately
                if (letterSequence.length() == 2) {
                    hasPending = true;
                    row = letterSequence[0] - 'A';
                    col = letterSequence[1] - 'A';

                    // NEW BEHAVIOR: Move mouse immediately
                    mouseRow = row;
                    mouseCol = col;
                }
            }
        }
    }

    // Press SPACE to exit overlay
    bool pressSpace() {
        if (overlayActive) {
            overlayActive = false;
            clear();
            return true;
        }
        return false;
    }

    // Press RETURN to exit overlay
    bool pressReturn() {
        if (overlayActive) {
            overlayActive = false;
            clear();
            return true;
        }
        return false;
    }

    void pressEsc() {
        overlayActive = false;
        clear();
    }

    void clear() {
        hasPending = false;
        row = -1;
        col = -1;
        hasSubColumn = false;
        subColumn = -1;
        letterSequence.clear();
        mouseRow = -1;
        mouseCol = -1;
    }

    bool hasPendingCell() const { return hasPending; }
    int getRow() const { return row; }
    int getCol() const { return col; }
    bool hasSubCol() const { return hasSubColumn; }
    int getSubCol() const { return subColumn; }
    std::string getSequence() const { return letterSequence; }
    bool isOverlayActive() const { return overlayActive; }
    int getMouseRow() const { return mouseRow; }
    int getMouseCol() const { return mouseCol; }
};

class SubColumnTest : public ::testing::Test {
protected:
    SubColumnState state;

    void SetUp() override {
        state.clear();
        state.activateOverlay();
    }
};

// Test that 2 letters create pending cell and move mouse
TEST_F(SubColumnTest, TwoLettersCreatePendingAndMoveMouse) {
    state.addLetter('B');
    state.addLetter('M');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
    EXPECT_EQ(state.getRow(), 1);
    EXPECT_EQ(state.getCol(), 12);
    // NEW: Mouse moves immediately
    EXPECT_EQ(state.getMouseRow(), 1);
    EXPECT_EQ(state.getMouseCol(), 12);
}

// Test that 3rd letter moves to sub-cell but does NOT auto-exit overlay
TEST_F(SubColumnTest, ThirdLetterMovesButStaysInOverlay) {
    state.addLetter('B');
    state.addLetter('M');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());

    state.addLetter('K');  // 3rd letter - moves to sub-cell but doesn't exit

    // NEW BEHAVIOR: State is cleared for next selection but overlay stays active
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
    EXPECT_TRUE(state.isOverlayActive());  // Overlay still active!
    EXPECT_EQ(state.getSequence(), "");     // Sequence cleared for next cell
}

// Test that typing 4th letter starts new sequence
TEST_F(SubColumnTest, FourthLetterStartsNewSequence) {
    state.addLetter('A');
    state.addLetter('A');
    state.addLetter('H');  // 3rd letter - auto-executes and clears

    // State should be cleared
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());

    // Type new letter - starts new sequence
    state.addLetter('B');
    EXPECT_FALSE(state.hasPendingCell()); // Only 1 letter
    EXPECT_EQ(state.getSequence(), "B");
}

// Test SPACE after 2 letters (no sub-column)
TEST_F(SubColumnTest, SpaceAfterTwoLetters) {
    state.addLetter('C');
    state.addLetter('D');
    EXPECT_FALSE(state.hasSubCol());

    bool executed = state.pressSpace();
    EXPECT_TRUE(executed);
    EXPECT_FALSE(state.hasPendingCell());
}

// Test SPACE after 3 letters (should exit overlay)
TEST_F(SubColumnTest, SpaceAfterThreeLetters) {
    state.addLetter('C');
    state.addLetter('D');
    state.addLetter('E'); // 3rd letter moves to sub-cell but doesn't exit

    // NEW BEHAVIOR: State cleared but overlay still active
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
    EXPECT_TRUE(state.isOverlayActive());  // Overlay still active

    // SPACE should exit overlay
    bool exited = state.pressSpace();
    EXPECT_TRUE(exited);
    EXPECT_FALSE(state.isOverlayActive());
}

// Test RETURN after 2 letters (no sub-column)
TEST_F(SubColumnTest, ReturnAfterTwoLetters) {
    state.addLetter('C');
    state.addLetter('D');
    EXPECT_FALSE(state.hasSubCol());

    bool executed = state.pressReturn();
    EXPECT_TRUE(executed);
    EXPECT_FALSE(state.hasPendingCell());
}

// Test RETURN after 3 letters (should exit overlay)
TEST_F(SubColumnTest, ReturnAfterThreeLetters) {
    state.addLetter('C');
    state.addLetter('D');
    state.addLetter('E'); // 3rd letter moves to sub-cell but doesn't exit

    // NEW BEHAVIOR: State cleared but overlay still active
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
    EXPECT_TRUE(state.isOverlayActive());  // Overlay still active

    // RETURN should exit overlay
    bool exited = state.pressReturn();
    EXPECT_TRUE(exited);
    EXPECT_FALSE(state.isOverlayActive());
}

// Test ESC clears pending cell (before 3rd letter)
TEST_F(SubColumnTest, EscClearsPendingCell) {
    state.addLetter('A');
    state.addLetter('B');
    EXPECT_TRUE(state.hasPendingCell());

    state.pressEsc();
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
}

// Test all sub-cell values (A-R = 0-17) for 3x6 grid auto-execute
TEST_F(SubColumnTest, AllSubColumnValues) {
    // Test A-R (18 positions for 3x6 grid) - each should auto-execute
    for (char c = 'A'; c <= 'R'; c++) {
        state.clear();
        state.addLetter('M');
        state.addLetter('M');
        state.addLetter(c); // Should auto-execute and clear

        // State should be cleared after auto-execution
        EXPECT_FALSE(state.hasPendingCell());
        EXPECT_FALSE(state.hasSubCol());
    }

    // Test that S-Z are ignored (only A-R supported)
    state.clear();
    state.addLetter('M');
    state.addLetter('M');
    state.addLetter('S');
    EXPECT_TRUE(state.hasPendingCell()); // S should be ignored, pending cell remains
    EXPECT_FALSE(state.hasSubCol());

    state.clear();
    state.addLetter('M');
    state.addLetter('M');
    state.addLetter('Z');
    EXPECT_TRUE(state.hasPendingCell()); // Z should be ignored, pending cell remains
    EXPECT_FALSE(state.hasSubCol());
}

// Test sub-column with corner cells - verify auto-execution
TEST_F(SubColumnTest, SubColumnTopLeft_AAA) {
    state.addLetter('A');
    state.addLetter('A');
    EXPECT_EQ(state.getRow(), 0);
    EXPECT_EQ(state.getCol(), 0);

    state.addLetter('A'); // Auto-executes and clears

    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
}

TEST_F(SubColumnTest, SubColumnTopRight_AZR) {
    state.addLetter('A');
    state.addLetter('Z');
    EXPECT_EQ(state.getRow(), 0);
    EXPECT_EQ(state.getCol(), 25);

    state.addLetter('R'); // R is the last valid sub-cell (17) - auto-executes

    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
}

TEST_F(SubColumnTest, SubColumnBottomLeft_ZAA) {
    state.addLetter('Z');
    state.addLetter('A');
    EXPECT_EQ(state.getRow(), 25);
    EXPECT_EQ(state.getCol(), 0);

    state.addLetter('A'); // Auto-executes and clears

    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
}

TEST_F(SubColumnTest, SubColumnBottomRight_ZZR) {
    state.addLetter('Z');
    state.addLetter('Z');
    EXPECT_EQ(state.getRow(), 25);
    EXPECT_EQ(state.getCol(), 25);

    state.addLetter('R'); // R is the last valid sub-cell (17) - auto-executes

    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
}

// Test typing 4 letters: 3rd auto-executes, 4th starts new sequence
TEST_F(SubColumnTest, FourLetters) {
    state.addLetter('A');  // 1st
    state.addLetter('B');  // 2nd
    state.addLetter('C');  // 3rd - auto-executes and clears

    // State should be cleared after 3rd letter
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());

    state.addLetter('D');  // 4th - starts new sequence

    // Should have just "D" in sequence
    EXPECT_FALSE(state.hasPendingCell()); // Only 1 letter
    EXPECT_EQ(state.getSequence(), "D");
}

// Test coordinate calculation for cell in 26x26 grid (before 3rd letter)
TEST_F(SubColumnTest, CellCoordinateCalculation) {
    // Assume 1920x1080 monitor
    const float monitorWidth = 1920.0f;
    const float monitorHeight = 1080.0f;
    const int gridSize = 26;

    const float cellWidth = monitorWidth / gridSize;
    const float cellHeight = monitorHeight / gridSize;

    // Cell BM = row 1, col 12
    state.addLetter('B');
    state.addLetter('M');

    const float cellX = state.getCol() * cellWidth;
    const float cellY = state.getRow() * cellHeight;
    const float cellCenterX = cellX + cellWidth / 2.0f;
    const float cellCenterY = cellY + cellHeight / 2.0f;

    // Verify cell center is reasonable
    EXPECT_GT(cellCenterX, 0.0f);
    EXPECT_LT(cellCenterX, monitorWidth);
    EXPECT_GT(cellCenterY, 0.0f);
    EXPECT_LT(cellCenterY, monitorHeight);

    // Verify correct row and col
    EXPECT_EQ(state.getRow(), 1);  // B = 1
    EXPECT_EQ(state.getCol(), 12); // M = 12
}

// Test lowercase letters work and auto-execute
TEST_F(SubColumnTest, LowercaseSubColumn) {
    state.addLetter('a');
    state.addLetter('b');
    EXPECT_TRUE(state.hasPendingCell());

    state.addLetter('c'); // 3rd letter - auto-executes and clears

    // State should be cleared after auto-execution
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.hasSubCol());
}
