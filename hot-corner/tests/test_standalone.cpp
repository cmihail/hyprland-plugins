#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>

class StandaloneTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

struct MockVector2D {
    double x, y;

    MockVector2D(double x = 0, double y = 0) : x(x), y(y) {}
};

struct MockMonitor {
    MockVector2D m_position;

    MockMonitor(double x = 0, double y = 0) : m_position(x, y) {}
};

class HotCornerLogic {
public:
    static constexpr int DEFAULT_CORNER_SIZE = 10;
    static constexpr int DEFAULT_DELAY_MS = 1000;

    static bool isInTopLeftCorner(const MockVector2D& mousePos, const MockMonitor& monitor, int cornerSize) {
        return (mousePos.x <= monitor.m_position.x + cornerSize) &&
               (mousePos.y <= monitor.m_position.y + cornerSize);
    }

    static bool isInTopRightCorner(const MockVector2D& mousePos, const MockMonitor& monitor, int cornerSize, int screenWidth) {
        return (mousePos.x >= monitor.m_position.x + screenWidth - cornerSize) &&
               (mousePos.y <= monitor.m_position.y + cornerSize);
    }

    static bool isInBottomLeftCorner(const MockVector2D& mousePos, const MockMonitor& monitor, int cornerSize, int screenHeight) {
        return (mousePos.x <= monitor.m_position.x + cornerSize) &&
               (mousePos.y >= monitor.m_position.y + screenHeight - cornerSize);
    }

    static bool isInBottomRightCorner(const MockVector2D& mousePos, const MockMonitor& monitor, int cornerSize, int screenWidth, int screenHeight) {
        return (mousePos.x >= monitor.m_position.x + screenWidth - cornerSize) &&
               (mousePos.y >= monitor.m_position.y + screenHeight - cornerSize);
    }

    static int getCornerType(const MockVector2D& mousePos, const MockMonitor& monitor, int cornerSize, int screenWidth = 1920, int screenHeight = 1080) {
        if (isInTopLeftCorner(mousePos, monitor, cornerSize)) return 0;
        if (isInTopRightCorner(mousePos, monitor, cornerSize, screenWidth)) return 1;
        if (isInBottomLeftCorner(mousePos, monitor, cornerSize, screenHeight)) return 2;
        if (isInBottomRightCorner(mousePos, monitor, cornerSize, screenWidth, screenHeight)) return 3;
        return -1; // Not in any corner
    }
};

class MockHotCornerState {
public:
    bool isInHotCorner = false;
    std::chrono::steady_clock::time_point enterTime;
    bool timerActive = false;

    void reset() {
        isInHotCorner = false;
        timerActive = false;
    }

    void enterCorner() {
        isInHotCorner = true;
        enterTime = std::chrono::steady_clock::now();
    }

    void leaveCorner() {
        isInHotCorner = false;
        timerActive = false;
    }

    bool shouldExecuteCommand(int delayMs) const {
        if (!isInHotCorner) return false;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - enterTime);
        return elapsed.count() >= delayMs;
    }
};

TEST_F(StandaloneTest, HotCornerConstants) {
    EXPECT_EQ(HotCornerLogic::DEFAULT_CORNER_SIZE, 10);
    EXPECT_EQ(HotCornerLogic::DEFAULT_DELAY_MS, 1000);
}

TEST_F(StandaloneTest, TopLeftCornerDetection) {
    MockMonitor monitor(0, 0);
    int cornerSize = 10;

    // Inside corner
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(5, 5), monitor, cornerSize));
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(0, 0), monitor, cornerSize));
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(10, 10), monitor, cornerSize));

    // Outside corner
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(15, 5), monitor, cornerSize));
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(5, 15), monitor, cornerSize));
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(15, 15), monitor, cornerSize));
}

TEST_F(StandaloneTest, TopRightCornerDetection) {
    MockMonitor monitor(0, 0);
    int cornerSize = 10;
    int screenWidth = 1920;

    // Inside corner
    EXPECT_TRUE(HotCornerLogic::isInTopRightCorner(MockVector2D(1915, 5), monitor, cornerSize, screenWidth));
    EXPECT_TRUE(HotCornerLogic::isInTopRightCorner(MockVector2D(1920, 0), monitor, cornerSize, screenWidth));
    EXPECT_TRUE(HotCornerLogic::isInTopRightCorner(MockVector2D(1910, 10), monitor, cornerSize, screenWidth));

    // Outside corner
    EXPECT_FALSE(HotCornerLogic::isInTopRightCorner(MockVector2D(1900, 5), monitor, cornerSize, screenWidth));
    EXPECT_FALSE(HotCornerLogic::isInTopRightCorner(MockVector2D(1915, 15), monitor, cornerSize, screenWidth));
}

TEST_F(StandaloneTest, BottomLeftCornerDetection) {
    MockMonitor monitor(0, 0);
    int cornerSize = 10;
    int screenHeight = 1080;

    // Inside corner
    EXPECT_TRUE(HotCornerLogic::isInBottomLeftCorner(MockVector2D(5, 1075), monitor, cornerSize, screenHeight));
    EXPECT_TRUE(HotCornerLogic::isInBottomLeftCorner(MockVector2D(0, 1080), monitor, cornerSize, screenHeight));
    EXPECT_TRUE(HotCornerLogic::isInBottomLeftCorner(MockVector2D(10, 1070), monitor, cornerSize, screenHeight));

    // Outside corner
    EXPECT_FALSE(HotCornerLogic::isInBottomLeftCorner(MockVector2D(15, 1075), monitor, cornerSize, screenHeight));
    EXPECT_FALSE(HotCornerLogic::isInBottomLeftCorner(MockVector2D(5, 1065), monitor, cornerSize, screenHeight));
}

TEST_F(StandaloneTest, BottomRightCornerDetection) {
    MockMonitor monitor(0, 0);
    int cornerSize = 10;
    int screenWidth = 1920;
    int screenHeight = 1080;

    // Inside corner
    EXPECT_TRUE(HotCornerLogic::isInBottomRightCorner(MockVector2D(1915, 1075), monitor, cornerSize, screenWidth, screenHeight));
    EXPECT_TRUE(HotCornerLogic::isInBottomRightCorner(MockVector2D(1920, 1080), monitor, cornerSize, screenWidth, screenHeight));
    EXPECT_TRUE(HotCornerLogic::isInBottomRightCorner(MockVector2D(1910, 1070), monitor, cornerSize, screenWidth, screenHeight));

    // Outside corner
    EXPECT_FALSE(HotCornerLogic::isInBottomRightCorner(MockVector2D(1900, 1075), monitor, cornerSize, screenWidth, screenHeight));
    EXPECT_FALSE(HotCornerLogic::isInBottomRightCorner(MockVector2D(1915, 1065), monitor, cornerSize, screenWidth, screenHeight));
}

TEST_F(StandaloneTest, CornerTypeIdentification) {
    MockMonitor monitor(0, 0);
    int cornerSize = 10;

    EXPECT_EQ(HotCornerLogic::getCornerType(MockVector2D(5, 5), monitor, cornerSize), 0); // Top-left
    EXPECT_EQ(HotCornerLogic::getCornerType(MockVector2D(1915, 5), monitor, cornerSize), 1); // Top-right
    EXPECT_EQ(HotCornerLogic::getCornerType(MockVector2D(5, 1075), monitor, cornerSize), 2); // Bottom-left
    EXPECT_EQ(HotCornerLogic::getCornerType(MockVector2D(1915, 1075), monitor, cornerSize), 3); // Bottom-right
    EXPECT_EQ(HotCornerLogic::getCornerType(MockVector2D(960, 540), monitor, cornerSize), -1); // Center, no corner
}

TEST_F(StandaloneTest, CornerDetectionWithMonitorOffset) {
    MockMonitor monitor(100, 50);
    int cornerSize = 10;

    // Top-left corner with monitor offset
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(105, 55), monitor, cornerSize));
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(100, 50), monitor, cornerSize));
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(110, 60), monitor, cornerSize));

    // Outside corner with monitor offset - beyond the corner boundary
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(115, 55), monitor, cornerSize));
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(105, 65), monitor, cornerSize));
}

TEST_F(StandaloneTest, HotCornerStateMachine) {
    MockHotCornerState state;

    // Initial state
    EXPECT_FALSE(state.isInHotCorner);
    EXPECT_FALSE(state.timerActive);

    // Enter corner
    state.enterCorner();
    EXPECT_TRUE(state.isInHotCorner);

    // Leave corner
    state.leaveCorner();
    EXPECT_FALSE(state.isInHotCorner);
    EXPECT_FALSE(state.timerActive);

    // Reset state
    state.reset();
    EXPECT_FALSE(state.isInHotCorner);
    EXPECT_FALSE(state.timerActive);
}

TEST_F(StandaloneTest, CommandExecutionTiming) {
    MockHotCornerState state;
    int delayMs = 100;

    // Should not execute before entering corner
    EXPECT_FALSE(state.shouldExecuteCommand(delayMs));

    // Enter corner
    state.enterCorner();

    // Should not execute immediately
    EXPECT_FALSE(state.shouldExecuteCommand(delayMs));

    // Wait for delay
    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs + 10));

    // Should execute after delay
    EXPECT_TRUE(state.shouldExecuteCommand(delayMs));

    // Leave corner - should not execute
    state.leaveCorner();
    EXPECT_FALSE(state.shouldExecuteCommand(delayMs));
}

TEST_F(StandaloneTest, VariableCornerSizes) {
    MockMonitor monitor(0, 0);

    // Small corner
    int smallCorner = 5;
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(3, 3), monitor, smallCorner));
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(7, 3), monitor, smallCorner));

    // Large corner
    int largeCorner = 50;
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(30, 30), monitor, largeCorner));
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(45, 45), monitor, largeCorner));
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(55, 30), monitor, largeCorner));
}

TEST_F(StandaloneTest, BoundaryConditions) {
    MockMonitor monitor(0, 0);
    int cornerSize = 10;

    // Exact boundary cases
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(10, 10), monitor, cornerSize));
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(11, 10), monitor, cornerSize));
    EXPECT_FALSE(HotCornerLogic::isInTopLeftCorner(MockVector2D(10, 11), monitor, cornerSize));

    // Zero position
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(0, 0), monitor, cornerSize));

    // Negative coordinates (current logic allows them)
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(-1, 5), monitor, cornerSize));
    EXPECT_TRUE(HotCornerLogic::isInTopLeftCorner(MockVector2D(5, -1), monitor, cornerSize));
}