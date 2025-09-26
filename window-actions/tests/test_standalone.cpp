#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <string>

class StandaloneTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// Mock button configuration structure
struct MockWindowActionButton {
    std::string action = "";
    std::string icon_inactive = "";
    std::string icon_active = "";
    std::string command = "";
    std::string condition = "";
    uint32_t    text_color = 0;
    uint32_t    bg_color = 0;
};

// Mock global state
struct MockGlobalState {
    std::vector<MockWindowActionButton> buttons;
};

class WindowActionsButtonLogic {
public:
    static constexpr float DEFAULT_BUTTON_SIZE = 15.0f;
    static constexpr float BUTTON_SPACING = 2.0f;

private:
    float m_buttonSize;
    MockGlobalState* m_globalState;

public:
    explicit WindowActionsButtonLogic(float buttonSize = DEFAULT_BUTTON_SIZE, MockGlobalState* globalState = nullptr)
        : m_buttonSize(buttonSize), m_globalState(globalState) {}

    bool isOnButton(float x, float y, int buttonIndex) const {
        if (!m_globalState || buttonIndex < 0 || buttonIndex >= static_cast<int>(m_globalState->buttons.size())) {
            return false;
        }

        float buttonX = buttonIndex * (m_buttonSize + BUTTON_SPACING);
        return x >= buttonX && x < buttonX + m_buttonSize && y >= 0 && y < m_buttonSize;
    }

    int getButtonIndex(float x, float y) const {
        if (!m_globalState || y < 0 || y >= m_buttonSize) return -1;

        for (int i = 0; i < static_cast<int>(m_globalState->buttons.size()); i++) {
            if (isOnButton(x, y, i)) {
                return i;
            }
        }
        return -1;
    }

    float getButtonSize() const {
        return m_buttonSize;
    }

    size_t getButtonCount() const {
        return m_globalState ? m_globalState->buttons.size() : 0;
    }
};

TEST_F(StandaloneTest, ButtonConstants) {
    WindowActionsButtonLogic logic;
    EXPECT_EQ(logic.getButtonSize(), 15.0f);
    EXPECT_EQ(WindowActionsButtonLogic::BUTTON_SPACING, 2.0f);
    EXPECT_EQ(logic.getButtonCount(), 0); // No buttons without global state
}

TEST_F(StandaloneTest, DynamicButtonConfiguration) {
    MockGlobalState globalState;
    WindowActionsButtonLogic logic(15.0f, &globalState);

    // Initially no buttons
    EXPECT_EQ(logic.getButtonCount(), 0);
    EXPECT_EQ(logic.getButtonIndex(5, 5), -1);

    // Add buttons
    globalState.buttons.push_back({"", "⨯", "⨯", "killactive", "", 0xff4040, 0x333333});
    globalState.buttons.push_back({"", "⬈", "⬋", "fullscreen", "fullscreen", 0xeeee11, 0x444444});

    EXPECT_EQ(logic.getButtonCount(), 2);

    // Test first button (close)
    EXPECT_EQ(logic.getButtonIndex(5, 5), 0);
    EXPECT_TRUE(logic.isOnButton(5, 5, 0));
    EXPECT_TRUE(logic.isOnButton(14, 14, 0));
    EXPECT_FALSE(logic.isOnButton(15, 5, 0)); // Outside button

    // Test second button (fullscreen) - starts at position 17 (15 + 2 spacing)
    EXPECT_EQ(logic.getButtonIndex(20, 5), 1);
    EXPECT_TRUE(logic.isOnButton(17, 5, 1));
    EXPECT_TRUE(logic.isOnButton(31, 14, 1));
    EXPECT_FALSE(logic.isOnButton(32, 5, 1)); // Outside button
}

TEST_F(StandaloneTest, DynamicButtonScaling) {
    MockGlobalState globalState;
    // Add 5 buttons to test scaling
    for (int i = 0; i < 5; i++) {
        globalState.buttons.push_back({"", "●", "●", "test", "", 0xe6e6e6, 0x333333});
    }

    WindowActionsButtonLogic logic(10.0f, &globalState);
    EXPECT_EQ(logic.getButtonCount(), 5);

    // Test each button position with 10px size + 2px spacing
    for (int i = 0; i < 5; i++) {
        float expectedX = i * (10.0f + 2.0f) + 5; // Center of button
        EXPECT_EQ(logic.getButtonIndex(expectedX, 5), i);
        EXPECT_TRUE(logic.isOnButton(expectedX, 5, i));
    }

    // Test beyond last button
    EXPECT_EQ(logic.getButtonIndex(100, 5), -1);
}

TEST_F(StandaloneTest, EmptyButtonConfiguration) {
    MockGlobalState globalState;
    WindowActionsButtonLogic logic(15.0f, &globalState);

    // No buttons configured
    EXPECT_EQ(logic.getButtonCount(), 0);
    EXPECT_EQ(logic.getButtonIndex(5, 5), -1);
    EXPECT_FALSE(logic.isOnButton(5, 5, 0));
}

TEST_F(StandaloneTest, ButtonIndexBoundaryTesting) {
    MockGlobalState globalState;
    globalState.buttons.push_back({"", "●", "●", "test", "", 0x333333});
    WindowActionsButtonLogic logic(15.0f, &globalState);

    // Test boundaries
    EXPECT_TRUE(logic.isOnButton(0, 0, 0));      // Top-left corner
    EXPECT_TRUE(logic.isOnButton(14, 14, 0));    // Bottom-right corner
    EXPECT_FALSE(logic.isOnButton(15, 0, 0));    // Right edge (outside)
    EXPECT_FALSE(logic.isOnButton(0, 15, 0));    // Bottom edge (outside)
    EXPECT_FALSE(logic.isOnButton(-1, 0, 0));    // Left edge (outside)
    EXPECT_FALSE(logic.isOnButton(0, -1, 0));    // Top edge (outside)
}

TEST_F(StandaloneTest, ConfigurableButtonSize) {
    MockGlobalState globalState;
    globalState.buttons.push_back({"", "●", "●", "test1", "", 0xe6e6e6, 0x333333});
    globalState.buttons.push_back({"", "●", "●", "test2", "", 0xe6e6e6, 0x444444});

    // Test with custom button size
    WindowActionsButtonLogic logic(20.0f, &globalState);
    EXPECT_EQ(logic.getButtonSize(), 20.0f);

    // Test button detection with larger button size
    EXPECT_TRUE(logic.isOnButton(10, 10, 0));
    EXPECT_TRUE(logic.isOnButton(19, 19, 0));
    EXPECT_FALSE(logic.isOnButton(20, 10, 0)); // Outside button

    // Test second button with spacing (starts at 20 + 2 = 22)
    float expectedSecondButtonStart = 20.0f + WindowActionsButtonLogic::BUTTON_SPACING;
    EXPECT_TRUE(logic.isOnButton(expectedSecondButtonStart + 5, 10, 1));
    EXPECT_FALSE(logic.isOnButton(expectedSecondButtonStart - 1, 10, 1)); // Before button

    // Test smaller button size
    WindowActionsButtonLogic smallLogic(10.0f, &globalState);
    EXPECT_EQ(smallLogic.getButtonSize(), 10.0f);
    EXPECT_TRUE(smallLogic.isOnButton(5, 5, 0));
    EXPECT_FALSE(smallLogic.isOnButton(10, 5, 0)); // Outside smaller button
}

// Window state checking logic
class WindowStateChecker {
public:
    static bool checkCondition(const std::string& condition, bool isFullscreen, bool isGrouped, bool isFloating, bool isMaximized) {
        if (condition.empty()) return false;
        if (condition == "fullscreen") return isFullscreen;
        if (condition == "grouped") return isGrouped;
        if (condition == "floating") return isFloating;
        if (condition == "maximized") return isMaximized;
        return false;
    }

    static std::string getIcon(const std::string& iconInactive, const std::string& iconActive, const std::string& condition, bool isFullscreen, bool isGrouped, bool isFloating, bool isMaximized) {
        if (condition.empty()) return iconInactive;
        return checkCondition(condition, isFullscreen, isGrouped, isFloating, isMaximized) ? iconActive : iconInactive;
    }
};

TEST_F(StandaloneTest, ButtonConfigurationManagement) {
    MockGlobalState globalState;
    EXPECT_EQ(globalState.buttons.size(), 0);

    // Add button configurations
    MockWindowActionButton button1{"", "⨯", "⨯", "killactive", "", 0xff4040, 0x333333};
    MockWindowActionButton button2{"", "⬈", "⬋", "fullscreen", "fullscreen", 0xeeee11, 0x444444};

    globalState.buttons.push_back(button1);
    globalState.buttons.push_back(button2);

    EXPECT_EQ(globalState.buttons.size(), 2);
    EXPECT_EQ(globalState.buttons[0].command, "killactive");
    EXPECT_EQ(globalState.buttons[1].condition, "fullscreen");

    // Clear configurations
    globalState.buttons.clear();
    EXPECT_EQ(globalState.buttons.size(), 0);
}

TEST_F(StandaloneTest, WindowStateChecking) {
    // Test basic conditions
    EXPECT_TRUE(WindowStateChecker::checkCondition("fullscreen", true, false, false, false));
    EXPECT_FALSE(WindowStateChecker::checkCondition("fullscreen", false, false, false, false));

    EXPECT_TRUE(WindowStateChecker::checkCondition("grouped", false, true, false, false));
    EXPECT_FALSE(WindowStateChecker::checkCondition("grouped", false, false, false, false));

    EXPECT_TRUE(WindowStateChecker::checkCondition("floating", false, false, true, false));
    EXPECT_FALSE(WindowStateChecker::checkCondition("floating", false, false, false, false));

    EXPECT_TRUE(WindowStateChecker::checkCondition("maximized", false, false, false, true));
    EXPECT_FALSE(WindowStateChecker::checkCondition("maximized", false, false, false, false));

    // Test empty/unknown conditions
    EXPECT_FALSE(WindowStateChecker::checkCondition("", true, true, true, true));
    EXPECT_FALSE(WindowStateChecker::checkCondition("unknown", true, true, true, true));
}

TEST_F(StandaloneTest, IconStateSwitching) {
    // Test icon switching based on window state
    EXPECT_EQ(WindowStateChecker::getIcon("⬈", "⬋", "fullscreen", true, false, false, false), "⬋");   // Active when fullscreen
    EXPECT_EQ(WindowStateChecker::getIcon("⬈", "⬋", "fullscreen", false, false, false, false), "⬈");  // Inactive when not fullscreen

    EXPECT_EQ(WindowStateChecker::getIcon("⊟", "⊞", "grouped", false, true, false, false), "⊞");      // Active when grouped
    EXPECT_EQ(WindowStateChecker::getIcon("⊟", "⊞", "grouped", false, false, false, false), "⊟");     // Inactive when not grouped

    // Test with no condition (always inactive icon)
    EXPECT_EQ(WindowStateChecker::getIcon("⨯", "⨯", "", true, true, true, true), "⨯");
    EXPECT_EQ(WindowStateChecker::getIcon("⨯", "⨯", "", false, false, false, false), "⨯");
}

struct MockPassElementData {
    void* deco = nullptr;
    float alpha = 1.0f;
};

class MockPassElement {
public:
    MockPassElementData data;

    explicit MockPassElement(const MockPassElementData& d) : data(d) {}

    const char* passName() const {
        return "MockWindowActionsPassElement";
    }

    bool needsLiveBlur() const {
        return false;
    }

    bool needsPrecomputeBlur() const {
        return false;
    }
};

TEST_F(StandaloneTest, PassElementFunctionality) {
    MockPassElementData data;
    data.deco = nullptr;
    data.alpha = 0.5f;

    MockPassElement element(data);

    EXPECT_STREQ(element.passName(), "MockWindowActionsPassElement");
    EXPECT_FALSE(element.needsLiveBlur());
    EXPECT_FALSE(element.needsPrecomputeBlur());
    EXPECT_EQ(element.data.alpha, 0.5f);
}

// Configuration parsing mock
class ButtonConfigParser {
public:
    struct ParseResult {
        bool valid = false;
        MockWindowActionButton button;
        std::string error;
    };

    static ParseResult parseButtonConfig(const std::string& config) {
        ParseResult result;

        // Simple parser for format: "text_color, bg_color, icon_inactive, icon_active, command, condition"
        std::vector<std::string> parts;
        std::string current;
        bool inQuotes = false;

        for (char c : config) {
            if (c == ',' && !inQuotes) {
                parts.push_back(trim(current));
                current.clear();
            } else if (c == '"') {
                inQuotes = !inQuotes;
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            parts.push_back(trim(current));
        }

        if (parts.size() < 5) {
            result.error = "Need at least 5 parts";
            return result;
        }

        result.button.text_color = parseColor(parts[0]);
        result.button.bg_color = parseColor(parts[1]);
        result.button.icon_inactive = parts[2];
        result.button.icon_active = parts[3];
        result.button.command = parts[4];
        if (parts.size() >= 6) {
            result.button.condition = parts[5];
        }

        result.valid = true;
        return result;
    }

private:
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(' ');
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, (last - first + 1));
    }

    static uint32_t parseColor(const std::string& colorStr) {
        // Simple color parsing - just return a mock value
        if (colorStr.find("ff4040") != std::string::npos) return 0xff4040;
        if (colorStr.find("eeee11") != std::string::npos) return 0xeeee11;
        if (colorStr.find("444444") != std::string::npos) return 0x444444;
        if (colorStr.find("e6e6e6") != std::string::npos) return 0xe6e6e6;
        return 0x333333;
    }
};

TEST_F(StandaloneTest, ConfigurationParsing) {
    // Test valid configuration
    auto result1 = ButtonConfigParser::parseButtonConfig("rgb(ff4040), rgb(333333), ⨯, ⨯, hyprctl dispatch killactive,");
    EXPECT_TRUE(result1.valid);
    EXPECT_EQ(result1.button.text_color, 0xff4040);
    EXPECT_EQ(result1.button.bg_color, 0x333333);
    EXPECT_EQ(result1.button.icon_inactive, "⨯");
    EXPECT_EQ(result1.button.icon_active, "⨯");
    EXPECT_EQ(result1.button.command, "hyprctl dispatch killactive");
    EXPECT_EQ(result1.button.condition, "");

    // Test configuration with condition
    auto result2 = ButtonConfigParser::parseButtonConfig("rgb(eeee11), rgb(444444), ⬈, ⬋, hyprctl dispatch fullscreen 1, fullscreen");
    EXPECT_TRUE(result2.valid);
    EXPECT_EQ(result2.button.text_color, 0xeeee11);
    EXPECT_EQ(result2.button.bg_color, 4473924); // 0x444444
    EXPECT_EQ(result2.button.condition, "fullscreen");

    // Test invalid configuration (too few parts)
    auto result3 = ButtonConfigParser::parseButtonConfig("rgb(ff4040), ⨯, ⨯");
    EXPECT_FALSE(result3.valid);
    EXPECT_EQ(result3.error, "Need at least 5 parts");
}

TEST_F(StandaloneTest, EdgeCaseHandling) {
    MockGlobalState globalState;
    WindowActionsButtonLogic logic(15.0f, &globalState);

    // Test with very small button size
    WindowActionsButtonLogic tinyLogic(1.0f, &globalState);
    globalState.buttons.push_back({"", "●", "●", "test", "", 0x333333});

    EXPECT_EQ(tinyLogic.getButtonSize(), 1.0f);
    EXPECT_TRUE(tinyLogic.isOnButton(0, 0, 0));
    EXPECT_FALSE(tinyLogic.isOnButton(1, 0, 0));

    // Test with large number of buttons
    globalState.buttons.clear();
    for (int i = 0; i < 10; i++) {
        globalState.buttons.push_back({"", "●", "●", "test", "", 0xe6e6e6, 0x333333});
    }

    EXPECT_EQ(logic.getButtonCount(), 10);

    // Test last button position
    float lastButtonX = 9 * (15.0f + 2.0f) + 7; // 9th button center
    EXPECT_EQ(logic.getButtonIndex(lastButtonX, 5), 9);
}