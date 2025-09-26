#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>

#include <algorithm>

#include "WindowActionsBar.hpp"
#include "globals.hpp"

static Hyprlang::CParseResult onNewButton(const char* COMMAND, const char* VALUE) {
    std::string value(VALUE);
    CVarList vars(value, 0, ',', true);

    // Expected format: text_color, bg_color, icon_inactive, icon_active, command, [condition]
    // Or with empty colors: , , icon_inactive, icon_active, command, [condition]

    if (vars.size() < 5) {
        Hyprlang::CParseResult result;
        result.setError("Invalid button config (need at least 5 args: text_color, bg_color, icon_inactive, icon_active, command)");
        return result;
    }

    SWindowActionButton button;

    // Parse text color (first arg)
    std::string textColorStr = vars[0];
    if (!textColorStr.empty()) {
        auto rgba_result = configStringToInt(textColorStr);
        if (!rgba_result.has_value()) {
            Hyprlang::CParseResult result;
            result.setError("Invalid text color in button config");
            return result;
        }
        button.text_color = CHyprColor(rgba_result.value());
    } else {
        // Default text color: rgba(e6e6e6ff)
        button.text_color = CHyprColor(0xe6e6e6ff);
    }

    // Parse background color (second arg)
    std::string bgColorStr = vars[1];
    if (!bgColorStr.empty()) {
        auto rgba_result = configStringToInt(bgColorStr);
        if (!rgba_result.has_value()) {
            Hyprlang::CParseResult result;
            result.setError("Invalid background color in button config");
            return result;
        }
        button.bg_color = CHyprColor(rgba_result.value());
    } else {
        // Default background color: rgba(333333dd)
        button.bg_color = CHyprColor(0x333333dd);
    }

    // Parse icons and command
    button.icon_inactive = vars[2];
    button.icon_active = vars[3];
    button.command = vars[4];

    // Parse optional condition (6th arg)
    if (vars.size() >= 6) {
        button.condition = vars[5];
    }

    g_pGlobalState->buttons.push_back(button);

    // Print parsed values for debugging
    Debug::log(LOG, "[window-actions] Added button: text_color={}, bg_color={}, inactive={}, active={}, cmd={}, condition={}",
               textColorStr, bgColorStr, button.icon_inactive, button.icon_active, button.command, button.condition);

    return Hyprlang::CParseResult{};
}

static void onPreConfigReload() {
    Debug::log(LOG, "[window-actions] Clearing {} button configs", g_pGlobalState->buttons.size());
    g_pGlobalState->buttons.clear();
}

static void onNewWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    if (!PWINDOW->m_X11DoesntWantBorders) {
        if (std::ranges::any_of(PWINDOW->m_windowDecorations, [](const auto& d) { return d->getDisplayName() == "WindowActionsBar"; }))
            return;

        auto bar = makeUnique<CWindowActionsBar>(PWINDOW);
        g_pGlobalState->bars.emplace_back(bar);
        bar->m_self = bar;
        HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, std::move(bar));
    }
}

static void onCloseWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    const auto BARIT = std::find_if(g_pGlobalState->bars.begin(), g_pGlobalState->bars.end(), [PWINDOW](const auto& bar) { return bar->getOwner() == PWINDOW; });

    if (BARIT == g_pGlobalState->bars.end())
        return;

    PWINDOW->removeWindowDeco(BARIT->get());
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[window-actions] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[window-actions] Version mismatch");
    }

    g_pGlobalState = makeUnique<SGlobalState>();

    // Register configuration values
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:window_actions:button_size", Hyprlang::INT{15});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:window_actions:action_button", Hyprlang::INT{272}); // BTN_LEFT

    // Register config keyword for window_actions_button
    HyprlandAPI::addConfigKeyword(PHANDLE, "window_actions_button", onNewButton, Hyprlang::SHandlerOptions{});

    // Register preConfigReload handler
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); });

    // Reload config to apply registered values
    HyprlandAPI::reloadConfig();

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onCloseWindow(self, data); });

    for (auto& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        onNewWindow(nullptr, std::any(w));
    }

    return {"window-actions", "Window actions plugin for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    for (auto& m : g_pCompositor->m_monitors)
        m->m_scheduledRecalc = true;

    g_pHyprRenderer->m_renderPass.removeAllOfType("CWindowActionsPassElement");
}