#include <gtest/gtest.h>
#include <vector>
#include <memory>

class StandaloneTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

class WindowActionsButtonLogic {
public:
    static constexpr float DEFAULT_BUTTON_SIZE = 15.0f;
    static constexpr float BUTTON_SPACING = 2.0f;
    static constexpr int NUM_BUTTONS = 4;

private:
    float m_buttonSize;

public:
    explicit WindowActionsButtonLogic(float buttonSize = DEFAULT_BUTTON_SIZE) : m_buttonSize(buttonSize) {}

    bool isOnButton(float x, float y, int buttonIndex) const {
        float buttonX = buttonIndex * (m_buttonSize + BUTTON_SPACING);
        return x >= buttonX && x < buttonX + m_buttonSize && y >= 0 && y < m_buttonSize;
    }

    int getButtonIndex(float x, float y) const {
        if (y < 0 || y >= m_buttonSize) return -1;

        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (isOnButton(x, y, i)) {
                return i;
            }
        }
        return -1;
    }

    bool isOnCloseButton(float x, float y) const {
        return isOnButton(x, y, 0);
    }

    bool isOnFullscreenButton(float x, float y) const {
        return isOnButton(x, y, 1);
    }

    bool isOnGroupButton(float x, float y) const {
        return isOnButton(x, y, 2);
    }

    bool isOnFloatingButton(float x, float y) const {
        return isOnButton(x, y, 3);
    }

    float getButtonSize() const {
        return m_buttonSize;
    }
};

TEST_F(StandaloneTest, ButtonConstants) {
    WindowActionsButtonLogic logic;
    EXPECT_EQ(logic.getButtonSize(), 15.0f);
    EXPECT_EQ(WindowActionsButtonLogic::BUTTON_SPACING, 2.0f);
    EXPECT_EQ(WindowActionsButtonLogic::NUM_BUTTONS, 4);
}

TEST_F(StandaloneTest, CloseButtonDetection) {
    WindowActionsButtonLogic logic;
    EXPECT_TRUE(logic.isOnCloseButton(5, 5));
    EXPECT_TRUE(logic.isOnCloseButton(10, 10));
    EXPECT_FALSE(logic.isOnCloseButton(20, 5));
    EXPECT_FALSE(logic.isOnCloseButton(5, 25));
    EXPECT_FALSE(logic.isOnCloseButton(-5, 5));
}

TEST_F(StandaloneTest, FullscreenButtonDetection) {
    WindowActionsButtonLogic logic;
    EXPECT_TRUE(logic.isOnFullscreenButton(17, 5));
    EXPECT_TRUE(logic.isOnFullscreenButton(25, 10));
    EXPECT_FALSE(logic.isOnFullscreenButton(5, 5));
    EXPECT_FALSE(logic.isOnFullscreenButton(40, 5));
    EXPECT_FALSE(logic.isOnFullscreenButton(20, 25));
}

TEST_F(StandaloneTest, GroupButtonDetection) {
    WindowActionsButtonLogic logic;
    EXPECT_TRUE(logic.isOnGroupButton(34, 5));
    EXPECT_TRUE(logic.isOnGroupButton(42, 10));
    EXPECT_FALSE(logic.isOnGroupButton(20, 5));
    EXPECT_FALSE(logic.isOnGroupButton(60, 5));
    EXPECT_FALSE(logic.isOnGroupButton(37, 25));
}

TEST_F(StandaloneTest, FloatingButtonDetection) {
    WindowActionsButtonLogic logic;
    EXPECT_TRUE(logic.isOnFloatingButton(51, 5));
    EXPECT_TRUE(logic.isOnFloatingButton(59, 10));
    EXPECT_FALSE(logic.isOnFloatingButton(40, 5));
    EXPECT_FALSE(logic.isOnFloatingButton(80, 5));
    EXPECT_FALSE(logic.isOnFloatingButton(54, 25));
}

TEST_F(StandaloneTest, ButtonIndexCalculation) {
    WindowActionsButtonLogic logic;
    EXPECT_EQ(logic.getButtonIndex(5, 5), 0);
    EXPECT_EQ(logic.getButtonIndex(20, 5), 1);
    EXPECT_EQ(logic.getButtonIndex(37, 5), 2);
    EXPECT_EQ(logic.getButtonIndex(54, 5), 3);

    EXPECT_EQ(logic.getButtonIndex(100, 5), -1);
    EXPECT_EQ(logic.getButtonIndex(5, 20), -1);
    EXPECT_EQ(logic.getButtonIndex(-1, 5), -1);
}

TEST_F(StandaloneTest, ButtonBoundaryTesting) {
    WindowActionsButtonLogic logic;
    EXPECT_TRUE(logic.isOnButton(0, 0, 0));
    EXPECT_TRUE(logic.isOnButton(14, 14, 0));
    EXPECT_FALSE(logic.isOnButton(15, 0, 0));
    EXPECT_FALSE(logic.isOnButton(0, 15, 0));
}

TEST_F(StandaloneTest, ConfigurableButtonSize) {
    // Test with custom button size
    WindowActionsButtonLogic logic(20.0f);
    EXPECT_EQ(logic.getButtonSize(), 20.0f);

    // Test button detection with larger button size
    EXPECT_TRUE(logic.isOnCloseButton(10, 10));
    EXPECT_TRUE(logic.isOnCloseButton(19, 19));
    EXPECT_FALSE(logic.isOnCloseButton(20, 10)); // Outside button

    // Test second button with spacing
    float expectedSecondButtonStart = 20.0f + WindowActionsButtonLogic::BUTTON_SPACING;
    EXPECT_TRUE(logic.isOnFullscreenButton(expectedSecondButtonStart + 5, 10));
    EXPECT_FALSE(logic.isOnFullscreenButton(expectedSecondButtonStart - 1, 10)); // Before button

    // Test smaller button size
    WindowActionsButtonLogic smallLogic(10.0f);
    EXPECT_EQ(smallLogic.getButtonSize(), 10.0f);
    EXPECT_TRUE(smallLogic.isOnCloseButton(5, 5));
    EXPECT_FALSE(smallLogic.isOnCloseButton(10, 5)); // Outside smaller button
}

class MockGlobalState {
public:
    std::vector<std::shared_ptr<int>> bars;

    bool isEmpty() const {
        return bars.empty();
    }

    size_t size() const {
        return bars.size();
    }

    void addBar() {
        bars.push_back(std::make_shared<int>(42));
    }

    void clear() {
        bars.clear();
    }
};

TEST_F(StandaloneTest, GlobalStateManagement) {
    MockGlobalState state;
    EXPECT_TRUE(state.isEmpty());
    EXPECT_EQ(state.size(), 0);

    state.addBar();
    EXPECT_FALSE(state.isEmpty());
    EXPECT_EQ(state.size(), 1);

    state.addBar();
    EXPECT_EQ(state.size(), 2);

    state.clear();
    EXPECT_TRUE(state.isEmpty());
    EXPECT_EQ(state.size(), 0);
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