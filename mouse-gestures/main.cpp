#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <cmath>
#include <cstdlib>
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

inline HANDLE PHANDLE = nullptr;

// Gesture action configuration
struct GestureAction {
    Stroke pattern;
    std::string command;
    std::string name;  // Optional name for display
};

// Mouse gesture state
struct MouseGestureState {
    bool rightButtonPressed = false;
    Vector2D mouseDownPos = {0, 0};
    bool dragDetected = false;
    std::vector<Vector2D> path;
    std::chrono::steady_clock::time_point pressTime;
    uint32_t pressButton = 0;
    uint32_t pressTimeMs = 0;

    void reset() {
        rightButtonPressed = false;
        mouseDownPos = {0, 0};
        dragDetected = false;
        path.clear();
        pressButton = 0;
        pressTimeMs = 0;
    }
};

MouseGestureState g_gestureState;
std::vector<GestureAction> g_gestureActions;
bool g_recordMode = false;

// Hook handles
SP<HOOK_CALLBACK_FN> g_mouseButtonHook;
SP<HOOK_CALLBACK_FN> g_mouseMoveHook;
SP<HOOK_CALLBACK_FN> g_renderHook;

// Debug logging helper
static void debugLog(const std::string& message) {
    try {
        std::ofstream file("/tmp/hyprland-debug", std::ios::app);
        if (file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            file << "[" << ms << "] " << message << "\n";
            file.close();
        }
    } catch (...) {
        // Ignore debug log errors
    }
}









// Convert path to stroke and find best matching gesture action
static const GestureAction* findMatchingGestureAction(const std::vector<Vector2D>& path) {
    debugLog("=== findMatchingGestureAction START ===");
    debugLog("Path size: " + std::to_string(path.size()));

    if (path.size() < 2) {
        debugLog("Path too small, returning nullptr");
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
            debugLog("Match threshold config not available");
            return nullptr;
        }

        const double matchThreshold = static_cast<double>(**PMATCHTHRESHOLD);
        debugLog("Match threshold: " + std::to_string(matchThreshold));

        // Create stroke from path
        debugLog("Creating stroke from path...");
        Stroke inputStroke;
        for (size_t i = 0; i < path.size(); i++) {
            if (i < 5 || i >= path.size() - 5) {
                debugLog("  Point " + std::to_string(i) + ": (" +
                    std::to_string(path[i].x) + ", " +
                    std::to_string(path[i].y) + ")");
            } else if (i == 5) {
                debugLog("  ... (" + std::to_string(path.size() - 10) +
                    " more points) ...");
            }

            if (!inputStroke.addPoint(path[i].x, path[i].y)) {
                debugLog("Failed to add point to stroke");
                return nullptr;
            }
        }

        if (!inputStroke.finish()) {
            debugLog("Failed to finish stroke");
            return nullptr;
        }

        debugLog("Input stroke created successfully with " +
            std::to_string(inputStroke.size()) + " points");
        debugLog("Input stroke data: " + inputStroke.serialize().substr(0, 200) + "...");

        // Find best matching gesture
        debugLog("Comparing against " + std::to_string(g_gestureActions.size()) +
            " configured gestures");

        if (g_gestureActions.empty()) {
            debugLog("WARNING: No gestures configured!");
            debugLog("  Check /tmp/hyprland-debug for PLUGIN_INIT logs");
            debugLog("  Config keyword: plugin:mouse_gestures:gesture_action");
        }

        const GestureAction* bestMatch = nullptr;
        double bestCost = matchThreshold;

        for (size_t i = 0; i < g_gestureActions.size(); i++) {
            const auto& action = g_gestureActions[i];

            std::string actionName = action.name.empty() ?
                "Action_" + std::to_string(i) : action.name;

            debugLog("  Checking gesture: " + actionName);

            if (!action.pattern.isFinished()) {
                debugLog("    Pattern not finished, skipping");
                continue;
            }

            debugLog("    Pattern has " + std::to_string(action.pattern.size()) + " points");
            debugLog("    Pattern data: " + action.pattern.serialize().substr(0, 200) + "...");

            double cost = inputStroke.compare(action.pattern);
            debugLog("    Comparison cost: " + std::to_string(cost));
            debugLog("    Current best cost: " + std::to_string(bestCost));

            if (cost < bestCost) {
                bestCost = cost;
                bestMatch = &action;
                debugLog("    NEW BEST MATCH! Cost: " + std::to_string(cost));
            }
        }

        if (bestMatch) {
            std::string matchName = bestMatch->name.empty() ? "unnamed" : bestMatch->name;
            debugLog("FINAL MATCH: " + matchName + " with cost " +
                std::to_string(bestCost));
        } else {
            debugLog("NO MATCH FOUND (all costs > threshold)");
        }

        debugLog("=== findMatchingGestureAction END ===");
        return bestMatch;
    } catch (const std::exception& e) {
        debugLog("Exception in findMatchingGestureAction: " + std::string(e.what()));
        return nullptr;
    }
}

// Write gesture result to file
static void writeGestureToFile(const Stroke& stroke, const GestureAction* matchedAction) {
    try {
        std::ifstream checkFile("/tmp/hyprland-mouse");
        bool fileExists = checkFile.good();
        checkFile.close();

        std::ofstream file("/tmp/hyprland-mouse", fileExists ? std::ios::app : std::ios::out);
        if (!file.is_open()) {
            return;
        }

        if (fileExists) {
            file << "\n";
        }

        file << "Gesture detected:\n";
        file << "  Path points: " << g_gestureState.path.size() << "\n";
        file << "  Stroke data: " << stroke.serialize() << "\n";

        if (matchedAction) {
            file << "  Matched: " << (matchedAction->name.empty() ?
                matchedAction->command : matchedAction->name) << "\n";
        } else {
            file << "  No match found\n";
        }

        file.close();
    } catch (const std::exception&) {
        // Silently fail on file write errors
    }
}

// Handle detected gesture - analyze path and display result
static void handleGestureDetected() {
    debugLog("\n>>> GESTURE DETECTED <<<");
    debugLog("Path size: " + std::to_string(g_gestureState.path.size()));

    if (g_gestureState.path.size() <= 1) {
        debugLog("Path too small, ignoring");
        return;
    }

    try {
        // Create stroke from path
        Stroke inputStroke;
        for (const auto& point : g_gestureState.path) {
            if (!inputStroke.addPoint(point.x, point.y)) {
                debugLog("Failed to add point to stroke");
                return;
            }
        }

        if (!inputStroke.finish()) {
            debugLog("Failed to finish stroke");
            return;
        }

        // If in record mode, save the stroke data
        if (g_recordMode) {
            debugLog("RECORD MODE: Saving gesture");

            try {
                std::string strokeData = inputStroke.serialize();

                // Write to file
                std::ofstream file("/tmp/hyprland-mouse-gesture-recorded");
                if (file.is_open()) {
                    file << strokeData;
                    file.close();

                    HyprlandAPI::addNotification(
                        PHANDLE,
                        "[mouse-gestures] Gesture recorded",
                        {0, 0.5, 1, 1},
                        3000
                    );
                } else {
                    HyprlandAPI::addNotification(
                        PHANDLE,
                        "[mouse-gestures] Failed to save gesture",
                        {1, 0, 0, 1},
                        3000
                    );
                }
            } catch (const std::exception&) {
                HyprlandAPI::addNotification(
                    PHANDLE,
                    "[mouse-gestures] Error recording gesture",
                    {1, 0, 0, 1},
                    3000
                );
            }

            g_recordMode = false;
            return;
        }

        // Check for matching gesture action
        debugLog("Calling findMatchingGestureAction...");
        const GestureAction* matchingAction = findMatchingGestureAction(g_gestureState.path);

        // Write to debug file
        writeGestureToFile(inputStroke, matchingAction);

        if (matchingAction) {
            std::string actionName = matchingAction->name.empty() ?
                matchingAction->command : matchingAction->name;
            debugLog("EXECUTING COMMAND: " + matchingAction->command);

            try {
                // Execute the command in a new thread to avoid blocking
                std::thread([command = matchingAction->command]() {
                    try {
                        system(command.c_str());
                    } catch (...) {
                        // Ignore command execution errors
                    }
                }).detach();
            } catch (const std::exception&) {
                debugLog("Failed to create thread for command execution");
            }
        } else {
            debugLog("NO MATCH - showing notification");
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Gesture not matched",
                {0, 1, 0, 1},
                3000
            );
        }
    } catch (const std::exception& e) {
        debugLog("Exception in handleGestureDetected: " + std::string(e.what()));
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Error processing gesture",
            {1, 0, 0, 1},
            3000
        );
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
    debugLog(">>> onGestureAction CALLED <<<");
    debugLog("COMMAND: " + std::string(COMMAND ? COMMAND : "(null)"));

    try {
        if (!VALUE) {
            debugLog("ERROR: VALUE is null");
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] gesture_action value is null",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        std::string value(VALUE);
        debugLog("VALUE: " + value.substr(0, 150) +
            (value.length() > 150 ? "..." : ""));

        // Find the LAST pipe delimiter (to support pipes in command)
        size_t pipePos = value.rfind('|');
        if (pipePos == std::string::npos) {
            debugLog("ERROR: No pipe delimiter '|' found in value");
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Format: command|stroke_data",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        debugLog("Pipe found at position: " + std::to_string(pipePos));

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

        debugLog("Parsed stroke_data length: " + std::to_string(strokeData.length()));
        debugLog("Parsed command: " + command);

        // Validate
        if (strokeData.empty()) {
            debugLog("ERROR: stroke_data is empty");
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] stroke_data cannot be empty",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        if (command.empty()) {
            debugLog("ERROR: command is empty");
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] command cannot be empty",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        // Create action
        GestureAction action;
        action.name = "";  // No name in simple format
        action.command = command;
        action.pattern = Stroke::deserialize(strokeData);

        if (!action.pattern.isFinished() || action.pattern.size() < 2) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Invalid stroke data in gesture_action",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        g_gestureActions.push_back(action);

        debugLog(">>> Config: Added gesture action <<<");
        debugLog("  Command: " + command);
        debugLog("  Stroke points: " + std::to_string(action.pattern.size()));
        debugLog("  Stroke data (first 100 chars): " + strokeData.substr(0, 100) +
            (strokeData.length() > 100 ? "..." : ""));

    } catch (const std::exception& e) {
        debugLog("Exception parsing gesture_action: " + std::string(e.what()));
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Error parsing gesture_action",
            {1, 0, 0, 1},
            5000
        );
    }

    return Hyprlang::CParseResult{};
}

// Write configured gesture actions to file
static void writeConfiguredGesturesToFile() {
    try {
        std::ifstream checkFile("/tmp/hyprland-mouse");
        bool fileExists = checkFile.good();
        checkFile.close();

        std::ofstream file("/tmp/hyprland-mouse", fileExists ? std::ios::app : std::ios::out);
        if (!file.is_open()) {
            return;
        }

        if (fileExists) {
            file << "\n";
        }

        file << "Configured gesture actions:\n";
        if (g_gestureActions.empty()) {
            file << "  (none)\n";
        } else {
            for (size_t i = 0; i < g_gestureActions.size(); i++) {
                file << "  " << (i + 1) << ". ";
                if (!g_gestureActions[i].name.empty()) {
                    file << g_gestureActions[i].name << " -> ";
                }
                file << g_gestureActions[i].command << "\n";
                file << "     Points: " << g_gestureActions[i].pattern.size() << "\n";
            }
        }

        file.close();
    } catch (const std::exception&) {
        // Silently fail on file write errors
    }
}

// Handler to clear gesture actions on config reload
static void onPreConfigReload() {
    debugLog(">>> Config reload - clearing " +
        std::to_string(g_gestureActions.size()) + " gesture actions");
    g_gestureActions.clear();
}

static SDispatchResult mouseGesturesDispatch(std::string arg) {
    if (arg == "record") {
        g_recordMode = !g_recordMode;
        if (g_recordMode) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Recording mode enabled - draw gesture with action button",
                {0, 0.5, 1, 1},
                5000
            );
        } else {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Recording mode disabled",
                {0, 0.5, 1, 1},
                3000
            );
        }
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

            // Only handle configured action button
            if (e.button != actionButton)
                return;

            if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
                // Action button pressed - record position and time
                if (!g_pInputManager) {
                    return;
                }

                const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();
                g_gestureState.rightButtonPressed = true;
                g_gestureState.mouseDownPos = mousePos;
                g_gestureState.dragDetected = false;
                g_gestureState.path.clear();
                g_gestureState.path.push_back(mousePos);
                g_gestureState.pressTime = std::chrono::steady_clock::now();
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

            // Check if drag threshold exceeded (if not yet detected)
            if (!g_gestureState.dragDetected) {
                if (checkDragThresholdExceeded(mousePos)) {
                    g_gestureState.dragDetected = true;
                }
            }

            // Record path point if drag is active (but don't consume move events)
            if (g_gestureState.dragDetected) {
                g_gestureState.path.push_back(mousePos);
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

    debugLog("========================================");
    debugLog(">>> PLUGIN_INIT START <<<");

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        throw std::runtime_error("[mouse-gestures] Version mismatch");
    }

    // Register configuration values
    debugLog("Registering config values...");
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

    // Register config keyword for gesture_action
    debugLog("Registering keyword: plugin:mouse_gestures:gesture_action");
    HyprlandAPI::addConfigKeyword(
        PHANDLE,
        "plugin:mouse_gestures:gesture_action",
        onGestureAction,
        Hyprlang::SHandlerOptions{}
    );
    debugLog("Keyword registered successfully");

    // Register preConfigReload handler
    static auto preConfigReloadHook = HyprlandAPI::registerCallbackDynamic(
        PHANDLE,
        "preConfigReload",
        [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); }
    );

    // Register configReloaded handler to write gestures after reload
    static auto configReloadedHook = HyprlandAPI::registerCallbackDynamic(
        PHANDLE,
        "configReloaded",
        [&](void* self, SCallbackInfo& info, std::any data) { writeConfiguredGesturesToFile(); }
    );

    // Reload config to apply registered values
    debugLog("Calling HyprlandAPI::reloadConfig()...");
    HyprlandAPI::reloadConfig();
    debugLog("Config reloaded. Configured gestures count: " +
        std::to_string(g_gestureActions.size()));

    // Write configured gestures to file
    writeConfiguredGesturesToFile();

    HyprlandAPI::addDispatcherV2(PHANDLE, "mouse-gestures", mouseGesturesDispatch);

    // Setup mouse event hooks
    setupMouseButtonHook();
    setupMouseMoveHook();

    debugLog(">>> PLUGIN_INIT COMPLETE <<<");
    debugLog("========================================");

    return {"mouse-gestures", "Mouse gestures for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {

}
