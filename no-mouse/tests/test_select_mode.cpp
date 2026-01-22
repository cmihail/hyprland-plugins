#include <gtest/gtest.h>

// Select mode state class to test button press/release behavior
class SelectModeState {
private:
    bool overlayActive;
    bool selectMode;      // vs move mode
    bool buttonPressed;
    bool hasPending;
    int row;
    int col;
    int mouseRow;
    int mouseCol;
    std::string letterSequence;

public:
    SelectModeState() : overlayActive(false), selectMode(false), buttonPressed(false),
                        hasPending(false), row(-1), col(-1),
                        mouseRow(-1), mouseCol(-1), letterSequence("") {}

    // Activate overlay in move mode
    void activateMoveMode() {
        overlayActive = true;
        selectMode = false;
        buttonPressed = false;
    }

    // Activate overlay in select mode (press button immediately)
    void activateSelectMode() {
        overlayActive = true;
        selectMode = true;
        buttonPressed = true;  // Button pressed on activation
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

                // Move mouse immediately
                mouseRow = row;
                mouseCol = col;
            }
        }
    }

    // Press SPACE to exit overlay (and release button if in select mode)
    bool pressSpace() {
        if (overlayActive) {
            overlayActive = false;
            if (selectMode && buttonPressed) {
                buttonPressed = false;  // Release button on exit
            }
            clear();
            return true;
        }
        return false;
    }

    // Press ESC to exit overlay (and release button if in select mode)
    void pressEsc() {
        if (overlayActive) {
            overlayActive = false;
            if (selectMode && buttonPressed) {
                buttonPressed = false;  // Release button on exit
            }
            clear();
        }
    }

    void clear() {
        hasPending = false;
        row = -1;
        col = -1;
        letterSequence.clear();
        mouseRow = -1;
        mouseCol = -1;
        selectMode = false;
    }

    bool isOverlayActive() const { return overlayActive; }
    bool isSelectMode() const { return selectMode; }
    bool isButtonPressed() const { return buttonPressed; }
    bool hasPendingCell() const { return hasPending; }
    int getMouseRow() const { return mouseRow; }
    int getMouseCol() const { return mouseCol; }
    std::string getSequence() const { return letterSequence; }
};

class SelectModeTest : public ::testing::Test {
protected:
    SelectModeState state;

    void SetUp() override {
        // Start fresh for each test
    }
};

// Test move mode doesn't press button
TEST_F(SelectModeTest, MoveModeNoButtonPress) {
    state.activateMoveMode();
    EXPECT_TRUE(state.isOverlayActive());
    EXPECT_FALSE(state.isSelectMode());
    EXPECT_FALSE(state.isButtonPressed());
}

// Test select mode presses button on activation
TEST_F(SelectModeTest, SelectModePressesButton) {
    state.activateSelectMode();
    EXPECT_TRUE(state.isOverlayActive());
    EXPECT_TRUE(state.isSelectMode());
    EXPECT_TRUE(state.isButtonPressed());
}

// Test select mode releases button on SPACE exit
TEST_F(SelectModeTest, SelectModeReleasesButtonOnSpace) {
    state.activateSelectMode();
    EXPECT_TRUE(state.isButtonPressed());

    state.pressSpace();
    EXPECT_FALSE(state.isOverlayActive());
    EXPECT_FALSE(state.isButtonPressed());
}

// Test select mode releases button on ESC exit
TEST_F(SelectModeTest, SelectModeReleasesButtonOnEsc) {
    state.activateSelectMode();
    EXPECT_TRUE(state.isButtonPressed());

    state.pressEsc();
    EXPECT_FALSE(state.isOverlayActive());
    EXPECT_FALSE(state.isButtonPressed());
}

// Test move mode doesn't affect button state on exit
TEST_F(SelectModeTest, MoveModeNoButtonStateOnExit) {
    state.activateMoveMode();
    EXPECT_FALSE(state.isButtonPressed());

    state.pressSpace();
    EXPECT_FALSE(state.isOverlayActive());
    EXPECT_FALSE(state.isButtonPressed());
}

// Test select mode with mouse movement - button stays pressed
TEST_F(SelectModeTest, SelectModeButtonStaysPressedDuringMovement) {
    state.activateSelectMode();
    EXPECT_TRUE(state.isButtonPressed());

    // Type 2 letters to move mouse
    state.addLetter('A');
    state.addLetter('B');

    // Button should still be pressed while overlay is active
    EXPECT_TRUE(state.isOverlayActive());
    EXPECT_TRUE(state.isButtonPressed());
    EXPECT_EQ(state.getMouseRow(), 0);
    EXPECT_EQ(state.getMouseCol(), 1);
}

// Test select mode with multiple movements - button stays pressed
TEST_F(SelectModeTest, SelectModeButtonStaysPressedThroughMultipleMovements) {
    state.activateSelectMode();
    EXPECT_TRUE(state.isButtonPressed());

    // First movement
    state.addLetter('A');
    state.addLetter('A');
    EXPECT_TRUE(state.isButtonPressed());
    EXPECT_EQ(state.getMouseRow(), 0);
    EXPECT_EQ(state.getMouseCol(), 0);

    // Second movement
    state.addLetter('B');
    state.addLetter('M');
    EXPECT_TRUE(state.isButtonPressed());
    EXPECT_EQ(state.getMouseRow(), 1);
    EXPECT_EQ(state.getMouseCol(), 12);

    // Third movement
    state.addLetter('Z');
    state.addLetter('Z');
    EXPECT_TRUE(state.isButtonPressed());
    EXPECT_EQ(state.getMouseRow(), 25);
    EXPECT_EQ(state.getMouseCol(), 25);

    // Now exit
    state.pressSpace();
    EXPECT_FALSE(state.isButtonPressed());
}

// Test workflow: select mode -> move -> move -> move -> exit
TEST_F(SelectModeTest, SelectModeCompleteWorkflow) {
    // Activate select mode
    state.activateSelectMode();
    EXPECT_TRUE(state.isSelectMode());
    EXPECT_TRUE(state.isButtonPressed());

    // Move to cell AA (simulating drag selection start)
    state.addLetter('A');
    state.addLetter('A');
    EXPECT_TRUE(state.isButtonPressed());  // Still holding button

    // Move to cell ZZ (simulating drag selection end)
    state.addLetter('Z');
    state.addLetter('Z');
    EXPECT_TRUE(state.isButtonPressed());  // Still holding button

    // Exit overlay - release button
    state.pressSpace();
    EXPECT_FALSE(state.isOverlayActive());
    EXPECT_FALSE(state.isButtonPressed());  // Button released
}

// Test move mode complete workflow (no button interaction)
TEST_F(SelectModeTest, MoveModeCompleteWorkflow) {
    // Activate move mode
    state.activateMoveMode();
    EXPECT_FALSE(state.isSelectMode());
    EXPECT_FALSE(state.isButtonPressed());

    // Move to cell AA
    state.addLetter('A');
    state.addLetter('A');
    EXPECT_FALSE(state.isButtonPressed());  // No button

    // Move to cell ZZ
    state.addLetter('Z');
    state.addLetter('Z');
    EXPECT_FALSE(state.isButtonPressed());  // No button

    // Exit overlay
    state.pressSpace();
    EXPECT_FALSE(state.isOverlayActive());
    EXPECT_FALSE(state.isButtonPressed());
}

// Test that select mode is cleared on exit
TEST_F(SelectModeTest, SelectModeStateClearedOnExit) {
    state.activateSelectMode();
    EXPECT_TRUE(state.isSelectMode());

    state.pressSpace();
    EXPECT_FALSE(state.isSelectMode());
}

// Test ESC in select mode
TEST_F(SelectModeTest, SelectModeEscReleasesButton) {
    state.activateSelectMode();
    EXPECT_TRUE(state.isButtonPressed());

    // Type some letters
    state.addLetter('M');
    state.addLetter('M');
    EXPECT_TRUE(state.hasPendingCell());
    EXPECT_TRUE(state.isButtonPressed());

    // Press ESC to cancel
    state.pressEsc();
    EXPECT_FALSE(state.isOverlayActive());
    EXPECT_FALSE(state.isButtonPressed());
    EXPECT_FALSE(state.hasPendingCell());
}
