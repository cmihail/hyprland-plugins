#include <gtest/gtest.h>
#include <string>

// Letter sequence class to test backspace logic
class LetterSequence {
private:
    std::string sequence;

public:
    void addLetter(char letter) {
        // Ensure uppercase
        if (letter >= 'a' && letter <= 'z') {
            letter = letter - 'a' + 'A';
        }

        if (letter >= 'A' && letter <= 'Z') {
            sequence += letter;

            // Limit to 2 characters
            if (sequence.length() > 2) {
                sequence = sequence.substr(sequence.length() - 2);
            }
        }
    }

    void backspace() {
        if (!sequence.empty()) {
            sequence.pop_back();
        }
    }

    void clear() {
        sequence.clear();
    }

    std::string get() const {
        return sequence;
    }

    size_t length() const {
        return sequence.length();
    }

    int getRow() const {
        if (sequence.length() >= 1) {
            return sequence[0] - 'A';
        }
        return -1;
    }

    int getCol() const {
        if (sequence.length() >= 2) {
            return sequence[1] - 'A';
        }
        return -1;
    }
};

// Mock state for testing visual feedback and pending cell state
class OverlayState {
public:
    bool hasPendingCell = false;
    int pendingRow = -1;
    int pendingCol = -1;
    bool hasSubColumn = false;
    int subColumn = -1;

    void updateFromSequence(const LetterSequence& seq) {
        if (seq.length() == 2) {
            hasPendingCell = true;
            pendingRow = seq.getRow();
            pendingCol = seq.getCol();
        } else if (seq.length() == 1) {
            hasPendingCell = false;
            pendingRow = -1;
            pendingCol = -1;
        } else {
            hasPendingCell = false;
            pendingRow = -1;
            pendingCol = -1;
        }

        // Clear sub-column on any sequence change
        hasSubColumn = false;
        subColumn = -1;
    }

    void reset() {
        hasPendingCell = false;
        pendingRow = -1;
        pendingCol = -1;
        hasSubColumn = false;
        subColumn = -1;
    }
};

class BackspaceTest : public ::testing::Test {
protected:
    LetterSequence seq;
    OverlayState state;

    void SetUp() override {
        seq.clear();
        state.reset();
    }
};

// Test backspace on empty sequence (should not crash)
TEST_F(BackspaceTest, BackspaceOnEmpty) {
    EXPECT_EQ(seq.length(), 0);
    seq.backspace();
    EXPECT_EQ(seq.length(), 0);
    EXPECT_EQ(seq.get(), "");
}

// Test backspace with single letter
TEST_F(BackspaceTest, BackspaceOnSingleLetter) {
    seq.addLetter('A');
    EXPECT_EQ(seq.length(), 1);
    EXPECT_EQ(seq.get(), "A");

    seq.backspace();
    EXPECT_EQ(seq.length(), 0);
    EXPECT_EQ(seq.get(), "");
}

// Test backspace with two letters
TEST_F(BackspaceTest, BackspaceOnTwoLetters) {
    seq.addLetter('A');
    seq.addLetter('B');
    EXPECT_EQ(seq.length(), 2);
    EXPECT_EQ(seq.get(), "AB");

    seq.backspace();
    EXPECT_EQ(seq.length(), 1);
    EXPECT_EQ(seq.get(), "A");
}

// Test multiple backspaces
TEST_F(BackspaceTest, MultipleBackspaces) {
    seq.addLetter('A');
    seq.addLetter('B');
    EXPECT_EQ(seq.get(), "AB");

    seq.backspace();
    EXPECT_EQ(seq.get(), "A");

    seq.backspace();
    EXPECT_EQ(seq.get(), "");

    seq.backspace(); // Should not crash
    EXPECT_EQ(seq.get(), "");
}

// Test backspace after adding more than 2 letters
TEST_F(BackspaceTest, BackspaceAfterThreeLetters) {
    seq.addLetter('A');
    seq.addLetter('B');
    seq.addLetter('C');
    EXPECT_EQ(seq.get(), "BC"); // Should have only last 2

    seq.backspace();
    EXPECT_EQ(seq.get(), "B");
}

// Test state transitions with backspace
TEST_F(BackspaceTest, StateTransition_TwoLettersToOne) {
    seq.addLetter('C');
    seq.addLetter('E');
    state.updateFromSequence(seq);

    EXPECT_TRUE(state.hasPendingCell);
    EXPECT_EQ(state.pendingRow, 2);  // C = 2
    EXPECT_EQ(state.pendingCol, 4);  // E = 4

    seq.backspace();
    state.updateFromSequence(seq);

    EXPECT_FALSE(state.hasPendingCell);
    EXPECT_EQ(state.pendingRow, -1);
    EXPECT_EQ(state.pendingCol, -1);
}

// Test state transitions from one letter to zero
TEST_F(BackspaceTest, StateTransition_OneLetterToZero) {
    seq.addLetter('M');
    state.updateFromSequence(seq);

    EXPECT_FALSE(state.hasPendingCell);
    EXPECT_EQ(seq.getRow(), 12);  // M = 12

    seq.backspace();
    state.updateFromSequence(seq);

    EXPECT_FALSE(state.hasPendingCell);
    EXPECT_EQ(seq.getRow(), -1);
}

// Test clearing sub-column state on backspace
TEST_F(BackspaceTest, ClearSubColumnOnBackspace) {
    seq.addLetter('A');
    seq.addLetter('B');
    state.updateFromSequence(seq);

    // Simulate sub-column selection
    state.hasSubColumn = true;
    state.subColumn = 5;

    seq.backspace();
    state.updateFromSequence(seq);

    EXPECT_FALSE(state.hasSubColumn);
    EXPECT_EQ(state.subColumn, -1);
}

// Test row calculation after backspace
TEST_F(BackspaceTest, RowCalculationAfterBackspace) {
    seq.addLetter('Z');
    seq.addLetter('Y');
    EXPECT_EQ(seq.getRow(), 25);  // Z = 25
    EXPECT_EQ(seq.getCol(), 24);  // Y = 24

    seq.backspace();
    EXPECT_EQ(seq.getRow(), 25);  // Z = 25
    EXPECT_EQ(seq.getCol(), -1);  // No second letter
}

// Test alternating add/backspace
TEST_F(BackspaceTest, AlternatingAddBackspace) {
    seq.addLetter('A');
    EXPECT_EQ(seq.get(), "A");

    seq.backspace();
    EXPECT_EQ(seq.get(), "");

    seq.addLetter('B');
    EXPECT_EQ(seq.get(), "B");

    seq.addLetter('C');
    EXPECT_EQ(seq.get(), "BC");

    seq.backspace();
    EXPECT_EQ(seq.get(), "B");

    seq.addLetter('D');
    EXPECT_EQ(seq.get(), "BD");
}

// Test backspace preserves remaining letters correctly
TEST_F(BackspaceTest, PreservesFirstLetter) {
    seq.addLetter('M');
    seq.addLetter('N');
    EXPECT_EQ(seq.get(), "MN");

    seq.backspace();
    EXPECT_EQ(seq.get(), "M");
    EXPECT_EQ(seq.getRow(), 12);  // M should still be correctly stored
}

// Test complete workflow: type, backspace, retype
TEST_F(BackspaceTest, CompleteWorkflow) {
    // Type first sequence
    seq.addLetter('A');
    seq.addLetter('B');
    state.updateFromSequence(seq);
    EXPECT_TRUE(state.hasPendingCell);
    EXPECT_EQ(state.pendingRow, 0);
    EXPECT_EQ(state.pendingCol, 1);

    // Backspace to fix mistake
    seq.backspace();
    state.updateFromSequence(seq);
    EXPECT_FALSE(state.hasPendingCell);

    // Type correct second letter
    seq.addLetter('C');
    state.updateFromSequence(seq);
    EXPECT_TRUE(state.hasPendingCell);
    EXPECT_EQ(state.pendingRow, 0);
    EXPECT_EQ(state.pendingCol, 2);
}

// Test edge case: rapid backspacing
TEST_F(BackspaceTest, RapidBackspacing) {
    seq.addLetter('X');
    seq.addLetter('Y');

    for (int i = 0; i < 10; i++) {
        seq.backspace();
    }

    EXPECT_EQ(seq.get(), "");
    EXPECT_EQ(seq.length(), 0);
}

// Test visual state for 1 letter (row selection)
TEST_F(BackspaceTest, VisualStateForOneLetter) {
    seq.addLetter('F');
    state.updateFromSequence(seq);

    EXPECT_EQ(seq.length(), 1);
    EXPECT_EQ(seq.getRow(), 5);  // F = 5
    EXPECT_FALSE(state.hasPendingCell);  // Not a full cell yet
}

// Test visual state for 2 letters (cell with sub-grid)
TEST_F(BackspaceTest, VisualStateForTwoLetters) {
    seq.addLetter('F');
    seq.addLetter('G');
    state.updateFromSequence(seq);

    EXPECT_EQ(seq.length(), 2);
    EXPECT_TRUE(state.hasPendingCell);
    EXPECT_EQ(state.pendingRow, 5);  // F = 5
    EXPECT_EQ(state.pendingCol, 6);  // G = 6
}

// Test backspace maintains correct state for visual filtering
TEST_F(BackspaceTest, BackspaceRestoresRowView) {
    // Start with cell view (2 letters)
    seq.addLetter('H');
    seq.addLetter('I');
    state.updateFromSequence(seq);
    EXPECT_TRUE(state.hasPendingCell);

    // Backspace should restore row view (1 letter)
    seq.backspace();
    state.updateFromSequence(seq);
    EXPECT_FALSE(state.hasPendingCell);
    EXPECT_EQ(seq.length(), 1);
    EXPECT_EQ(seq.getRow(), 7);  // H = 7
}

// Test backspace from row view to full grid
TEST_F(BackspaceTest, BackspaceRestoresFullGrid) {
    // Start with row view (1 letter)
    seq.addLetter('J');
    state.updateFromSequence(seq);
    EXPECT_EQ(seq.length(), 1);
    EXPECT_FALSE(state.hasPendingCell);

    // Backspace should restore full grid (0 letters)
    seq.backspace();
    state.updateFromSequence(seq);
    EXPECT_EQ(seq.length(), 0);
    EXPECT_FALSE(state.hasPendingCell);
    EXPECT_EQ(seq.getRow(), -1);
}
