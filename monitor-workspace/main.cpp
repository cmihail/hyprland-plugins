#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <any>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <sstream>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

struct MonitorLayout {
    std::map<std::string, std::vector<int>> monitorWorkspaces;
};

struct PluginState {
    std::map<int, MonitorLayout> layouts;
    int currentMonitorCount = 0;
};

static std::unique_ptr<PluginState> g_pState;

static std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

static std::vector<std::string> getSortedMonitorNames() {
    std::vector<std::pair<std::string, std::pair<double, double>>> monitorPositions;

    for (auto& monitor : g_pCompositor->m_monitors) {
        if (monitor->m_enabled) {
            monitorPositions.push_back({
                monitor->m_name,
                {monitor->m_position.x, monitor->m_position.y}
            });
        }
    }

    // Sort by x position, then by y if x is equal
    std::sort(monitorPositions.begin(), monitorPositions.end(),
        [](const auto& a, const auto& b) {
            if (a.second.first != b.second.first) {
                return a.second.first < b.second.first;
            }
            return a.second.second < b.second.second;
        });

    std::vector<std::string> sortedNames;
    for (const auto& [name, pos] : monitorPositions) {
        sortedNames.push_back(name);
    }

    return sortedNames;
}

static void parseLayoutConfig(const std::string& configValue, MonitorLayout& layout) {
    // Format: m1:1,3,5,7,9;m2:2,4,6,8,10
    auto monitorEntries = splitString(configValue, ';');

    for (const auto& entry : monitorEntries) {
        auto parts = splitString(entry, ':');
        if (parts.size() != 2) {
            HyprlandAPI::addNotification(PHANDLE, "[monitor-workspace] Invalid layout format", {1, 0, 0, 1}, 5000);
            continue;
        }

        std::string monitorName = parts[0];
        auto workspaceStrs = splitString(parts[1], ',');

        std::vector<int> workspaces;
        for (const auto& wsStr : workspaceStrs) {
            try {
                workspaces.push_back(std::stoi(wsStr));
            } catch (...) {
                HyprlandAPI::addNotification(PHANDLE, "[monitor-workspace] Invalid workspace number", {1, 0, 0, 1}, 5000);
            }
        }

        layout.monitorWorkspaces[monitorName] = workspaces;
    }
}

static void loadConfiguration() {
    g_pState->layouts.clear();

    // Try to load layouts for different monitor counts (1-10 monitors)
    for (int i = 1; i <= 10; i++) {
        std::string configKey = "plugin:monitor_workspace:layout_" + std::to_string(i) + "_monitors";
        auto* configValue = HyprlandAPI::getConfigValue(PHANDLE, configKey);

        if (configValue) {
            std::string layoutStr = std::any_cast<Hyprlang::STRING>(configValue->getValue());
            if (!layoutStr.empty()) {
                MonitorLayout layout;
                parseLayoutConfig(layoutStr, layout);
                g_pState->layouts[i] = layout;
            }
        }
    }
}

static void updateMonitorCount() {
    int connectedMonitors = 0;

    for (auto& monitor : g_pCompositor->m_monitors) {
        if (monitor->m_enabled) {
            connectedMonitors++;
        }
    }

    if (connectedMonitors != g_pState->currentMonitorCount) {
        g_pState->currentMonitorCount = connectedMonitors;

        // Check if we have a layout for this monitor count
        if (g_pState->layouts.find(connectedMonitors) != g_pState->layouts.end()) {
            // Get sorted monitor names
            auto sortedMonitors = getSortedMonitorNames();

            // Map relative monitor names (m1, m2, m3) to actual monitor names
            std::map<std::string, std::string> monitorMapping;
            for (size_t i = 0; i < sortedMonitors.size(); i++) {
                std::string relativeMonitorName = "m" + std::to_string(i + 1);
                monitorMapping[relativeMonitorName] = sortedMonitors[i];
            }

            // Apply the layout configuration with mapped monitor names
            const auto& layout = g_pState->layouts[connectedMonitors];
            for (const auto& [relativeMonitor, workspaces] : layout.monitorWorkspaces) {
                auto it = monitorMapping.find(relativeMonitor);
                if (it == monitorMapping.end()) {
                    continue;
                }

                std::string actualMonitor = it->second;

                // Get the target monitor
                auto monitor = g_pCompositor->getMonitorFromString(actualMonitor);
                if (!monitor) {
                    continue;
                }

                // Move each workspace to the target monitor
                for (int wsId : workspaces) {
                    auto workspace = g_pCompositor->getWorkspaceByID(wsId);
                    if (!workspace) {
                        // Create the workspace if it doesn't exist
                        workspace = g_pCompositor->createNewWorkspace(wsId, monitor->m_id, "");
                    } else if (workspace->m_monitor != monitor) {
                        // Move the workspace to the target monitor
                        g_pCompositor->moveWorkspaceToMonitor(workspace, monitor, true);
                    }
                }
            }

            // Force each monitor to switch to its first workspace
            for (const auto& [relativeMonitor, workspaces] : layout.monitorWorkspaces) {
                if (workspaces.empty()) continue;

                auto it = monitorMapping.find(relativeMonitor);
                if (it == monitorMapping.end()) continue;

                std::string actualMonitor = it->second;
                auto monitor = g_pCompositor->getMonitorFromString(actualMonitor);
                if (!monitor) continue;

                int firstWorkspaceId = workspaces[0];
                auto workspace = g_pCompositor->getWorkspaceByID(firstWorkspaceId);
                if (workspace && monitor->m_activeWorkspace != workspace) {
                    monitor->changeWorkspace(workspace, false, true, true);
                }
            }
        }
    }
}

static void onMonitorAdded(void* self, SCallbackInfo& info, std::any data) {
    updateMonitorCount();
}

static void onMonitorRemoved(void* self, SCallbackInfo& info, std::any data) {
    updateMonitorCount();
}

static void onConfigReloaded(void* self, SCallbackInfo& info, std::any data) {
    loadConfiguration();
    updateMonitorCount();
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        throw std::runtime_error("[monitor-workspace] Version mismatch");
    }

    g_pState = std::make_unique<PluginState>();

    // Register configuration values for up to 10 monitor layouts
    for (int i = 1; i <= 10; i++) {
        std::string configKey = "plugin:monitor_workspace:layout_" + std::to_string(i) + "_monitors";
        HyprlandAPI::addConfigValue(PHANDLE, configKey, Hyprlang::STRING{""});
    }

    // Reload config to apply registered values
    HyprlandAPI::reloadConfig();

    // Load the configuration
    loadConfiguration();

    // Register event callbacks
    static auto monitorAddedHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorAdded", onMonitorAdded);
    static auto monitorRemovedHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "monitorRemoved", onMonitorRemoved);
    static auto configReloadedHook = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", onConfigReloaded);

    // Initialize current monitor count
    updateMonitorCount();

    return {"monitor-workspace", "Automatically manages workspace placement based on connected monitors", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pState.reset();
}