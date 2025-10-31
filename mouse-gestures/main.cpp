#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

static SDispatchResult mouseGesturesDispatch(std::string arg) {
    if (arg == "register") {
        HyprlandAPI::addNotification(PHANDLE, "[mouse-gestures] Register command received", {0, 0.5, 1, 1}, 5000);
    }
    return {};
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

    HyprlandAPI::addNotification(PHANDLE, "[mouse-gestures] Plugin registered successfully", {0, 1, 0, 1}, 5000);

    return {"mouse-gestures", "Mouse gestures for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
}
