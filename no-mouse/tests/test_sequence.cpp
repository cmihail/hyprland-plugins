#include <gtest/gtest.h>
#include <string>

// Letter sequence class to test the logic
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

    bool isReady() const {
        return sequence.length() == 2;
    }

    bool isValid() const {
        if (sequence.length() != 2) {
            return false;
        }

        char first = sequence[0];
        char second = sequence[1];

        return (first >= 'A' && first <= 'Z' && second >= 'A' && second <= 'Z');
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

    void clear() {
        sequence.clear();
    }

    std::string get() const {
        return sequence;
    }

    size_t length() const {
        return sequence.length();
    }
};

class LetterSequenceTest : public ::testing::Test {
protected:
    LetterSequence seq;

    void SetUp() override {
        seq.clear();
    }
};

// Test single letter addition
TEST_F(LetterSequenceTest, AddSingleLetter) {
    seq.addLetter('A');
    EXPECT_EQ(seq.get(), "A");
    EXPECT_EQ(seq.length(), 1);
    EXPECT_FALSE(seq.isReady());
}

// Test two letter addition
TEST_F(LetterSequenceTest, AddTwoLetters) {
    seq.addLetter('A');
    seq.addLetter('B');
    EXPECT_EQ(seq.get(), "AB");
    EXPECT_EQ(seq.length(), 2);
    EXPECT_TRUE(seq.isReady());
}

// Test three letters (should keep last 2)
TEST_F(LetterSequenceTest, AddThreeLetters_KeepsLastTwo) {
    seq.addLetter('A');
    seq.addLetter('B');
    seq.addLetter('C');
    EXPECT_EQ(seq.get(), "BC");
    EXPECT_EQ(seq.length(), 2);
    EXPECT_TRUE(seq.isReady());
}

// Test four letters (should keep last 2)
TEST_F(LetterSequenceTest, AddFourLetters_KeepsLastTwo) {
    seq.addLetter('A');
    seq.addLetter('B');
    seq.addLetter('C');
    seq.addLetter('D');
    EXPECT_EQ(seq.get(), "CD");
    EXPECT_EQ(seq.length(), 2);
}

// Test lowercase to uppercase conversion
TEST_F(LetterSequenceTest, LowercaseConversion_a) {
    seq.addLetter('a');
    EXPECT_EQ(seq.get(), "A");
}

TEST_F(LetterSequenceTest, LowercaseConversion_z) {
    seq.addLetter('z');
    EXPECT_EQ(seq.get(), "Z");
}

TEST_F(LetterSequenceTest, MixedCase) {
    seq.addLetter('a');
    seq.addLetter('B');
    EXPECT_EQ(seq.get(), "AB");
}

// Test sequence validation
TEST_F(LetterSequenceTest, ValidSequence_AA) {
    seq.addLetter('A');
    seq.addLetter('A');
    EXPECT_TRUE(seq.isValid());
}

TEST_F(LetterSequenceTest, ValidSequence_ZZ) {
    seq.addLetter('Z');
    seq.addLetter('Z');
    EXPECT_TRUE(seq.isValid());
}

TEST_F(LetterSequenceTest, ValidSequence_AZ) {
    seq.addLetter('A');
    seq.addLetter('Z');
    EXPECT_TRUE(seq.isValid());
}

TEST_F(LetterSequenceTest, ValidSequence_ZA) {
    seq.addLetter('Z');
    seq.addLetter('A');
    EXPECT_TRUE(seq.isValid());
}

TEST_F(LetterSequenceTest, InvalidSequence_OneLetter) {
    seq.addLetter('A');
    EXPECT_FALSE(seq.isValid());
}

TEST_F(LetterSequenceTest, InvalidSequence_Empty) {
    EXPECT_FALSE(seq.isValid());
}

// Test row/column calculation
TEST_F(LetterSequenceTest, RowColCalculation_AA) {
    seq.addLetter('A');
    seq.addLetter('A');
    EXPECT_EQ(seq.getRow(), 0);
    EXPECT_EQ(seq.getCol(), 0);
}

TEST_F(LetterSequenceTest, RowColCalculation_ZZ) {
    seq.addLetter('Z');
    seq.addLetter('Z');
    EXPECT_EQ(seq.getRow(), 25);
    EXPECT_EQ(seq.getCol(), 25);
}

TEST_F(LetterSequenceTest, RowColCalculation_BM) {
    seq.addLetter('B');
    seq.addLetter('M');
    EXPECT_EQ(seq.getRow(), 1);   // B = 1
    EXPECT_EQ(seq.getCol(), 12);  // M = 12
}

TEST_F(LetterSequenceTest, RowColCalculation_MN) {
    seq.addLetter('M');
    seq.addLetter('N');
    EXPECT_EQ(seq.getRow(), 12);  // M = 12
    EXPECT_EQ(seq.getCol(), 13);  // N = 13
}

// Test clearing
TEST_F(LetterSequenceTest, ClearSequence) {
    seq.addLetter('A');
    seq.addLetter('B');
    EXPECT_EQ(seq.length(), 2);

    seq.clear();
    EXPECT_EQ(seq.length(), 0);
    EXPECT_EQ(seq.get(), "");
    EXPECT_FALSE(seq.isReady());
}

// Test edge cases
TEST_F(LetterSequenceTest, AllUppercaseLetters) {
    for (char c = 'A'; c <= 'Z'; c++) {
        LetterSequence testSeq;
        testSeq.addLetter(c);
        EXPECT_EQ(testSeq.length(), 1);
        EXPECT_GE(testSeq.getRow(), 0);
        EXPECT_LE(testSeq.getRow(), 25);
    }
}

TEST_F(LetterSequenceTest, AllLowercaseLetters) {
    for (char c = 'a'; c <= 'z'; c++) {
        LetterSequence testSeq;
        testSeq.addLetter(c);
        EXPECT_EQ(testSeq.length(), 1);
        // Should be converted to uppercase
        char expected = c - 'a' + 'A';
        EXPECT_EQ(testSeq.get()[0], expected);
    }
}

// Test sequence after processing
TEST_F(LetterSequenceTest, MultipleSequences) {
    seq.addLetter('A');
    seq.addLetter('A');
    EXPECT_EQ(seq.get(), "AA");

    seq.clear();
    EXPECT_EQ(seq.get(), "");

    seq.addLetter('Z');
    seq.addLetter('Z');
    EXPECT_EQ(seq.get(), "ZZ");
}

// Test boundary letters
TEST_F(LetterSequenceTest, FirstLetter_A) {
    seq.addLetter('A');
    EXPECT_EQ(seq.getRow(), 0);
}

TEST_F(LetterSequenceTest, LastLetter_Z) {
    seq.addLetter('Z');
    EXPECT_EQ(seq.getRow(), 25);
}

// Test isReady state transitions
TEST_F(LetterSequenceTest, ReadyStateTransitions) {
    EXPECT_FALSE(seq.isReady());

    seq.addLetter('A');
    EXPECT_FALSE(seq.isReady());

    seq.addLetter('B');
    EXPECT_TRUE(seq.isReady());

    seq.addLetter('C');
    EXPECT_TRUE(seq.isReady());

    seq.clear();
    EXPECT_FALSE(seq.isReady());
}
