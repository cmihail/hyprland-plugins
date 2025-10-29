#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <memory>

inline HANDLE PHANDLE = nullptr;

// Global background texture shared across all monitors
inline SP<CTexture> g_pBackgroundTexture;
