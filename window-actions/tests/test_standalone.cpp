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
    static constexpr float BUTTON_SIZE = 15.0f;
    static constexpr float BUTTON_SPACING = 2.0f;
    static constexpr int NUM_BUTTONS = 4;

    static bool isOnButton(float x, float y, int buttonIndex) {
        float buttonX = buttonIndex * (BUTTON_SIZE + BUTTON_SPACING);
        return x >= buttonX && x < buttonX + BUTTON_SIZE && y >= 0 && y < BUTTON_SIZE;
    }

    static int getButtonIndex(float x, float y) {
        if (y < 0 || y >= BUTTON_SIZE) return -1;

        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (isOnButton(x, y, i)) {
                return i;
            }
        }
        return -1;
    }

    static bool isOnCloseButton(float x, float y) {
        return isOnButton(x, y, 0);
    }

    static bool isOnFullscreenButton(float x, float y) {
        return isOnButton(x, y, 1);
    }

    static bool isOnGroupButton(float x, float y) {
        return isOnButton(x, y, 2);
    }

    static bool isOnFloatingButton(float x, float y) {
        return isOnButton(x, y, 3);
    }
};

TEST_F(StandaloneTest, ButtonConstants) {
    EXPECT_EQ(WindowActionsButtonLogic::BUTTON_SIZE, 15.0f);
    EXPECT_EQ(WindowActionsButtonLogic::BUTTON_SPACING, 2.0f);
    EXPECT_EQ(WindowActionsButtonLogic::NUM_BUTTONS, 4);
}

TEST_F(StandaloneTest, CloseButtonDetection) {
    EXPECT_TRUE(WindowActionsButtonLogic::isOnCloseButton(5, 5));
    EXPECT_TRUE(WindowActionsButtonLogic::isOnCloseButton(10, 10));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnCloseButton(20, 5));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnCloseButton(5, 25));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnCloseButton(-5, 5));
}

TEST_F(StandaloneTest, FullscreenButtonDetection) {
    EXPECT_TRUE(WindowActionsButtonLogic::isOnFullscreenButton(17, 5));
    EXPECT_TRUE(WindowActionsButtonLogic::isOnFullscreenButton(25, 10));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnFullscreenButton(5, 5));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnFullscreenButton(40, 5));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnFullscreenButton(20, 25));
}

TEST_F(StandaloneTest, GroupButtonDetection) {
    EXPECT_TRUE(WindowActionsButtonLogic::isOnGroupButton(34, 5));
    EXPECT_TRUE(WindowActionsButtonLogic::isOnGroupButton(42, 10));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnGroupButton(20, 5));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnGroupButton(60, 5));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnGroupButton(37, 25));
}

TEST_F(StandaloneTest, FloatingButtonDetection) {
    EXPECT_TRUE(WindowActionsButtonLogic::isOnFloatingButton(51, 5));
    EXPECT_TRUE(WindowActionsButtonLogic::isOnFloatingButton(59, 10));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnFloatingButton(40, 5));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnFloatingButton(80, 5));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnFloatingButton(54, 25));
}

TEST_F(StandaloneTest, ButtonIndexCalculation) {
    EXPECT_EQ(WindowActionsButtonLogic::getButtonIndex(5, 5), 0);
    EXPECT_EQ(WindowActionsButtonLogic::getButtonIndex(20, 5), 1);
    EXPECT_EQ(WindowActionsButtonLogic::getButtonIndex(37, 5), 2);
    EXPECT_EQ(WindowActionsButtonLogic::getButtonIndex(54, 5), 3);

    EXPECT_EQ(WindowActionsButtonLogic::getButtonIndex(100, 5), -1);
    EXPECT_EQ(WindowActionsButtonLogic::getButtonIndex(5, 20), -1);
    EXPECT_EQ(WindowActionsButtonLogic::getButtonIndex(-1, 5), -1);
}

TEST_F(StandaloneTest, ButtonBoundaryTesting) {
    EXPECT_TRUE(WindowActionsButtonLogic::isOnButton(0, 0, 0));
    EXPECT_TRUE(WindowActionsButtonLogic::isOnButton(14, 14, 0));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnButton(15, 0, 0));
    EXPECT_FALSE(WindowActionsButtonLogic::isOnButton(0, 15, 0));
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