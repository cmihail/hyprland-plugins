#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <string>

// Mock AnimatedVariable for float
template<typename T>
class MockAnimatedVariable {
public:
    T currentValue;
    T goalValue;
    std::function<void()> updateCallback;
    std::function<void()> endCallback;

    MockAnimatedVariable(const T& initial) : currentValue(initial), goalValue(initial) {}

    void setValue(const T& val) {
        currentValue = val;
    }

    T value() const {
        return currentValue;
    }

    MockAnimatedVariable& operator=(const T& val) {
        goalValue = val;
        return *this;
    }

    void setUpdateCallback(std::function<void()> cb) {
        updateCallback = cb;
    }

    void setCallbackOnEnd(std::function<void()> cb) {
        endCallback = cb;
    }

    // Simulate animation completion
    void finishAnimation() {
        currentValue = goalValue;
        if (endCallback) {
            endCallback();
        }
    }

    // Simulate animation step
    void stepAnimation(float progress) {
        if (progress >= 1.0f) {
            finishAnimation();
        } else {
            // Linear interpolation for float
            currentValue = currentValue + (goalValue - currentValue) * progress;
            if (updateCallback) {
                updateCallback();
            }
        }
    }
};

template<typename T>
using PHLANIMVAR = std::shared_ptr<MockAnimatedVariable<T>>;

// Global state to test
std::unordered_map<size_t, PHLANIMVAR<float>> g_gestureScaleAnims;
std::unordered_map<size_t, PHLANIMVAR<float>> g_gestureAlphaAnims;
std::unordered_set<size_t> g_gesturesPendingRemoval;

// Test fixture
class GestureAnimationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_gestureScaleAnims.clear();
        g_gestureAlphaAnims.clear();
        g_gesturesPendingRemoval.clear();
    }

    void TearDown() override {
        g_gestureScaleAnims.clear();
        g_gestureAlphaAnims.clear();
        g_gesturesPendingRemoval.clear();
    }
};

// Test: Animation initialization for new gesture
TEST_F(GestureAnimationsTest, NewGestureInitializesAnimations) {
    size_t gestureIndex = 0;

    // Simulate creating animations for a new gesture
    auto& scaleVar = g_gestureScaleAnims[gestureIndex];
    auto& alphaVar = g_gestureAlphaAnims[gestureIndex];

    scaleVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);
    alphaVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);

    // Set initial values (start invisible/tiny)
    scaleVar->setValue(0.0f);
    alphaVar->setValue(0.0f);

    // Set goals (animate to visible/full size)
    *scaleVar = 1.0f;
    *alphaVar = 1.0f;

    // Verify animations exist and have correct values
    EXPECT_TRUE(g_gestureScaleAnims.count(gestureIndex) > 0);
    EXPECT_TRUE(g_gestureAlphaAnims.count(gestureIndex) > 0);
    EXPECT_EQ(scaleVar->value(), 0.0f);
    EXPECT_EQ(alphaVar->value(), 0.0f);
    EXPECT_EQ(scaleVar->goalValue, 1.0f);
    EXPECT_EQ(alphaVar->goalValue, 1.0f);
}

// Test: Animation progresses correctly for gesture addition
TEST_F(GestureAnimationsTest, AddAnimationProgresses) {
    size_t gestureIndex = 0;

    auto& scaleVar = g_gestureScaleAnims[gestureIndex];
    auto& alphaVar = g_gestureAlphaAnims[gestureIndex];

    scaleVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);
    alphaVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);

    scaleVar->setValue(0.0f);
    alphaVar->setValue(0.0f);
    *scaleVar = 1.0f;
    *alphaVar = 1.0f;

    // Simulate animation progress
    scaleVar->stepAnimation(0.5f);
    alphaVar->stepAnimation(0.5f);

    // At 50% progress, values should be around 0.5
    EXPECT_GT(scaleVar->value(), 0.4f);
    EXPECT_LT(scaleVar->value(), 0.6f);
    EXPECT_GT(alphaVar->value(), 0.4f);
    EXPECT_LT(alphaVar->value(), 0.6f);

    // Complete the animation
    scaleVar->finishAnimation();
    alphaVar->finishAnimation();

    EXPECT_EQ(scaleVar->value(), 1.0f);
    EXPECT_EQ(alphaVar->value(), 1.0f);
}

// Test: Removal animation initialization
TEST_F(GestureAnimationsTest, RemovalAnimationInitialization) {
    size_t gestureIndex = 0;

    // Mark gesture for removal
    g_gesturesPendingRemoval.insert(gestureIndex);

    // Create removal animations (reverse: 1.0 -> 0.0)
    auto& scaleVar = g_gestureScaleAnims[gestureIndex];
    auto& alphaVar = g_gestureAlphaAnims[gestureIndex];

    scaleVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);
    alphaVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);

    scaleVar->setValue(1.0f);
    alphaVar->setValue(1.0f);
    *scaleVar = 0.0f;
    *alphaVar = 0.0f;

    // Verify removal state
    EXPECT_TRUE(g_gesturesPendingRemoval.count(gestureIndex) > 0);
    EXPECT_EQ(scaleVar->value(), 1.0f);
    EXPECT_EQ(alphaVar->value(), 1.0f);
    EXPECT_EQ(scaleVar->goalValue, 0.0f);
    EXPECT_EQ(alphaVar->goalValue, 0.0f);
}

// Test: Removal animation progresses correctly
TEST_F(GestureAnimationsTest, RemovalAnimationProgresses) {
    size_t gestureIndex = 0;

    g_gesturesPendingRemoval.insert(gestureIndex);

    auto& scaleVar = g_gestureScaleAnims[gestureIndex];
    auto& alphaVar = g_gestureAlphaAnims[gestureIndex];

    scaleVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);
    alphaVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);

    scaleVar->setValue(1.0f);
    alphaVar->setValue(1.0f);
    *scaleVar = 0.0f;
    *alphaVar = 0.0f;

    // Simulate animation progress
    scaleVar->stepAnimation(0.5f);
    alphaVar->stepAnimation(0.5f);

    // At 50% progress, values should be around 0.5
    EXPECT_GT(scaleVar->value(), 0.4f);
    EXPECT_LT(scaleVar->value(), 0.6f);
    EXPECT_GT(alphaVar->value(), 0.4f);
    EXPECT_LT(alphaVar->value(), 0.6f);

    // Complete the animation
    scaleVar->finishAnimation();
    alphaVar->finishAnimation();

    EXPECT_EQ(scaleVar->value(), 0.0f);
    EXPECT_EQ(alphaVar->value(), 0.0f);
}

// Test: Multiple gestures can have independent animations
TEST_F(GestureAnimationsTest, MultipleGestureAnimations) {
    // Add animations for multiple gestures
    for (size_t i = 0; i < 3; ++i) {
        auto& scaleVar = g_gestureScaleAnims[i];
        auto& alphaVar = g_gestureAlphaAnims[i];

        scaleVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);
        alphaVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);

        scaleVar->setValue(0.0f);
        alphaVar->setValue(0.0f);
        *scaleVar = 1.0f;
        *alphaVar = 1.0f;
    }

    // Verify all animations exist
    EXPECT_EQ(g_gestureScaleAnims.size(), 3);
    EXPECT_EQ(g_gestureAlphaAnims.size(), 3);

    // Animate each to different progress levels
    g_gestureScaleAnims[0]->stepAnimation(0.25f);
    g_gestureScaleAnims[1]->stepAnimation(0.5f);
    g_gestureScaleAnims[2]->stepAnimation(0.75f);

    // Verify independent progress
    EXPECT_LT(g_gestureScaleAnims[0]->value(), g_gestureScaleAnims[1]->value());
    EXPECT_LT(g_gestureScaleAnims[1]->value(), g_gestureScaleAnims[2]->value());
}

// Test: Animation cleanup
TEST_F(GestureAnimationsTest, AnimationCleanup) {
    // Create animations for a gesture
    size_t gestureIndex = 0;
    auto& scaleVar = g_gestureScaleAnims[gestureIndex];
    auto& alphaVar = g_gestureAlphaAnims[gestureIndex];

    scaleVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);
    alphaVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);

    EXPECT_EQ(g_gestureScaleAnims.size(), 1);
    EXPECT_EQ(g_gestureAlphaAnims.size(), 1);

    // Cleanup
    g_gestureScaleAnims.clear();
    g_gestureAlphaAnims.clear();
    g_gesturesPendingRemoval.clear();

    EXPECT_EQ(g_gestureScaleAnims.size(), 0);
    EXPECT_EQ(g_gestureAlphaAnims.size(), 0);
    EXPECT_EQ(g_gesturesPendingRemoval.size(), 0);
}

// Test: Callbacks are invoked
TEST_F(GestureAnimationsTest, CallbacksAreInvoked) {
    size_t gestureIndex = 0;
    bool updateCallbackInvoked = false;
    bool endCallbackInvoked = false;

    auto& scaleVar = g_gestureScaleAnims[gestureIndex];
    scaleVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);
    scaleVar->setValue(0.0f);
    *scaleVar = 1.0f;

    scaleVar->setUpdateCallback([&updateCallbackInvoked]() {
        updateCallbackInvoked = true;
    });

    scaleVar->setCallbackOnEnd([&endCallbackInvoked]() {
        endCallbackInvoked = true;
    });

    // Step animation should trigger update callback
    scaleVar->stepAnimation(0.5f);
    EXPECT_TRUE(updateCallbackInvoked);
    EXPECT_FALSE(endCallbackInvoked);

    // Finish animation should trigger end callback
    scaleVar->finishAnimation();
    EXPECT_TRUE(endCallbackInvoked);
}

// Test: Animation values are within valid range
TEST_F(GestureAnimationsTest, AnimationValuesInRange) {
    size_t gestureIndex = 0;

    auto& scaleVar = g_gestureScaleAnims[gestureIndex];
    auto& alphaVar = g_gestureAlphaAnims[gestureIndex];

    scaleVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);
    alphaVar = std::make_shared<MockAnimatedVariable<float>>(1.0f);

    scaleVar->setValue(0.0f);
    alphaVar->setValue(0.0f);
    *scaleVar = 1.0f;
    *alphaVar = 1.0f;

    // Test at various progress points
    for (float progress = 0.0f; progress <= 1.0f; progress += 0.1f) {
        scaleVar->stepAnimation(progress);
        alphaVar->stepAnimation(progress);

        // Values should always be in [0, 1] range
        EXPECT_GE(scaleVar->value(), 0.0f);
        EXPECT_LE(scaleVar->value(), 1.0f);
        EXPECT_GE(alphaVar->value(), 0.0f);
        EXPECT_LE(alphaVar->value(), 1.0f);
    }
}
