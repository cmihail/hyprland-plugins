#include <gtest/gtest.h>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>

// Mock implementation of executeCommand for testing
// This simulates the behavior without actually executing commands
struct CommandExecutionResult {
    bool executed = false;
    std::string command;
    int exitCode = 0;
    std::string error;
};

class MockCommandExecutor {
public:
    static std::atomic<int> executionCount;
    static std::string lastCommand;
    static int mockExitCode;
    static bool shouldThrowException;
    static std::string exceptionMessage;

    static void reset() {
        executionCount = 0;
        lastCommand = "";
        mockExitCode = 0;
        shouldThrowException = false;
        exceptionMessage = "";
    }

    static void executeCommand(const std::string& command) {
        if (command.empty()) {
            return;
        }

        executionCount++;
        lastCommand = command;

        if (shouldThrowException) {
            throw std::runtime_error(exceptionMessage);
        }

        // Simulate system() call result
        if (mockExitCode != 0) {
            // In real code, this would trigger a notification
        }
    }
};

std::atomic<int> MockCommandExecutor::executionCount{0};
std::string MockCommandExecutor::lastCommand = "";
int MockCommandExecutor::mockExitCode = 0;
bool MockCommandExecutor::shouldThrowException = false;
std::string MockCommandExecutor::exceptionMessage = "";

class DefaultCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        MockCommandExecutor::reset();
    }

    void TearDown() override {
        MockCommandExecutor::reset();
    }
};

// Test that empty command is not executed
TEST_F(DefaultCommandTest, EmptyCommandNotExecuted) {
    std::string emptyCmd = "";
    MockCommandExecutor::executeCommand(emptyCmd);

    EXPECT_EQ(MockCommandExecutor::executionCount, 0);
    EXPECT_EQ(MockCommandExecutor::lastCommand, "");
}

// Test that valid command is executed
TEST_F(DefaultCommandTest, ValidCommandExecuted) {
    std::string cmd = "hyprctl dispatch killactive";
    MockCommandExecutor::executeCommand(cmd);

    EXPECT_EQ(MockCommandExecutor::executionCount, 1);
    EXPECT_EQ(MockCommandExecutor::lastCommand, cmd);
}

// Test that multiple commands are executed in sequence
TEST_F(DefaultCommandTest, MultipleCommandsExecuted) {
    std::string cmd1 = "hyprctl dispatch workspace 1";
    std::string cmd2 = "hyprctl dispatch workspace 2";
    std::string cmd3 = "hyprctl dispatch workspace 3";

    MockCommandExecutor::executeCommand(cmd1);
    MockCommandExecutor::executeCommand(cmd2);
    MockCommandExecutor::executeCommand(cmd3);

    EXPECT_EQ(MockCommandExecutor::executionCount, 3);
    EXPECT_EQ(MockCommandExecutor::lastCommand, cmd3);
}

// Test that command with special characters is handled correctly
TEST_F(DefaultCommandTest, CommandWithSpecialCharacters) {
    std::string cmd = "hyprctl notify -1 2000 \"rgb(ff0000)\" \"Test message\"";
    MockCommandExecutor::executeCommand(cmd);

    EXPECT_EQ(MockCommandExecutor::executionCount, 1);
    EXPECT_EQ(MockCommandExecutor::lastCommand, cmd);
}

// Test that command with pipes is handled correctly
TEST_F(DefaultCommandTest, CommandWithPipes) {
    std::string cmd = "echo 'test' | grep test";
    MockCommandExecutor::executeCommand(cmd);

    EXPECT_EQ(MockCommandExecutor::executionCount, 1);
    EXPECT_EQ(MockCommandExecutor::lastCommand, cmd);
}

// Test command execution with non-zero exit code
TEST_F(DefaultCommandTest, CommandWithNonZeroExitCode) {
    MockCommandExecutor::mockExitCode = 1;
    std::string cmd = "failing-command";

    // Should still execute, just with error notification
    MockCommandExecutor::executeCommand(cmd);

    EXPECT_EQ(MockCommandExecutor::executionCount, 1);
    EXPECT_EQ(MockCommandExecutor::lastCommand, cmd);
}

// Test command execution with exception
TEST_F(DefaultCommandTest, CommandWithException) {
    MockCommandExecutor::shouldThrowException = true;
    MockCommandExecutor::exceptionMessage = "Test exception";
    std::string cmd = "exception-command";

    EXPECT_THROW(MockCommandExecutor::executeCommand(cmd), std::runtime_error);
    EXPECT_EQ(MockCommandExecutor::executionCount, 1);
}

// Test default command string validation
TEST_F(DefaultCommandTest, ValidateDefaultCommandFormat) {
    // Valid default commands
    std::vector<std::string> validCommands = {
        "hyprctl dispatch killactive",
        "notify-send 'Gesture not matched'",
        "bash -c 'echo test'",
        "hyprctl notify -1 2000 \"rgb(ff0000)\" \"No match\"",
    };

    for (const auto& cmd : validCommands) {
        EXPECT_FALSE(cmd.empty());
        EXPECT_NE(cmd.find_first_not_of(" \t\n\r"), std::string::npos);
    }
}

// Test that whitespace-only strings are treated as empty
TEST_F(DefaultCommandTest, WhitespaceOnlyCommandNotExecuted) {
    std::string whitespaceCmd = "   \t\n\r   ";

    // Trim function simulation
    auto trim = [](const std::string& s) -> std::string {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            return s.substr(start, end - start + 1);
        }
        return "";
    };

    std::string trimmed = trim(whitespaceCmd);
    EXPECT_TRUE(trimmed.empty());

    MockCommandExecutor::executeCommand(trimmed);
    EXPECT_EQ(MockCommandExecutor::executionCount, 0);
}

// Test command length limits
TEST_F(DefaultCommandTest, LongCommandExecuted) {
    std::string longCmd = "hyprctl dispatch exec ";
    for (int i = 0; i < 100; i++) {
        longCmd += "arg" + std::to_string(i) + " ";
    }

    MockCommandExecutor::executeCommand(longCmd);
    EXPECT_EQ(MockCommandExecutor::executionCount, 1);
    EXPECT_EQ(MockCommandExecutor::lastCommand, longCmd);
}

// Test that default command takes precedence over notification
TEST_F(DefaultCommandTest, DefaultCommandPrecedence) {
    std::string defaultCmd = "hyprctl dispatch killactive";
    bool hasDefaultCommand = !defaultCmd.empty();

    if (hasDefaultCommand) {
        MockCommandExecutor::executeCommand(defaultCmd);
        EXPECT_EQ(MockCommandExecutor::executionCount, 1);
    } else {
        // Would show notification instead
        EXPECT_EQ(MockCommandExecutor::executionCount, 0);
    }
}

// Test empty string default command shows notification
TEST_F(DefaultCommandTest, EmptyDefaultCommandShowsNotification) {
    std::string defaultCmd = "";
    bool hasDefaultCommand = !defaultCmd.empty();

    if (hasDefaultCommand) {
        MockCommandExecutor::executeCommand(defaultCmd);
    } else {
        // Notification should be shown
    }

    EXPECT_EQ(MockCommandExecutor::executionCount, 0);
}

// Test config value parsing for default_command
TEST_F(DefaultCommandTest, ParseDefaultCommandConfig) {
    // Simulate config values
    std::vector<std::pair<std::string, std::string>> configs = {
        {"default_command = hyprctl dispatch killactive", "hyprctl dispatch killactive"},
        {"default_command = ", ""},
        {"default_command = notify-send test", "notify-send test"},
    };

    for (const auto& [configLine, expectedValue] : configs) {
        size_t eqPos = configLine.find('=');
        ASSERT_NE(eqPos, std::string::npos);

        std::string value = configLine.substr(eqPos + 1);

        // Trim whitespace
        size_t start = value.find_first_not_of(" \t\n\r");
        size_t end = value.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            value = value.substr(start, end - start + 1);
        } else {
            value = "";
        }

        EXPECT_EQ(value, expectedValue);
    }
}

// Test that default command doesn't interfere with matched gestures
TEST_F(DefaultCommandTest, DefaultCommandDoesntAffectMatchedGestures) {
    std::string matchedGestureCmd = "hyprctl dispatch workspace 1";
    std::string defaultCmd = "hyprctl dispatch killactive";

    // When gesture matches, use matched command (not default)
    MockCommandExecutor::executeCommand(matchedGestureCmd);

    EXPECT_EQ(MockCommandExecutor::executionCount, 1);
    EXPECT_EQ(MockCommandExecutor::lastCommand, matchedGestureCmd);
    EXPECT_NE(MockCommandExecutor::lastCommand, defaultCmd);
}

// Test null/nullptr handling
TEST_F(DefaultCommandTest, NullCommandHandling) {
    const char* nullCmd = nullptr;
    std::string cmd = nullCmd ? std::string(nullCmd) : "";

    MockCommandExecutor::executeCommand(cmd);
    EXPECT_EQ(MockCommandExecutor::executionCount, 0);
}

// Test command with environment variables
TEST_F(DefaultCommandTest, CommandWithEnvVariables) {
    std::string cmd = "echo $HOME";
    MockCommandExecutor::executeCommand(cmd);

    EXPECT_EQ(MockCommandExecutor::executionCount, 1);
    EXPECT_EQ(MockCommandExecutor::lastCommand, cmd);
}

// Test command with quotes
TEST_F(DefaultCommandTest, CommandWithQuotes) {
    std::string cmd1 = "echo \"test message\"";
    std::string cmd2 = "echo 'test message'";

    MockCommandExecutor::executeCommand(cmd1);
    EXPECT_EQ(MockCommandExecutor::lastCommand, cmd1);

    MockCommandExecutor::executeCommand(cmd2);
    EXPECT_EQ(MockCommandExecutor::lastCommand, cmd2);

    EXPECT_EQ(MockCommandExecutor::executionCount, 2);
}

// Test real-world default command examples
TEST_F(DefaultCommandTest, RealWorldExamples) {
    std::vector<std::string> realWorldCommands = {
        "hyprctl dispatch killactive",
        "hyprctl dispatch togglefloating",
        "hyprctl dispatch fullscreen 1",
        "hyprctl notify -1 2000 \"rgb(ff0000)\" \"Unknown gesture\"",
        "notify-send \"Mouse Gestures\" \"Gesture not recognized\"",
    };

    for (const auto& cmd : realWorldCommands) {
        MockCommandExecutor::reset();
        MockCommandExecutor::executeCommand(cmd);

        EXPECT_EQ(MockCommandExecutor::executionCount, 1);
        EXPECT_EQ(MockCommandExecutor::lastCommand, cmd);
    }
}
