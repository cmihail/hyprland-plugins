#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <wayland-server.h>
#include <wayland-server-protocol.h>
#include <linux/input-event-codes.h>
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
#include "RecordModePassElement.hpp"

inline HANDLE PHANDLE = nullptr;

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
        timestampedPath.clear();
        pressButton = 0;
        pressTimeMs = 0;
    }
};

MouseGestureState g_gestureState;
std::vector<GestureAction> g_gestureActions;
bool g_recordMode = false;
bool g_lastRecordMode = false;

// Hook handles
SP<HOOK_CALLBACK_FN> g_mouseButtonHook;
SP<HOOK_CALLBACK_FN> g_mouseMoveHook;
SP<HOOK_CALLBACK_FN> g_renderHook;

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
            // Generate ASCII art for the gesture
            Stroke previewStroke = Stroke::deserialize(strokeData);
            auto asciiArt = AsciiGestureRenderer::render(previewStroke);

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

            // Insert ASCII art first, then gesture_action
            int insertPos = mouseGesturesSectionEnd;
            for (const auto& artLine : asciiArt) {
                lines.insert(lines.begin() + insertPos, "    " + artLine);
                insertPos++;
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
            // Generate ASCII art for the gesture
            Stroke previewStroke = Stroke::deserialize(strokeData);
            auto asciiArt = AsciiGestureRenderer::render(previewStroke);

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

            // Add ASCII art with proper indentation
            for (const auto& artLine : asciiArt) {
                newSection.push_back("    " + artLine);
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

    // Generate ASCII art for the gesture
    Stroke previewStroke = Stroke::deserialize(strokeData);
    auto asciiArt = AsciiGestureRenderer::render(previewStroke);

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

    // Write ASCII art
    for (const auto& artLine : asciiArt) {
        outFile << "    " << artLine << "\n";
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
                // Detect when record mode changes and trigger damage
                if (g_recordMode != g_lastRecordMode) {
                    g_lastRecordMode = g_recordMode;
                    damageAllMonitors();
                }

                // Only add overlay when in record mode
                if (!g_recordMode) {
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
                    makeUnique<CRecordModePassElement>(monitor)
                );

                // Schedule next frame to keep overlay visible continuously
                try {
                    if (g_pCompositor) {
                        g_pCompositor->scheduleFrameForMonitor(monitor);
                    }
                } catch (...) {
                    // Silently catch scheduling errors
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

            // Get configured action button (default: BTN_RIGHT = 273)
            static auto* const PACTIONBUTTON = (Hyprlang::INT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:action_button"
                )->getDataStaticPtr();

            if (!PACTIONBUTTON || !*PACTIONBUTTON) {
                return;
            }

            const uint32_t actionButton = static_cast<uint32_t>(**PACTIONBUTTON);

            // If in record mode and a non-action button is pressed, cancel recording
            if (g_recordMode && e.button != actionButton &&
                e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
                g_recordMode = false;
                g_gestureState.reset();

                // Damage all monitors to remove overlay immediately
                damageAllMonitors();

                // Consume the event to prevent it from going through
                info.cancelled = true;
                return;
            }

            // Only handle configured action button for gesture detection
            if (e.button != actionButton)
                return;

            if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
                // Action button pressed - record position and time
                if (!g_pInputManager) {
                    return;
                }

                const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();
                auto now = std::chrono::steady_clock::now();
                g_gestureState.rightButtonPressed = true;
                g_gestureState.mouseDownPos = mousePos;
                g_gestureState.dragDetected = false;
                g_gestureState.path.clear();
                g_gestureState.path.push_back(mousePos);
                g_gestureState.timestampedPath.clear();
                g_gestureState.timestampedPath.push_back({mousePos, now});
                g_gestureState.pressTime = now;
                g_gestureState.pressButton = actionButton;
                g_gestureState.pressTimeMs = e.timeMs;

                // Consume the press - will replay on release if no drag detected
                info.cancelled = true;
            } else {
                // Action button released
                if (g_gestureState.dragDetected) {
                    handleGestureDetected();
                } else {
                    replayButtonEvents(e.timeMs);
                }

                // Always consume the release event
                info.cancelled = true;

                // Reset state
                g_gestureState.reset();
            }
        } catch (const std::exception&) {
            // Catch all exceptions to prevent crashing Hyprland
            // Reset state on error
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

            // Check if drag threshold exceeded (if not yet detected)
            if (!g_gestureState.dragDetected) {
                if (checkDragThresholdExceeded(mousePos)) {
                    g_gestureState.dragDetected = true;
                }
            }

            // Record path point if drag is active (but don't consume move events)
            if (g_gestureState.dragDetected) {
                g_gestureState.path.push_back(mousePos);
                g_gestureState.timestampedPath.push_back({mousePos, now});

                // Get trail fade duration from config
                static auto* const PFADEDURATION = (Hyprlang::INT* const*)
                    HyprlandAPI::getConfigValue(
                        PHANDLE,
                        "plugin:mouse_gestures:trail_fade_duration_ms"
                    )->getDataStaticPtr();

                int fadeDurationMs = 500;  // Default
                if (PFADEDURATION && *PFADEDURATION) {
                    fadeDurationMs = static_cast<int>(**PFADEDURATION);
                }

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

                // Damage all monitors to trigger redraw with new trail
                if (g_recordMode) {
                    damageAllMonitors();
                }
            }
        } catch (const std::exception&) {
            // Catch all exceptions to prevent crashing Hyprland
        }
    };

    g_mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onMouseMove);
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
        "plugin:mouse_gestures:action_button",
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
        "plugin:mouse_gestures:trail_circle_radius",
        Hyprlang::FLOAT{8.0}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:trail_fade_duration_ms",
        Hyprlang::INT{500}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:trail_color_r",
        Hyprlang::FLOAT{0.4}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:trail_color_g",
        Hyprlang::FLOAT{0.8}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:trail_color_b",
        Hyprlang::FLOAT{1.0}
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
        [&](void* self, SCallbackInfo& info, std::any data) {}
    );

    // Reload config to apply registered values
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addDispatcherV2(PHANDLE, "mouse-gestures", mouseGesturesDispatch);

    // Setup mouse event hooks
    setupMouseButtonHook();
    setupMouseMoveHook();
    setupRenderHook();


    return {"mouse-gestures", "Mouse gestures for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {

}
