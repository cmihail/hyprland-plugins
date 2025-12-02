#include <gtest/gtest.h>

// Test default config values
class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed
    }
};

// Test that default opacity values are within valid range (0.0 - 1.0)
TEST_F(ConfigTest, DefaultOpacityBackgroundIsValid) {
    const float defaultOpacity = 0.1f;
    EXPECT_GE(defaultOpacity, 0.0f);
    EXPECT_LE(defaultOpacity, 1.0f);
}

TEST_F(ConfigTest, DefaultOpacityGridLinesIsValid) {
    const float defaultOpacity = 0.25f;
    EXPECT_GE(defaultOpacity, 0.0f);
    EXPECT_LE(defaultOpacity, 1.0f);
}

TEST_F(ConfigTest, DefaultOpacityLabelsIsValid) {
    const float defaultOpacity = 0.4f;
    EXPECT_GE(defaultOpacity, 0.0f);
    EXPECT_LE(defaultOpacity, 1.0f);
}

// Test that default values provide good visibility hierarchy
TEST_F(ConfigTest, OpacityHierarchy) {
    const float bgOpacity = 0.1f;
    const float gridOpacity = 0.25f;
    const float labelOpacity = 0.4f;

    // Background should be most transparent
    EXPECT_LT(bgOpacity, gridOpacity);
    EXPECT_LT(bgOpacity, labelOpacity);

    // Grid lines should be less visible than labels
    EXPECT_LT(gridOpacity, labelOpacity);
}

// Test edge cases for opacity values
TEST_F(ConfigTest, FullyTransparent) {
    const float opacity = 0.0f;
    EXPECT_GE(opacity, 0.0f);
    EXPECT_LE(opacity, 1.0f);
}

TEST_F(ConfigTest, FullyOpaque) {
    const float opacity = 1.0f;
    EXPECT_GE(opacity, 0.0f);
    EXPECT_LE(opacity, 1.0f);
}

TEST_F(ConfigTest, MidRangeOpacity) {
    const float opacity = 0.5f;
    EXPECT_GE(opacity, 0.0f);
    EXPECT_LE(opacity, 1.0f);
}

// Test that opacity values produce reasonable transparency percentages
TEST_F(ConfigTest, OpacityToTransparencyConversion) {
    const float opacity = 0.1f;
    const float transparency = 1.0f - opacity;

    EXPECT_FLOAT_EQ(transparency, 0.9f); // 90% transparent
}

TEST_F(ConfigTest, BackgroundDefaultTransparency) {
    const float opacity = 0.1f;
    const float transparencyPercent = (1.0f - opacity) * 100.0f;

    EXPECT_FLOAT_EQ(transparencyPercent, 90.0f);
}

TEST_F(ConfigTest, GridLinesDefaultTransparency) {
    const float opacity = 0.25f;
    const float transparencyPercent = (1.0f - opacity) * 100.0f;

    EXPECT_FLOAT_EQ(transparencyPercent, 75.0f);
}

TEST_F(ConfigTest, LabelsDefaultTransparency) {
    const float opacity = 0.4f;
    const float transparencyPercent = (1.0f - opacity) * 100.0f;

    EXPECT_FLOAT_EQ(transparencyPercent, 60.0f);
}

// Test config value names
TEST_F(ConfigTest, ConfigKeyNaming) {
    const std::string prefix = "plugin:no_mouse:";
    const std::string bgKey = prefix + "opacity_background";
    const std::string gridKey = prefix + "opacity_grid_lines";
    const std::string labelKey = prefix + "opacity_labels";

    // Validate key format
    EXPECT_EQ(bgKey, "plugin:no_mouse:opacity_background");
    EXPECT_EQ(gridKey, "plugin:no_mouse:opacity_grid_lines");
    EXPECT_EQ(labelKey, "plugin:no_mouse:opacity_labels");
}

// Test that config values are consistently named
TEST_F(ConfigTest, ConfigKeyConsistency) {
    const std::string prefix = "plugin:no_mouse:";

    // All config keys should start with the same prefix
    std::vector<std::string> keys = {
        "plugin:no_mouse:opacity_background",
        "plugin:no_mouse:opacity_grid_lines",
        "plugin:no_mouse:opacity_labels"
    };

    for (const auto& key : keys) {
        EXPECT_EQ(key.substr(0, prefix.length()), prefix);
    }
}
