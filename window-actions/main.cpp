#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include <algorithm>

#include "WindowActionsBar.hpp"
#include "globals.hpp"

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