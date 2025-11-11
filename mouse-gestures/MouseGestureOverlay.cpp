#include "MouseGestureOverlay.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <chrono>
#include <cmath>

extern HANDLE PHANDLE;

// Forward declarations from main.cpp
struct PathPoint {
    Vector2D position;
    std::chrono::steady_clock::time_point timestamp;
};

struct MouseGestureState {
    bool rightButtonPressed;
    Vector2D mouseDownPos;
    bool dragDetected;
    std::vector<Vector2D> path;
    std::vector<PathPoint> timestampedPath;
    std::chrono::steady_clock::time_point pressTime;
    uint32_t pressButton;
    uint32_t pressTimeMs;
};

extern MouseGestureState g_gestureState;
extern bool g_recordMode;

CMouseGestureOverlay::CMouseGestureOverlay(PHLMONITOR monitor) : pMonitor(monitor) {
    // Store the monitor this overlay is for
}

void CMouseGestureOverlay::draw(const CRegion& damage) {
    try {
        // Safety check: ensure OpenGL context is valid
        if (!g_pHyprOpenGL) {
            return;
        }

        // Get the monitor this overlay is bound to
        auto monitor = pMonitor.lock();
        if (!monitor) {
            return;
        }

        // Additional safety check: verify this is the currently rendering monitor
        auto currentMonitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
        if (!currentMonitor || currentMonitor != monitor) {
            return;
        }

        // Render dimming overlay only in record mode
        if (g_recordMode) {
            // Create a semi-transparent black overlay covering the entire monitor
            CBox overlayBox = CBox{{0, 0}, monitor->m_size};

            // Get configured dim opacity
            static auto* const PDIMOPACITY = (Hyprlang::FLOAT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:dim_opacity"
                )->getDataStaticPtr();

            float dimOpacity = 0.2f;  // Default fallback
            if (PDIMOPACITY && *PDIMOPACITY) {
                dimOpacity = static_cast<float>(**PDIMOPACITY);
                // Clamp opacity between 0.0 and 1.0
                if (dimOpacity < 0.0f) dimOpacity = 0.0f;
                if (dimOpacity > 1.0f) dimOpacity = 1.0f;
            }

            // Render the dimming overlay
            // Using a dark color to dim the screen while keeping windows visible
            CHyprColor dimColor{0.0, 0.0, 0.0, dimOpacity};

            // Create a damage region covering the entire screen
            CRegion fullDamage{0, 0, INT16_MAX, INT16_MAX};

            g_pHyprOpenGL->renderRect(overlayBox, dimColor, {.damage = &fullDamage});
        }

        // Render trail circles when drag is active OR during fade-out after drag
        // We check dragDetected to avoid showing trail before threshold is crossed
        // But once drag was detected, trail continues rendering until all points fade
        if (!g_gestureState.timestampedPath.empty() &&
            (g_gestureState.dragDetected || !g_gestureState.rightButtonPressed)) {
            // Get trail configuration
            static auto* const PCIRCLERADIUS = (Hyprlang::FLOAT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:trail_circle_radius"
                )->getDataStaticPtr();

            static auto* const PFADEDURATION = (Hyprlang::INT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:trail_fade_duration_ms"
                )->getDataStaticPtr();

            static auto* const PCOLORR = (Hyprlang::FLOAT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:trail_color_r"
                )->getDataStaticPtr();

            static auto* const PCOLORG = (Hyprlang::FLOAT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:trail_color_g"
                )->getDataStaticPtr();

            static auto* const PCOLORB = (Hyprlang::FLOAT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:trail_color_b"
                )->getDataStaticPtr();

            float circleRadius = 8.0f;
            int fadeDurationMs = 500;
            float colorR = 0.4f;  // Nice cyan/blue color
            float colorG = 0.8f;
            float colorB = 1.0f;

            if (PCIRCLERADIUS && *PCIRCLERADIUS) {
                circleRadius = static_cast<float>(**PCIRCLERADIUS);
            }
            if (PFADEDURATION && *PFADEDURATION) {
                fadeDurationMs = static_cast<int>(**PFADEDURATION);
            }
            if (PCOLORR && *PCOLORR) colorR = static_cast<float>(**PCOLORR);
            if (PCOLORG && *PCOLORG) colorG = static_cast<float>(**PCOLORG);
            if (PCOLORB && *PCOLORB) colorB = static_cast<float>(**PCOLORB);

            auto now = std::chrono::steady_clock::now();

            // Clean up old path points that are beyond fade duration
            auto fadeThreshold = now - std::chrono::milliseconds(fadeDurationMs);
            auto it = g_gestureState.timestampedPath.begin();
            while (it != g_gestureState.timestampedPath.end() &&
                   it->timestamp < fadeThreshold) {
                it++;
            }
            if (it != g_gestureState.timestampedPath.begin()) {
                g_gestureState.timestampedPath.erase(
                    g_gestureState.timestampedPath.begin(), it
                );
            }

            // Render circles for each point in the path
            for (const auto& pathPoint : g_gestureState.timestampedPath) {
                // Calculate age of this point in milliseconds
                auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - pathPoint.timestamp
                ).count();

                // Calculate opacity based on age (linear fade)
                float opacity = 1.0f - (static_cast<float>(ageMs) /
                                        static_cast<float>(fadeDurationMs));
                if (opacity < 0.0f) opacity = 0.0f;
                if (opacity > 1.0f) opacity = 1.0f;

                // Transform global coordinates to monitor-local coordinates
                Vector2D localPos = pathPoint.position - monitor->m_position;

                // Create circle box
                CBox circleBox{
                    {localPos.x - circleRadius,
                     localPos.y - circleRadius},
                    {circleRadius * 2, circleRadius * 2}
                };

                // Render the circle with fading opacity
                CHyprColor circleColor{colorR, colorG, colorB, opacity};
                g_pHyprOpenGL->renderRect(
                    circleBox,
                    circleColor,
                    {.round = static_cast<int>(circleRadius)}
                );
            }

            // Continue scheduling frames while there are points to fade out
            if (!g_gestureState.timestampedPath.empty()) {
                try {
                    if (g_pCompositor) {
                        g_pCompositor->scheduleFrameForMonitor(monitor);
                    }
                } catch (...) {
                    // Silently catch scheduling errors
                }
            }
        }

    } catch (const std::exception& e) {
        // Silently catch and ignore errors to prevent compositor crash
    } catch (...) {
        // Catch any other unexpected errors
    }
}

bool CMouseGestureOverlay::needsLiveBlur() {
    try {
        return false;
    } catch (...) {
        return false;
    }
}

bool CMouseGestureOverlay::needsPrecomputeBlur() {
    try {
        return false;
    } catch (...) {
        return false;
    }
}

std::optional<CBox> CMouseGestureOverlay::boundingBox() {
    try {
        // Get the monitor this overlay is bound to
        auto monitor = pMonitor.lock();
        if (!monitor) {
            // Return a valid empty box instead of nullopt to avoid skipping render
            return CBox{{0, 0}, {0, 0}};
        }

        // Return the full monitor size as the bounding box
        return CBox{{0, 0}, monitor->m_size};

    } catch (const std::exception& e) {
        // Return a valid empty box on error
        return CBox{{0, 0}, {0, 0}};
    } catch (...) {
        // Return a valid empty box on error
        return CBox{{0, 0}, {0, 0}};
    }
}
