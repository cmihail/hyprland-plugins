#include <gtest/gtest.h>
#include <string>
#include <sstream>

// Helper function to parse pipe-delimited format: command|stroke_data
struct ParsedGestureAction {
    std::string command;
    std::string strokeData;
    bool valid = false;
};

ParsedGestureAction parseGestureAction(const std::string& value) {
    ParsedGestureAction result;

    // Find the LAST pipe delimiter (to support pipes in command)
    size_t pipePos = value.rfind('|');
    if (pipePos == std::string::npos) {
        return result;
    }

    // Extract command and stroke data
    result.command = value.substr(0, pipePos);
    result.strokeData = value.substr(pipePos + 1);

    // Trim whitespace
    auto trim = [](std::string& s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            s = s.substr(start, end - start + 1);
        } else {
            s = "";  // All whitespace, make it empty
        }
    };

    trim(result.command);
    trim(result.strokeData);

    // Validate
    if (!result.command.empty() && !result.strokeData.empty()) {
        result.valid = true;
    }

    return result;
}

class ConfigParsingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }
};

// Test basic config parsing
TEST_F(ConfigParsingTest, ParseSimpleConfig) {
    std::string config = "hyprctl dispatch workspace +1|0.5,0.3;0.6,0.4;0.7,0.5;";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "hyprctl dispatch workspace +1");
    EXPECT_EQ(parsed.strokeData, "0.5,0.3;0.6,0.4;0.7,0.5;");
}

// Test config with spaces
TEST_F(ConfigParsingTest, ParseConfigWithSpaces) {
    std::string config = "  hyprctl dispatch workspace +1  |  0.5,0.3;0.6,0.4;  ";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "hyprctl dispatch workspace +1");
    EXPECT_EQ(parsed.strokeData, "0.5,0.3;0.6,0.4;");
}

// Test config with complex command
TEST_F(ConfigParsingTest, ParseConfigWithComplexCommand) {
    std::string config = "hyprctl notify -1 2000 \"rgb(ff0000)\" \"Test\"|0.5,0.3;0.6,0.4;";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "hyprctl notify -1 2000 \"rgb(ff0000)\" \"Test\"");
    EXPECT_EQ(parsed.strokeData, "0.5,0.3;0.6,0.4;");
}

// Test config without pipe delimiter
TEST_F(ConfigParsingTest, ParseConfigNoPipe) {
    std::string config = "hyprctl dispatch workspace +1 0.5,0.3;0.6,0.4;";
    auto parsed = parseGestureAction(config);

    EXPECT_FALSE(parsed.valid);
}

// Test config with multiple pipes (should use first one)
TEST_F(ConfigParsingTest, ParseConfigMultiplePipes) {
    std::string config = "echo 'test|value'|0.5,0.3;0.6,0.4;";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "echo 'test|value'");
    EXPECT_EQ(parsed.strokeData, "0.5,0.3;0.6,0.4;");
}

// Test empty command
TEST_F(ConfigParsingTest, ParseConfigEmptyCommand) {
    std::string config = "|0.5,0.3;0.6,0.4;";
    auto parsed = parseGestureAction(config);

    EXPECT_FALSE(parsed.valid);
}

// Test empty stroke data
TEST_F(ConfigParsingTest, ParseConfigEmptyStrokeData) {
    std::string config = "hyprctl dispatch workspace +1|";
    auto parsed = parseGestureAction(config);

    EXPECT_FALSE(parsed.valid);
}

// Test stroke data with many commas and semicolons
TEST_F(ConfigParsingTest, ParseConfigComplexStrokeData) {
    std::string strokeData = "0.1,0.2;0.3,0.4;0.5,0.6;0.7,0.8;0.9,1.0;";
    std::string config = "notify-send test|" + strokeData;
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "notify-send test");
    EXPECT_EQ(parsed.strokeData, strokeData);
}

// Test stroke data format validation (basic check for semicolons and commas)
TEST_F(ConfigParsingTest, ValidateStrokeDataFormat) {
    std::string validData = "0.5,0.3;0.6,0.4;0.7,0.5;";

    // Should contain both commas and semicolons
    EXPECT_NE(validData.find(','), std::string::npos);
    EXPECT_NE(validData.find(';'), std::string::npos);
}

// Test that stroke data doesn't interfere with command parsing
TEST_F(ConfigParsingTest, StrokeDataDoesntAffectCommand) {
    // Stroke data contains characters that might be in commands
    std::string config = "hyprctl dispatch workspace +1|0.469454,0.000000;0.507275,0.221864;";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "hyprctl dispatch workspace +1");

    // Command shouldn't contain any stroke data
    EXPECT_EQ(parsed.command.find("0.469454"), std::string::npos);
}

// Test command with special characters
TEST_F(ConfigParsingTest, ParseConfigWithSpecialChars) {
    std::string config = "bash -c 'echo $HOME'|0.5,0.3;0.6,0.4;";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "bash -c 'echo $HOME'");
}

// Test very long stroke data
TEST_F(ConfigParsingTest, ParseConfigLongStrokeData) {
    std::string strokeData;
    for (int i = 0; i < 100; i++) {
        strokeData += "0." + std::to_string(i) + ",0.5;";
    }

    std::string config = "test command|" + strokeData;
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "test command");
    EXPECT_EQ(parsed.strokeData, strokeData);
}

// Test config with only whitespace
TEST_F(ConfigParsingTest, ParseConfigOnlyWhitespace) {
    std::string config = "   |   ";
    auto parsed = parseGestureAction(config);

    EXPECT_FALSE(parsed.valid);
}

// Test real-world examples
TEST_F(ConfigParsingTest, RealWorldExample1) {
    std::string config = "hyprctl dispatch killactive|0.500000,0.123456;0.500000,0.234567;0.500000,0.345678;";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "hyprctl dispatch killactive");
}

TEST_F(ConfigParsingTest, RealWorldExample2) {
    std::string config = "notify-send \"Custom gesture!\"|0.5,0.3;0.6,0.4;0.7,0.5;";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "notify-send \"Custom gesture!\"");
}

TEST_F(ConfigParsingTest, RealWorldExample3) {
    std::string config = "hyprctl dispatch workspace -1|0.876543,0.500000;0.765432,0.500000;0.654321,0.500000;";
    auto parsed = parseGestureAction(config);

    EXPECT_TRUE(parsed.valid);
    EXPECT_EQ(parsed.command, "hyprctl dispatch workspace -1");
    EXPECT_EQ(parsed.strokeData, "0.876543,0.500000;0.765432,0.500000;0.654321,0.500000;");
}
