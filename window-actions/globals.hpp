#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Texture.hpp>

inline HANDLE PHANDLE = nullptr;

class CWindowActionsBar;

struct SGlobalState {
    std::vector<WP<CWindowActionsBar>> bars;
};

inline UP<SGlobalState> g_pGlobalState;