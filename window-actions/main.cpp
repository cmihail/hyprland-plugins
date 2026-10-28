#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/shared/parserUtils/ParserUtils.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprutils/string/VarList.hpp>

using Hyprutils::String::CVarList;

#include <algorithm>

#include "ButtonConfig.hpp"
#include "WindowActionsBar.hpp"
#include "globals.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

static std::expected<void, std::string> addButton(const std::string& value) {
    auto fields = splitButtonConfig(value);
    if (!fields)
        return std::unexpected(fields.error());

    SWindowActionButton button;

    // Parse text color (first arg)
    if (!fields->textColor.empty()) {
        auto rgba_result = Config::ParserUtils::parseColor(fields->textColor);
        if (!rgba_result.has_value())
            return std::unexpected("Invalid text color in button config");
        button.text_color = CHyprColor(rgba_result.value());
    } else {
        // Default text color: rgba(e6e6e6ff)
        button.text_color = CHyprColor(0xe6e6e6ff);
    }

    // Parse background color (second arg)
    if (!fields->bgColor.empty()) {
        auto rgba_result = Config::ParserUtils::parseColor(fields->bgColor);
        if (!rgba_result.has_value())
            return std::unexpected("Invalid background color in button config");
        button.bg_color = CHyprColor(rgba_result.value());
    } else {
        // Default background color: rgba(333333dd)
        button.bg_color = CHyprColor(0x333333dd);
    }

    button.icon_inactive = fields->iconInactive;
    button.icon_active = fields->iconActive;
    button.command = fields->command;
    button.condition = fields->condition;

    g_pGlobalState->buttons.push_back(button);

    // Print parsed values for debugging
    Log::logger->log(Log::INFO,
        "[window-actions] Added button: text_color={}, bg_color={}, "
        "inactive={}, active={}, cmd={}, condition={}",
        fields->textColor, fields->bgColor, button.icon_inactive, button.icon_active,
        button.command, button.condition);

    return {};
}

static Hyprlang::CParseResult onNewButton(const char* COMMAND, const char* VALUE) {
    Hyprlang::CParseResult result;
    if (auto added = addButton(VALUE); !added)
        result.setError(added.error().c_str());
    return result;
}

// hl.plugin.window_actions.add_button("<same value as the window_actions_button keyword>")
static int luaAddButton(lua_State* L) {
    const char* value = luaL_checkstring(L, 1);
    bool ok = true;
    // lua_error longjmps, so C++ temporaries must be destroyed before it runs
    {
        auto added = addButton(value);
        if (!added) {
            ok = false;
            lua_pushstring(L, added.error().c_str());
        }
    }
    return ok ? 0 : lua_error(L);
}

static void onPreConfigReload() {
    Log::logger->log(Log::INFO, "[window-actions] Clearing {} button configs",
                     g_pGlobalState->buttons.size());
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
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[window-actions] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[window-actions] Version mismatch");
    }

    g_pGlobalState = makeUnique<SGlobalState>();

    // Register configuration values (Lua: plugin.window_actions.<name>)
    g_config.buttonSize = makeShared<Config::Values::CIntValue>("plugin:window_actions:button_size", "button size in pixels", 15);
    g_config.actionButton = makeShared<Config::Values::CIntValue>("plugin:window_actions:action_button", "mouse button that triggers actions", 272); // BTN_LEFT
    g_config.unhoveredAlpha = makeShared<Config::Values::CFloatValue>("plugin:window_actions:unhovered_alpha", "button alpha when not hovered", 1.0f);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config.buttonSize);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config.actionButton);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config.unhoveredAlpha);

    // The keyword only registers under the legacy config manager, the Lua function only under the Lua one
    HyprlandAPI::addConfigKeyword(PHANDLE, "window_actions_button", onNewButton, Hyprlang::SHandlerOptions{});
    HyprlandAPI::addLuaFunction(PHANDLE, "window_actions", "add_button", luaAddButton);

    // Register preConfigReload handler
    static auto P3 = Event::bus()->m_events.config.preReload.listen([]() { onPreConfigReload(); });

    // Reload config to apply registered values
    HyprlandAPI::reloadConfig();

    static auto P1 = Event::bus()->m_events.window.open.listen(
        [](PHLWINDOW window) { onNewWindow(nullptr, std::any(window)); });
    static auto P2 = Event::bus()->m_events.window.close.listen(
        [](PHLWINDOW window) { onCloseWindow(nullptr, std::any(window)); });

    for (auto& w : Desktop::windowState()->windows()) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        onNewWindow(nullptr, std::any(w));
    }

    return {"window-actions", "Window actions plugin for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    for (auto& m : State::monitorState()->monitors())
        m->m_scheduledRecalc = true;

    g_pHyprRenderer->m_renderPass.removeAllOfType("CWindowActionsPassElement");

    g_config = {};
}