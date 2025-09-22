#define WLR_USE_UNSTABLE

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

inline HANDLE PHANDLE = nullptr;

void logToFile(const std::string& message) {
    std::ofstream logFile("/tmp/hyprland-plugin-window-actions", std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream timestamp;
        timestamp << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

        logFile << "[" << timestamp.str() << "] " << message << std::endl;
        logFile.close();
    }
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    logToFile("Plugin window-actions loading...");
    logToFile("Plugin window-actions loaded successfully");

    return {"window-actions", "Window actions plugin for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    logToFile("Plugin window-actions unloading...");
    logToFile("Plugin window-actions unloaded successfully");
}