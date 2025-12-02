#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Texture.hpp>

inline HANDLE PHANDLE = nullptr;

class CWindowActionsBar;

struct SWindowActionButton {
    std::string action = "";
    std::string icon_inactive = "";
    std::string icon_active = "";
    std::string command = "";
    std::string condition = "";
    CHyprColor  text_color = {0};
    CHyprColor  bg_color = {0};
};

struct SGlobalState {
    std::vector<WP<CWindowActionsBar>> bars;
    std::vector<SWindowActionButton>   buttons;
};

inline UP<SGlobalState> g_pGlobalState;