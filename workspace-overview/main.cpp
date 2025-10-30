#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "globals.hpp"
#include "overview.hpp"

// Function hooks
inline CFunctionHook* g_pRenderWorkspaceHook = nullptr;
inline CFunctionHook* g_pAddDamageHookA      = nullptr;
inline CFunctionHook* g_pAddDamageHookB      = nullptr;
typedef void (*origRenderWorkspace)(void*, PHLMONITOR, PHLWORKSPACE, timespec*, const CBox&);
typedef void (*origAddDamageA)(void*, const CBox&);
typedef void (*origAddDamageB)(void*, const pixman_region32_t*);

static bool renderingOverview = false;

static void hkRenderWorkspace(void* thisptr, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* now, const CBox& geometry) {
    auto it = g_pOverviews.find(pMonitor);
    if (it == g_pOverviews.end() || renderingOverview || it->second->blockOverviewRendering)
        ((origRenderWorkspace)(g_pRenderWorkspaceHook->m_original))(thisptr, pMonitor, pWorkspace, now, geometry);
    else
        it->second->render();
}

static void hkAddDamageA(void* thisptr, const CBox& box) {
    const auto PMONITOR = (CMonitor*)thisptr;
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
    const auto PMONITOR = (CMonitor*)thisptr;
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
    Debug::log(LOG, "[workspace-overview] Overview dispatch called with arg: {}", arg);

    const auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
    if (!PMONITOR) {
        Debug::log(ERR, "[workspace-overview] No monitor found");
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
            for (auto& monitor : g_pCompositor->m_monitors) {
                if (!monitor || !monitor->m_activeWorkspace)
                    continue;
                // Set the last monitor so the overview constructor uses the correct monitor
                g_pCompositor->m_lastMonitor = monitor;
                renderingOverview = true;
                g_pOverviews[monitor] = std::make_unique<COverview>(monitor->m_activeWorkspace);
                renderingOverview = false;
            }
            // Restore the last monitor to the focused one
            g_pCompositor->m_lastMonitor = PMONITOR;
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
    for (auto& monitor : g_pCompositor->m_monitors) {
        if (!monitor || !monitor->m_activeWorkspace)
            continue;
        // Set the last monitor so the overview constructor uses the correct monitor
        g_pCompositor->m_lastMonitor = monitor;
        renderingOverview = true;
        g_pOverviews[monitor] = std::make_unique<COverview>(monitor->m_activeWorkspace);
        renderingOverview = false;
    }
    // Restore the last monitor to the focused one
    g_pCompositor->m_lastMonitor = PMONITOR;
    return {};
}

static void failNotif(const std::string& reason) {
    Debug::log(ERR, "[workspace-overview] Failure in initialization: {}", reason);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        failNotif("Version mismatch (headers ver is not equal to running hyprland ver)");
        throw std::runtime_error("[workspace-overview] Version mismatch");
    }

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace");
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

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "_ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    if (FNS.empty()) {
        failNotif("no fns for hook _ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
        throw std::runtime_error("[workspace-overview] No fns for hook _ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    }

    g_pAddDamageHookA = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageA);

    bool success = g_pRenderWorkspaceHook->hook();
    success      = success && g_pAddDamageHookA->hook();
    success      = success && g_pAddDamageHookB->hook();

    if (!success) {
        failNotif("Failed initializing hooks");
        throw std::runtime_error("[workspace-overview] Failed initializing hooks");
    }

    static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preRender", [](void* self, SCallbackInfo& info, std::any param) {
        for (auto& [monitor, overview] : g_pOverviews) {
            if (overview)
                overview->onPreRender();
        }
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "workspace-overview", workspaceOverviewDispatch);

    // Register config options
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:workspace_overview:background_path",
                                 Hyprlang::STRING{""});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:workspace_overview:active_border_color",
                                 Hyprlang::INT{0x4c7fa6ff});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:workspace_overview:active_border_size",
                                 Hyprlang::FLOAT{4.0f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:workspace_overview:placeholder_plus_color",
                                 Hyprlang::INT{0xffffffcc});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:workspace_overview:placeholder_plus_size",
                                 Hyprlang::FLOAT{8.0f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:workspace_overview:drop_zone_color",
                                 Hyprlang::INT{0xffffffcc});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:workspace_overview:placeholders_num",
                                 Hyprlang::INT{5});

    // Register config change callback to reload all config values
    static auto configCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "configReloaded", [](void* self, SCallbackInfo& info, std::any param) {
            // Load background path
            auto* const PBACKGROUNDPATH =
                HyprlandAPI::getConfigValue(PHANDLE,
                                            "plugin:workspace_overview:background_path");
            if (PBACKGROUNDPATH) {
                try {
                    auto pathValue = PBACKGROUNDPATH->getValue();
                    std::string pathStr = std::any_cast<Hyprlang::STRING>(pathValue);
                    loadBackgroundImage(pathStr);
                } catch (const std::bad_any_cast& e) {
                    Debug::log(ERR,
                               "[workspace-overview] Failed to read background_path: {}",
                               e.what());
                }
            }

            // Load active border color
            auto* const PACTIVEBORDERCOLOR =
                HyprlandAPI::getConfigValue(PHANDLE,
                                            "plugin:workspace_overview:active_border_color");
            if (PACTIVEBORDERCOLOR) {
                try {
                    auto colorValue = PACTIVEBORDERCOLOR->getValue();
                    int64_t colorInt = std::any_cast<Hyprlang::INT>(colorValue);
                    g_activeBorderColor = CHyprColor{(uint32_t)colorInt};
                } catch (const std::bad_any_cast& e) {
                    Debug::log(ERR,
                               "[workspace-overview] Failed to read active_border_color: {}",
                               e.what());
                }
            }

            // Load active border size
            auto* const PACTIVEBORDERSIZE =
                HyprlandAPI::getConfigValue(PHANDLE,
                                            "plugin:workspace_overview:active_border_size");
            if (PACTIVEBORDERSIZE) {
                try {
                    auto sizeValue = PACTIVEBORDERSIZE->getValue();
                    g_activeBorderSize = std::any_cast<Hyprlang::FLOAT>(sizeValue);
                } catch (const std::bad_any_cast& e) {
                    Debug::log(ERR,
                               "[workspace-overview] Failed to read active_border_size: {}",
                               e.what());
                }
            }

            // Load placeholder plus color
            auto* const PPLACEHOLDERPLUSCOLOR =
                HyprlandAPI::getConfigValue(PHANDLE,
                                            "plugin:workspace_overview:placeholder_plus_color");
            if (PPLACEHOLDERPLUSCOLOR) {
                try {
                    auto colorValue = PPLACEHOLDERPLUSCOLOR->getValue();
                    int64_t colorInt = std::any_cast<Hyprlang::INT>(colorValue);
                    g_placeholderPlusColor = CHyprColor{(uint32_t)colorInt};
                } catch (const std::bad_any_cast& e) {
                    Debug::log(ERR,
                               "[workspace-overview] Failed to read placeholder_plus_color: {}",
                               e.what());
                }
            }

            // Load placeholder plus size
            auto* const PPLACEHOLDERPLUSSIZE =
                HyprlandAPI::getConfigValue(PHANDLE,
                                            "plugin:workspace_overview:placeholder_plus_size");
            if (PPLACEHOLDERPLUSSIZE) {
                try {
                    auto sizeValue = PPLACEHOLDERPLUSSIZE->getValue();
                    g_placeholderPlusSize = std::any_cast<Hyprlang::FLOAT>(sizeValue);
                } catch (const std::bad_any_cast& e) {
                    Debug::log(ERR,
                               "[workspace-overview] Failed to read placeholder_plus_size: {}",
                               e.what());
                }
            }

            // Load drop zone color
            auto* const PDROPZONECOLOR =
                HyprlandAPI::getConfigValue(PHANDLE,
                                            "plugin:workspace_overview:drop_zone_color");
            if (PDROPZONECOLOR) {
                try {
                    auto colorValue = PDROPZONECOLOR->getValue();
                    int64_t colorInt = std::any_cast<Hyprlang::INT>(colorValue);
                    g_dropZoneColor = CHyprColor{(uint32_t)colorInt};
                } catch (const std::bad_any_cast& e) {
                    Debug::log(ERR,
                               "[workspace-overview] Failed to read drop_zone_color: {}",
                               e.what());
                }
            }

            // Load placeholders num
            auto* const PPLACEHOLDERSNUM =
                HyprlandAPI::getConfigValue(PHANDLE,
                                            "plugin:workspace_overview:placeholders_num");
            if (PPLACEHOLDERSNUM) {
                try {
                    auto numValue = PPLACEHOLDERSNUM->getValue();
                    int64_t numInt = std::any_cast<Hyprlang::INT>(numValue);
                    g_placeholdersNum = (int)numInt;
                } catch (const std::bad_any_cast& e) {
                    Debug::log(ERR,
                               "[workspace-overview] Failed to read placeholders_num: {}",
                               e.what());
                }
            }
        });

    // Load all config values on startup
    auto* const PBACKGROUNDPATH =
        HyprlandAPI::getConfigValue(PHANDLE, "plugin:workspace_overview:background_path");
    if (PBACKGROUNDPATH) {
        try {
            auto pathValue = PBACKGROUNDPATH->getValue();
            std::string pathStr = std::any_cast<Hyprlang::STRING>(pathValue);
            if (!pathStr.empty()) {
                loadBackgroundImage(pathStr);
            }
        } catch (const std::bad_any_cast& e) {
            Debug::log(ERR, "[workspace-overview] Failed to read background_path: {}",
                       e.what());
        }
    }

    auto* const PACTIVEBORDERCOLOR =
        HyprlandAPI::getConfigValue(PHANDLE, "plugin:workspace_overview:active_border_color");
    if (PACTIVEBORDERCOLOR) {
        try {
            auto colorValue = PACTIVEBORDERCOLOR->getValue();
            int64_t colorInt = std::any_cast<Hyprlang::INT>(colorValue);
            g_activeBorderColor = CHyprColor{(uint32_t)colorInt};
        } catch (const std::bad_any_cast& e) {
            Debug::log(ERR, "[workspace-overview] Failed to read active_border_color: {}",
                       e.what());
        }
    }

    auto* const PACTIVEBORDERSIZE =
        HyprlandAPI::getConfigValue(PHANDLE, "plugin:workspace_overview:active_border_size");
    if (PACTIVEBORDERSIZE) {
        try {
            auto sizeValue = PACTIVEBORDERSIZE->getValue();
            g_activeBorderSize = std::any_cast<Hyprlang::FLOAT>(sizeValue);
        } catch (const std::bad_any_cast& e) {
            Debug::log(ERR, "[workspace-overview] Failed to read active_border_size: {}",
                       e.what());
        }
    }

    auto* const PPLACEHOLDERPLUSCOLOR =
        HyprlandAPI::getConfigValue(PHANDLE, "plugin:workspace_overview:placeholder_plus_color");
    if (PPLACEHOLDERPLUSCOLOR) {
        try {
            auto colorValue = PPLACEHOLDERPLUSCOLOR->getValue();
            int64_t colorInt = std::any_cast<Hyprlang::INT>(colorValue);
            g_placeholderPlusColor = CHyprColor{(uint32_t)colorInt};
        } catch (const std::bad_any_cast& e) {
            Debug::log(ERR, "[workspace-overview] Failed to read placeholder_plus_color: {}",
                       e.what());
        }
    }

    auto* const PPLACEHOLDERPLUSSIZE =
        HyprlandAPI::getConfigValue(PHANDLE, "plugin:workspace_overview:placeholder_plus_size");
    if (PPLACEHOLDERPLUSSIZE) {
        try {
            auto sizeValue = PPLACEHOLDERPLUSSIZE->getValue();
            g_placeholderPlusSize = std::any_cast<Hyprlang::FLOAT>(sizeValue);
        } catch (const std::bad_any_cast& e) {
            Debug::log(ERR, "[workspace-overview] Failed to read placeholder_plus_size: {}",
                       e.what());
        }
    }

    auto* const PDROPZONECOLOR =
        HyprlandAPI::getConfigValue(PHANDLE, "plugin:workspace_overview:drop_zone_color");
    if (PDROPZONECOLOR) {
        try {
            auto colorValue = PDROPZONECOLOR->getValue();
            int64_t colorInt = std::any_cast<Hyprlang::INT>(colorValue);
            g_dropZoneColor = CHyprColor{(uint32_t)colorInt};
        } catch (const std::bad_any_cast& e) {
            Debug::log(ERR, "[workspace-overview] Failed to read drop_zone_color: {}",
                       e.what());
        }
    }

    auto* const PPLACEHOLDERSNUM =
        HyprlandAPI::getConfigValue(PHANDLE, "plugin:workspace_overview:placeholders_num");
    if (PPLACEHOLDERSNUM) {
        try {
            auto numValue = PPLACEHOLDERSNUM->getValue();
            int64_t numInt = std::any_cast<Hyprlang::INT>(numValue);
            g_placeholdersNum = (int)numInt;
        } catch (const std::bad_any_cast& e) {
            Debug::log(ERR, "[workspace-overview] Failed to read placeholders_num: {}",
                       e.what());
        }
    }

    Debug::log(LOG, "[workspace-overview] Plugin initialized successfully");

    return {"workspace-overview", "Workspace overview plugin for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    Debug::log(LOG, "[workspace-overview] Plugin exiting");
    g_pHyprRenderer->m_renderPass.removeAllOfType("COverviewPassElement");
    g_pBackgroundTexture.reset();
}
