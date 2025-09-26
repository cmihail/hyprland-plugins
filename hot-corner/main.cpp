#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <any>
#include <chrono>
#include <memory>
#include <string>
#include <cstdlib>
#include <thread>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Renderer.hpp>

inline HANDLE PHANDLE = nullptr;

struct HotCornerState {
    bool isInHotCorner = false;
    std::chrono::steady_clock::time_point enterTime;
    bool timerActive = false;
    std::thread* timerThread = nullptr;
};

static std::unique_ptr<HotCornerState> g_pHotCornerState;

static void executeHotCornerCommand() {
    auto* const PCOMMAND_VAL = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hot_corner:command");

    std::string commandStr = "";
    if (PCOMMAND_VAL) {
        commandStr = std::any_cast<Hyprlang::STRING>(PCOMMAND_VAL->getValue());
    }

    if (!commandStr.empty()) {
        g_pKeybindManager->m_dispatchers["exec"](commandStr);
    }
}

static void startHotCornerTimer() {
    if (g_pHotCornerState->timerActive) {
        return; // Timer already running
    }

    auto* const PDELAYMS_VAL = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hot_corner:delay_ms");
    int delayMs = 1000; // default

    if (PDELAYMS_VAL) {
        delayMs = std::any_cast<Hyprlang::INT>(PDELAYMS_VAL->getValue());
    }

    g_pHotCornerState->timerActive = true;
    g_pHotCornerState->enterTime = std::chrono::steady_clock::now();

    // Clean up previous timer thread if it exists
    if (g_pHotCornerState->timerThread && g_pHotCornerState->timerThread->joinable()) {
        g_pHotCornerState->timerThread->join();
        delete g_pHotCornerState->timerThread;
    }

    g_pHotCornerState->timerThread = new std::thread([delayMs]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

        // Check if still in hot corner after delay
        if (g_pHotCornerState->isInHotCorner && g_pHotCornerState->timerActive) {
            executeHotCornerCommand();
        }

        g_pHotCornerState->timerActive = false;
    });
}

static void onMouseMotion(void* self, SCallbackInfo& info, std::any data) {
    const auto MOUSEPOS = g_pInputManager->getMouseCoordsInternal();
    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();

    if (!PMONITOR)
        return;

    // Get configuration values
    auto* const PCORNERSIZE_VAL = HyprlandAPI::getConfigValue(PHANDLE, "plugin:hot_corner:corner_size");
    int cornerSize = 10; // default

    if (PCORNERSIZE_VAL) {
        cornerSize = std::any_cast<Hyprlang::INT>(PCORNERSIZE_VAL->getValue());
    }

    bool wasInCorner = g_pHotCornerState->isInHotCorner;
    bool isInTopLeftCorner = (MOUSEPOS.x <= PMONITOR->m_position.x + cornerSize) &&
                            (MOUSEPOS.y <= PMONITOR->m_position.y + cornerSize);

    g_pHotCornerState->isInHotCorner = isInTopLeftCorner;

    if (isInTopLeftCorner && !wasInCorner) {
        // Entered hot corner - start timer
        startHotCornerTimer();
    } else if (!isInTopLeftCorner && wasInCorner) {
        // Left hot corner - cancel timer
        g_pHotCornerState->timerActive = false;
    }
}


APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        throw std::runtime_error("[hot-corner] Version mismatch");
    }

    g_pHotCornerState = std::make_unique<HotCornerState>();

    // Register configuration values
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hot_corner:command", Hyprlang::STRING{""});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hot_corner:delay_ms", Hyprlang::INT{1000});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hot_corner:corner_size", Hyprlang::INT{10});

    // Reload config to apply registered values
    HyprlandAPI::reloadConfig();

    static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "mouseMove", onMouseMotion);

    return {"hot-corner", "Hot corner notification plugin for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_pHotCornerState) {
        g_pHotCornerState->timerActive = false;
        if (g_pHotCornerState->timerThread && g_pHotCornerState->timerThread->joinable()) {
            g_pHotCornerState->timerThread->join();
            delete g_pHotCornerState->timerThread;
        }
    }
    g_pHotCornerState.reset();
}