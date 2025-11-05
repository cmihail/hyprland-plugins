#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>
#include <vector>

// Mock GestureAction struct
struct GestureAction {
    std::vector<std::string> pattern;
    std::string command;
};

// Mock function to simulate executing a command
void executeCommand(const std::string& command) {
    std::thread([command]() {
        system(command.c_str());
    }).detach();
}

TEST(ThreadingTest, CommandExecutionIsNonBlocking) {
    // Use a command that will take a noticeable amount of time to execute
    std::string long_running_command = "sleep 1";

    auto start_time = std::chrono::high_resolution_clock::now();

    executeCommand(long_running_command);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // The main thread should not be blocked, so the duration should be very short,
    // much less than the 1-second sleep in the command.
    EXPECT_LT(duration.count(), 100);
}
