#include <gtest/gtest.h>
#include <chrono>
#include <unordered_map>

class StandaloneTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

struct MockVector2D {
    double x = 0;
    double y = 0;
};

struct MockBox {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
};

struct MockMonitor {
    MockVector2D position;
    MockVector2D size;
};

struct MockCopyIndicator {
    MockVector2D position;
    std::chrono::steady_clock::time_point startTime;
    bool active = false;
    float opacity = 0.0f;
};

// Mirrors the plugin logic with an injectable clock so timing can be tested
class CopyIndicatorLogic {
public:
    static constexpr int ICON_WIDTH = 100;
    static constexpr int ICON_HEIGHT = 50;
    static constexpr int DISPLAY_DURATION_MS = 500;

    enum class RenderAction { NONE, DRAW, CLEAR };

    std::unordered_map<int, MockCopyIndicator> indicators;
    int damageCount = 0;
    int scheduledFrames = 0;
    bool shuttingDown = false;

    void activate(int monitorId, MockVector2D pos, std::chrono::steady_clock::time_point now) {
        auto& indicator = indicators[monitorId];
        indicator.position = pos;
        indicator.active = true;
        indicator.startTime = now;
        indicator.opacity = 1.0f;
        damageCount++;
        scheduledFrames++;
    }

    static void update(MockCopyIndicator& indicator, std::chrono::steady_clock::time_point now) {
        if (!indicator.active)
            return;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - indicator.startTime).count();

        if (elapsed < DISPLAY_DURATION_MS) {
            indicator.opacity = 1.0f;
        } else {
            indicator.active = false;
            indicator.opacity = 0.0f;
        }
    }

    RenderAction handleRender(MockCopyIndicator& indicator,
                              std::chrono::steady_clock::time_point now) {
        bool wasActive = indicator.active;
        update(indicator, now);

        if (indicator.active) {
            scheduledFrames++;
            return RenderAction::DRAW;
        } else if (wasActive) {
            damageCount++;
            scheduledFrames++;
            return RenderAction::CLEAR;
        }
        return RenderAction::NONE;
    }

    RenderAction onRender(int monitorId, std::chrono::steady_clock::time_point now) {
        if (shuttingDown)
            return RenderAction::NONE;

        auto it = indicators.find(monitorId);
        if (it == indicators.end() || !it->second.active)
            return RenderAction::NONE;

        return handleRender(it->second, now);
    }

    static bool shouldDraw(const MockCopyIndicator& indicator) {
        return indicator.active && indicator.opacity > 0.01f;
    }

    static MockBox iconBox(const MockCopyIndicator& indicator, const MockMonitor& monitor) {
        return {
            indicator.position.x - monitor.position.x - ICON_WIDTH / 2.0,
            indicator.position.y - monitor.position.y - ICON_HEIGHT / 2.0,
            ICON_WIDTH,
            ICON_HEIGHT
        };
    }

    static MockBox boundingBox(const MockMonitor* monitor) {
        if (!monitor)
            return {0, 0, 0, 0};
        return {0, 0, monitor->size.x, monitor->size.y};
    }

    void shutdown() {
        shuttingDown = true;
        indicators.clear();
    }
};

using namespace std::chrono_literals;

TEST_F(StandaloneTest, IconConstants) {
    EXPECT_EQ(CopyIndicatorLogic::ICON_WIDTH, 100);
    EXPECT_EQ(CopyIndicatorLogic::ICON_HEIGHT, 50);
    EXPECT_EQ(CopyIndicatorLogic::DISPLAY_DURATION_MS, 500);
}

TEST_F(StandaloneTest, DefaultIndicatorIsInactive) {
    MockCopyIndicator indicator;
    EXPECT_FALSE(indicator.active);
    EXPECT_EQ(indicator.opacity, 0.0f);
    EXPECT_FALSE(CopyIndicatorLogic::shouldDraw(indicator));
}

TEST_F(StandaloneTest, ActivateSetsState) {
    CopyIndicatorLogic logic;
    auto now = std::chrono::steady_clock::now();

    logic.activate(0, {300, 200}, now);

    auto& indicator = logic.indicators[0];
    EXPECT_TRUE(indicator.active);
    EXPECT_EQ(indicator.opacity, 1.0f);
    EXPECT_EQ(indicator.position.x, 300);
    EXPECT_EQ(indicator.position.y, 200);
    EXPECT_EQ(indicator.startTime, now);
    EXPECT_EQ(logic.damageCount, 1);
    EXPECT_EQ(logic.scheduledFrames, 1);
}

TEST_F(StandaloneTest, ReactivateRestartsTimerAndMovesIndicator) {
    CopyIndicatorLogic logic;
    auto start = std::chrono::steady_clock::now();

    logic.activate(0, {100, 100}, start);
    logic.activate(0, {400, 250}, start + 400ms);

    auto& indicator = logic.indicators[0];
    EXPECT_EQ(indicator.position.x, 400);
    EXPECT_EQ(indicator.position.y, 250);

    // 800ms after the first activation but only 400ms after the second
    CopyIndicatorLogic::update(indicator, start + 800ms);
    EXPECT_TRUE(indicator.active);
}

TEST_F(StandaloneTest, StaysVisibleBeforeTimeout) {
    MockCopyIndicator indicator;
    auto start = std::chrono::steady_clock::now();
    indicator.active = true;
    indicator.startTime = start;

    for (auto elapsed : {0ms, 100ms, 250ms, 499ms}) {
        CopyIndicatorLogic::update(indicator, start + elapsed);
        EXPECT_TRUE(indicator.active) << "elapsed=" << elapsed.count();
        EXPECT_EQ(indicator.opacity, 1.0f) << "elapsed=" << elapsed.count();
    }
}

TEST_F(StandaloneTest, HidesAtTimeout) {
    MockCopyIndicator indicator;
    auto start = std::chrono::steady_clock::now();
    indicator.active = true;
    indicator.opacity = 1.0f;
    indicator.startTime = start;

    CopyIndicatorLogic::update(indicator, start + 500ms);
    EXPECT_FALSE(indicator.active);
    EXPECT_EQ(indicator.opacity, 0.0f);
}

TEST_F(StandaloneTest, UpdateIgnoresInactiveIndicator) {
    MockCopyIndicator indicator;
    indicator.opacity = 0.5f;

    CopyIndicatorLogic::update(indicator, std::chrono::steady_clock::now());
    EXPECT_FALSE(indicator.active);
    EXPECT_EQ(indicator.opacity, 0.5f);
}

TEST_F(StandaloneTest, ShouldDrawRequiresVisibleOpacity) {
    MockCopyIndicator indicator;
    indicator.active = true;

    indicator.opacity = 1.0f;
    EXPECT_TRUE(CopyIndicatorLogic::shouldDraw(indicator));

    indicator.opacity = 0.01f;
    EXPECT_FALSE(CopyIndicatorLogic::shouldDraw(indicator));

    indicator.opacity = 0.02f;
    EXPECT_TRUE(CopyIndicatorLogic::shouldDraw(indicator));

    indicator.active = false;
    EXPECT_FALSE(CopyIndicatorLogic::shouldDraw(indicator));
}

TEST_F(StandaloneTest, RenderDrawsWhileActive) {
    CopyIndicatorLogic logic;
    auto start = std::chrono::steady_clock::now();
    logic.activate(0, {100, 100}, start);

    EXPECT_EQ(logic.onRender(0, start + 100ms), CopyIndicatorLogic::RenderAction::DRAW);
    EXPECT_EQ(logic.scheduledFrames, 2);
}

TEST_F(StandaloneTest, RenderClearsOnceAfterTimeout) {
    CopyIndicatorLogic logic;
    auto start = std::chrono::steady_clock::now();
    logic.activate(0, {100, 100}, start);

    EXPECT_EQ(logic.onRender(0, start + 600ms), CopyIndicatorLogic::RenderAction::CLEAR);
    EXPECT_EQ(logic.damageCount, 2);
    EXPECT_EQ(logic.scheduledFrames, 2);

    // Later frames do nothing once the indicator is gone
    EXPECT_EQ(logic.onRender(0, start + 700ms), CopyIndicatorLogic::RenderAction::NONE);
    EXPECT_EQ(logic.damageCount, 2);
    EXPECT_EQ(logic.scheduledFrames, 2);
}

TEST_F(StandaloneTest, RenderIgnoresUnknownMonitor) {
    CopyIndicatorLogic logic;
    auto start = std::chrono::steady_clock::now();
    logic.activate(0, {100, 100}, start);

    EXPECT_EQ(logic.onRender(1, start), CopyIndicatorLogic::RenderAction::NONE);
    EXPECT_EQ(logic.indicators.count(1), 0u);
}

TEST_F(StandaloneTest, IndicatorsArePerMonitor) {
    CopyIndicatorLogic logic;
    auto start = std::chrono::steady_clock::now();
    logic.activate(0, {100, 100}, start);
    logic.activate(1, {2000, 100}, start + 300ms);

    // Monitor 0 expired, monitor 1 still within its window
    EXPECT_EQ(logic.onRender(0, start + 600ms), CopyIndicatorLogic::RenderAction::CLEAR);
    EXPECT_EQ(logic.onRender(1, start + 600ms), CopyIndicatorLogic::RenderAction::DRAW);
}

TEST_F(StandaloneTest, IconBoxCenteredOnCursor) {
    MockMonitor monitor{{0, 0}, {1920, 1080}};
    MockCopyIndicator indicator;
    indicator.position = {500, 300};

    auto box = CopyIndicatorLogic::iconBox(indicator, monitor);
    EXPECT_DOUBLE_EQ(box.x, 450);
    EXPECT_DOUBLE_EQ(box.y, 275);
    EXPECT_DOUBLE_EQ(box.w, 100);
    EXPECT_DOUBLE_EQ(box.h, 50);
}

TEST_F(StandaloneTest, IconBoxUsesMonitorLocalCoordinates) {
    MockMonitor monitor{{1920, 200}, {2560, 1440}};
    MockCopyIndicator indicator;
    indicator.position = {2000, 500};

    auto box = CopyIndicatorLogic::iconBox(indicator, monitor);
    EXPECT_DOUBLE_EQ(box.x, 30);
    EXPECT_DOUBLE_EQ(box.y, 275);
}

TEST_F(StandaloneTest, IconBoxAtMonitorOrigin) {
    MockMonitor monitor{{1920, 0}, {1920, 1080}};
    MockCopyIndicator indicator;
    indicator.position = {1920, 0};

    auto box = CopyIndicatorLogic::iconBox(indicator, monitor);
    EXPECT_DOUBLE_EQ(box.x, -50);
    EXPECT_DOUBLE_EQ(box.y, -25);
}

TEST_F(StandaloneTest, BoundingBoxCoversMonitor) {
    MockMonitor monitor{{1920, 0}, {2560, 1440}};

    auto box = CopyIndicatorLogic::boundingBox(&monitor);
    EXPECT_DOUBLE_EQ(box.x, 0);
    EXPECT_DOUBLE_EQ(box.y, 0);
    EXPECT_DOUBLE_EQ(box.w, 2560);
    EXPECT_DOUBLE_EQ(box.h, 1440);
}

TEST_F(StandaloneTest, BoundingBoxWithoutMonitorIsEmpty) {
    auto box = CopyIndicatorLogic::boundingBox(nullptr);
    EXPECT_DOUBLE_EQ(box.w, 0);
    EXPECT_DOUBLE_EQ(box.h, 0);
}

TEST_F(StandaloneTest, ShutdownStopsRendering) {
    CopyIndicatorLogic logic;
    auto start = std::chrono::steady_clock::now();
    logic.activate(0, {100, 100}, start);

    logic.shutdown();
    EXPECT_TRUE(logic.indicators.empty());
    EXPECT_EQ(logic.onRender(0, start + 100ms), CopyIndicatorLogic::RenderAction::NONE);
}
