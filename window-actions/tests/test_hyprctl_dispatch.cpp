#include <gtest/gtest.h>
#include "hyprctl_dispatch.hpp"

TEST(HyprctlDispatchTest, SplitsDispatcherAndArgs) {
    auto d = parseHyprctlDispatch("hyprctl dispatch workspace r-1");
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->dispatcher, "workspace");
    EXPECT_EQ(d->args, "r-1");

    d = parseHyprctlDispatch("  hyprctl dispatch layoutmsg focus l  ");
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->dispatcher, "layoutmsg");
    EXPECT_EQ(d->args, "focus l");
}

TEST(HyprctlDispatchTest, DispatcherWithoutArgs) {
    auto d = parseHyprctlDispatch("hyprctl dispatch killactive");
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->dispatcher, "killactive");
    EXPECT_EQ(d->args, "");
}

TEST(HyprctlDispatchTest, PluginDispatchers) {
    auto d = parseHyprctlDispatch("hyprctl dispatch workspace-overview toggle");
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->dispatcher, "workspace-overview");
    EXPECT_EQ(d->args, "toggle");
}

TEST(HyprctlDispatchTest, LeavesOtherCommandsToTheShell) {
    EXPECT_FALSE(parseHyprctlDispatch("alacritty").has_value());
    EXPECT_FALSE(parseHyprctlDispatch("hyprctl notify -1 2000 \"rgb(ff0000)\" hi").has_value());
    EXPECT_FALSE(parseHyprctlDispatch("hyprctl dispatch").has_value());
    EXPECT_FALSE(parseHyprctlDispatch("hyprctl dispatch ").has_value());
    EXPECT_FALSE(parseHyprctlDispatch("hyprctl -j dispatch killactive").has_value());
    EXPECT_FALSE(parseHyprctlDispatch("hyprctl dispatch 'hl.dsp.window.close()'").has_value());
    EXPECT_FALSE(parseHyprctlDispatch("hyprctl dispatch killactive && notify-send done").has_value());
    EXPECT_FALSE(parseHyprctlDispatch("hyprctl dispatch exec ~/bin/x").has_value());
    EXPECT_FALSE(parseHyprctlDispatch("hyprctl dispatch exec $TERM").has_value());
}
