#include <gtest/gtest.h>
#include <string>
#include <vector>

class StandaloneTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// Mock dispatcher argument parser
class DispatcherArgumentParser {
public:
    static std::string parseCommand(const std::string& arg) {
        return arg;
    }

    static bool isValidCommand(const std::string& command) {
        return command == "register" || command == "start" || command == "stop";
    }
};

TEST_F(StandaloneTest, DispatcherArgumentParsing) {
    EXPECT_EQ(DispatcherArgumentParser::parseCommand("register"), "register");
    EXPECT_EQ(DispatcherArgumentParser::parseCommand("start"), "start");
    EXPECT_EQ(DispatcherArgumentParser::parseCommand("stop"), "stop");
    EXPECT_EQ(DispatcherArgumentParser::parseCommand(""), "");
}

TEST_F(StandaloneTest, DispatcherCommandValidation) {
    EXPECT_TRUE(DispatcherArgumentParser::isValidCommand("register"));
    EXPECT_TRUE(DispatcherArgumentParser::isValidCommand("start"));
    EXPECT_TRUE(DispatcherArgumentParser::isValidCommand("stop"));
    EXPECT_FALSE(DispatcherArgumentParser::isValidCommand("invalid"));
    EXPECT_FALSE(DispatcherArgumentParser::isValidCommand(""));
}

// Mock notification message builder
class NotificationMessageBuilder {
public:
    static std::string buildMessage(const std::string& command) {
        if (command == "register") {
            return "[mouse-gestures] Register command received";
        } else if (command == "start") {
            return "[mouse-gestures] Start command received";
        } else if (command == "stop") {
            return "[mouse-gestures] Stop command received";
        }
        return "[mouse-gestures] Unknown command";
    }

    static std::string getPrefix() {
        return "[mouse-gestures]";
    }
};

TEST_F(StandaloneTest, NotificationMessageBuilding) {
    EXPECT_EQ(NotificationMessageBuilder::buildMessage("register"), "[mouse-gestures] Register command received");
    EXPECT_EQ(NotificationMessageBuilder::buildMessage("start"), "[mouse-gestures] Start command received");
    EXPECT_EQ(NotificationMessageBuilder::buildMessage("stop"), "[mouse-gestures] Stop command received");
    EXPECT_EQ(NotificationMessageBuilder::buildMessage("invalid"), "[mouse-gestures] Unknown command");
}

TEST_F(StandaloneTest, NotificationPrefix) {
    EXPECT_EQ(NotificationMessageBuilder::getPrefix(), "[mouse-gestures]");
}

// Mock gesture state
class GestureState {
private:
    bool m_isRecording = false;
    bool m_isEnabled = false;
    std::vector<std::pair<int, int>> m_points;

public:
    void startRecording() {
        m_isRecording = true;
        m_points.clear();
    }

    void stopRecording() {
        m_isRecording = false;
    }

    bool isRecording() const {
        return m_isRecording;
    }

    void setEnabled(bool enabled) {
        m_isEnabled = enabled;
    }

    bool isEnabled() const {
        return m_isEnabled;
    }

    void addPoint(int x, int y) {
        if (m_isRecording) {
            m_points.push_back({x, y});
        }
    }

    size_t getPointCount() const {
        return m_points.size();
    }

    void clear() {
        m_points.clear();
        m_isRecording = false;
    }

    const std::vector<std::pair<int, int>>& getPoints() const {
        return m_points;
    }
};

TEST_F(StandaloneTest, GestureStateInitialization) {
    GestureState state;
    EXPECT_FALSE(state.isRecording());
    EXPECT_FALSE(state.isEnabled());
    EXPECT_EQ(state.getPointCount(), 0);
}

TEST_F(StandaloneTest, GestureRecordingControl) {
    GestureState state;

    state.startRecording();
    EXPECT_TRUE(state.isRecording());

    state.stopRecording();
    EXPECT_FALSE(state.isRecording());
}

TEST_F(StandaloneTest, GestureEnabledState) {
    GestureState state;

    state.setEnabled(true);
    EXPECT_TRUE(state.isEnabled());

    state.setEnabled(false);
    EXPECT_FALSE(state.isEnabled());
}

TEST_F(StandaloneTest, GesturePointTracking) {
    GestureState state;

    // Points should not be added when not recording
    state.addPoint(10, 20);
    EXPECT_EQ(state.getPointCount(), 0);

    // Start recording and add points
    state.startRecording();
    state.addPoint(10, 20);
    state.addPoint(30, 40);
    state.addPoint(50, 60);
    EXPECT_EQ(state.getPointCount(), 3);

    // Verify points
    const auto& points = state.getPoints();
    EXPECT_EQ(points[0].first, 10);
    EXPECT_EQ(points[0].second, 20);
    EXPECT_EQ(points[1].first, 30);
    EXPECT_EQ(points[1].second, 40);
    EXPECT_EQ(points[2].first, 50);
    EXPECT_EQ(points[2].second, 60);
}

TEST_F(StandaloneTest, GestureStateClear) {
    GestureState state;

    state.startRecording();
    state.addPoint(10, 20);
    state.addPoint(30, 40);
    EXPECT_EQ(state.getPointCount(), 2);
    EXPECT_TRUE(state.isRecording());

    state.clear();
    EXPECT_EQ(state.getPointCount(), 0);
    EXPECT_FALSE(state.isRecording());
}

TEST_F(StandaloneTest, GesturePointsClearedOnStart) {
    GestureState state;

    state.startRecording();
    state.addPoint(10, 20);
    state.addPoint(30, 40);
    EXPECT_EQ(state.getPointCount(), 2);

    // Starting recording again should clear previous points
    state.startRecording();
    EXPECT_EQ(state.getPointCount(), 0);
}

// Mock gesture recognizer
class GestureRecognizer {
public:
    enum class Direction {
        NONE,
        UP,
        DOWN,
        LEFT,
        RIGHT
    };

    static Direction recognizeSimpleGesture(const std::vector<std::pair<int, int>>& points) {
        if (points.size() < 2) {
            return Direction::NONE;
        }

        int dx = points.back().first - points.front().first;
        int dy = points.back().second - points.front().second;

        int absDx = abs(dx);
        int absDy = abs(dy);

        if (absDx > absDy) {
            return (dx > 0) ? Direction::RIGHT : Direction::LEFT;
        } else {
            return (dy > 0) ? Direction::DOWN : Direction::UP;
        }
    }

    static std::string directionToString(Direction dir) {
        switch (dir) {
            case Direction::UP: return "UP";
            case Direction::DOWN: return "DOWN";
            case Direction::LEFT: return "LEFT";
            case Direction::RIGHT: return "RIGHT";
            default: return "NONE";
        }
    }
};

TEST_F(StandaloneTest, GestureRecognitionInsufficientPoints) {
    std::vector<std::pair<int, int>> emptyPoints;
    EXPECT_EQ(GestureRecognizer::recognizeSimpleGesture(emptyPoints), GestureRecognizer::Direction::NONE);

    std::vector<std::pair<int, int>> singlePoint = {{10, 10}};
    EXPECT_EQ(GestureRecognizer::recognizeSimpleGesture(singlePoint), GestureRecognizer::Direction::NONE);
}

TEST_F(StandaloneTest, GestureRecognitionHorizontalGestures) {
    // Right gesture
    std::vector<std::pair<int, int>> rightPoints = {{0, 0}, {100, 10}};
    EXPECT_EQ(GestureRecognizer::recognizeSimpleGesture(rightPoints), GestureRecognizer::Direction::RIGHT);

    // Left gesture
    std::vector<std::pair<int, int>> leftPoints = {{100, 0}, {0, 10}};
    EXPECT_EQ(GestureRecognizer::recognizeSimpleGesture(leftPoints), GestureRecognizer::Direction::LEFT);
}

TEST_F(StandaloneTest, GestureRecognitionVerticalGestures) {
    // Down gesture
    std::vector<std::pair<int, int>> downPoints = {{0, 0}, {10, 100}};
    EXPECT_EQ(GestureRecognizer::recognizeSimpleGesture(downPoints), GestureRecognizer::Direction::DOWN);

    // Up gesture
    std::vector<std::pair<int, int>> upPoints = {{0, 100}, {10, 0}};
    EXPECT_EQ(GestureRecognizer::recognizeSimpleGesture(upPoints), GestureRecognizer::Direction::UP);
}

TEST_F(StandaloneTest, GestureRecognitionMultiplePoints) {
    // Right gesture with multiple points
    std::vector<std::pair<int, int>> points = {{0, 0}, {25, 5}, {50, 8}, {75, 12}, {100, 15}};
    EXPECT_EQ(GestureRecognizer::recognizeSimpleGesture(points), GestureRecognizer::Direction::RIGHT);
}

TEST_F(StandaloneTest, DirectionToString) {
    EXPECT_EQ(GestureRecognizer::directionToString(GestureRecognizer::Direction::UP), "UP");
    EXPECT_EQ(GestureRecognizer::directionToString(GestureRecognizer::Direction::DOWN), "DOWN");
    EXPECT_EQ(GestureRecognizer::directionToString(GestureRecognizer::Direction::LEFT), "LEFT");
    EXPECT_EQ(GestureRecognizer::directionToString(GestureRecognizer::Direction::RIGHT), "RIGHT");
    EXPECT_EQ(GestureRecognizer::directionToString(GestureRecognizer::Direction::NONE), "NONE");
}

// Mock gesture command mapper
class GestureCommandMapper {
private:
    std::map<std::string, std::string> m_commandMap;

public:
    void registerCommand(const std::string& gesture, const std::string& command) {
        m_commandMap[gesture] = command;
    }

    std::string getCommand(const std::string& gesture) const {
        auto it = m_commandMap.find(gesture);
        if (it != m_commandMap.end()) {
            return it->second;
        }
        return "";
    }

    bool hasCommand(const std::string& gesture) const {
        return m_commandMap.find(gesture) != m_commandMap.end();
    }

    void clear() {
        m_commandMap.clear();
    }

    size_t getCommandCount() const {
        return m_commandMap.size();
    }
};

TEST_F(StandaloneTest, CommandMapperInitialization) {
    GestureCommandMapper mapper;
    EXPECT_EQ(mapper.getCommandCount(), 0);
    EXPECT_FALSE(mapper.hasCommand("UP"));
}

TEST_F(StandaloneTest, CommandMapperRegistration) {
    GestureCommandMapper mapper;

    mapper.registerCommand("UP", "hyprctl dispatch workspace e-1");
    mapper.registerCommand("DOWN", "hyprctl dispatch workspace e+1");

    EXPECT_EQ(mapper.getCommandCount(), 2);
    EXPECT_TRUE(mapper.hasCommand("UP"));
    EXPECT_TRUE(mapper.hasCommand("DOWN"));
    EXPECT_FALSE(mapper.hasCommand("LEFT"));
}

TEST_F(StandaloneTest, CommandMapperRetrieval) {
    GestureCommandMapper mapper;

    mapper.registerCommand("UP", "hyprctl dispatch workspace e-1");
    mapper.registerCommand("DOWN", "hyprctl dispatch workspace e+1");
    mapper.registerCommand("LEFT", "hyprctl dispatch workspace -1");
    mapper.registerCommand("RIGHT", "hyprctl dispatch workspace +1");

    EXPECT_EQ(mapper.getCommand("UP"), "hyprctl dispatch workspace e-1");
    EXPECT_EQ(mapper.getCommand("DOWN"), "hyprctl dispatch workspace e+1");
    EXPECT_EQ(mapper.getCommand("LEFT"), "hyprctl dispatch workspace -1");
    EXPECT_EQ(mapper.getCommand("RIGHT"), "hyprctl dispatch workspace +1");
    EXPECT_EQ(mapper.getCommand("UNKNOWN"), "");
}

TEST_F(StandaloneTest, CommandMapperClear) {
    GestureCommandMapper mapper;

    mapper.registerCommand("UP", "command1");
    mapper.registerCommand("DOWN", "command2");
    EXPECT_EQ(mapper.getCommandCount(), 2);

    mapper.clear();
    EXPECT_EQ(mapper.getCommandCount(), 0);
    EXPECT_FALSE(mapper.hasCommand("UP"));
}

TEST_F(StandaloneTest, CommandMapperOverwrite) {
    GestureCommandMapper mapper;

    mapper.registerCommand("UP", "command1");
    EXPECT_EQ(mapper.getCommand("UP"), "command1");

    mapper.registerCommand("UP", "command2");
    EXPECT_EQ(mapper.getCommand("UP"), "command2");
    EXPECT_EQ(mapper.getCommandCount(), 1);
}

// Integration test for complete gesture flow
TEST_F(StandaloneTest, CompleteGestureFlow) {
    GestureState state;
    GestureCommandMapper mapper;

    mapper.registerCommand("RIGHT", "hyprctl dispatch workspace +1");
    mapper.registerCommand("LEFT", "hyprctl dispatch workspace -1");

    state.startRecording();
    EXPECT_TRUE(state.isRecording());

    state.addPoint(0, 50);
    state.addPoint(25, 52);
    state.addPoint(50, 48);
    state.addPoint(75, 51);
    state.addPoint(100, 49);

    state.stopRecording();
    EXPECT_FALSE(state.isRecording());
    EXPECT_EQ(state.getPointCount(), 5);

    auto direction = GestureRecognizer::recognizeSimpleGesture(state.getPoints());
    EXPECT_EQ(direction, GestureRecognizer::Direction::RIGHT);

    std::string directionStr = GestureRecognizer::directionToString(direction);
    EXPECT_EQ(directionStr, "RIGHT");

    std::string command = mapper.getCommand(directionStr);
    EXPECT_EQ(command, "hyprctl dispatch workspace +1");
}
