#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <cmath>
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

// Mouse gesture state
struct MouseGestureState {
    bool rightButtonPressed = false;
    Vector2D mouseDownPos = {0, 0};
    bool dragDetected = false;

    void reset() {
        rightButtonPressed = false;
        mouseDownPos = {0, 0};
        dragDetected = false;
    }
};

MouseGestureState g_gestureState;

// Hook handles
SP<HOOK_CALLBACK_FN> g_mouseButtonHook;
SP<HOOK_CALLBACK_FN> g_mouseMoveHook;

// Constants
constexpr float DRAG_THRESHOLD_PX = 50.0f;

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
        } else {
            // Right button released - reset state
            g_gestureState.reset();
        }

        // Do NOT consume the event - let it pass through
    };

    g_mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onMouseButton);
}

static void setupMouseMoveHook() {
    auto onMouseMove = [](void* self, SCallbackInfo& info, std::any param) {
        // Only check drag if right button is pressed and drag not yet detected
        if (!g_gestureState.rightButtonPressed || g_gestureState.dragDetected)
            return;

        const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();
        const float distanceX = std::abs(mousePos.x - g_gestureState.mouseDownPos.x);
        const float distanceY = std::abs(mousePos.y - g_gestureState.mouseDownPos.y);

        // Check if drag threshold exceeded
        if (distanceX > DRAG_THRESHOLD_PX || distanceY > DRAG_THRESHOLD_PX) {
            g_gestureState.dragDetected = true;

            // Send notification
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Right-click drag detected!",
                {0, 1, 0, 1},
                3000
            );
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
