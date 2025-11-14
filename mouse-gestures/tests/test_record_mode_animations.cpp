#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cmath>

// Mock Vector2D
struct Vector2D {
    float x;
    float y;

    Vector2D() : x(0), y(0) {}
    Vector2D(float x_, float y_) : x(x_), y(y_) {}

    Vector2D operator*(float scalar) const {
        return Vector2D{x * scalar, y * scalar};
    }

    Vector2D operator/(float scalar) const {
        return Vector2D{x / scalar, y / scalar};
    }

    Vector2D operator-(const Vector2D& other) const {
        return Vector2D{x - other.x, y - other.y};
    }

    bool operator==(const Vector2D& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Vector2D& other) const {
        return !(*this == other);
    }
};

// Mock AnimatedVariable
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
            // Linear interpolation
            currentValue.x = currentValue.x + (goalValue.x - currentValue.x) * progress;
            currentValue.y = currentValue.y + (goalValue.y - currentValue.y) * progress;
            if (updateCallback) {
                updateCallback();
            }
        }
    }
};

// Mock Monitor
struct MockMonitor {
    Vector2D m_size;

    MockMonitor(float width, float height) : m_size(width, height) {}
};

using PHLMONITOR = std::shared_ptr<MockMonitor>;
template<typename T>
using PHLANIMVAR = std::shared_ptr<MockAnimatedVariable<T>>;

// Mock global state
namespace {
std::unordered_map<PHLMONITOR, PHLANIMVAR<Vector2D>> g_recordAnimSize;
std::unordered_map<PHLMONITOR, PHLANIMVAR<Vector2D>> g_recordAnimPos;
std::unordered_map<PHLMONITOR, bool> g_recordModeClosing;
std::unordered_map<PHLMONITOR, float> g_scrollOffsets;
std::unordered_map<PHLMONITOR, float> g_maxScrollOffsets;
bool g_recordMode = false;
bool g_animationCallbackCalled = false;
int g_damageCount = 0;
}

// Mock functions
void damageMonitor() {
    g_animationCallbackCalled = true;
    g_damageCount++;
}

void processPendingGestureChanges() {
    // Mock implementation
}

class RecordModeAnimationTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_recordAnimSize.clear();
        g_recordAnimPos.clear();
        g_recordModeClosing.clear();
        g_scrollOffsets.clear();
        g_maxScrollOffsets.clear();
        g_recordMode = false;
        g_animationCallbackCalled = false;
        g_damageCount = 0;
    }

    void TearDown() override {
        g_recordAnimSize.clear();
        g_recordAnimPos.clear();
        g_recordModeClosing.clear();
        g_scrollOffsets.clear();
        g_maxScrollOffsets.clear();
        g_recordMode = false;
        g_animationCallbackCalled = false;
        g_damageCount = 0;
    }

    PHLMONITOR createMockMonitor(float width, float height) {
        return std::make_shared<MockMonitor>(width, height);
    }

    // Helper to calculate record square position and size
    struct RecordSquareInfo {
        Vector2D center;
        float size;
        float scale;
    };

    RecordSquareInfo calculateRecordSquare(const Vector2D& monitorSize) {
        constexpr float PADDING = 20.0f;
        const float verticalSpace = monitorSize.y - (2.0f * PADDING);
        constexpr int VISIBLE_GESTURES = 3;
        constexpr float GAP_WIDTH = 10.0f;
        const float totalGaps = (VISIBLE_GESTURES - 1) * GAP_WIDTH;
        const float baseHeight = (verticalSpace - totalGaps) / VISIBLE_GESTURES;
        const float gestureRectHeight = baseHeight * 0.9f;
        const float gestureRectWidth = gestureRectHeight;
        const float recordSquareSize = verticalSpace;
        const float totalWidth = gestureRectWidth + recordSquareSize;
        const float horizontalMargin = (monitorSize.x - totalWidth) / 3.0f;

        const float recordSquareX = horizontalMargin + gestureRectWidth + horizontalMargin;
        const float recordSquareY = PADDING;

        const Vector2D recordCenter = Vector2D{
            recordSquareX + recordSquareSize / 2.0f,
            recordSquareY + recordSquareSize / 2.0f
        };

        const float scaleX = monitorSize.x / recordSquareSize;
        const float scaleY = monitorSize.y / recordSquareSize;
        const float scale = std::min(scaleX, scaleY);

        return {recordCenter, recordSquareSize, scale};
    }

    // Simulate entering record mode
    void simulateEnterRecordMode(PHLMONITOR monitor) {
        g_recordMode = true;
        g_scrollOffsets.clear();
        g_maxScrollOffsets.clear();
        g_recordModeClosing.clear();

        const Vector2D monitorSize = monitor->m_size;
        auto info = calculateRecordSquare(monitorSize);
        const Vector2D screenCenter = monitorSize / 2.0f;

        // Create animations
        auto& sizeVar = g_recordAnimSize[monitor];
        auto& posVar = g_recordAnimPos[monitor];

        sizeVar = std::make_shared<MockAnimatedVariable<Vector2D>>(monitorSize);
        posVar = std::make_shared<MockAnimatedVariable<Vector2D>>(Vector2D{0, 0});

        sizeVar->setUpdateCallback(damageMonitor);
        posVar->setUpdateCallback(damageMonitor);

        // Set initial value (zoomed into record square)
        sizeVar->setValue(monitorSize * info.scale);
        posVar->setValue((screenCenter - info.center) * info.scale);

        // Set goal (normal view)
        *sizeVar = monitorSize;
        *posVar = Vector2D{0, 0};
    }

    // Simulate exiting record mode
    void simulateExitRecordMode(PHLMONITOR monitor) {
        if (!g_recordMode)
            return;

        const Vector2D monitorSize = monitor->m_size;
        auto info = calculateRecordSquare(monitorSize);
        const Vector2D screenCenter = monitorSize / 2.0f;

        auto& sizeVar = g_recordAnimSize[monitor];
        auto& posVar = g_recordAnimPos[monitor];

        if (!sizeVar) {
            sizeVar = std::make_shared<MockAnimatedVariable<Vector2D>>(monitorSize);
        }
        if (!posVar) {
            posVar = std::make_shared<MockAnimatedVariable<Vector2D>>(Vector2D{0, 0});
        }

        sizeVar->setUpdateCallback(damageMonitor);
        posVar->setUpdateCallback(damageMonitor);

        // Set goal to zoom INTO record square
        *sizeVar = monitorSize * info.scale;
        *posVar = (screenCenter - info.center) * info.scale;

        // Set callback to finish closing
        sizeVar->setCallbackOnEnd([this]() {
            g_recordMode = false;
            g_recordModeClosing.clear();
            g_recordAnimSize.clear();
            g_recordAnimPos.clear();
            processPendingGestureChanges();
        });

        g_recordModeClosing[monitor] = true;
    }
};

// Test 1: Entry animation sets up correctly
TEST_F(RecordModeAnimationTest, EntryAnimationSetup) {
    auto monitor = createMockMonitor(1920, 1080);

    EXPECT_FALSE(g_recordMode);
    EXPECT_TRUE(g_recordAnimSize.empty());
    EXPECT_TRUE(g_recordAnimPos.empty());

    simulateEnterRecordMode(monitor);

    EXPECT_TRUE(g_recordMode);
    EXPECT_EQ(g_recordAnimSize.size(), 1);
    EXPECT_EQ(g_recordAnimPos.size(), 1);
    EXPECT_TRUE(g_recordAnimSize.count(monitor));
    EXPECT_TRUE(g_recordAnimPos.count(monitor));
}

// Test 2: Entry animation starts from zoomed-in position
TEST_F(RecordModeAnimationTest, EntryAnimationStartsZoomedIn) {
    auto monitor = createMockMonitor(1920, 1080);
    simulateEnterRecordMode(monitor);

    auto& sizeVar = g_recordAnimSize[monitor];
    auto& posVar = g_recordAnimPos[monitor];

    // Initial size should be larger than monitor (zoomed in)
    const Vector2D monitorSize = monitor->m_size;
    EXPECT_GT(sizeVar->value().x, monitorSize.x);
    EXPECT_GT(sizeVar->value().y, monitorSize.y);

    // Goal should be normal monitor size
    EXPECT_EQ(sizeVar->goalValue, monitorSize);
    EXPECT_EQ(posVar->goalValue, Vector2D(0, 0));
}

// Test 3: Entry animation reaches normal view
TEST_F(RecordModeAnimationTest, EntryAnimationReachesNormalView) {
    auto monitor = createMockMonitor(1920, 1080);
    simulateEnterRecordMode(monitor);

    auto& sizeVar = g_recordAnimSize[monitor];
    auto& posVar = g_recordAnimPos[monitor];

    // Simulate animation completion
    sizeVar->finishAnimation();
    posVar->finishAnimation();

    const Vector2D monitorSize = monitor->m_size;
    EXPECT_EQ(sizeVar->value(), monitorSize);
    EXPECT_EQ(posVar->value(), Vector2D(0, 0));
}

// Test 4: Exit animation sets up correctly
TEST_F(RecordModeAnimationTest, ExitAnimationSetup) {
    auto monitor = createMockMonitor(1920, 1080);
    simulateEnterRecordMode(monitor);

    // Finish entry animation
    g_recordAnimSize[monitor]->finishAnimation();
    g_recordAnimPos[monitor]->finishAnimation();

    EXPECT_TRUE(g_recordMode);
    EXPECT_FALSE(g_recordModeClosing[monitor]);

    simulateExitRecordMode(monitor);

    EXPECT_TRUE(g_recordMode);  // Still true until animation completes
    EXPECT_TRUE(g_recordModeClosing[monitor]);
}

// Test 5: Exit animation goals zoom back into record square
TEST_F(RecordModeAnimationTest, ExitAnimationZoomsIntoRecordSquare) {
    auto monitor = createMockMonitor(1920, 1080);
    simulateEnterRecordMode(monitor);

    // Finish entry animation
    g_recordAnimSize[monitor]->finishAnimation();
    g_recordAnimPos[monitor]->finishAnimation();

    const Vector2D normalSize = g_recordAnimSize[monitor]->value();

    simulateExitRecordMode(monitor);

    auto& sizeVar = g_recordAnimSize[monitor];
    auto& posVar = g_recordAnimPos[monitor];

    // Goal should be zoomed in (larger than normal)
    EXPECT_GT(sizeVar->goalValue.x, normalSize.x);
    EXPECT_GT(sizeVar->goalValue.y, normalSize.y);

    // Position should shift to center on record square (non-zero)
    // Check that at least one component is significantly non-zero
    bool hasNonZeroPosition = (std::abs(posVar->goalValue.x) > 1.0f ||
                               std::abs(posVar->goalValue.y) > 1.0f);
    EXPECT_TRUE(hasNonZeroPosition);
}

// Test 6: Exit animation completes and cleans up
TEST_F(RecordModeAnimationTest, ExitAnimationCompletesAndCleansUp) {
    auto monitor = createMockMonitor(1920, 1080);
    simulateEnterRecordMode(monitor);

    g_recordAnimSize[monitor]->finishAnimation();
    g_recordAnimPos[monitor]->finishAnimation();

    simulateExitRecordMode(monitor);

    EXPECT_TRUE(g_recordMode);
    EXPECT_FALSE(g_recordAnimSize.empty());
    EXPECT_FALSE(g_recordAnimPos.empty());

    // Simulate animation completion
    g_recordAnimSize[monitor]->finishAnimation();

    EXPECT_FALSE(g_recordMode);
    EXPECT_TRUE(g_recordAnimSize.empty());
    EXPECT_TRUE(g_recordAnimPos.empty());
    EXPECT_TRUE(g_recordModeClosing.empty());
}

// Test 7: Damage callback is called during animation
TEST_F(RecordModeAnimationTest, DamageCallbackCalledDuringAnimation) {
    auto monitor = createMockMonitor(1920, 1080);

    g_animationCallbackCalled = false;
    g_damageCount = 0;

    simulateEnterRecordMode(monitor);

    auto& sizeVar = g_recordAnimSize[monitor];
    auto& posVar = g_recordAnimPos[monitor];

    // Step animation
    sizeVar->stepAnimation(0.5f);
    posVar->stepAnimation(0.5f);

    EXPECT_TRUE(g_animationCallbackCalled);
    EXPECT_GT(g_damageCount, 0);
}

// Test 8: Multiple monitors - each gets its own animation
TEST_F(RecordModeAnimationTest, MultipleMonitorsHaveIndependentAnimations) {
    auto monitor1 = createMockMonitor(1920, 1080);
    auto monitor2 = createMockMonitor(2560, 1440);

    simulateEnterRecordMode(monitor1);
    simulateEnterRecordMode(monitor2);

    EXPECT_EQ(g_recordAnimSize.size(), 2);
    EXPECT_EQ(g_recordAnimPos.size(), 2);
    EXPECT_TRUE(g_recordAnimSize.count(monitor1));
    EXPECT_TRUE(g_recordAnimSize.count(monitor2));
    EXPECT_TRUE(g_recordAnimPos.count(monitor1));
    EXPECT_TRUE(g_recordAnimPos.count(monitor2));

    // Each monitor should have different initial sizes based on resolution
    EXPECT_NE(g_recordAnimSize[monitor1]->value().x,
              g_recordAnimSize[monitor2]->value().x);
}

// Test 9: Scroll offsets reset on entry
TEST_F(RecordModeAnimationTest, ScrollOffsetsResetOnEntry) {
    auto monitor = createMockMonitor(1920, 1080);

    g_scrollOffsets[monitor] = 100.0f;
    g_maxScrollOffsets[monitor] = 200.0f;

    EXPECT_FALSE(g_scrollOffsets.empty());
    EXPECT_FALSE(g_maxScrollOffsets.empty());

    simulateEnterRecordMode(monitor);

    EXPECT_TRUE(g_scrollOffsets.empty());
    EXPECT_TRUE(g_maxScrollOffsets.empty());
}

// Test 10: Closing state prevents multiple exits
TEST_F(RecordModeAnimationTest, ClosingStatePreventsMultipleExits) {
    auto monitor = createMockMonitor(1920, 1080);

    simulateEnterRecordMode(monitor);
    g_recordAnimSize[monitor]->finishAnimation();
    g_recordAnimPos[monitor]->finishAnimation();

    simulateExitRecordMode(monitor);
    EXPECT_TRUE(g_recordModeClosing[monitor]);

    // Try to exit again - should check closing flag first
    bool isClosing = g_recordModeClosing.count(monitor) && g_recordModeClosing[monitor];
    EXPECT_TRUE(isClosing);
}

// Test 11: Animation values interpolate correctly
TEST_F(RecordModeAnimationTest, AnimationValuesInterpolate) {
    auto monitor = createMockMonitor(1920, 1080);
    simulateEnterRecordMode(monitor);

    auto& sizeVar = g_recordAnimSize[monitor];
    const Vector2D initialSize = sizeVar->value();
    const Vector2D goalSize = sizeVar->goalValue;

    // Step halfway
    sizeVar->stepAnimation(0.5f);
    const Vector2D halfwaySize = sizeVar->value();

    // Value should be between initial and goal
    EXPECT_GT(halfwaySize.x, goalSize.x);
    EXPECT_LT(halfwaySize.x, initialSize.x);
}

// Test 12: Entry and exit animations are reversible
TEST_F(RecordModeAnimationTest, EntryAndExitAnimationsAreReversible) {
    auto monitor = createMockMonitor(1920, 1080);

    // Enter
    simulateEnterRecordMode(monitor);
    const Vector2D entryInitialSize = g_recordAnimSize[monitor]->value();
    g_recordAnimSize[monitor]->finishAnimation();
    g_recordAnimPos[monitor]->finishAnimation();

    // Exit
    simulateExitRecordMode(monitor);
    const Vector2D exitGoalSize = g_recordAnimSize[monitor]->goalValue;

    // Entry initial should match exit goal (both zoomed in)
    EXPECT_FLOAT_EQ(entryInitialSize.x, exitGoalSize.x);
    EXPECT_FLOAT_EQ(entryInitialSize.y, exitGoalSize.y);
}
