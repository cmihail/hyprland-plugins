#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
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

inline HANDLE PHANDLE = nullptr;

// Gesture directions
enum class Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    UP_RIGHT,
    UP_LEFT,
    DOWN_RIGHT,
    DOWN_LEFT,
    NONE
};

const char* directionToString(Direction dir) {
    switch (dir) {
        case Direction::UP: return "UP";
        case Direction::DOWN: return "DOWN";
        case Direction::LEFT: return "LEFT";
        case Direction::RIGHT: return "RIGHT";
        case Direction::UP_RIGHT: return "UP_RIGHT";
        case Direction::UP_LEFT: return "UP_LEFT";
        case Direction::DOWN_RIGHT: return "DOWN_RIGHT";
        case Direction::DOWN_LEFT: return "DOWN_LEFT";
        default: return "NONE";
    }
}

// Parse direction string to Direction enum
static Direction stringToDirection(const std::string& str) {
    if (str == "UP") return Direction::UP;
    if (str == "DOWN") return Direction::DOWN;
    if (str == "LEFT") return Direction::LEFT;
    if (str == "RIGHT") return Direction::RIGHT;
    if (str == "UP_RIGHT") return Direction::UP_RIGHT;
    if (str == "UP_LEFT") return Direction::UP_LEFT;
    if (str == "DOWN_RIGHT") return Direction::DOWN_RIGHT;
    if (str == "DOWN_LEFT") return Direction::DOWN_LEFT;
    return Direction::NONE;
}

// Gesture action configuration
struct GestureAction {
    std::vector<Direction> pattern;
    std::string command;
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

// Hook handles
SP<HOOK_CALLBACK_FN> g_mouseButtonHook;
SP<HOOK_CALLBACK_FN> g_mouseMoveHook;
SP<HOOK_CALLBACK_FN> g_renderHook;

// Gesture line visualization state
struct GestureLineState {
    bool isVisible = false;
    Vector2D startPos;
    Vector2D endPos;
    CHyprColor color = CHyprColor{1.0, 1.0, 1.0, 0.8};
    wl_event_source* hideTimer = nullptr;
};

GestureLineState g_lineState;

// Constants
constexpr float MIN_SEGMENT_LENGTH = 10.0f;
constexpr float GESTURE_LINE_WIDTH = 3.0f;

// Debug logging helper
static void debugLog(const std::string& message) {
    std::ofstream file("/tmp/hyprland-debug", std::ios::app);
    if (file.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;

        char timeStr[100];
        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&time));

        file << "[" << timeStr << "." << ms.count() << "] " << message << "\n";
        file.close();
    }
}

// PassElement for rendering gesture line
class CGestureLinePassElement : public IPassElement {
  public:
    CGestureLinePassElement() = default;
    virtual ~CGestureLinePassElement() = default;

    virtual void draw(const CRegion& damage) {
        try {
            if (!g_lineState.isVisible) {
                debugLog("draw() called but line not visible");
                return;
            }

            debugLog("draw() called - rendering line");
            CRegion damageCopy{0, 0, INT16_MAX, INT16_MAX};

            // Calculate line parameters
            float dx = g_lineState.endPos.x - g_lineState.startPos.x;
            float dy = g_lineState.endPos.y - g_lineState.startPos.y;
            float length = std::sqrt(dx * dx + dy * dy);

            debugLog("Line params: dx=" + std::to_string(dx) +
                    " dy=" + std::to_string(dy) +
                    " length=" + std::to_string(length));

            if (length < 1.0f) {
                debugLog("Line too short, skipping render");
                return;
            }

            // Draw line as a rotated rectangle
            // For simplicity, draw as horizontal/vertical approximation
            CBox lineBox;
            if (std::abs(dx) > std::abs(dy)) {
                // More horizontal
                lineBox = CBox{
                    std::min(g_lineState.startPos.x, g_lineState.endPos.x),
                    g_lineState.startPos.y - GESTURE_LINE_WIDTH / 2.0f,
                    std::abs(dx),
                    GESTURE_LINE_WIDTH
                };
                debugLog("Drawing horizontal line");
            } else {
                // More vertical
                lineBox = CBox{
                    g_lineState.startPos.x - GESTURE_LINE_WIDTH / 2.0f,
                    std::min(g_lineState.startPos.y, g_lineState.endPos.y),
                    GESTURE_LINE_WIDTH,
                    std::abs(dy)
                };
                debugLog("Drawing vertical line");
            }

            debugLog("LineBox: x=" + std::to_string(lineBox.x) +
                    " y=" + std::to_string(lineBox.y) +
                    " w=" + std::to_string(lineBox.w) +
                    " h=" + std::to_string(lineBox.h));

            g_pHyprOpenGL->renderRect(lineBox, g_lineState.color, {.damage = &damageCopy});
            debugLog("renderRect completed successfully");
        } catch (const std::exception& e) {
            debugLog("Exception in draw(): " + std::string(e.what()));
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Error rendering gesture line: " + std::string(e.what()),
                {1, 0, 0, 1},
                3000
            );
            // Hide the line to prevent repeated errors
            g_lineState.isVisible = false;
        } catch (...) {
            debugLog("Unknown exception in draw()");
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Unknown error rendering gesture line",
                {1, 0, 0, 1},
                3000
            );
            // Hide the line to prevent repeated errors
            g_lineState.isVisible = false;
        }
    }

    virtual bool needsLiveBlur() { return false; }
    virtual bool needsPrecomputeBlur() { return false; }

    virtual std::optional<CBox> boundingBox() {
        debugLog("boundingBox() called, isVisible=" + std::to_string(g_lineState.isVisible));

        if (!g_lineState.isVisible) {
            debugLog("boundingBox() returning nullopt - line not visible");
            return std::nullopt;
        }

        float minX = std::min(g_lineState.startPos.x, g_lineState.endPos.x);
        float minY = std::min(g_lineState.startPos.y, g_lineState.endPos.y);
        float maxX = std::max(g_lineState.startPos.x, g_lineState.endPos.x);
        float maxY = std::max(g_lineState.startPos.y, g_lineState.endPos.y);

        CBox box = CBox{
            minX - GESTURE_LINE_WIDTH,
            minY - GESTURE_LINE_WIDTH,
            maxX - minX + GESTURE_LINE_WIDTH * 2,
            maxY - minY + GESTURE_LINE_WIDTH * 2
        };

        debugLog("boundingBox() returning box: x=" + std::to_string(box.x) +
                " y=" + std::to_string(box.y) +
                " w=" + std::to_string(box.w) +
                " h=" + std::to_string(box.h));

        return box;
    }

    virtual CRegion opaqueRegion() { return CRegion(); }
    virtual const char* passName() { return "CGestureLinePassElement"; }
};

// Hide gesture line and clean up
static void hideGestureLine() {
    debugLog("hideGestureLine() called");
    g_lineState.isVisible = false;

    if (g_lineState.hideTimer) {
        wl_event_source_remove(g_lineState.hideTimer);
        g_lineState.hideTimer = nullptr;
        debugLog("Timer removed");
    }

    // No need to remove from render pass - we add it per-frame in the render hook
    debugLog("Line hidden (will no longer be added by render hook)");

    auto monitor = g_pCompositor->m_lastMonitor.lock();
    if (monitor) {
        g_pHyprRenderer->damageMonitor(monitor);
        debugLog("Monitor damaged for redraw");
    } else {
        debugLog("No monitor available for damage");
    }
}

// Show gesture line and schedule hide after 3 seconds
static void showGestureLine(const Vector2D& start, const Vector2D& end) {
    try {
        debugLog("showGestureLine() called - start: (" +
                std::to_string(start.x) + "," + std::to_string(start.y) +
                ") end: (" + std::to_string(end.x) + "," + std::to_string(end.y) + ")");

        // Clean up any existing line
        hideGestureLine();

        // Set up new line
        g_lineState.isVisible = true;
        g_lineState.startPos = start;
        g_lineState.endPos = end;
        debugLog("Line state set to visible");

        // Don't add to render pass here - it will be added by the render hook
        debugLog("Line ready to render (will be added by render hook)");

        // Damage monitor to trigger redraw
        auto monitor = g_pCompositor->m_lastMonitor.lock();
        if (monitor) {
            g_pHyprRenderer->damageMonitor(monitor);
            debugLog("Monitor damaged - should trigger redraw");
        } else {
            debugLog("WARNING: No monitor available to damage!");
        }

        // Schedule hide after 3 seconds
        auto* timer = wl_event_loop_add_timer(
            wl_display_get_event_loop(g_pCompositor->m_wlDisplay),
            [](void* data) -> int {
                std::ofstream file("/tmp/hyprland-debug", std::ios::app);
                if (file.is_open()) {
                    file << "Timer expired - hiding line\n";
                    file.close();
                }
                hideGestureLine();
                return 0;
            },
            nullptr
        );

        if (timer) {
            g_lineState.hideTimer = timer;
            wl_event_source_timer_update(timer, 3000);
            debugLog("Timer scheduled for 3000ms");
        } else {
            debugLog("ERROR: Failed to create timer!");
            throw std::runtime_error("Failed to create timer for gesture line");
        }
    } catch (const std::exception& e) {
        debugLog("Exception in showGestureLine(): " + std::string(e.what()));
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Error drawing gesture line: " + std::string(e.what()),
            {1, 0, 0, 1},
            3000
        );
    } catch (...) {
        debugLog("Unknown exception in showGestureLine()");
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Unknown error drawing gesture line",
            {1, 0, 0, 1},
            3000
        );
    }
}

// Calculate direction from two points
static Direction calculateDirection(const Vector2D& from, const Vector2D& to) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;

    // Calculate distance
    float distance = std::sqrt(dx * dx + dy * dy);
    if (distance < MIN_SEGMENT_LENGTH) {
        return Direction::NONE;
    }

    // Calculate angle in radians, then convert to degrees
    float angle = std::atan2(dy, dx) * 180.0f / M_PI;

    // Normalize angle to 0-360 range
    if (angle < 0) {
        angle += 360.0f;
    }

    // Determine direction based on angle (8 directions)
    // RIGHT: 337.5 - 22.5 (or -22.5 to 22.5)
    // DOWN_RIGHT: 22.5 - 67.5
    // DOWN: 67.5 - 112.5
    // DOWN_LEFT: 112.5 - 157.5
    // LEFT: 157.5 - 202.5
    // UP_LEFT: 202.5 - 247.5
    // UP: 247.5 - 292.5
    // UP_RIGHT: 292.5 - 337.5

    if (angle >= 337.5f || angle < 22.5f) {
        return Direction::RIGHT;
    } else if (angle >= 22.5f && angle < 67.5f) {
        return Direction::DOWN_RIGHT;
    } else if (angle >= 67.5f && angle < 112.5f) {
        return Direction::DOWN;
    } else if (angle >= 112.5f && angle < 157.5f) {
        return Direction::DOWN_LEFT;
    } else if (angle >= 157.5f && angle < 202.5f) {
        return Direction::LEFT;
    } else if (angle >= 202.5f && angle < 247.5f) {
        return Direction::UP_LEFT;
    } else if (angle >= 247.5f && angle < 292.5f) {
        return Direction::UP;
    } else {
        return Direction::UP_RIGHT;
    }
}

// Analyze gesture path and return sequence of directions
static std::vector<Direction> analyzeGesture(
    const std::vector<Vector2D>& path
) {
    std::vector<Direction> rawDirections;

    if (path.size() < 2) {
        return rawDirections;
    }

    // Calculate direction for each consecutive pair of points
    for (size_t i = 1; i < path.size(); i++) {
        Direction dir = calculateDirection(path[i - 1], path[i]);
        if (dir != Direction::NONE) {
            rawDirections.push_back(dir);
        }
    }

    // Merge consecutive duplicate directions
    std::vector<Direction> gesture;
    if (!rawDirections.empty()) {
        gesture.push_back(rawDirections[0]);
        for (size_t i = 1; i < rawDirections.size(); i++) {
            if (rawDirections[i] != gesture.back()) {
                gesture.push_back(rawDirections[i]);
            }
        }
    }

    return gesture;
}

// Check if detected gesture matches configured pattern
static const GestureAction* findMatchingGestureAction(const std::vector<Direction>& gesture) {
    for (const auto& action : g_gestureActions) {
        if (action.pattern.size() != gesture.size()) {
            continue;
        }

        bool matches = true;
        for (size_t i = 0; i < gesture.size(); i++) {
            if (gesture[i] != action.pattern[i]) {
                matches = false;
                break;
            }
        }

        if (matches) {
            return &action;
        }
    }
    return nullptr;
}

// Write gesture result to file
static void writeGestureToFile(const std::vector<Direction>& gesture) {
    // Check if file exists
    std::ifstream checkFile("/tmp/hyprland-mouse");
    bool fileExists = checkFile.good();
    checkFile.close();

    // Open in append mode if exists, otherwise create new
    std::ofstream file("/tmp/hyprland-mouse", fileExists ? std::ios::app : std::ios::out);
    if (!file.is_open()) {
        return;
    }

    if (fileExists) {
        file << "\n";
    }

    file << "Gesture detected: ";
    if (gesture.empty()) {
        file << "NONE\n";
    } else {
        for (size_t i = 0; i < gesture.size(); i++) {
            file << directionToString(gesture[i]);
            if (i < gesture.size() - 1) {
                file << " -> ";
            }
        }
        file << "\n";
    }

    file << "Path points: " << g_gestureState.path.size() << "\n";
    file.close();
}

// Handle detected gesture - analyze path and display result
static void handleGestureDetected() {
    if (g_gestureState.path.size() <= 1) {
        return;
    }

    std::vector<Direction> gesture = analyzeGesture(g_gestureState.path);
    writeGestureToFile(gesture);

    // Draw line from start to end point
    if (!g_gestureState.path.empty()) {
        Vector2D startPos = g_gestureState.path.front();
        Vector2D endPos = g_gestureState.path.back();
        debugLog("handleGestureDetected() - calling showGestureLine");
        showGestureLine(startPos, endPos);
    } else {
        debugLog("handleGestureDetected() - path is empty, cannot draw line");
    }

    // Build gesture string for notification
    std::string gestureStr = "Gesture: ";
    if (gesture.empty()) {
        gestureStr += "NONE";
    } else {
        for (size_t i = 0; i < gesture.size(); i++) {
            gestureStr += directionToString(gesture[i]);
            if (i < gesture.size() - 1) {
                gestureStr += " -> ";
            }
        }
    }

    // Check for matching gesture action
    const GestureAction* matchingAction = findMatchingGestureAction(gesture);
    if (matchingAction) {
        // Execute the command
        system(matchingAction->command.c_str());

        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] " + gestureStr + " -> Executed: " + matchingAction->command,
            {0, 1, 0, 1},
            3000
        );
    } else {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] " + gestureStr,
            {0, 1, 0, 1},
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
    const float dragThreshold = static_cast<float>(**PDRAGTHRESHOLD);

    const float distanceX = std::abs(
        mousePos.x - g_gestureState.mouseDownPos.x
    );
    const float distanceY = std::abs(
        mousePos.y - g_gestureState.mouseDownPos.y
    );

    return (distanceX > dragThreshold || distanceY > dragThreshold);
}

// Config handler for gesture_action keyword
static Hyprlang::CParseResult onGestureAction(const char* COMMAND, const char* VALUE) {
    std::string value(VALUE);

    // Find the comma separator between pattern and command
    size_t commaPos = value.find(',');
    if (commaPos == std::string::npos) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Invalid gesture_action format "
            "(expected: DIRECTION1 DIRECTION2, command)",
            {1, 0, 0, 1},
            5000
        );
        return Hyprlang::CParseResult{};
    }

    std::string patternStr = value.substr(0, commaPos);
    std::string command = value.substr(commaPos + 1);

    // Trim whitespace from command
    size_t cmdStart = command.find_first_not_of(" \t");
    if (cmdStart != std::string::npos) {
        command = command.substr(cmdStart);
    }

    // Parse pattern (space-separated directions)
    GestureAction action;
    std::istringstream iss(patternStr);
    std::string dirStr;

    while (iss >> dirStr) {
        Direction dir = stringToDirection(dirStr);
        if (dir == Direction::NONE) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Invalid direction in gesture pattern: " + dirStr,
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }
        action.pattern.push_back(dir);
    }

    if (action.pattern.empty()) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Gesture pattern cannot be empty",
            {1, 0, 0, 1},
            5000
        );
        return Hyprlang::CParseResult{};
    }

    action.command = command;
    g_gestureActions.push_back(action);

    return Hyprlang::CParseResult{};
}

// Write configured gesture actions to file
static void writeConfiguredGesturesToFile() {
    // Check if file exists
    std::ifstream checkFile("/tmp/hyprland-mouse");
    bool fileExists = checkFile.good();
    checkFile.close();

    // Open in append mode if exists, otherwise create new
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
            for (size_t j = 0; j < g_gestureActions[i].pattern.size(); j++) {
                file << directionToString(g_gestureActions[i].pattern[j]);
                if (j < g_gestureActions[i].pattern.size() - 1) {
                    file << " ";
                }
            }
            file << " -> " << g_gestureActions[i].command << "\n";
        }
    }

    file.close();
}

// Handler to clear gesture actions on config reload
static void onPreConfigReload() {
    g_gestureActions.clear();
}

static SDispatchResult mouseGesturesDispatch(std::string arg) {
    if (arg == "register") {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Register command received",
            {0, 0.5, 1, 1},
            5000
        );
    }
    return {};
}

static void setupMouseButtonHook() {
    auto onMouseButton = [](void* self, SCallbackInfo& info, std::any param) {
        auto e = std::any_cast<IPointer::SButtonEvent>(param);

        // Get configured action button (default: BTN_RIGHT = 273)
        static auto* const PACTIONBUTTON = (Hyprlang::INT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE,
                "plugin:mouse_gestures:action_button"
            )->getDataStaticPtr();
        const uint32_t actionButton = static_cast<uint32_t>(**PACTIONBUTTON);

        // Only handle configured action button
        if (e.button != actionButton)
            return;

        if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
            // Action button pressed - record position and time
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
    };

    g_mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onMouseButton);
}

static void setupMouseMoveHook() {
    auto onMouseMove = [](void* self, SCallbackInfo& info, std::any param) {
        if (!g_gestureState.rightButtonPressed)
            return;

        const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();

        // Check if drag threshold exceeded (if not yet detected)
        if (!g_gestureState.dragDetected) {
            if (checkDragThresholdExceeded(mousePos)) {
                g_gestureState.dragDetected = true;
                HyprlandAPI::addNotification(
                    PHANDLE,
                    "[mouse-gestures] Drag detected - recording gesture...",
                    {0, 1, 0, 1},
                    2000
                );
            }
        }

        // Record path point if drag is active (but don't consume move events)
        if (g_gestureState.dragDetected) {
            g_gestureState.path.push_back(mousePos);
        }
    };

    g_mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onMouseMove);
}

static void setupRenderHook() {
    auto onRender = [](void* self, SCallbackInfo& info, std::any param) {
        try {
            if (!g_lineState.isVisible)
                return;

            debugLog("PreRender hook - adding CGestureLinePassElement to render pass");
            g_pHyprRenderer->m_renderPass.add(makeUnique<CGestureLinePassElement>());
        } catch (const std::exception& e) {
            debugLog("Exception in preRender hook: " + std::string(e.what()));
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Error in preRender hook: " + std::string(e.what()),
                {1, 0, 0, 1},
                3000
            );
            // Hide the line to prevent repeated errors
            g_lineState.isVisible = false;
        } catch (...) {
            debugLog("Unknown exception in preRender hook");
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Unknown error in preRender hook",
                {1, 0, 0, 1},
                3000
            );
            // Hide the line to prevent repeated errors
            g_lineState.isVisible = false;
        }
    };

    g_renderHook = g_pHookSystem->hookDynamic("preRender", onRender);
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

    // Register config keyword for gesture_action
    HyprlandAPI::addConfigKeyword(
        PHANDLE,
        "gesture_action",
        onGestureAction,
        Hyprlang::SHandlerOptions{}
    );

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
    HyprlandAPI::reloadConfig();

    // Write configured gestures to file
    writeConfiguredGesturesToFile();

    HyprlandAPI::addDispatcherV2(PHANDLE, "mouse-gestures", mouseGesturesDispatch);

    // Setup mouse event hooks
    setupMouseButtonHook();
    setupMouseMoveHook();
    setupRenderHook();

    HyprlandAPI::addNotification(
        PHANDLE,
        "[mouse-gestures] Plugin registered successfully",
        {0, 1, 0, 1},
        5000
    );

    return {"mouse-gestures", "Mouse gestures for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    hideGestureLine();
}
