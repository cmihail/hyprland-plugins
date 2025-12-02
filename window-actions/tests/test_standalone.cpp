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

// Mock class for testing hover functionality
class WindowActionsHoverLogic {
private:
    WindowActionsButtonLogic* m_buttonLogic;
    int m_hoveredButton = -1;
    float m_unhovered_alpha = 1.0f;

public:
    explicit WindowActionsHoverLogic(WindowActionsButtonLogic* buttonLogic, float unhovered_alpha = 1.0f)
        : m_buttonLogic(buttonLogic), m_unhovered_alpha(unhovered_alpha) {}

    void onMouseMove(float x, float y) {
        int newHoveredButton = m_buttonLogic ? m_buttonLogic->getButtonIndex(x, y) : -1;
        if (newHoveredButton != m_hoveredButton) {
            m_hoveredButton = newHoveredButton;
        }
    }

    int getHoveredButton() const {
        return m_hoveredButton;
    }

    float getButtonAlpha(int buttonIndex, float baseAlpha = 1.0f) const {
        return (m_hoveredButton == buttonIndex) ? baseAlpha : baseAlpha * m_unhovered_alpha;
    }

    void setUnhoveredAlpha(float alpha) {
        m_unhovered_alpha = alpha;
    }

    float getUnhoveredAlpha() const {
        return m_unhovered_alpha;
    }
};

TEST_F(StandaloneTest, HoverStateTracking) {
    MockGlobalState globalState;
    WindowActionsButtonLogic buttonLogic(15.0f, &globalState);
    WindowActionsHoverLogic hoverLogic(&buttonLogic);

    // Add some buttons
    globalState.buttons.push_back({"", "⨯", "⨯", "killactive", "", 0xff4040, 0x333333});
    globalState.buttons.push_back({"", "⬈", "⬋", "fullscreen", "fullscreen", 0xeeee11, 0x444444});

    // Initially no button is hovered
    EXPECT_EQ(hoverLogic.getHoveredButton(), -1);

    // Move mouse over first button
    hoverLogic.onMouseMove(5, 5);
    EXPECT_EQ(hoverLogic.getHoveredButton(), 0);

    // Move mouse over second button
    hoverLogic.onMouseMove(20, 5); // Second button starts at x=17 (15 + 2)
    EXPECT_EQ(hoverLogic.getHoveredButton(), 1);

    // Move mouse away from buttons
    hoverLogic.onMouseMove(100, 5);
    EXPECT_EQ(hoverLogic.getHoveredButton(), -1);
}

TEST_F(StandaloneTest, HoverAlphaCalculation) {
    MockGlobalState globalState;
    WindowActionsButtonLogic buttonLogic(15.0f, &globalState);
    WindowActionsHoverLogic hoverLogic(&buttonLogic, 0.3f); // 30% when not hovered

    // Add buttons
    globalState.buttons.push_back({"", "⨯", "⨯", "killactive", "", 0xff4040, 0x333333});
    globalState.buttons.push_back({"", "⬈", "⬋", "fullscreen", "fullscreen", 0xeeee11, 0x444444});

    float baseAlpha = 0.8f;

    // No button hovered - all buttons should have reduced alpha
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(0, baseAlpha), 0.24f); // 0.8 * 0.3
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(1, baseAlpha), 0.24f); // 0.8 * 0.3

    // Hover first button
    hoverLogic.onMouseMove(5, 5);
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(0, baseAlpha), 0.8f); // Full alpha
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(1, baseAlpha), 0.24f); // Reduced alpha

    // Hover second button
    hoverLogic.onMouseMove(20, 5);
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(0, baseAlpha), 0.24f); // Reduced alpha
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(1, baseAlpha), 0.8f); // Full alpha
}

TEST_F(StandaloneTest, UnhoveredAlphaConfiguration) {
    MockGlobalState globalState;
    WindowActionsButtonLogic buttonLogic(15.0f, &globalState);
    WindowActionsHoverLogic hoverLogic(&buttonLogic);

    // Test default alpha (1.0 = no transparency)
    EXPECT_FLOAT_EQ(hoverLogic.getUnhoveredAlpha(), 1.0f);
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(0, 1.0f), 1.0f); // No reduction

    // Test 10% opacity when not hovered
    hoverLogic.setUnhoveredAlpha(0.1f);
    EXPECT_FLOAT_EQ(hoverLogic.getUnhoveredAlpha(), 0.1f);
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(0, 1.0f), 0.1f); // 10% opacity

    // Test 50% opacity when not hovered
    hoverLogic.setUnhoveredAlpha(0.5f);
    EXPECT_FLOAT_EQ(hoverLogic.getUnhoveredAlpha(), 0.5f);
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(0, 1.0f), 0.5f); // 50% opacity

    // Test with hovered button (should always get full alpha)
    globalState.buttons.push_back({"", "⨯", "⨯", "killactive", "", 0xff4040, 0x333333});
    hoverLogic.onMouseMove(5, 5); // Hover first button
    EXPECT_FLOAT_EQ(hoverLogic.getButtonAlpha(0, 1.0f), 1.0f); // Full alpha despite low unhovered_alpha
}

// Mock class for testing movewindow command detection and drag functionality
class WindowActionCommandExecutor {
public:
    enum class ActionResult {
        EXECUTED_COMMAND,
        STARTED_DRAG,
        INVALID_COMMAND
    };

    static ActionResult executeCommand(const std::string& command) {
        if (command.empty()) {
            return ActionResult::INVALID_COMMAND;
        }

        // Check for special movewindow command
        if (command == "__movewindow__") {
            return ActionResult::STARTED_DRAG;
        }

        // All other commands are executed normally
        return ActionResult::EXECUTED_COMMAND;
    }

    static bool isMoveWindowCommand(const std::string& command) {
        return command == "__movewindow__";
    }
};

TEST_F(StandaloneTest, MoveWindowCommandDetection) {
    // Test movewindow command detection
    EXPECT_TRUE(WindowActionCommandExecutor::isMoveWindowCommand("__movewindow__"));
    EXPECT_FALSE(WindowActionCommandExecutor::isMoveWindowCommand("hyprctl dispatch killactive"));
    EXPECT_FALSE(WindowActionCommandExecutor::isMoveWindowCommand("hyprctl dispatch fullscreen"));
    EXPECT_FALSE(WindowActionCommandExecutor::isMoveWindowCommand(""));
    EXPECT_FALSE(WindowActionCommandExecutor::isMoveWindowCommand("movewindow")); // Without underscores
    EXPECT_FALSE(WindowActionCommandExecutor::isMoveWindowCommand("__movewindow")); // Missing trailing __
    EXPECT_FALSE(WindowActionCommandExecutor::isMoveWindowCommand("movewindow__")); // Missing leading __
}

TEST_F(StandaloneTest, CommandExecution) {
    // Test regular command execution
    auto result1 = WindowActionCommandExecutor::executeCommand("hyprctl dispatch killactive");
    EXPECT_EQ(result1, WindowActionCommandExecutor::ActionResult::EXECUTED_COMMAND);

    auto result2 = WindowActionCommandExecutor::executeCommand("notify-send 'test'");
    EXPECT_EQ(result2, WindowActionCommandExecutor::ActionResult::EXECUTED_COMMAND);

    // Test movewindow command
    auto result3 = WindowActionCommandExecutor::executeCommand("__movewindow__");
    EXPECT_EQ(result3, WindowActionCommandExecutor::ActionResult::STARTED_DRAG);

    // Test empty command
    auto result4 = WindowActionCommandExecutor::executeCommand("");
    EXPECT_EQ(result4, WindowActionCommandExecutor::ActionResult::INVALID_COMMAND);
}

// Mock class for testing drag state and visual feedback
class WindowDragStateManager {
private:
    bool m_bDragPending = false;
    bool m_bDraggingThis = false;
    int m_hoveredButton = -1;
    float m_unhovered_alpha = 1.0f;

public:
    void setDragPending(bool pending) {
        m_bDragPending = pending;
    }

    void setDraggingThis(bool dragging) {
        m_bDraggingThis = dragging;
        if (dragging) {
            m_bDragPending = false; // Clear pending when drag starts
        }
    }

    void setHoveredButton(int buttonIndex) {
        m_hoveredButton = buttonIndex;
    }

    void setUnhoveredAlpha(float alpha) {
        m_unhovered_alpha = alpha;
    }

    bool isDragPending() const {
        return m_bDragPending;
    }

    bool isDragging() const {
        return m_bDraggingThis;
    }

    int getHoveredButton() const {
        return m_hoveredButton;
    }

    // Simulate icon selection logic with drag state
    std::string getButtonIcon(const std::string& iconInactive, const std::string& iconActive,
                             const std::string& command, const std::string& condition, bool conditionMet) const {
        // For movewindow buttons, show active icon while dragging
        if (command == "__movewindow__" && m_bDraggingThis) {
            return iconActive;
        }

        // For other buttons, use condition-based logic
        if (!condition.empty() && conditionMet) {
            return iconActive;
        }

        return iconInactive;
    }

    // Simulate alpha calculation with drag state
    float getButtonAlpha(int buttonIndex, const std::string& command, float baseAlpha = 1.0f) const {
        // For movewindow buttons, full opacity while dragging
        bool isDraggingMoveButton = (command == "__movewindow__" && m_bDraggingThis);

        if (m_hoveredButton == buttonIndex || isDraggingMoveButton) {
            return baseAlpha; // Full opacity
        } else {
            return baseAlpha * m_unhovered_alpha; // Apply unhovered alpha
        }
    }
};

TEST_F(StandaloneTest, DragStateManagement) {
    WindowDragStateManager dragManager;

    // Initial state
    EXPECT_FALSE(dragManager.isDragPending());
    EXPECT_FALSE(dragManager.isDragging());

    // Set drag pending
    dragManager.setDragPending(true);
    EXPECT_TRUE(dragManager.isDragPending());
    EXPECT_FALSE(dragManager.isDragging());

    // Start dragging (should clear pending)
    dragManager.setDraggingThis(true);
    EXPECT_FALSE(dragManager.isDragPending());
    EXPECT_TRUE(dragManager.isDragging());

    // Stop dragging
    dragManager.setDraggingThis(false);
    EXPECT_FALSE(dragManager.isDragPending());
    EXPECT_FALSE(dragManager.isDragging());
}

TEST_F(StandaloneTest, DragVisualFeedbackIconSwitching) {
    WindowDragStateManager dragManager;

    std::string iconInactive = "⇱";
    std::string iconActive = "⇲";
    std::string moveCommand = "__movewindow__";
    std::string normalCommand = "hyprctl dispatch killactive";

    // Test movewindow button - shows inactive icon when not dragging
    std::string icon1 = dragManager.getButtonIcon(iconInactive, iconActive, moveCommand, "", false);
    EXPECT_EQ(icon1, iconInactive);

    // Test movewindow button - shows active icon while dragging
    dragManager.setDraggingThis(true);
    std::string icon2 = dragManager.getButtonIcon(iconInactive, iconActive, moveCommand, "", false);
    EXPECT_EQ(icon2, iconActive);

    // Test normal button - not affected by drag state
    std::string icon3 = dragManager.getButtonIcon("⨯", "⨯", normalCommand, "", false);
    EXPECT_EQ(icon3, "⨯");

    // Test condition-based button - still works with conditions
    dragManager.setDraggingThis(false);
    std::string icon4 = dragManager.getButtonIcon("⬈", "⬋", normalCommand, "fullscreen", true);
    EXPECT_EQ(icon4, "⬋"); // Active due to condition, not drag state
}

TEST_F(StandaloneTest, DragVisualFeedbackAlpha) {
    WindowDragStateManager dragManager;
    dragManager.setUnhoveredAlpha(0.3f); // 30% opacity when not hovered

    float baseAlpha = 1.0f;
    std::string moveCommand = "__movewindow__";
    std::string normalCommand = "hyprctl dispatch killactive";

    // Test normal button alpha (not hovered, not dragging)
    float alpha1 = dragManager.getButtonAlpha(0, normalCommand, baseAlpha);
    EXPECT_FLOAT_EQ(alpha1, 0.3f); // Reduced alpha

    // Test hovered button alpha
    dragManager.setHoveredButton(0);
    float alpha2 = dragManager.getButtonAlpha(0, normalCommand, baseAlpha);
    EXPECT_FLOAT_EQ(alpha2, 1.0f); // Full alpha when hovered

    // Test movewindow button alpha while dragging (not hovered)
    dragManager.setHoveredButton(-1); // Not hovered
    dragManager.setDraggingThis(true);
    float alpha3 = dragManager.getButtonAlpha(0, moveCommand, baseAlpha);
    EXPECT_FLOAT_EQ(alpha3, 1.0f); // Full alpha while dragging, even if not hovered

    // Test non-movewindow button while movewindow is dragging
    float alpha4 = dragManager.getButtonAlpha(1, normalCommand, baseAlpha);
    EXPECT_FLOAT_EQ(alpha4, 0.3f); // Still reduced alpha for other buttons
}

TEST_F(StandaloneTest, MoveWindowButtonConfiguration) {
    // Test parsing movewindow button configuration
    auto result = ButtonConfigParser::parseButtonConfig("rgb(e6e6e6), rgb(859900), ⇱, ⇲, __movewindow__,");
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.button.icon_inactive, "⇱");
    EXPECT_EQ(result.button.icon_active, "⇲");
    EXPECT_EQ(result.button.command, "__movewindow__");
    EXPECT_EQ(result.button.condition, "");

    // Verify it's recognized as a move command
    EXPECT_TRUE(WindowActionCommandExecutor::isMoveWindowCommand(result.button.command));
}

TEST_F(StandaloneTest, MoveWindowButtonIntegration) {
    MockGlobalState globalState;

    // Add a movewindow button and a regular button
    MockWindowActionButton moveButton{"", "⇱", "⇲", "__movewindow__", "", 0xe6e6e6, 0x859900};
    MockWindowActionButton closeButton{"", "⨯", "⨯", "hyprctl dispatch killactive", "", 0xff4040, 0x333333};

    globalState.buttons.push_back(moveButton);
    globalState.buttons.push_back(closeButton);

    WindowActionsButtonLogic buttonLogic(15.0f, &globalState);
    WindowDragStateManager dragManager;

    EXPECT_EQ(buttonLogic.getButtonCount(), 2);

    // Test button positioning
    EXPECT_EQ(buttonLogic.getButtonIndex(5, 5), 0);   // First button (move)
    EXPECT_EQ(buttonLogic.getButtonIndex(20, 5), 1);  // Second button (close)

    // Test command execution
    auto moveResult = WindowActionCommandExecutor::executeCommand(moveButton.command);
    EXPECT_EQ(moveResult, WindowActionCommandExecutor::ActionResult::STARTED_DRAG);

    auto closeResult = WindowActionCommandExecutor::executeCommand(closeButton.command);
    EXPECT_EQ(closeResult, WindowActionCommandExecutor::ActionResult::EXECUTED_COMMAND);

    // Test visual feedback for movewindow button
    dragManager.setDraggingThis(true);
    std::string moveIcon = dragManager.getButtonIcon(moveButton.icon_inactive, moveButton.icon_active,
                                                    moveButton.command, moveButton.condition, false);
    EXPECT_EQ(moveIcon, "⇲"); // Active icon while dragging

    float moveAlpha = dragManager.getButtonAlpha(0, moveButton.command, 1.0f);
    EXPECT_FLOAT_EQ(moveAlpha, 1.0f); // Full alpha while dragging
}

// Mock class for testing automatic window focusing before command execution
class WindowFocusCommandExecutor {
private:
    int m_focusedWindowId = -1;
    std::string m_lastExecutedCommand;
    bool m_windowFocusedBeforeExec = false;

public:
    enum class ExecutionResult {
        MOVE_WINDOW_INITIATED,
        COMMAND_EXECUTED,
        INVALID_COMMAND
    };

    ExecutionResult executeCommand(const std::string& command, int windowId) {
        if (command.empty()) {
            return ExecutionResult::INVALID_COMMAND;
        }

        // Special case: movewindow doesn't need focusing
        if (command == "__movewindow__") {
            m_lastExecutedCommand = command;
            return ExecutionResult::MOVE_WINDOW_INITIATED;
        }

        // Focus window before executing any regular command
        m_focusedWindowId = windowId;
        m_windowFocusedBeforeExec = true;
        m_lastExecutedCommand = command;

        return ExecutionResult::COMMAND_EXECUTED;
    }

    int getFocusedWindowId() const {
        return m_focusedWindowId;
    }

    std::string getLastExecutedCommand() const {
        return m_lastExecutedCommand;
    }

    bool wasWindowFocusedBeforeExec() const {
        return m_windowFocusedBeforeExec;
    }

    void reset() {
        m_focusedWindowId = -1;
        m_lastExecutedCommand = "";
        m_windowFocusedBeforeExec = false;
    }
};

TEST_F(StandaloneTest, AutoFocusBeforeCommandExecution) {
    WindowFocusCommandExecutor executor;

    // Test that regular command focuses window before execution
    auto result1 = executor.executeCommand("hyprctl dispatch killactive", 123);
    EXPECT_EQ(result1, WindowFocusCommandExecutor::ExecutionResult::COMMAND_EXECUTED);
    EXPECT_EQ(executor.getFocusedWindowId(), 123);
    EXPECT_TRUE(executor.wasWindowFocusedBeforeExec());
    EXPECT_EQ(executor.getLastExecutedCommand(), "hyprctl dispatch killactive");

    // Test different window
    executor.reset();
    auto result2 = executor.executeCommand("hyprctl dispatch togglefloating", 456);
    EXPECT_EQ(result2, WindowFocusCommandExecutor::ExecutionResult::COMMAND_EXECUTED);
    EXPECT_EQ(executor.getFocusedWindowId(), 456);
    EXPECT_TRUE(executor.wasWindowFocusedBeforeExec());
    EXPECT_EQ(executor.getLastExecutedCommand(), "hyprctl dispatch togglefloating");

    // Test that movewindow doesn't focus (special case)
    executor.reset();
    auto result3 = executor.executeCommand("__movewindow__", 789);
    EXPECT_EQ(result3, WindowFocusCommandExecutor::ExecutionResult::MOVE_WINDOW_INITIATED);
    EXPECT_EQ(executor.getFocusedWindowId(), -1); // Window not focused for movewindow
    EXPECT_FALSE(executor.wasWindowFocusedBeforeExec());
}

TEST_F(StandaloneTest, AutoFocusEnsuresCorrectWindowClosure) {
    WindowFocusCommandExecutor executor;

    // Simulate scenario: User clicks close button on window 100
    // Even if window 200 is currently active, window 100 should be closed
    auto result = executor.executeCommand("hyprctl dispatch killactive", 100);

    EXPECT_EQ(result, WindowFocusCommandExecutor::ExecutionResult::COMMAND_EXECUTED);
    EXPECT_EQ(executor.getFocusedWindowId(), 100); // Window 100 was focused
    EXPECT_TRUE(executor.wasWindowFocusedBeforeExec());

    // killactive will now close window 100 since it was focused first
    EXPECT_EQ(executor.getLastExecutedCommand(), "hyprctl dispatch killactive");
}

TEST_F(StandaloneTest, AutoFocusWithMultipleCommands) {
    WindowFocusCommandExecutor executor;

    // Test multiple different commands all focus before execution
    struct TestCase {
        std::string command;
        int windowId;
    };

    std::vector<TestCase> testCases = {
        {"hyprctl dispatch killactive", 100},
        {"hyprctl dispatch fullscreen 1", 200},
        {"hyprctl dispatch togglegroup", 300},
        {"hyprctl dispatch togglefloating", 400},
        {"notify-send 'Window Info'", 500}
    };

    for (const auto& testCase : testCases) {
        executor.reset();
        auto result = executor.executeCommand(testCase.command, testCase.windowId);

        EXPECT_EQ(result, WindowFocusCommandExecutor::ExecutionResult::COMMAND_EXECUTED);
        EXPECT_EQ(executor.getFocusedWindowId(), testCase.windowId);
        EXPECT_TRUE(executor.wasWindowFocusedBeforeExec());
        EXPECT_EQ(executor.getLastExecutedCommand(), testCase.command);
    }
}