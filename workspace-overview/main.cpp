#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include "globals.hpp"
#include "overview.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// Function hooks
inline CFunctionHook* g_pRenderWorkspaceHook = nullptr;
inline CFunctionHook* g_pAddDamageHookA      = nullptr;
inline CFunctionHook* g_pAddDamageHookB      = nullptr;
typedef void (*origRenderWorkspace)(void*, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&,
                                    const CBox&);
typedef void (*origAddDamageA)(void*, const CBox&);
typedef void (*origAddDamageB)(void*, const pixman_region32_t*);

static bool renderingOverview = false;

static void hkRenderWorkspace(void* thisptr, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace,
                              const Time::steady_tp& now, const CBox& geometry) {
    auto it = g_pOverviews.find(pMonitor);
    if (it == g_pOverviews.end() || renderingOverview || it->second->blockOverviewRendering)
        ((origRenderWorkspace)(g_pRenderWorkspaceHook->m_original))(thisptr, pMonitor, pWorkspace, now, geometry);
    else
        it->second->render();
}

static void hkAddDamageA(void* thisptr, const CBox& box) {
    const auto PMONITOR = (Monitor::CMonitor*)thisptr;
    const auto PMONITORSHARED = PMONITOR->m_self.lock();

    if (!PMONITORSHARED) {
        ((origAddDamageA)g_pAddDamageHookA->m_original)(thisptr, box);
        return;
    }

    auto it = g_pOverviews.find(PMONITORSHARED);
    if (it == g_pOverviews.end() || it->second->blockDamageReporting) {
        ((origAddDamageA)g_pAddDamageHookA->m_original)(thisptr, box);
        return;
    }

    it->second->onDamageReported();
}

static void hkAddDamageB(void* thisptr, const pixman_region32_t* rg) {
    const auto PMONITOR = (Monitor::CMonitor*)thisptr;
    const auto PMONITORSHARED = PMONITOR->m_self.lock();

    if (!PMONITORSHARED) {
        ((origAddDamageB)g_pAddDamageHookB->m_original)(thisptr, rg);
        return;
    }

    auto it = g_pOverviews.find(PMONITORSHARED);
    if (it == g_pOverviews.end() || it->second->blockDamageReporting) {
        ((origAddDamageB)g_pAddDamageHookB->m_original)(thisptr, rg);
        return;
    }

    it->second->onDamageReported();
}

static SDispatchResult workspaceOverviewDispatch(std::string arg) {
    Log::logger->log(Log::INFO, "[workspace-overview] Overview dispatch called with arg: {}", arg);

    const auto PMONITOR = State::monitorState()->query().vec(Pointer::mgr()->position()).run();
    if (!PMONITOR) {
        Log::logger->log(Log::ERR, "[workspace-overview] No monitor found");
        return {};
    }

    if (arg == "toggle") {
        auto it = g_pOverviews.find(PMONITOR);
        if (it != g_pOverviews.end()) {
            // Overview exists for focused monitor, close all overviews
            // Make a copy of keys to avoid iterator invalidation during close()
            std::vector<PHLMONITOR> monitorsToClose;
            for (const auto& [mon, overview] : g_pOverviews) {
                monitorsToClose.push_back(mon);
            }
            for (const auto& mon : monitorsToClose) {
                auto monIt = g_pOverviews.find(mon);
                if (monIt != g_pOverviews.end())
                    monIt->second->close();
            }
        } else {
            // No overview for focused monitor, create overview on all monitors
            // Create them one at a time to avoid rendering conflicts
            for (auto& monitor : State::monitorState()->monitors()) {
                if (!monitor || !monitor->m_activeWorkspace)
                    continue;
                renderingOverview = true;
                g_pOverviews[monitor] = std::make_unique<COverview>(monitor->m_activeWorkspace, monitor);
                renderingOverview = false;
            }
        }
        return {};
    }

    if (arg == "close" || arg == "off") {
        // Close all overviews
        std::vector<PHLMONITOR> monitorsToClose;
        for (const auto& [mon, overview] : g_pOverviews) {
            monitorsToClose.push_back(mon);
        }
        for (const auto& mon : monitorsToClose) {
            auto monIt = g_pOverviews.find(mon);
            if (monIt != g_pOverviews.end())
                monIt->second->close();
        }
        return {};
    }

    // Default: open on all monitors if not already open
    if (!g_pOverviews.empty())
        return {};

    // Create them one at a time to avoid rendering conflicts
    for (auto& monitor : State::monitorState()->monitors()) {
        if (!monitor || !monitor->m_activeWorkspace)
            continue;
        renderingOverview = true;
        g_pOverviews[monitor] = std::make_unique<COverview>(monitor->m_activeWorkspace, monitor);
        renderingOverview = false;
    }
    return {};
}

static void loadConfig() {
    if (!g_config.backgroundPath)
        return;

    loadBackgroundImage(g_config.backgroundPath->value());

    g_activeWorkspaceColor = CHyprColor{(uint32_t)g_config.activeWorkspaceColor->value()};
    g_activeBorderSize = g_config.borderSize->value();
    g_placeholderPlusColor = CHyprColor{(uint32_t)g_config.placeholderPlusColor->value()};
    g_placeholderPlusSize = g_config.placeholderPlusSize->value();
    g_dropWindowColor = CHyprColor{(uint32_t)g_config.dropWindowColor->value()};
    g_dropWorkspaceColor = CHyprColor{(uint32_t)g_config.dropWorkspaceColor->value()};
    g_placeholdersNum = (int)g_config.placeholdersNum->value();
    g_dragThreshold = g_config.dragThreshold->value();
    g_dragWindowActionButton = (uint32_t)g_config.dragWindowActionButton->value();
    g_dragWorkspaceActionButton = (uint32_t)g_config.dragWorkspaceActionButton->value();
    g_selectWorkspaceActionButton = (uint32_t)g_config.selectWorkspaceActionButton->value();

    const auto killButton = g_config.killWindowActionButton->value();
    g_killWindowActionButton = killButton == 0 ? std::nullopt : std::optional<uint32_t>{(uint32_t)killButton};
}

// hl.plugin.workspace_overview.dispatch(arg), same arg as the legacy workspace-overview dispatcher
static int luaDispatch(lua_State* L) {
    const char* arg = luaL_optstring(L, 1, "");
    bool ok = true;
    // lua_error longjmps, so C++ temporaries must be destroyed before it runs
    {
        auto result = workspaceOverviewDispatch(arg);
        if (!result.success) {
            ok = false;
            lua_pushstring(L, result.error.c_str());
        }
    }
    return ok ? 0 : lua_error(L);
}

static void failNotif(const std::string& reason) {
    Log::logger->log(Log::ERR, "[workspace-overview] Failure in initialization: {}", reason);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        failNotif("Version mismatch (headers ver is not equal to running hyprland ver)");
        throw std::runtime_error("[workspace-overview] Version mismatch");
    }

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE,
        "_ZN6Render13IHyprRenderer15renderWorkspaceEN9Hyprutils6Memory14CSharedPointerIN7Monitor"
        "8CMonitorEEENS3_I10CWorkspaceEERKNSt6chrono10time_pointINS9_3_V212steady_clockENS9_8durat"
        "ionIlSt5ratioILl1ELl1000000000EEEEEERKNS1_4Math4CBoxE");
    if (FNS.empty()) {
        failNotif("no fns for hook renderWorkspace");
        throw std::runtime_error("[workspace-overview] No fns for hook renderWorkspace");
    }

    g_pRenderWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkRenderWorkspace);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "addDamageEPK15pixman_region32");
    if (FNS.empty()) {
        failNotif("no fns for hook addDamageEPK15pixman_region32");
        throw std::runtime_error("[workspace-overview] No fns for hook addDamageEPK15pixman_region32");
    }

    g_pAddDamageHookB = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageB);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE,
        "_ZN7Monitor8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    if (FNS.empty()) {
        failNotif("no fns for hook _ZN7Monitor8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
        throw std::runtime_error("[workspace-overview] No fns for hook "
                                 "_ZN7Monitor8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    }

    g_pAddDamageHookA = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageA);

    bool success = g_pRenderWorkspaceHook->hook();
    success      = success && g_pAddDamageHookA->hook();
    success      = success && g_pAddDamageHookB->hook();

    if (!success) {
        failNotif("Failed initializing hooks");
        throw std::runtime_error("[workspace-overview] Failed initializing hooks");
    }

    static auto P = Event::bus()->m_events.render.pre.listen([](PHLMONITOR pMonitor) {
        for (auto& [monitor, overview] : g_pOverviews) {
            if (overview)
                overview->onPreRender();
        }
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "workspace-overview", workspaceOverviewDispatch);
    HyprlandAPI::addLuaFunction(PHANDLE, "workspace_overview", "dispatch", luaDispatch);

    // Register config options (Lua: plugin.workspace_overview.<name>)
    using namespace Config::Values;
    g_config.backgroundPath = makeShared<CStringValue>("plugin:workspace_overview:background_path", "background image path", "");
    g_config.activeWorkspaceColor = makeShared<CColorValue>("plugin:workspace_overview:active_workspace_color", "active workspace border color", 0x4c7fa6ff);
    g_config.borderSize = makeShared<CFloatValue>("plugin:workspace_overview:border_size", "active workspace border size", 4.0f);
    g_config.placeholderPlusColor = makeShared<CColorValue>("plugin:workspace_overview:placeholder_plus_color", "placeholder plus sign color", 0xffffffcc);
    g_config.placeholderPlusSize = makeShared<CFloatValue>("plugin:workspace_overview:placeholder_plus_size", "placeholder plus sign size", 8.0f);
    g_config.dropWindowColor = makeShared<CColorValue>("plugin:workspace_overview:drop_window_color", "window drop target color", 0xffffffcc);
    g_config.dropWorkspaceColor = makeShared<CColorValue>("plugin:workspace_overview:drop_workspace_color", "workspace drop target color", 0xffffffcc);
    g_config.placeholdersNum = makeShared<CIntValue>("plugin:workspace_overview:placeholders_num", "number of placeholder workspaces", 5);
    g_config.dragThreshold = makeShared<CFloatValue>("plugin:workspace_overview:drag_threshold", "drag threshold in pixels", 50.0f);
    g_config.dragWindowActionButton = makeShared<CIntValue>("plugin:workspace_overview:drag_window_action_button", "mouse button that drags windows", 272);
    g_config.dragWorkspaceActionButton = makeShared<CIntValue>("plugin:workspace_overview:drag_workspace_action_button", "mouse button that drags workspaces", 274);
    g_config.selectWorkspaceActionButton = makeShared<CIntValue>("plugin:workspace_overview:select_workspace_action_button", "mouse button that selects a workspace", 272);
    g_config.killWindowActionButton = makeShared<CIntValue>("plugin:workspace_overview:kill_window_action_button", "mouse button that kills a window, 0 to disable", 0);

    for (const SP<IValue>& value : std::initializer_list<SP<IValue>>{
             g_config.backgroundPath, g_config.activeWorkspaceColor, g_config.borderSize, g_config.placeholderPlusColor, g_config.placeholderPlusSize,
             g_config.dropWindowColor, g_config.dropWorkspaceColor, g_config.placeholdersNum, g_config.dragThreshold, g_config.dragWindowActionButton,
             g_config.dragWorkspaceActionButton, g_config.selectWorkspaceActionButton, g_config.killWindowActionButton}) {
        HyprlandAPI::addConfigValueV2(PHANDLE, value);
    }

    static auto configCallback = Event::bus()->m_events.config.reloaded.listen([]() { loadConfig(); });

    loadConfig();

    Log::logger->log(Log::INFO, "[workspace-overview] Plugin initialized successfully");

    return {"workspace-overview", "Workspace overview plugin for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    Log::logger->log(Log::INFO, "[workspace-overview] Plugin exiting");
    g_pHyprRenderer->m_renderPass.removeAllOfType("COverviewPassElement");
    g_pBackgroundTexture.reset();
    g_config = {};
}
