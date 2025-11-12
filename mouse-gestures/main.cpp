#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <wayland-server.h>
#include <wayland-server-protocol.h>
#include <linux/input-event-codes.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <hyprland/src/plugins/PluginAPI.hpp>
#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#undef private

#include "stroke.hpp"
#include "ascii_gesture.hpp"
#include "MouseGestureOverlay.hpp"

inline HANDLE PHANDLE = nullptr;

// Global background texture shared across all monitors
inline SP<CTexture> g_pBackgroundTexture;

// Gesture action configuration
struct GestureAction {
    Stroke pattern;
    std::string command;
    std::string name;  // Optional name for display
};

// Timestamped path point for trail rendering
struct PathPoint {
    Vector2D position;
    std::chrono::steady_clock::time_point timestamp;
};

// Mouse gesture state
struct MouseGestureState {
    bool rightButtonPressed = false;
    Vector2D mouseDownPos = {0, 0};
    bool dragDetected = false;
    std::vector<Vector2D> path;  // For gesture matching
    std::vector<PathPoint> timestampedPath;  // For trail rendering
    std::chrono::steady_clock::time_point pressTime;
    uint32_t pressButton = 0;
    uint32_t pressTimeMs = 0;

    void reset() {
        rightButtonPressed = false;
        mouseDownPos = {0, 0};
        dragDetected = false;
        path.clear();
        // Don't clear timestampedPath - let points fade out naturally
        pressButton = 0;
        pressTimeMs = 0;
    }
};

MouseGestureState g_gestureState;
std::vector<GestureAction> g_gestureActions;
bool g_recordMode = false;
bool g_lastRecordMode = false;
bool g_pluginShuttingDown = false;

// Scroll offset for gesture list in record mode
float g_scrollOffset = 0.0f;
float g_maxScrollOffset = 0.0f;

// Hook handles
SP<HOOK_CALLBACK_FN> g_mouseButtonHook;
SP<HOOK_CALLBACK_FN> g_mouseMoveHook;
SP<HOOK_CALLBACK_FN> g_mouseAxisHook;
SP<HOOK_CALLBACK_FN> g_renderHook;

// Helper function to check if there are any visible trail points
static bool hasVisibleTrailPoints() {
    if (g_gestureState.timestampedPath.empty()) {
        return false;
    }

    try {
        // Get trail fade duration from config
        static auto* const PFADEDURATION = (Hyprlang::INT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE,
                "plugin:mouse_gestures:trail_fade_duration_ms"
            )->getDataStaticPtr();

        if (!PFADEDURATION || !*PFADEDURATION) {
            return false;
        }

        const int fadeDurationMs = **PFADEDURATION;
        auto now = std::chrono::steady_clock::now();

        // Check if any point is still visible (not fully faded)
        for (const auto& point : g_gestureState.timestampedPath) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - point.timestamp
            ).count();

            if (age <= fadeDurationMs) {
                return true;
            }
        }

        return false;
    } catch (...) {
        return false;
    }
}

// Helper function to damage all monitors and schedule frames
static void damageAllMonitors() {
    try {
        if (!g_pCompositor || !g_pHyprRenderer) {
            return;
        }

        for (auto& monitor : g_pCompositor->m_monitors) {
            if (!monitor) {
                continue;
            }

            try {
                g_pHyprRenderer->damageMonitor(monitor);
                g_pCompositor->scheduleFrameForMonitor(monitor);
            } catch (const std::exception& e) {
                // Silently catch per-monitor errors
            } catch (...) {
                // Catch any other errors
            }
        }
    } catch (const std::exception& e) {
        // Silently catch to prevent crashes
    } catch (...) {
        // Catch any other errors
    }
}

// Helper function to execute a command with error handling
static void executeCommand(const std::string& command) {
    if (command.empty()) {
        return;
    }

    try {
        std::thread([command]() {
            try {
                system(command.c_str());
            } catch (const std::exception& e) {
                // Silently catch errors
            } catch (...) {
                // Silently catch errors
            }
        }).detach();
    } catch (const std::exception& e) {
        // Silently catch errors
    } catch (...) {
        // Silently catch errors
    }
}

// Helper function to check if position is inside record mode right square
static bool isInsideRecordSquare(const Vector2D& mousePos, PHLMONITOR monitor) {
    if (!monitor) {
        return false;
    }

    // Calculate the right square bounds (matching renderRecordModeUI)
    constexpr float PADDING = 20.0f;
    constexpr float GAP_WIDTH = 10.0f;
    constexpr int VISIBLE_GESTURES = 3;

    const Vector2D& monitorSize = monitor->m_size;
    const Vector2D& monitorPos = monitor->m_position;

    const float verticalSpace = monitorSize.y - (2.0f * PADDING);
    const float totalGaps = (VISIBLE_GESTURES - 1) * GAP_WIDTH;
    const float baseHeight = (verticalSpace - totalGaps) / VISIBLE_GESTURES;
    const float gestureRectHeight = baseHeight * 0.9f;
    const float gestureRectWidth = gestureRectHeight;
    const float recordSquareSize = verticalSpace;
    const float totalWidth = gestureRectWidth + recordSquareSize;
    const float horizontalMargin = (monitorSize.x - totalWidth) / 3.0f;

    // Right square position and size
    const float recordSquareX = monitorPos.x + horizontalMargin + gestureRectWidth + horizontalMargin;
    const float recordSquareY = monitorPos.y + PADDING;

    // Check if mouse is inside the right square
    return mousePos.x >= recordSquareX &&
           mousePos.x <= recordSquareX + recordSquareSize &&
           mousePos.y >= recordSquareY &&
           mousePos.y <= recordSquareY + recordSquareSize;
}

// Helper function to add gesture to Hyprland config file
static bool addGestureToConfig(const std::string& strokeData) {
    const char* home = std::getenv("HOME");
    if (!home) {
        return false;
    }

    std::string configBasePath = std::string(home) + "/.config/hypr";
    std::vector<std::string> configPaths = {
        configBasePath + "/config/plugins.conf",
        configBasePath + "/hyprland.conf"
    };

    // Step 1: Try to find existing mouse_gestures section
    for (const auto& configPath : configPaths) {
        std::ifstream inFile(configPath);
        if (!inFile.is_open()) continue;

        std::vector<std::string> lines;
        std::string line;
        bool foundMouseGestures = false;
        int mouseGesturesSectionEnd = -1;
        int currentLine = 0;
        int braceDepth = 0;
        int pluginBraceDepth = 0;
        bool inPlugin = false;

        // Read the file and find mouse_gestures section
        while (std::getline(inFile, line)) {
            lines.push_back(line);

            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

            if (trimmed.find("plugin") == 0 && trimmed.find("{") != std::string::npos) {
                inPlugin = true;
                pluginBraceDepth = 1;
            } else if (inPlugin) {
                for (char c : trimmed) {
                    if (c == '{') pluginBraceDepth++;
                    else if (c == '}') pluginBraceDepth--;
                }
                if (pluginBraceDepth == 0) inPlugin = false;
            }

            if (trimmed.find("mouse_gestures") == 0 && trimmed.find("{") != std::string::npos) {
                foundMouseGestures = true;
                braceDepth = 1;
            } else if (foundMouseGestures && braceDepth > 0) {
                for (char c : trimmed) {
                    if (c == '{') braceDepth++;
                    else if (c == '}') braceDepth--;
                }
                if (braceDepth == 0) {
                    mouseGesturesSectionEnd = currentLine;
                }
            }
            currentLine++;
        }
        inFile.close();

        if (foundMouseGestures && mouseGesturesSectionEnd > 0) {
            // Check if ASCII art comments are enabled
            static auto* const PENABLEASCIIART = (Hyprlang::STRING const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:enable_ascii_art_comments"
                )->getDataStaticPtr();

            bool enableAsciiArt = PENABLEASCIIART && *PENABLEASCIIART &&
                                  std::string(*PENABLEASCIIART) == "true";

            // Get default command for config from config
            static auto* const PDEFAULTCMDFORCONFIG = (Hyprlang::STRING const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:default_command_for_config"
                )->getDataStaticPtr();

            std::string defaultCmd;
            if (PDEFAULTCMDFORCONFIG && *PDEFAULTCMDFORCONFIG) {
                // Config value is defined (even if empty string)
                defaultCmd = std::string(*PDEFAULTCMDFORCONFIG);
            } else {
                // Config value is not defined (null), use fallback
                defaultCmd = "hyprctl notify -1 2000 \"rgb(ff0000)\" "
                    "\"modify me in config file " + configPath + "\"";
            }

            std::string gestureAction = "    gesture_action = " + defaultCmd + "|" + strokeData;

            // Insert ASCII art first (if enabled), then gesture_action
            int insertPos = mouseGesturesSectionEnd;
            if (enableAsciiArt) {
                // Generate ASCII art for the gesture
                Stroke previewStroke = Stroke::deserialize(strokeData);
                auto asciiArt = AsciiGestureRenderer::render(previewStroke);

                for (const auto& artLine : asciiArt) {
                    lines.insert(lines.begin() + insertPos, "    " + artLine);
                    insertPos++;
                }
            }
            lines.insert(lines.begin() + insertPos, gestureAction);

            // Write back to file
            std::ofstream outFile(configPath);
            if (!outFile.is_open()) return false;

            for (const auto& l : lines) {
                outFile << l << "\n";
            }
            outFile.close();
            return true;
        }
    }

    // Step 2: If no mouse_gestures section found, try to find plugin section
    for (const auto& configPath : configPaths) {
        std::ifstream inFile(configPath);
        if (!inFile.is_open()) continue;

        std::vector<std::string> lines;
        std::string line;
        int pluginSectionEnd = -1;
        int currentLine = 0;
        int braceDepth = 0;
        bool inPluginSection = false;

        while (std::getline(inFile, line)) {
            lines.push_back(line);

            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

            if (trimmed.find("plugin") == 0 && trimmed.find("{") != std::string::npos) {
                inPluginSection = true;
                braceDepth = 1;
            } else if (inPluginSection && braceDepth > 0) {
                for (char c : trimmed) {
                    if (c == '{') braceDepth++;
                    else if (c == '}') braceDepth--;
                }
                if (braceDepth == 0) {
                    pluginSectionEnd = currentLine;
                }
            }
            currentLine++;
        }
        inFile.close();

        if (pluginSectionEnd > 0) {
            // Check if ASCII art comments are enabled
            static auto* const PENABLEASCIIART = (Hyprlang::STRING const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:enable_ascii_art_comments"
                )->getDataStaticPtr();

            bool enableAsciiArt = PENABLEASCIIART && *PENABLEASCIIART &&
                                  std::string(*PENABLEASCIIART) == "true";

            // Get default command for config from config
            static auto* const PDEFAULTCMDFORCONFIG = (Hyprlang::STRING const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:default_command_for_config"
                )->getDataStaticPtr();

            std::string defaultCmd;
            if (PDEFAULTCMDFORCONFIG && *PDEFAULTCMDFORCONFIG) {
                // Config value is defined (even if empty string)
                defaultCmd = std::string(*PDEFAULTCMDFORCONFIG);
            } else {
                // Config value is not defined (null), use fallback
                defaultCmd = "hyprctl notify -1 2000 \"rgb(ff0000)\" "
                    "\"modify me in config file " + configPath + "\"";
            }

            // Add mouse_gestures section before the closing brace of plugin section
            std::vector<std::string> newSection = {
                "",
                "  mouse_gestures {"
            };

            // Add ASCII art with proper indentation (if enabled)
            if (enableAsciiArt) {
                // Generate ASCII art for the gesture
                Stroke previewStroke = Stroke::deserialize(strokeData);
                auto asciiArt = AsciiGestureRenderer::render(previewStroke);

                for (const auto& artLine : asciiArt) {
                    newSection.push_back("    " + artLine);
                }
            }

            // Add gesture action
            newSection.push_back("    gesture_action = " + defaultCmd + "|" + strokeData);
            newSection.push_back("  }");

            for (int i = newSection.size() - 1; i >= 0; i--) {
                lines.insert(lines.begin() + pluginSectionEnd, newSection[i]);
            }

            // Write back to file
            std::ofstream outFile(configPath);
            if (!outFile.is_open()) return false;

            for (const auto& l : lines) {
                outFile << l << "\n";
            }
            outFile.close();
            return true;
        }
    }

    // Step 3: If no plugin section found, add to hyprland.conf
    std::string hyprlandConf = configBasePath + "/hyprland.conf";
    std::ofstream outFile(hyprlandConf, std::ios::app);
    if (!outFile.is_open()) return false;

    // Check if ASCII art comments are enabled
    static auto* const PENABLEASCIIART = (Hyprlang::STRING const*)
        HyprlandAPI::getConfigValue(
            PHANDLE,
            "plugin:mouse_gestures:enable_ascii_art_comments"
        )->getDataStaticPtr();

    bool enableAsciiArt = PENABLEASCIIART && *PENABLEASCIIART &&
                          std::string(*PENABLEASCIIART) == "true";

    // Get default command for config from config
    static auto* const PDEFAULTCMDFORCONFIG = (Hyprlang::STRING const*)
        HyprlandAPI::getConfigValue(
            PHANDLE,
            "plugin:mouse_gestures:default_command_for_config"
        )->getDataStaticPtr();

    std::string defaultCmd;
    if (PDEFAULTCMDFORCONFIG && *PDEFAULTCMDFORCONFIG) {
        // Config value is defined (even if empty string)
        defaultCmd = std::string(*PDEFAULTCMDFORCONFIG);
    } else {
        // Config value is not defined (null), use fallback
        defaultCmd = "hyprctl notify -1 2000 \"rgb(ff0000)\" "
            "\"modify me in config file " + hyprlandConf + "\"";
    }

    outFile << "\nplugin {\n";
    outFile << "  mouse_gestures {\n";

    // Write ASCII art (if enabled)
    if (enableAsciiArt) {
        // Generate ASCII art for the gesture
        Stroke previewStroke = Stroke::deserialize(strokeData);
        auto asciiArt = AsciiGestureRenderer::render(previewStroke);

        for (const auto& artLine : asciiArt) {
            outFile << "    " << artLine << "\n";
        }
    }

    outFile << "    gesture_action = " << defaultCmd << "|" << strokeData << "\n";
    outFile << "  }\n";
    outFile << "}\n";
    outFile.close();

    return true;
}







// Convert path to stroke and find best matching gesture action
static const GestureAction* findMatchingGestureAction(const std::vector<Vector2D>& path) {

    if (path.size() < 2) {
        return nullptr;
    }

    try {
        // Get match threshold from config
        static auto* const PMATCHTHRESHOLD = (Hyprlang::FLOAT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE,
                "plugin:mouse_gestures:match_threshold"
            )->getDataStaticPtr();

        if (!PMATCHTHRESHOLD || !*PMATCHTHRESHOLD) {
            return nullptr;
        }

        const double matchThreshold = static_cast<double>(**PMATCHTHRESHOLD);

        // Create stroke from path
        Stroke inputStroke;
        for (size_t i = 0; i < path.size(); i++) {
            if (!inputStroke.addPoint(path[i].x, path[i].y)) {
                return nullptr;
            }
        }

        if (!inputStroke.finish()) {
            return nullptr;
        }

        // Find best matching gesture
        if (g_gestureActions.empty()) {
            return nullptr;
        }

        const GestureAction* bestMatch = nullptr;
        double bestCost = matchThreshold;

        for (size_t i = 0; i < g_gestureActions.size(); i++) {
            const auto& action = g_gestureActions[i];

            std::string actionName = action.name.empty() ?
                "Action_" + std::to_string(i) : action.name;


            if (!action.pattern.isFinished()) {
                continue;
            }


            double cost = inputStroke.compare(action.pattern);

            if (cost < bestCost) {
                bestCost = cost;
                bestMatch = &action;
            }
        }

        if (bestMatch) {
            std::string matchName = bestMatch->name.empty() ? "unnamed" : bestMatch->name;
        } else {
        }

        return bestMatch;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

// Handle detected gesture - analyze path and display result
static void handleGestureDetected() {

    if (g_gestureState.path.size() <= 1) {
        return;
    }

    try {
        // Create stroke from path
        Stroke inputStroke;
        for (const auto& point : g_gestureState.path) {
            if (!inputStroke.addPoint(point.x, point.y)) {
                return;
            }
        }

        if (!inputStroke.finish()) {
            return;
        }

        // If in record mode, save the stroke data
        if (g_recordMode) {

            try {
                // First check if this gesture matches any existing gesture
                const GestureAction* matchingAction =
                    findMatchingGestureAction(g_gestureState.path);

                if (matchingAction) {
                    // Gesture already exists, notify user
                    std::string message = "[mouse-gestures] Duplicated "
                        "gesture with command: " + matchingAction->command;
                    HyprlandAPI::addNotification(
                        PHANDLE,
                        message,
                        {1, 0.65, 0, 1},  // Orange color for warning
                        5000
                    );
                } else {
                    // No match, add to config file
                    std::string strokeData = inputStroke.serialize();

                    if (addGestureToConfig(strokeData)) {
                        HyprlandAPI::addNotification(
                            PHANDLE,
                            "[mouse-gestures] Gesture added to config",
                            {0, 0.5, 1, 1},
                            3000
                        );
                    }
                }
            } catch (const std::exception&) {
                // Silently catch errors
            }

            g_recordMode = false;
            // Clear trail from the recorded gesture
            g_gestureState.timestampedPath.clear();
            // Damage all monitors to remove overlay
            damageAllMonitors();
            return;
        }

        // Check for matching gesture action
        const GestureAction* matchingAction = findMatchingGestureAction(g_gestureState.path);

        if (matchingAction) {
            executeCommand(matchingAction->command);
        }
    } catch (const std::exception& e) {
        // Silently catch errors
    }
}

// Handle no gesture detected - replay press and release events
static void replayButtonEvents(uint32_t releaseTimeMs) {
    if (!g_pSeatManager) {
        return;
    }

    g_pSeatManager->sendPointerButton(
        g_gestureState.pressTimeMs,
        g_gestureState.pressButton,
        WL_POINTER_BUTTON_STATE_PRESSED
    );
    g_pSeatManager->sendPointerButton(
        releaseTimeMs,
        g_gestureState.pressButton,
        WL_POINTER_BUTTON_STATE_RELEASED
    );
}

// Check if drag threshold is exceeded within time window
static bool checkDragThresholdExceeded(const Vector2D& mousePos) {
    try {
        // Calculate time elapsed since button press
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - g_gestureState.pressTime
        ).count();

        // Only detect gesture if within 100ms window
        if (elapsed > 100) {
            return false;
        }

        // Get configured drag threshold (default: 50.0)
        static auto* const PDRAGTHRESHOLD = (Hyprlang::INT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE,
                "plugin:mouse_gestures:drag_threshold"
            )->getDataStaticPtr();

        if (!PDRAGTHRESHOLD || !*PDRAGTHRESHOLD) {
            return false;
        }

        const float dragThreshold = static_cast<float>(**PDRAGTHRESHOLD);

        const float distanceX = std::abs(
            mousePos.x - g_gestureState.mouseDownPos.x
        );
        const float distanceY = std::abs(
            mousePos.y - g_gestureState.mouseDownPos.y
        );

        return (distanceX > dragThreshold || distanceY > dragThreshold);
    } catch (const std::exception&) {
        return false;
    }
}

// Config handler for gesture_action keyword
// NEW FORMAT: gesture_action = <command>|<stroke_data>
// The pipe character | separates command from stroke data
// Stroke data contains x,y;x,y;... (commas and semicolons are preserved)
static Hyprlang::CParseResult onGestureAction(const char* COMMAND, const char* VALUE) {

    try {
        if (!VALUE) {
            return Hyprlang::CParseResult{};
        }

        std::string value(VALUE);

        // Find the LAST pipe delimiter (to support pipes in command)
        size_t pipePos = value.rfind('|');
        if (pipePos == std::string::npos) {
            return Hyprlang::CParseResult{};
        }


        // Extract command and stroke data (swapped order!)
        std::string command = value.substr(0, pipePos);
        std::string strokeData = value.substr(pipePos + 1);

        // Trim whitespace
        size_t start = strokeData.find_first_not_of(" \t\n\r");
        size_t end = strokeData.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            strokeData = strokeData.substr(start, end - start + 1);
        } else {
            strokeData = "";  // All whitespace, make it empty
        }

        start = command.find_first_not_of(" \t\n\r");
        end = command.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            command = command.substr(start, end - start + 1);
        } else {
            command = "";  // All whitespace, make it empty
        }


        // Validate stroke data (command can be empty)
        if (strokeData.empty()) {
            return Hyprlang::CParseResult{};
        }

        // Create action
        GestureAction action;
        action.name = "";  // No name in simple format
        action.command = command;
        action.pattern = Stroke::deserialize(strokeData);

        if (!action.pattern.isFinished() || action.pattern.size() < 2) {
            return Hyprlang::CParseResult{};
        }

        g_gestureActions.push_back(action);


    } catch (const std::exception& e) {
        // Silently catch errors
    }

    return Hyprlang::CParseResult{};
}

// Handler to clear gesture actions on config reload
static void onPreConfigReload() {
    g_gestureActions.clear();
}

static void setupRenderHook() {
    try {
        auto onRender = [](void* self, SCallbackInfo& info, std::any param) {
            try {
                // Don't render if plugin is shutting down
                if (g_pluginShuttingDown) {
                    return;
                }

                // Detect when record mode changes and trigger damage
                if (g_recordMode != g_lastRecordMode) {
                    g_lastRecordMode = g_recordMode;
                    damageAllMonitors();
                }

                // Only render when there's something to show
                bool shouldRender = g_recordMode ||
                                   (!g_gestureState.timestampedPath.empty() &&
                                    (g_gestureState.dragDetected ||
                                     !g_gestureState.rightButtonPressed));

                if (!shouldRender) {
                    return;
                }

                // Safety checks
                if (!g_pHyprOpenGL || !g_pHyprRenderer) {
                    return;
                }

                // Get the current monitor being rendered
                auto monitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
                if (!monitor) {
                    return;
                }

                // Add the overlay to the render pass for this specific monitor
                // This hook is called once per monitor per frame, so each
                // monitor will get its own overlay instance
                g_pHyprRenderer->m_renderPass.add(
                    makeUnique<CMouseGestureOverlay>(monitor)
                );

                // Only schedule next frame if we need continuous rendering:
                // - In record mode (UI visible)
                // - Actively dragging (button pressed and drag detected)
                // - Trail points are still visible and fading
                bool needsContinuousRendering = g_recordMode ||
                    (g_gestureState.rightButtonPressed && g_gestureState.dragDetected) ||
                    hasVisibleTrailPoints();

                if (needsContinuousRendering) {
                    try {
                        if (g_pCompositor) {
                            g_pCompositor->scheduleFrameForMonitor(monitor);
                        }
                    } catch (...) {
                        // Silently catch scheduling errors
                    }
                }

            } catch (const std::exception& e) {
                // Silently catch to prevent compositor crash
            } catch (...) {
                // Catch any other unexpected errors
            }
        };

        g_renderHook = g_pHookSystem->hookDynamic("render", onRender);

    } catch (const std::exception& e) {
        // Failed to set up render hook - not critical, just skip the feature
    } catch (...) {
        // Catch any unexpected errors during hook setup
    }
}

static SDispatchResult mouseGesturesDispatch(std::string arg) {
    try {
        if (arg == "record") {
            g_recordMode = !g_recordMode;

            // Clear trail and reset gesture state when toggling record mode
            g_gestureState.timestampedPath.clear();
            g_gestureState.reset();

            // Immediately damage all monitors to show/hide overlay
            damageAllMonitors();
        }
    } catch (const std::exception& e) {
        // Catch errors to prevent crashes
    } catch (...) {
        // Catch any other unexpected errors
    }
    return {};
}

static void setupMouseButtonHook() {
    auto onMouseButton = [](void* self, SCallbackInfo& info, std::any param) {
        try {
            auto e = std::any_cast<IPointer::SButtonEvent>(param);

            // Get configured drag button (default: BTN_RIGHT = 273)
            static auto* const PDRAGBUTTON = (Hyprlang::INT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:drag_button"
                )->getDataStaticPtr();

            if (!PDRAGBUTTON || !*PDRAGBUTTON) {
                return;
            }

            const uint32_t dragButton = static_cast<uint32_t>(**PDRAGBUTTON);

            // If in record mode and a non-drag button is pressed, exit record mode
            if (g_recordMode && e.button != dragButton &&
                e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
                g_recordMode = false;
                g_gestureState.reset();
                g_gestureState.timestampedPath.clear();

                // Damage all monitors to remove overlay immediately
                damageAllMonitors();

                // Consume the event to prevent it from going through
                info.cancelled = true;
                return;
            }

            // Only handle configured drag button for gesture detection
            if (e.button != dragButton)
                return;

            if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
                // Drag button pressed - record position and time
                if (!g_pInputManager) {
                    return;
                }

                const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();

                // In record mode, only allow gesture recording in the right square
                if (g_recordMode) {
                    auto monitor = g_pCompositor->getMonitorFromVector(mousePos);
                    if (!isInsideRecordSquare(mousePos, monitor)) {
                        // Not in the right square - consume event but don't record gesture
                        info.cancelled = true;
                        return;
                    }
                }

                auto now = std::chrono::steady_clock::now();
                g_gestureState.rightButtonPressed = true;
                g_gestureState.mouseDownPos = mousePos;
                g_gestureState.dragDetected = false;
                g_gestureState.path.clear();
                g_gestureState.path.push_back(mousePos);
                g_gestureState.timestampedPath.clear();
                g_gestureState.timestampedPath.push_back({mousePos, now});
                g_gestureState.pressTime = now;
                g_gestureState.pressButton = dragButton;
                g_gestureState.pressTimeMs = e.timeMs;

                // Consume the press - will replay on release if no drag detected
                info.cancelled = true;
            } else {
                // Drag button released
                if (g_gestureState.dragDetected) {
                    handleGestureDetected();
                } else {
                    // No drag detected - clear trail immediately
                    g_gestureState.timestampedPath.clear();
                    replayButtonEvents(e.timeMs);
                }

                // Always consume the release event
                info.cancelled = true;

                // Reset state
                g_gestureState.reset();
            }
        } catch (const std::exception&) {
            // Catch all exceptions to prevent crashing Hyprland
            // Reset state on error and clear trail
            g_gestureState.timestampedPath.clear();
            g_gestureState.reset();
        }
    };

    g_mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onMouseButton);
}

static void setupMouseMoveHook() {
    auto onMouseMove = [](void* self, SCallbackInfo& info, std::any param) {
        try {
            if (!g_gestureState.rightButtonPressed)
                return;

            if (!g_pInputManager) {
                return;
            }

            const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();
            auto now = std::chrono::steady_clock::now();

            // Always record timestamped path points while button is pressed
            // This ensures we capture the full gesture from the start
            g_gestureState.timestampedPath.push_back({mousePos, now});

            // Check if drag threshold exceeded (if not yet detected)
            if (!g_gestureState.dragDetected) {
                if (checkDragThresholdExceeded(mousePos)) {
                    g_gestureState.dragDetected = true;
                }
            }

            // Record path point for gesture matching
            // In record mode: always record all points from the start
            // In normal mode: only record after drag threshold is exceeded
            if (g_recordMode || g_gestureState.dragDetected) {
                g_gestureState.path.push_back(mousePos);

                // Damage all monitors to trigger redraw with new trail
                // Cleanup of old points is handled in the render pass
                damageAllMonitors();
            }
        } catch (const std::exception&) {
            // Catch all exceptions to prevent crashing Hyprland
        }
    };

    g_mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onMouseMove);
}

static void setupMouseAxisHook() {
    auto onMouseAxis = [](void* self, SCallbackInfo& info, std::any param) {
        try {
            // Only handle scrolling when in record mode
            if (!g_recordMode) {
                return;
            }

            // Extract event from map (same as workspace-overview)
            auto eventMap = std::any_cast<
                std::unordered_map<std::string, std::any>
            >(param);
            auto e = std::any_cast<IPointer::SAxisEvent>(eventMap["event"]);

            // Handle vertical scrolling
            if (e.axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
                // Scroll speed multiplier
                constexpr float SCROLL_SPEED = 30.0f;

                g_scrollOffset += e.delta * SCROLL_SPEED;

                // Clamp scroll offset
                if (g_scrollOffset < 0.0f) {
                    g_scrollOffset = 0.0f;
                }
                if (g_scrollOffset > g_maxScrollOffset) {
                    g_scrollOffset = g_maxScrollOffset;
                }

                // Damage monitors to redraw with new scroll position
                damageAllMonitors();

                // Consume the event to prevent it from scrolling other things
                info.cancelled = true;
            }
        } catch (const std::exception&) {
            // Catch all exceptions to prevent crashing Hyprland
        }
    };

    g_mouseAxisHook = g_pHookSystem->hookDynamic("mouseAxis", onMouseAxis);
}

// Helper function to convert pixel data to RGBA format
static std::vector<uint8_t> convertPixelDataToRGBA(const guchar* pixels, int width, int height,
                                                    int channels, int stride) {
    std::vector<uint8_t> pixelData(width * height * 4);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const guchar* src = pixels + y * stride + x * channels;
            uint8_t* dst = pixelData.data() + (y * width + x) * 4;

            if (channels == 4) {
                // RGBA format
                dst[0] = src[0];  // R
                dst[1] = src[1];  // G
                dst[2] = src[2];  // B
                dst[3] = src[3];  // A
            } else if (channels == 3) {
                // RGB format - add alpha channel
                dst[0] = src[0];  // R
                dst[1] = src[1];  // G
                dst[2] = src[2];  // B
                dst[3] = 255;     // A (fully opaque)
            }
        }
    }

    return pixelData;
}

// Helper function to create texture from pixel data
static bool createTextureFromPixelData(const std::vector<uint8_t>& pixelData,
                                       int width, int height) {
    const uint32_t drmFormat = DRM_FORMAT_ABGR8888;
    const uint32_t textureStride = width * 4;

    try {
        auto* pixels = const_cast<uint8_t*>(pixelData.data());
        g_pBackgroundTexture = makeShared<CTexture>(drmFormat, pixels, textureStride,
                                                     Vector2D{(double)width, (double)height},
                                                     true);
        return true;
    } catch (const std::exception&) {
        g_pBackgroundTexture.reset();
        return false;
    }
}

// Load background image from file path
static void loadBackgroundImage(const std::string& path) {
    if (path.empty()) {
        g_pBackgroundTexture.reset();
        return;
    }

    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(path.c_str(), &error);

    if (!pixbuf) {
        if (error)
            g_error_free(error);
        g_pBackgroundTexture.reset();
        return;
    }

    const int width = gdk_pixbuf_get_width(pixbuf);
    const int height = gdk_pixbuf_get_height(pixbuf);
    const int channels = gdk_pixbuf_get_n_channels(pixbuf);

    if (channels != 3 && channels != 4) {
        g_object_unref(pixbuf);
        g_pBackgroundTexture.reset();
        return;
    }

    const int stride = gdk_pixbuf_get_rowstride(pixbuf);
    const guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);

    auto pixelData = convertPixelDataToRGBA(pixels, width, height, channels, stride);
    g_object_unref(pixbuf);

    createTextureFromPixelData(pixelData, width, height);
}


APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;


    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        throw std::runtime_error("[mouse-gestures] Version mismatch");
    }

    // Register configuration values
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:drag_threshold",
        Hyprlang::INT{50}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:drag_button",
        Hyprlang::INT{273}
    ); // BTN_RIGHT
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:match_threshold",
        Hyprlang::FLOAT{0.15}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:default_command_for_config",
        Hyprlang::STRING{""}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:dim_opacity",
        Hyprlang::FLOAT{0.2}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:drag_trail_circle_radius",
        Hyprlang::FLOAT{8.0}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:trail_fade_duration_ms",
        Hyprlang::INT{500}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:drag_trail_color",
        Hyprlang::INT{0x4C7FA6FF}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:enable_ascii_art_comments",
        Hyprlang::STRING{""}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:background_path",
        Hyprlang::STRING{""}
    );

    // Register config keyword for gesture_action
    HyprlandAPI::addConfigKeyword(
        PHANDLE,
        "plugin:mouse_gestures:gesture_action",
        onGestureAction,
        Hyprlang::SHandlerOptions{}
    );

    // Register preConfigReload handler
    static auto preConfigReloadHook = HyprlandAPI::registerCallbackDynamic(
        PHANDLE,
        "preConfigReload",
        [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); }
    );

    // Register configReloaded handler
    static auto configReloadedHook = HyprlandAPI::registerCallbackDynamic(
        PHANDLE,
        "configReloaded",
        [&](void* self, SCallbackInfo& info, std::any data) {
            // Load background image
            auto* const PBACKGROUNDPATH =
                HyprlandAPI::getConfigValue(PHANDLE, "plugin:mouse_gestures:background_path");
            if (PBACKGROUNDPATH) {
                try {
                    auto pathValue = PBACKGROUNDPATH->getValue();
                    std::string pathStr = std::any_cast<Hyprlang::STRING>(pathValue);
                    loadBackgroundImage(pathStr);
                } catch (const std::exception&) {
                    // Failed to read background_path
                }
            }
        }
    );

    // Reload config to apply registered values
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addDispatcherV2(PHANDLE, "mouse-gestures", mouseGesturesDispatch);

    // Setup mouse event hooks
    setupMouseButtonHook();
    setupMouseMoveHook();
    setupMouseAxisHook();
    setupRenderHook();


    return {"mouse-gestures", "Mouse gestures for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    try {
        // Set shutdown flag FIRST to prevent render hook from adding new overlays
        // and to make existing overlays exit early
        g_pluginShuttingDown = true;

        // Exit record mode to prevent overlay rendering
        g_recordMode = false;

        // Unhook all event handlers to prevent callbacks from running
        // during cleanup. This automatically unregisters the callbacks.
        g_mouseButtonHook.reset();
        g_mouseMoveHook.reset();
        g_mouseAxisHook.reset();
        g_renderHook.reset();

        // Clear gesture state after hooks are removed
        g_gestureState.timestampedPath.clear();
        g_gestureState.reset();

        // Clear gesture actions
        g_gestureActions.clear();

        // Clear background texture
        g_pBackgroundTexture.reset();
    } catch (...) {
        // Silently catch any errors during cleanup
    }
}
