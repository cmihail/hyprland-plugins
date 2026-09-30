#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <memory>
#include <optional>

inline HANDLE PHANDLE = nullptr;

// Global background texture shared across all monitors
inline SP<CTexture> g_pBackgroundTexture;

// Global configuration values with defaults
inline CHyprColor g_activeWorkspaceColor = CHyprColor{0.3, 0.5, 0.7, 1.0};  // rgba(4c7fa6ff)
inline float g_activeBorderSize = 4.0f;
inline CHyprColor g_placeholderPlusColor = CHyprColor{1.0, 1.0, 1.0, 0.8};  // rgba(ffffffff) with 0.8 alpha
inline float g_placeholderPlusSize = 8.0f;
inline CHyprColor g_dropWindowColor = CHyprColor{1.0, 1.0, 1.0, 0.8};  // White with 0.8 alpha
inline CHyprColor g_dropWorkspaceColor = CHyprColor{1.0, 1.0, 1.0, 0.8};  // White with 0.8 alpha
inline int g_placeholdersNum = 5;
inline float g_dragThreshold = 50.0f;
inline uint32_t g_dragWindowActionButton = 272;  // BTN_LEFT
inline uint32_t g_dragWorkspaceActionButton = 274;  // BTN_MIDDLE
inline uint32_t g_selectWorkspaceActionButton = 272;  // BTN_LEFT
inline std::optional<uint32_t> g_killWindowActionButton = std::nullopt;  // No default
