#pragma once

#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>

// Config values, Lua: plugin.mouse_gestures.<name>
struct SMouseGesturesConfig {
    SP<Config::Values::CIntValue>    dragThreshold;
    SP<Config::Values::CIntValue>    dragButton;
    SP<Config::Values::CFloatValue>  matchThreshold;
    SP<Config::Values::CStringValue> defaultCommandForConfig;
    SP<Config::Values::CFloatValue>  dimOpacity;
    SP<Config::Values::CFloatValue>  dragTrailCircleRadius;
    SP<Config::Values::CIntValue>    trailFadeDurationMs;
    SP<Config::Values::CColorValue>  dragTrailColor;
    SP<Config::Values::CColorValue>  dragTrailEndColor;
    SP<Config::Values::CStringValue> enableAsciiArtComments;
    SP<Config::Values::CStringValue> backgroundPath;
    SP<Config::Values::CIntValue>    deleteGestureButton;
};

extern SMouseGesturesConfig g_config;
