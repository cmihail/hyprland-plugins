#include <gtest/gtest.h>
#include <linux/input-event-codes.h>

// Function under test (copied from main.cpp for testing)
static char keycodeToLetter(uint32_t keycode) {
    // QWERTY top row: Q W E R T Y U I O P
    if (keycode == KEY_Q) return 'Q';
    if (keycode == KEY_W) return 'W';
    if (keycode == KEY_E) return 'E';
    if (keycode == KEY_R) return 'R';
    if (keycode == KEY_T) return 'T';
    if (keycode == KEY_Y) return 'Y';
    if (keycode == KEY_U) return 'U';
    if (keycode == KEY_I) return 'I';
    if (keycode == KEY_O) return 'O';
    if (keycode == KEY_P) return 'P';

    // QWERTY middle row: A S D F G H J K L
    if (keycode == KEY_A) return 'A';
    if (keycode == KEY_S) return 'S';
    if (keycode == KEY_D) return 'D';
    if (keycode == KEY_F) return 'F';
    if (keycode == KEY_G) return 'G';
    if (keycode == KEY_H) return 'H';
    if (keycode == KEY_J) return 'J';
    if (keycode == KEY_K) return 'K';
    if (keycode == KEY_L) return 'L';

    // QWERTY bottom row: Z X C V B N M
    if (keycode == KEY_Z) return 'Z';
    if (keycode == KEY_X) return 'X';
    if (keycode == KEY_C) return 'C';
    if (keycode == KEY_V) return 'V';
    if (keycode == KEY_B) return 'B';
    if (keycode == KEY_N) return 'N';
    if (keycode == KEY_M) return 'M';

    return '\0'; // Not a letter key
}

class KeycodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }
};

// Test QWERTY top row mappings
TEST_F(KeycodeTest, TopRowQ) {
    EXPECT_EQ(keycodeToLetter(KEY_Q), 'Q');
}

TEST_F(KeycodeTest, TopRowW) {
    EXPECT_EQ(keycodeToLetter(KEY_W), 'W');
}

TEST_F(KeycodeTest, TopRowE) {
    EXPECT_EQ(keycodeToLetter(KEY_E), 'E');
}

TEST_F(KeycodeTest, TopRowR) {
    EXPECT_EQ(keycodeToLetter(KEY_R), 'R');
}

TEST_F(KeycodeTest, TopRowT) {
    EXPECT_EQ(keycodeToLetter(KEY_T), 'T');
}

TEST_F(KeycodeTest, TopRowY) {
    EXPECT_EQ(keycodeToLetter(KEY_Y), 'Y');
}

TEST_F(KeycodeTest, TopRowU) {
    EXPECT_EQ(keycodeToLetter(KEY_U), 'U');
}

TEST_F(KeycodeTest, TopRowI) {
    EXPECT_EQ(keycodeToLetter(KEY_I), 'I');
}

TEST_F(KeycodeTest, TopRowO) {
    EXPECT_EQ(keycodeToLetter(KEY_O), 'O');
}

TEST_F(KeycodeTest, TopRowP) {
    EXPECT_EQ(keycodeToLetter(KEY_P), 'P');
}

// Test QWERTY middle row mappings
TEST_F(KeycodeTest, MiddleRowA) {
    EXPECT_EQ(keycodeToLetter(KEY_A), 'A');
}

TEST_F(KeycodeTest, MiddleRowS) {
    EXPECT_EQ(keycodeToLetter(KEY_S), 'S');
}

TEST_F(KeycodeTest, MiddleRowD) {
    EXPECT_EQ(keycodeToLetter(KEY_D), 'D');
}

TEST_F(KeycodeTest, MiddleRowF) {
    EXPECT_EQ(keycodeToLetter(KEY_F), 'F');
}

TEST_F(KeycodeTest, MiddleRowG) {
    EXPECT_EQ(keycodeToLetter(KEY_G), 'G');
}

TEST_F(KeycodeTest, MiddleRowH) {
    EXPECT_EQ(keycodeToLetter(KEY_H), 'H');
}

TEST_F(KeycodeTest, MiddleRowJ) {
    EXPECT_EQ(keycodeToLetter(KEY_J), 'J');
}

TEST_F(KeycodeTest, MiddleRowK) {
    EXPECT_EQ(keycodeToLetter(KEY_K), 'K');
}

TEST_F(KeycodeTest, MiddleRowL) {
    EXPECT_EQ(keycodeToLetter(KEY_L), 'L');
}

// Test QWERTY bottom row mappings
TEST_F(KeycodeTest, BottomRowZ) {
    EXPECT_EQ(keycodeToLetter(KEY_Z), 'Z');
}

TEST_F(KeycodeTest, BottomRowX) {
    EXPECT_EQ(keycodeToLetter(KEY_X), 'X');
}

TEST_F(KeycodeTest, BottomRowC) {
    EXPECT_EQ(keycodeToLetter(KEY_C), 'C');
}

TEST_F(KeycodeTest, BottomRowV) {
    EXPECT_EQ(keycodeToLetter(KEY_V), 'V');
}

TEST_F(KeycodeTest, BottomRowB) {
    EXPECT_EQ(keycodeToLetter(KEY_B), 'B');
}

TEST_F(KeycodeTest, BottomRowN) {
    EXPECT_EQ(keycodeToLetter(KEY_N), 'N');
}

TEST_F(KeycodeTest, BottomRowM) {
    EXPECT_EQ(keycodeToLetter(KEY_M), 'M');
}

// Test invalid keycodes
TEST_F(KeycodeTest, InvalidKeycode_Numeric) {
    EXPECT_EQ(keycodeToLetter(KEY_1), '\0');
    EXPECT_EQ(keycodeToLetter(KEY_2), '\0');
    EXPECT_EQ(keycodeToLetter(KEY_0), '\0');
}

TEST_F(KeycodeTest, InvalidKeycode_Modifiers) {
    EXPECT_EQ(keycodeToLetter(KEY_LEFTSHIFT), '\0');
    EXPECT_EQ(keycodeToLetter(KEY_RIGHTSHIFT), '\0');
    EXPECT_EQ(keycodeToLetter(KEY_LEFTCTRL), '\0');
    EXPECT_EQ(keycodeToLetter(KEY_LEFTALT), '\0');
}

TEST_F(KeycodeTest, InvalidKeycode_Special) {
    EXPECT_EQ(keycodeToLetter(KEY_ESC), '\0');
    EXPECT_EQ(keycodeToLetter(KEY_ENTER), '\0');
    EXPECT_EQ(keycodeToLetter(KEY_SPACE), '\0');
    EXPECT_EQ(keycodeToLetter(KEY_TAB), '\0');
}

TEST_F(KeycodeTest, InvalidKeycode_Zero) {
    EXPECT_EQ(keycodeToLetter(0), '\0');
}

TEST_F(KeycodeTest, InvalidKeycode_Large) {
    EXPECT_EQ(keycodeToLetter(9999), '\0');
}

// Test that all 26 letters are covered
TEST_F(KeycodeTest, AllLettersCovered) {
    int letterCount = 0;

    // Count how many letter keys map to something
    if (keycodeToLetter(KEY_A) != '\0') letterCount++;
    if (keycodeToLetter(KEY_B) != '\0') letterCount++;
    if (keycodeToLetter(KEY_C) != '\0') letterCount++;
    if (keycodeToLetter(KEY_D) != '\0') letterCount++;
    if (keycodeToLetter(KEY_E) != '\0') letterCount++;
    if (keycodeToLetter(KEY_F) != '\0') letterCount++;
    if (keycodeToLetter(KEY_G) != '\0') letterCount++;
    if (keycodeToLetter(KEY_H) != '\0') letterCount++;
    if (keycodeToLetter(KEY_I) != '\0') letterCount++;
    if (keycodeToLetter(KEY_J) != '\0') letterCount++;
    if (keycodeToLetter(KEY_K) != '\0') letterCount++;
    if (keycodeToLetter(KEY_L) != '\0') letterCount++;
    if (keycodeToLetter(KEY_M) != '\0') letterCount++;
    if (keycodeToLetter(KEY_N) != '\0') letterCount++;
    if (keycodeToLetter(KEY_O) != '\0') letterCount++;
    if (keycodeToLetter(KEY_P) != '\0') letterCount++;
    if (keycodeToLetter(KEY_Q) != '\0') letterCount++;
    if (keycodeToLetter(KEY_R) != '\0') letterCount++;
    if (keycodeToLetter(KEY_S) != '\0') letterCount++;
    if (keycodeToLetter(KEY_T) != '\0') letterCount++;
    if (keycodeToLetter(KEY_U) != '\0') letterCount++;
    if (keycodeToLetter(KEY_V) != '\0') letterCount++;
    if (keycodeToLetter(KEY_W) != '\0') letterCount++;
    if (keycodeToLetter(KEY_X) != '\0') letterCount++;
    if (keycodeToLetter(KEY_Y) != '\0') letterCount++;
    if (keycodeToLetter(KEY_Z) != '\0') letterCount++;

    EXPECT_EQ(letterCount, 26) << "All 26 letters should be mapped";
}
