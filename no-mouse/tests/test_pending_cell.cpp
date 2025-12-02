#include <gtest/gtest.h>

// Pending cell state class to test the new SPACE key logic
class PendingCellState {
private:
    bool hasPending;
    int row;
    int col;
    std::string letterSequence;

public:
    PendingCellState() : hasPending(false), row(-1), col(-1), letterSequence("") {}

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

            // If we have 2 letters, set as pending
            if (letterSequence.length() == 2) {
                hasPending = true;
                row = letterSequence[0] - 'A';
                col = letterSequence[1] - 'A';
            }
        }
    }

    // Press SPACE to execute the pending cell
    bool pressSpace() {
        if (hasPending) {
            // Simulate execution
            clear();
            return true; // Movement executed
        }
        return false; // No pending cell
    }

    // Press RETURN to execute the pending cell (same as SPACE)
    bool pressReturn() {
        if (hasPending) {
            // Simulate execution
            clear();
            return true; // Movement executed
        }
        return false; // No pending cell
    }

    // Press ESC to cancel
    void pressEsc() {
        clear();
    }

    void clear() {
        hasPending = false;
        row = -1;
        col = -1;
        letterSequence.clear();
    }

    bool hasPendingCell() const { return hasPending; }
    int getRow() const { return row; }
    int getCol() const { return col; }
    std::string getSequence() const { return letterSequence; }
};

class PendingCellTest : public ::testing::Test {
protected:
    PendingCellState state;

    void SetUp() override {
        state.clear();
    }
};

// Test that adding 2 letters creates a pending cell
TEST_F(PendingCellTest, TwoLettersCreatePending) {
    state.addLetter('A');
    EXPECT_FALSE(state.hasPendingCell());

    state.addLetter('B');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_EQ(state.getRow(), 0);  // A = 0
    EXPECT_EQ(state.getCol(), 1);  // B = 1
}

// Test that SPACE executes pending cell
TEST_F(PendingCellTest, SpaceExecutesPending) {
    state.addLetter('B');
    state.addLetter('M');
    EXPECT_TRUE(state.hasPendingCell());

    bool executed = state.pressSpace();
    EXPECT_TRUE(executed);
    EXPECT_FALSE(state.hasPendingCell());
}

// Test that SPACE without pending cell does nothing
TEST_F(PendingCellTest, SpaceWithoutPendingDoesNothing) {
    EXPECT_FALSE(state.hasPendingCell());

    bool executed = state.pressSpace();
    EXPECT_FALSE(executed);
}

// Test that RETURN executes pending cell
TEST_F(PendingCellTest, ReturnExecutesPending) {
    state.addLetter('B');
    state.addLetter('M');
    EXPECT_TRUE(state.hasPendingCell());

    bool executed = state.pressReturn();
    EXPECT_TRUE(executed);
    EXPECT_FALSE(state.hasPendingCell());
}

// Test that RETURN without pending cell does nothing
TEST_F(PendingCellTest, ReturnWithoutPendingDoesNothing) {
    EXPECT_FALSE(state.hasPendingCell());

    bool executed = state.pressReturn();
    EXPECT_FALSE(executed);
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

// Test typing 3rd letter updates pending cell
TEST_F(PendingCellTest, ThirdLetterUpdatesPending) {
    state.addLetter('A');
    state.addLetter('B');
    EXPECT_EQ(state.getRow(), 0);
    EXPECT_EQ(state.getCol(), 1);

    state.addLetter('C');
    EXPECT_EQ(state.getRow(), 1);  // B = 1
    EXPECT_EQ(state.getCol(), 2);  // C = 2
    EXPECT_EQ(state.getSequence(), "BC");
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

// Test workflow: type letters, see pending, correct with new letters, then execute
TEST_F(PendingCellTest, CorrectSequenceBeforeExecuting) {
    // Type wrong cell first
    state.addLetter('A');
    state.addLetter('A');
    EXPECT_EQ(state.getRow(), 0);
    EXPECT_EQ(state.getCol(), 0);

    // Correct by typing new letters (keeps last 2)
    state.addLetter('B');
    EXPECT_EQ(state.getRow(), 0);  // A = 0
    EXPECT_EQ(state.getCol(), 1);  // B = 1

    state.addLetter('M');
    EXPECT_EQ(state.getRow(), 1);   // B = 1
    EXPECT_EQ(state.getCol(), 12);  // M = 12

    // Now execute
    bool executed = state.pressSpace();
    EXPECT_TRUE(executed);
    EXPECT_FALSE(state.hasPendingCell());
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
