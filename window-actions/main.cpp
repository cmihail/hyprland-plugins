#include <hyprland/src/includes.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

static void onNewWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    HyprlandAPI::addNotification(PHANDLE, "[window-actions] Window opened: " + PWINDOW->m_title,
                                CHyprColor{0.2, 1.0, 0.2, 1.0}, 3000);

    // TODO: Implement window actions for new window
}

static void onCloseWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    HyprlandAPI::addNotification(PHANDLE, "[window-actions] Window closed: " + PWINDOW->m_title,
                                CHyprColor{1.0, 0.2, 0.2, 1.0}, 3000);

    // TODO: Clean up window actions for closed window
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addNotification(PHANDLE, "[window-actions] Plugin loading...",
                                CHyprColor{0.2, 0.2, 1.0, 1.0}, 3000);

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow",
                                                         [&](void* self, SCallbackInfo& info, std::any data) {
                                                             onNewWindow(self, data);
                                                         });

    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow",
                                                         [&](void* self, SCallbackInfo& info, std::any data) {
                                                             onCloseWindow(self, data);
                                                         });

    HyprlandAPI::addNotification(PHANDLE, "[window-actions] Plugin loaded successfully",
                                CHyprColor{0.2, 1.0, 0.2, 1.0}, 3000);

    return {"window-actions", "Window actions plugin for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::addNotification(PHANDLE, "[window-actions] Plugin unloading...",
                                CHyprColor{1.0, 1.0, 0.2, 1.0}, 3000);
}