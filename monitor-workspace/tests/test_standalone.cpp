#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>

// Standalone implementations of plugin functions for testing

static std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

struct MonitorLayout {
    std::map<std::string, std::vector<int>> monitorWorkspaces;
};

static bool parseLayoutConfig(const std::string& configValue, MonitorLayout& layout) {
    // Format: m1:1,3,5,7,9;m2:2,4,6,8,10
    auto monitorEntries = splitString(configValue, ';');

    for (const auto& entry : monitorEntries) {
        auto parts = splitString(entry, ':');
        if (parts.size() != 2) {
            return false;
        }

        std::string monitorName = parts[0];
        auto workspaceStrs = splitString(parts[1], ',');

        std::vector<int> workspaces;
        for (const auto& wsStr : workspaceStrs) {
            try {
                workspaces.push_back(std::stoi(wsStr));
            } catch (...) {
                return false;
            }
        }

        layout.monitorWorkspaces[monitorName] = workspaces;
    }
    return true;
}

struct MockMonitor {
    std::string name;
    double x;
    double y;

    MockMonitor(const std::string& n, double xPos, double yPos)
        : name(n), x(xPos), y(yPos) {}
};

static std::vector<std::string> getSortedMonitorNames(const std::vector<MockMonitor>& monitors) {
    std::vector<std::pair<std::string, std::pair<double, double>>> monitorPositions;

    for (const auto& monitor : monitors) {
        monitorPositions.push_back({
            monitor.name,
            {monitor.x, monitor.y}
        });
    }

    // Sort by x position, then by y if x is equal
    std::sort(monitorPositions.begin(), monitorPositions.end(),
        [](const auto& a, const auto& b) {
            if (a.second.first != b.second.first) {
                return a.second.first < b.second.first;
            }
            return a.second.second < b.second.second;
        });

    std::vector<std::string> sortedNames;
    for (const auto& [name, pos] : monitorPositions) {
        sortedNames.push_back(name);
    }

    return sortedNames;
}

// Test: String splitting
TEST(StringUtilsTest, SplitString) {
    auto result = splitString("a,b,c", ',');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(StringUtilsTest, SplitStringEmpty) {
    auto result = splitString("", ',');
    EXPECT_EQ(result.size(), 0);
}

TEST(StringUtilsTest, SplitStringWithEmptyTokens) {
    auto result = splitString("a,,b", ',');
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
}

// Test: Layout parsing
TEST(LayoutParsingTest, ParseValidLayout) {
    MonitorLayout layout;
    bool success = parseLayoutConfig("m1:1,2,3;m2:4,5,6", layout);

    EXPECT_TRUE(success);
    ASSERT_EQ(layout.monitorWorkspaces.size(), 2);

    auto m1Workspaces = layout.monitorWorkspaces["m1"];
    ASSERT_EQ(m1Workspaces.size(), 3);
    EXPECT_EQ(m1Workspaces[0], 1);
    EXPECT_EQ(m1Workspaces[1], 2);
    EXPECT_EQ(m1Workspaces[2], 3);

    auto m2Workspaces = layout.monitorWorkspaces["m2"];
    ASSERT_EQ(m2Workspaces.size(), 3);
    EXPECT_EQ(m2Workspaces[0], 4);
    EXPECT_EQ(m2Workspaces[1], 5);
    EXPECT_EQ(m2Workspaces[2], 6);
}

TEST(LayoutParsingTest, ParseSingleMonitor) {
    MonitorLayout layout;
    bool success = parseLayoutConfig("m1:1,2,3,4,5,6,7,8,9,10", layout);

    EXPECT_TRUE(success);
    ASSERT_EQ(layout.monitorWorkspaces.size(), 1);

    auto m1Workspaces = layout.monitorWorkspaces["m1"];
    ASSERT_EQ(m1Workspaces.size(), 10);
    EXPECT_EQ(m1Workspaces[0], 1);
    EXPECT_EQ(m1Workspaces[9], 10);
}

TEST(LayoutParsingTest, ParseThreeMonitors) {
    MonitorLayout layout;
    bool success = parseLayoutConfig("m1:7;m2:2,5,6;m3:1,3,4", layout);

    EXPECT_TRUE(success);
    ASSERT_EQ(layout.monitorWorkspaces.size(), 3);

    EXPECT_EQ(layout.monitorWorkspaces["m1"].size(), 1);
    EXPECT_EQ(layout.monitorWorkspaces["m1"][0], 7);

    EXPECT_EQ(layout.monitorWorkspaces["m2"].size(), 3);
    EXPECT_EQ(layout.monitorWorkspaces["m3"].size(), 3);
}

TEST(LayoutParsingTest, ParseInvalidFormat) {
    MonitorLayout layout;
    bool success = parseLayoutConfig("m1:1,2,3;invalid", layout);
    EXPECT_FALSE(success);
}

TEST(LayoutParsingTest, ParseInvalidWorkspaceNumber) {
    MonitorLayout layout;
    bool success = parseLayoutConfig("m1:1,abc,3", layout);
    EXPECT_FALSE(success);
}

// Test: Monitor sorting
TEST(MonitorSortingTest, SortByXPosition) {
    std::vector<MockMonitor> monitors = {
        {"HDMI-A-1", 1920, 0},
        {"eDP-1", 0, 0},
        {"DP-1", 3840, 0}
    };

    auto sorted = getSortedMonitorNames(monitors);

    ASSERT_EQ(sorted.size(), 3);
    EXPECT_EQ(sorted[0], "eDP-1");      // x=0
    EXPECT_EQ(sorted[1], "HDMI-A-1");   // x=1920
    EXPECT_EQ(sorted[2], "DP-1");       // x=3840
}

TEST(MonitorSortingTest, SortByYWhenXEqual) {
    std::vector<MockMonitor> monitors = {
        {"Monitor-C", 0, 1080},
        {"Monitor-A", 0, 0},
        {"Monitor-B", 0, 540}
    };

    auto sorted = getSortedMonitorNames(monitors);

    ASSERT_EQ(sorted.size(), 3);
    EXPECT_EQ(sorted[0], "Monitor-A");  // y=0
    EXPECT_EQ(sorted[1], "Monitor-B");  // y=540
    EXPECT_EQ(sorted[2], "Monitor-C");  // y=1080
}

TEST(MonitorSortingTest, SortMixedPositions) {
    std::vector<MockMonitor> monitors = {
        {"Center", 1920, 0},
        {"TopLeft", 0, 0},
        {"BottomLeft", 0, 1080},
        {"Right", 3840, 500}
    };

    auto sorted = getSortedMonitorNames(monitors);

    ASSERT_EQ(sorted.size(), 4);
    EXPECT_EQ(sorted[0], "TopLeft");     // x=0, y=0
    EXPECT_EQ(sorted[1], "BottomLeft");  // x=0, y=1080
    EXPECT_EQ(sorted[2], "Center");      // x=1920
    EXPECT_EQ(sorted[3], "Right");       // x=3840
}

TEST(MonitorSortingTest, SingleMonitor) {
    std::vector<MockMonitor> monitors = {
        {"eDP-1", 0, 0}
    };

    auto sorted = getSortedMonitorNames(monitors);

    ASSERT_EQ(sorted.size(), 1);
    EXPECT_EQ(sorted[0], "eDP-1");
}

// Test: Monitor mapping
TEST(MonitorMappingTest, MapRelativeToActual) {
    std::vector<MockMonitor> monitors = {
        {"HDMI-A-1", 1920, 0},
        {"eDP-1", 0, 0}
    };

    auto sortedMonitors = getSortedMonitorNames(monitors);
    std::map<std::string, std::string> monitorMapping;

    for (size_t i = 0; i < sortedMonitors.size(); i++) {
        std::string relativeMonitorName = "m" + std::to_string(i + 1);
        monitorMapping[relativeMonitorName] = sortedMonitors[i];
    }

    ASSERT_EQ(monitorMapping.size(), 2);
    EXPECT_EQ(monitorMapping["m1"], "eDP-1");
    EXPECT_EQ(monitorMapping["m2"], "HDMI-A-1");
}

TEST(MonitorMappingTest, ThreeMonitorMapping) {
    std::vector<MockMonitor> monitors = {
        {"Monitor-3", 3840, 0},
        {"Monitor-1", 0, 0},
        {"Monitor-2", 1920, 0}
    };

    auto sortedMonitors = getSortedMonitorNames(monitors);
    std::map<std::string, std::string> monitorMapping;

    for (size_t i = 0; i < sortedMonitors.size(); i++) {
        std::string relativeMonitorName = "m" + std::to_string(i + 1);
        monitorMapping[relativeMonitorName] = sortedMonitors[i];
    }

    ASSERT_EQ(monitorMapping.size(), 3);
    EXPECT_EQ(monitorMapping["m1"], "Monitor-1");
    EXPECT_EQ(monitorMapping["m2"], "Monitor-2");
    EXPECT_EQ(monitorMapping["m3"], "Monitor-3");
}

// Integration test: Full workflow
TEST(IntegrationTest, FullLayoutApplication) {
    // Parse layout
    MonitorLayout layout;
    bool success = parseLayoutConfig("m1:1,2,3;m2:4,5,6", layout);
    EXPECT_TRUE(success);

    // Sort monitors
    std::vector<MockMonitor> monitors = {
        {"HDMI-A-1", 1920, 0},
        {"eDP-1", 0, 0}
    };
    auto sortedMonitors = getSortedMonitorNames(monitors);

    // Create mapping
    std::map<std::string, std::string> monitorMapping;
    for (size_t i = 0; i < sortedMonitors.size(); i++) {
        std::string relativeMonitorName = "m" + std::to_string(i + 1);
        monitorMapping[relativeMonitorName] = sortedMonitors[i];
    }

    // Verify layout application
    EXPECT_EQ(monitorMapping["m1"], "eDP-1");
    EXPECT_EQ(monitorMapping["m2"], "HDMI-A-1");

    auto m1Workspaces = layout.monitorWorkspaces["m1"];
    ASSERT_EQ(m1Workspaces.size(), 3);
    EXPECT_EQ(m1Workspaces[0], 1);
    EXPECT_EQ(m1Workspaces[1], 2);
    EXPECT_EQ(m1Workspaces[2], 3);

    auto m2Workspaces = layout.monitorWorkspaces["m2"];
    ASSERT_EQ(m2Workspaces.size(), 3);
    EXPECT_EQ(m2Workspaces[0], 4);
    EXPECT_EQ(m2Workspaces[1], 5);
    EXPECT_EQ(m2Workspaces[2], 6);
}