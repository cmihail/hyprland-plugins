#include <gtest/gtest.h>

// Pending cell state class to test the new immediate movement behavior
class PendingCellState {
private:
    bool hasPending;
    int row;
    int col;
    std::string letterSequence;
    bool overlayActive;
    int mouseRow;
    int mouseCol;

public:
    PendingCellState() : hasPending(false), row(-1), col(-1), letterSequence(""),
                         overlayActive(false), mouseRow(-1), mouseCol(-1) {}

    void activateOverlay() {
        overlayActive = true;
    }

    // Add a letter to the sequence
    void addLetter(char letter) {
        // Ensure uppercase
        if (letter >= 'a' && letter <= 'z') {
            letter = letter - 'a' + 'A';
        }

        if (letter >= 'A' && letter <= 'Z') {
            letterSequence += letter;

            // Limit to 2 characters
            if (letterSequence.length() > 2) {
                letterSequence = letterSequence.substr(letterSequence.length() - 2);
            }

            // If we have 2 letters, set as pending AND move mouse immediately
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

    // Press SPACE to exit overlay (no longer executes movement)
    bool pressSpace() {
        if (overlayActive) {
            // Just exit overlay
            overlayActive = false;
            clear();
            return true; // Overlay exited
        }
        return false; // Overlay not active
    }

    // Press RETURN to exit overlay (same as SPACE)
    bool pressReturn() {
        if (overlayActive) {
            // Just exit overlay
            overlayActive = false;
            clear();
            return true; // Overlay exited
        }
        return false; // Overlay not active
    }

    // Press ESC to cancel
    void pressEsc() {
        overlayActive = false;
        clear();
    }

    void clear() {
        hasPending = false;
        row = -1;
        col = -1;
        letterSequence.clear();
        mouseRow = -1;
        mouseCol = -1;
    }

    bool hasPendingCell() const { return hasPending; }
    int getRow() const { return row; }
    int getCol() const { return col; }
    std::string getSequence() const { return letterSequence; }
    bool isOverlayActive() const { return overlayActive; }
    int getMouseRow() const { return mouseRow; }
    int getMouseCol() const { return mouseCol; }
};

class PendingCellTest : public ::testing::Test {
protected:
    PendingCellState state;

    void SetUp() override {
        state.clear();
        state.activateOverlay();
    }
};

// Test that adding 2 letters creates a pending cell AND moves mouse immediately
TEST_F(PendingCellTest, TwoLettersCreatePendingAndMoveMouse) {
    state.addLetter('A');
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_EQ(state.getMouseRow(), -1);  // No movement yet

    state.addLetter('B');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), 0);  // A = 0
    EXPECT_EQ(state.getCol(), 1);  // B = 1
    // NEW BEHAVIOR: Mouse moves immediately
    EXPECT_EQ(state.getMouseRow(), 0);
    EXPECT_EQ(state.getMouseCol(), 1);
}

// Test that SPACE exits overlay (doesn't execute movement since it already happened)
TEST_F(PendingCellTest, SpaceExitsOverlay) {
    state.addLetter('B');
    state.addLetter('M');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_TRUE(state.isOverlayActive());
    EXPECT_EQ(state.getMouseRow(), 1);   // Mouse already moved
    EXPECT_EQ(state.getMouseCol(), 12);

    bool exited = state.pressSpace();
    EXPECT_TRUE(exited);
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.isOverlayActive());
}

// Test that SPACE without overlay active does nothing
TEST_F(PendingCellTest, SpaceWithoutOverlayDoesNothing) {
    state.pressEsc();  // Deactivate overlay
    EXPECT_FALSE(state.isOverlayActive());

    bool exited = state.pressSpace();
    EXPECT_FALSE(exited);
}

// Test that RETURN exits overlay
TEST_F(PendingCellTest, ReturnExitsOverlay) {
    state.addLetter('B');
    state.addLetter('M');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_TRUE(state.isOverlayActive());

    bool exited = state.pressReturn();
    EXPECT_TRUE(exited);
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.isOverlayActive());
}

// Test that RETURN without overlay active does nothing
TEST_F(PendingCellTest, ReturnWithoutOverlayDoesNothing) {
    state.pressEsc();  // Deactivate overlay
    EXPECT_FALSE(state.isOverlayActive());

    bool exited = state.pressReturn();
    EXPECT_FALSE(exited);
}

// Test that ESC clears pending cell
TEST_F(PendingCellTest, EscClearsPending) {
    state.addLetter('C');
    state.addLetter('D');
    EXPECT_TRUE(state.hasPendingCell());

    state.pressEsc();
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), -1);
    EXPECT_EQ(state.getCol(), -1);
}

// Test typing 3rd letter updates pending cell and moves mouse again
TEST_F(PendingCellTest, ThirdLetterUpdatesPendingAndMovesMouse) {
    state.addLetter('A');
    state.addLetter('B');
    EXPECT_EQ(state.getRow(), 0);
    EXPECT_EQ(state.getCol(), 1);
    EXPECT_EQ(state.getMouseRow(), 0);
    EXPECT_EQ(state.getMouseCol(), 1);

    state.addLetter('C');
    EXPECT_EQ(state.getRow(), 1);  // B = 1
    EXPECT_EQ(state.getCol(), 2);  // C = 2
    EXPECT_EQ(state.getSequence(), "BC");
    // Mouse moves again to new cell
    EXPECT_EQ(state.getMouseRow(), 1);
    EXPECT_EQ(state.getMouseCol(), 2);
}

// Test lowercase letters work
TEST_F(PendingCellTest, LowercaseLettersWork) {
    state.addLetter('a');
    state.addLetter('z');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), 0);   // A = 0
    EXPECT_EQ(state.getCol(), 25);  // Z = 25
}

// Test corner cells
TEST_F(PendingCellTest, TopLeftCell_AA) {
    state.addLetter('A');
    state.addLetter('A');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), 0);
    EXPECT_EQ(state.getCol(), 0);
}

TEST_F(PendingCellTest, TopRightCell_AZ) {
    state.addLetter('A');
    state.addLetter('Z');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), 0);
    EXPECT_EQ(state.getCol(), 25);
}

TEST_F(PendingCellTest, BottomLeftCell_ZA) {
    state.addLetter('Z');
    state.addLetter('A');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), 25);
    EXPECT_EQ(state.getCol(), 0);
}

TEST_F(PendingCellTest, BottomRightCell_ZZ) {
    state.addLetter('Z');
    state.addLetter('Z');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), 25);
    EXPECT_EQ(state.getCol(), 25);
}

// Test workflow: type letters, mouse moves, correct with new letters, then exit
TEST_F(PendingCellTest, CorrectSequenceAndMouseMovesMultipleTimes) {
    // Type first cell
    state.addLetter('A');
    state.addLetter('A');
    EXPECT_EQ(state.getRow(), 0);
    EXPECT_EQ(state.getCol(), 0);
    EXPECT_EQ(state.getMouseRow(), 0);  // Mouse moved to AA
    EXPECT_EQ(state.getMouseCol(), 0);

    // Correct by typing new letters (keeps last 2) - mouse moves again
    state.addLetter('B');
    EXPECT_EQ(state.getRow(), 0);  // A = 0
    EXPECT_EQ(state.getCol(), 1);  // B = 1
    EXPECT_EQ(state.getMouseRow(), 0);  // Mouse moved to AB
    EXPECT_EQ(state.getMouseCol(), 1);

    state.addLetter('M');
    EXPECT_EQ(state.getRow(), 1);   // B = 1
    EXPECT_EQ(state.getCol(), 12);  // M = 12
    EXPECT_EQ(state.getMouseRow(), 1);   // Mouse moved to BM
    EXPECT_EQ(state.getMouseCol(), 12);

    // Now exit overlay
    bool exited = state.pressSpace();
    EXPECT_TRUE(exited);
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_FALSE(state.isOverlayActive());
}

// Test that single letter doesn't create pending cell
TEST_F(PendingCellTest, SingleLetterNoPending) {
    state.addLetter('X');
    EXPECT_FALSE(state.hasPendingCell());
    EXPECT_EQ(state.getSequence(), "X");
}

// Test middle cell
TEST_F(PendingCellTest, MiddleCell_MM) {
    state.addLetter('M');
    state.addLetter('M');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), 12);  // M = 12
    EXPECT_EQ(state.getCol(), 12);  // M = 12
}
