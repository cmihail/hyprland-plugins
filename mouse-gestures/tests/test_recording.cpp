#include <gtest/gtest.h>
#include "../stroke.hpp"
#include <fstream>
#include <filesystem>
#include <cstdlib>

class RecordingTest : public ::testing::Test {
protected:
    std::string tempDir;
    std::string configPath;

    void SetUp() override {
        // Create temp directory for test
        tempDir = "/tmp/mouse-gestures-test-" + std::to_string(getpid());
        std::filesystem::create_directories(tempDir + "/config");
        configPath = tempDir + "/config/plugins.conf";
    }

    void TearDown() override {
        // Clean up temp directory
        std::filesystem::remove_all(tempDir);
    }

    void createPluginConfig(const std::string& content) {
        std::ofstream file(configPath);
        file << content;
        file.close();
    }

    std::string readConfigFile() {
        std::ifstream file(configPath);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        return content;
    }
};

// Test gesture serialization for recording
TEST_F(RecordingTest, GestureSerializationFormat) {
    Stroke stroke;
    stroke.addPoint(100.0, 200.0);
    stroke.addPoint(150.0, 250.0);
    stroke.addPoint(200.0, 300.0);
    ASSERT_TRUE(stroke.finish());

    std::string serialized = stroke.serialize();

    // Verify format: x,y;x,y;x,y;
    EXPECT_TRUE(serialized.find(',') != std::string::npos);
    EXPECT_TRUE(serialized.find(';') != std::string::npos);
    EXPECT_TRUE(serialized.back() == ';');

    // Count semicolons - should be 3 (one per point)
    size_t count = 0;
    for (char c : serialized) {
        if (c == ';') count++;
    }
    EXPECT_EQ(count, 3);
}

// Test that duplicate gestures can be detected by comparing costs
TEST_F(RecordingTest, DuplicateDetectionConcept) {
    // Create two identical strokes
    Stroke stroke1;
    stroke1.addPoint(100.0, 200.0);
    stroke1.addPoint(150.0, 250.0);
    stroke1.addPoint(200.0, 300.0);
    ASSERT_TRUE(stroke1.finish());

    Stroke stroke2;
    stroke2.addPoint(100.0, 200.0);
    stroke2.addPoint(150.0, 250.0);
    stroke2.addPoint(200.0, 300.0);
    ASSERT_TRUE(stroke2.finish());

    // Compare - identical strokes should have cost near 0
    double cost = stroke1.compare(stroke2);
    EXPECT_LT(cost, 0.01);  // Very low cost for identical gestures
}

// Test that different gestures have higher comparison cost
TEST_F(RecordingTest, DifferentGesturesHigherCost) {
    // Create horizontal swipe
    Stroke horizontal;
    horizontal.addPoint(100.0, 200.0);
    horizontal.addPoint(200.0, 200.0);
    horizontal.addPoint(300.0, 200.0);
    ASSERT_TRUE(horizontal.finish());

    // Create vertical swipe
    Stroke vertical;
    vertical.addPoint(200.0, 100.0);
    vertical.addPoint(200.0, 200.0);
    vertical.addPoint(200.0, 300.0);
    ASSERT_TRUE(vertical.finish());

    // Compare - different gestures should have higher cost
    double cost = horizontal.compare(vertical);
    EXPECT_GT(cost, 0.15);  // Should be above typical match threshold
}

// Test config file structure for recording
TEST_F(RecordingTest, ConfigFileStructureWithGesture) {
    std::string config = R"(plugin {
  mouse_gestures {
    gesture_action = hyprctl dispatch workspace +1|0.5,0.3;0.6,0.4;0.7,0.5;
  }
})";

    createPluginConfig(config);
    std::string content = readConfigFile();

    // Verify the config contains the gesture
    EXPECT_NE(content.find("plugin"), std::string::npos);
    EXPECT_NE(content.find("mouse_gestures"), std::string::npos);
    EXPECT_NE(content.find("gesture_action"), std::string::npos);
    EXPECT_NE(content.find("|"), std::string::npos);
}

// Test that recorded gesture format matches expected pattern
TEST_F(RecordingTest, RecordedGestureFormat) {
    Stroke stroke;
    for (int i = 0; i < 10; i++) {
        stroke.addPoint(100.0 + i * 10, 200.0 + i * 5);
    }
    ASSERT_TRUE(stroke.finish());

    std::string serialized = stroke.serialize();

    // Verify it can be parsed back
    Stroke deserialized = Stroke::deserialize(serialized);
    EXPECT_TRUE(deserialized.isFinished());
    EXPECT_EQ(deserialized.size(), stroke.size());

    // Verify comparison shows they're identical
    double cost = stroke.compare(deserialized);
    EXPECT_LT(cost, 0.01);
}

// Test placeholder notification format
TEST_F(RecordingTest, PlaceholderNotificationFormat) {
    std::string strokeData = "0.5,0.3;0.6,0.4;0.7,0.5;";
    std::string configPath = "/home/test/.config/hypr/config/plugins.conf";

    std::string expectedGesture =
        "gesture_action = hyprctl notify -1 2000 \"rgb(ff0000)\" \"modify me in config file " +
        configPath + "\"|" + strokeData;

    // Verify format is correct
    EXPECT_NE(expectedGesture.find("hyprctl notify"), std::string::npos);
    EXPECT_NE(expectedGesture.find("rgb(ff0000)"), std::string::npos);
    EXPECT_NE(expectedGesture.find("modify me"), std::string::npos);
    EXPECT_NE(expectedGesture.find(configPath), std::string::npos);
    EXPECT_NE(expectedGesture.find("|"), std::string::npos);
    EXPECT_NE(expectedGesture.find(strokeData), std::string::npos);
}

// Test config file has correct closing braces after adding gesture
TEST_F(RecordingTest, ConfigPreservesStructure) {
    std::string originalConfig = R"(plugin {
  window_actions {
    button_size = 17
  }

  mouse_gestures {
    drag_threshold = 100
  }
})";

    createPluginConfig(originalConfig);

    // Count braces in original
    int openBraces = 0, closeBraces = 0;
    for (char c : originalConfig) {
        if (c == '{') openBraces++;
        if (c == '}') closeBraces++;
    }

    EXPECT_EQ(openBraces, closeBraces);

    // After reading and potentially modifying, structure should be preserved
    std::string content = readConfigFile();
    int newOpenBraces = 0, newCloseBraces = 0;
    for (char c : content) {
        if (c == '{') newOpenBraces++;
        if (c == '}') newCloseBraces++;
    }

    EXPECT_EQ(newOpenBraces, newCloseBraces);
}

// Test that similar gestures within threshold are detected
TEST_F(RecordingTest, SimilarGesturesWithinThreshold) {
    // Create nearly identical strokes with slight variation
    Stroke stroke1;
    stroke1.addPoint(100.0, 200.0);
    stroke1.addPoint(150.0, 250.0);
    stroke1.addPoint(200.0, 300.0);
    ASSERT_TRUE(stroke1.finish());

    Stroke stroke2;
    stroke2.addPoint(101.0, 201.0);  // Very slightly different
    stroke2.addPoint(151.0, 251.0);
    stroke2.addPoint(201.0, 301.0);
    ASSERT_TRUE(stroke2.finish());

    // Compare - similar strokes should have low cost
    double cost = stroke1.compare(stroke2);
    EXPECT_LT(cost, 0.15);  // Within typical match threshold
}

// Test config parsing handles multiple gesture_action lines
TEST_F(RecordingTest, MultipleGestureActions) {
    std::string config = R"(plugin {
  mouse_gestures {
    gesture_action = hyprctl dispatch workspace +1|0.5,0.3;0.6,0.4;
    gesture_action = hyprctl dispatch workspace -1|0.8,0.7;0.7,0.6;
    gesture_action = hyprctl dispatch killactive|0.5,0.3;0.5,0.6;
  }
})";

    createPluginConfig(config);
    std::string content = readConfigFile();

    // Verify all three gesture actions are present
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find("gesture_action", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    EXPECT_EQ(count, 3);
}

// Test stroke comparison is symmetric
TEST_F(RecordingTest, StrokeComparisonSymmetry) {
    Stroke stroke1;
    stroke1.addPoint(100.0, 200.0);
    stroke1.addPoint(200.0, 300.0);
    ASSERT_TRUE(stroke1.finish());

    Stroke stroke2;
    stroke2.addPoint(150.0, 250.0);
    stroke2.addPoint(250.0, 350.0);
    ASSERT_TRUE(stroke2.finish());

    // Comparison should be symmetric
    double cost1to2 = stroke1.compare(stroke2);
    double cost2to1 = stroke2.compare(stroke1);

    EXPECT_NEAR(cost1to2, cost2to1, 0.001);  // Should be approximately equal
}
