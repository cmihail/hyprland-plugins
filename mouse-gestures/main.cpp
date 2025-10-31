#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <cmath>
#include <vector>
#include <fstream>
#include <wayland-server.h>
#include <linux/input-event-codes.h>
#include <hyprland/src/plugins/PluginAPI.hpp>
#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/devices/IPointer.hpp>
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

// Mouse gesture state
struct MouseGestureState {
    bool rightButtonPressed = false;
    Vector2D mouseDownPos = {0, 0};
    bool dragDetected = false;
    std::vector<Vector2D> path;

    void reset() {
        rightButtonPressed = false;
        mouseDownPos = {0, 0};
        dragDetected = false;
        path.clear();
    }
};

MouseGestureState g_gestureState;

// Hook handles
SP<HOOK_CALLBACK_FN> g_mouseButtonHook;
SP<HOOK_CALLBACK_FN> g_mouseMoveHook;

// Constants
constexpr float DRAG_THRESHOLD_PX = 50.0f;
constexpr float MIN_SEGMENT_LENGTH = 10.0f;

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

// Write gesture result to file
static void writeGestureToFile(const std::vector<Direction>& gesture) {
    std::ofstream file("/tmp/hyprland-mouse", std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return;
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

    file << "\nPath points: " << g_gestureState.path.size() << "\n";
    file.close();
}

static SDispatchResult mouseGesturesDispatch(std::string arg) {
    if (arg == "register") {
        HyprlandAPI::addNotification(PHANDLE, "[mouse-gestures] Register command received", {0, 0.5, 1, 1}, 5000);
    }
    return {};
}

static void setupMouseButtonHook() {
    auto onMouseButton = [](void* self, SCallbackInfo& info, std::any param) {
        auto e = std::any_cast<IPointer::SButtonEvent>(param);

        // Only handle right button
        if (e.button != BTN_RIGHT)
            return;

        if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
            // Right button pressed - record position
            const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();
            g_gestureState.rightButtonPressed = true;
            g_gestureState.mouseDownPos = mousePos;
            g_gestureState.dragDetected = false;
            g_gestureState.path.clear();
            g_gestureState.path.push_back(mousePos);
        } else {
            // Right button released - analyze gesture if drag detected
            if (g_gestureState.dragDetected && g_gestureState.path.size() > 1) {
                std::vector<Direction> gesture = analyzeGesture(
                    g_gestureState.path
                );
                writeGestureToFile(gesture);

                // Show notification with detected gesture
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
                HyprlandAPI::addNotification(
                    PHANDLE,
                    "[mouse-gestures] " + gestureStr,
                    {0, 1, 0, 1},
                    3000
                );
            }

            // Reset state
            g_gestureState.reset();
        }

        // Do NOT consume the event - let it pass through
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
            const float distanceX = std::abs(
                mousePos.x - g_gestureState.mouseDownPos.x
            );
            const float distanceY = std::abs(
                mousePos.y - g_gestureState.mouseDownPos.y
            );

            if (distanceX > DRAG_THRESHOLD_PX || distanceY > DRAG_THRESHOLD_PX) {
                g_gestureState.dragDetected = true;
                HyprlandAPI::addNotification(
                    PHANDLE,
                    "[mouse-gestures] Drag detected - recording gesture...",
                    {0, 1, 0, 1},
                    2000
                );
            }
        }

        // Record path point if drag is active
        if (g_gestureState.dragDetected) {
            g_gestureState.path.push_back(mousePos);
        }

        // Do NOT consume the event - let it pass through
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

    HyprlandAPI::addDispatcherV2(PHANDLE, "mouse-gestures", mouseGesturesDispatch);

    // Setup mouse event hooks
    setupMouseButtonHook();
    setupMouseMoveHook();

    HyprlandAPI::addNotification(PHANDLE, "[mouse-gestures] Plugin registered successfully", {0, 1, 0, 1}, 5000);

    return {"mouse-gestures", "Mouse gestures for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
}
