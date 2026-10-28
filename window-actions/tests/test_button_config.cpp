#include <gtest/gtest.h>
#include "ButtonConfig.hpp"

// The same string is accepted by the legacy window_actions_button keyword and
// by hl.plugin.window_actions.add_button under the Lua config.

TEST(ButtonConfigTest, ParsesFiveFieldsWithTrailingComma) {
    auto fields = splitButtonConfig("rgba(e6e6e6ff), rgba(dc322fdd), ⨯, ⨯, hyprctl dispatch killactive,");
    ASSERT_TRUE(fields.has_value());
    EXPECT_EQ(fields->textColor, "rgba(e6e6e6ff)");
    EXPECT_EQ(fields->bgColor, "rgba(dc322fdd)");
    EXPECT_EQ(fields->iconInactive, "⨯");
    EXPECT_EQ(fields->iconActive, "⨯");
    EXPECT_EQ(fields->command, "hyprctl dispatch killactive");
    EXPECT_EQ(fields->condition, "");
}

TEST(ButtonConfigTest, ParsesCondition) {
    auto fields = splitButtonConfig("rgba(e6e6e6ff), rgba(268bd2dd), ⬈, ⬋, hyprctl dispatch fullscreen 1, fullscreen");
    ASSERT_TRUE(fields.has_value());
    EXPECT_EQ(fields->command, "hyprctl dispatch fullscreen 1");
    EXPECT_EQ(fields->condition, "fullscreen");
}

TEST(ButtonConfigTest, ParsesInternalMoveCommand) {
    auto fields = splitButtonConfig("rgba(e6e6e6ff), rgba(cb4b16dd), ✥, ⊹, __movewindow__");
    ASSERT_TRUE(fields.has_value());
    EXPECT_EQ(fields->iconInactive, "✥");
    EXPECT_EQ(fields->iconActive, "⊹");
    EXPECT_EQ(fields->command, "__movewindow__");
}

TEST(ButtonConfigTest, ParsesLuaDispatchCommand) {
    auto fields = splitButtonConfig("rgba(e6e6e6ff), rgba(dc322fdd), ⨯, ⨯, hyprctl dispatch 'hl.dsp.window.close()',");
    ASSERT_TRUE(fields.has_value());
    EXPECT_EQ(fields->command, "hyprctl dispatch 'hl.dsp.window.close()'");
}

TEST(ButtonConfigTest, RejectsTooFewFields) {
    auto fields = splitButtonConfig("rgba(e6e6e6ff), rgba(dc322fdd), ⨯, ⨯");
    ASSERT_FALSE(fields.has_value());
    EXPECT_NE(fields.error().find("at least 5 args"), std::string::npos);
}

TEST(ButtonConfigTest, RejectsEmptyString) {
    EXPECT_FALSE(splitButtonConfig("").has_value());
}
