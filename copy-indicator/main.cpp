#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <fstream>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

// Indicator state per monitor
struct CopyIndicator {
    Vector2D position;
    std::chrono::steady_clock::time_point startTime;
    bool active = false;
    float opacity = 0.0f;
};

// Global state
std::unordered_map<PHLMONITOR, CopyIndicator> g_indicators;
SP<CTexture> g_cachedEmojiTexture;
SP<HOOK_CALLBACK_FN> g_renderHook;
bool g_pluginShuttingDown = false;

// Forward declarations
class CCopyIndicatorPassElement;

// Helper: Convert BGRA to RGBA pixel data
std::vector<uint8_t> convertBGRAtoRGBA(unsigned char* data, int stride,
                                       int width, int height) {
    std::vector<uint8_t> pixelData(width * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const auto SRC = data + y * stride + x * 4;
            auto dst = pixelData.data() + (y * width + x) * 4;
            dst[0] = SRC[2]; // R
            dst[1] = SRC[1]; // G
            dst[2] = SRC[0]; // B
            dst[3] = SRC[3]; // A
        }
    }
    return pixelData;
}

// Helper: Render text to cairo surface
void renderTextToCairo(cairo_t* cr, const std::string& text,
                       int width, int height) {
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* desc = pango_font_description_from_string("Sans 16");
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, text.c_str(), -1);

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    cairo_move_to(cr, (width - text_width) / 2.0, (height - text_height) / 2.0);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(desc);
    g_object_unref(layout);
}

// Create and cache the emoji texture once
SP<CTexture> createEmojiTexture() {
    const int ICON_SIZE = 100;
    const int ICON_HEIGHT = 50;
    const std::string TEXT = "📋";

    try {
        cairo_surface_t* surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, ICON_SIZE, ICON_HEIGHT);
        if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            if (surface) cairo_surface_destroy(surface);
            return nullptr;
        }

        cairo_t* cr = cairo_create(surface);
        if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            if (cr) cairo_destroy(cr);
            cairo_surface_destroy(surface);
            return nullptr;
        }

        renderTextToCairo(cr, TEXT, ICON_SIZE, ICON_HEIGHT);

        cairo_surface_flush(surface);
        unsigned char* data = cairo_image_surface_get_data(surface);
        int stride = cairo_image_surface_get_stride(surface);

        auto pixelData = convertBGRAtoRGBA(data, stride, ICON_SIZE, ICON_HEIGHT);

        const uint32_t drmFormat = DRM_FORMAT_ABGR8888;
        const uint32_t textureStride = ICON_SIZE * 4;
        auto tex = makeShared<CTexture>(drmFormat, pixelData.data(),
            textureStride, Vector2D{(double)ICON_SIZE, (double)ICON_HEIGHT}, true);

        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        return tex;
    } catch (...) {
        return nullptr;
    }
}

// PassElement for rendering the copy indicator
class CCopyIndicatorPassElement : public IPassElement {
  public:
    CCopyIndicatorPassElement(PHLMONITOR monitor, const CopyIndicator& indicator)
        : m_monitor(monitor), m_indicator(indicator) {}

    virtual void draw(const CRegion& damage) {
        try {
            if (!m_indicator.active || m_indicator.opacity <= 0.01f) {
                return;
            }

            if (!g_cachedEmojiTexture) {
                return;
            }

            if (!g_pHyprOpenGL) {
                return;
            }

            const int ICON_WIDTH = 100;
            const int ICON_HEIGHT = 50;

            // Calculate position (centered on saved cursor position)
            // Convert from global coordinates to monitor-local coordinates
            CBox box = {
                m_indicator.position.x - m_monitor->m_position.x - ICON_WIDTH / 2.0,
                m_indicator.position.y - m_monitor->m_position.y - ICON_HEIGHT / 2.0,
                ICON_WIDTH,
                ICON_HEIGHT
            };

            // Render the texture with opacity
            g_pHyprOpenGL->renderTexture(g_cachedEmojiTexture, box, {.a = m_indicator.opacity});

        } catch (const std::exception& e) {
        } catch (...) {
        }
    }

    virtual bool needsLiveBlur() {
        return false;
    }
    virtual bool needsPrecomputeBlur() {
        return false;
    }
    virtual std::optional<CBox> boundingBox() {
        if (!m_monitor) {
            return CBox{0, 0, 0, 0};
        }
        return CBox{0, 0, m_monitor->m_size.x, m_monitor->m_size.y};
    }
    virtual CRegion opaqueRegion() {
        return CRegion{};
    }
    virtual const char* passName() { return "CCopyIndicatorPassElement"; }

  private:
    PHLMONITOR m_monitor;
    CopyIndicator m_indicator;
};

// Update indicator opacity based on time elapsed
void updateIndicator(CopyIndicator& indicator) {
    if (!indicator.active)
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - indicator.startTime).count();

    if (elapsed < 500) {
        // Stay at full opacity
        indicator.opacity = 1.0f;
    } else {
        // Hide after 500ms
        indicator.active = false;
        indicator.opacity = 0.0f;
    }
}

// Helper: Schedule frame for monitor
void scheduleFrame(PHLMONITOR monitor) {
    try {
        if (g_pCompositor)
            g_pCompositor->scheduleFrameForMonitor(monitor);
    } catch (...) {
    }
}

// Helper: Handle indicator rendering for monitor
void handleIndicatorRender(PHLMONITOR monitor, CopyIndicator& indicator) {
    bool wasActive = indicator.active;
    updateIndicator(indicator);

    if (indicator.active) {
        g_pHyprRenderer->m_renderPass.add(
            makeUnique<CCopyIndicatorPassElement>(monitor, indicator));
        scheduleFrame(monitor);
    } else if (wasActive) {
        if (g_pHyprRenderer)
            g_pHyprRenderer->damageMonitor(monitor);
        scheduleFrame(monitor);
    }
}

// Render callback
void onRenderCallback(void* self, SCallbackInfo& info, std::any param) {
    try {
        if (g_pluginShuttingDown || !g_pHyprOpenGL || !g_pHyprRenderer)
            return;

        auto monitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
        if (!monitor)
            return;

        auto it = g_indicators.find(monitor);
        if (it == g_indicators.end() || !it->second.active)
            return;

        handleIndicatorRender(monitor, it->second);
    } catch (...) {
    }
}

// Setup render hook
void setupRenderHook() {
    try {
        g_renderHook = g_pHookSystem->hookDynamic("render", onRenderCallback);
    } catch (...) {
    }
}

// Helper: Activate indicator at position
void activateIndicator(PHLMONITOR monitor, Vector2D pos) {
    auto& indicator = g_indicators[monitor];
    indicator.position = pos;
    indicator.active = true;
    indicator.startTime = std::chrono::steady_clock::now();
    indicator.opacity = 1.0f;

    if (g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(monitor);
    if (g_pCompositor)
        g_pCompositor->scheduleFrameForMonitor(monitor);
}

// Dispatcher: show copy indicator
SDispatchResult dispatchShow(std::string arg) {
    try {
        if (!g_cachedEmojiTexture) {
            g_cachedEmojiTexture = createEmojiTexture();
            if (!g_cachedEmojiTexture)
                return {};
        }

        if (!g_pInputManager || !g_pCompositor)
            return {};

        auto mousePos = g_pInputManager->getMouseCoordsInternal();
        PHLMONITOR monitor = g_pCompositor->getMonitorFromVector(mousePos);
        if (!monitor)
            return {};

        activateIndicator(monitor, mousePos);
        return {};
    } catch (...) {
        return {};
    }
}

// Plugin initialization
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    try {
        const std::string HASH = __hyprland_api_get_hash();
        const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

        if (HASH != CLIENT_HASH) {
            throw std::runtime_error("[copy-indicator] Version mismatch");
        }

        // Register dispatcher
        HyprlandAPI::addDispatcherV2(PHANDLE, "copyindicator", dispatchShow);

        // Setup render hook
        setupRenderHook();

        return {"copy-indicator",
                "Shows a clipboard icon at mouse position when copying",
                "cmihail", "1.0"};
    } catch (const std::exception& e) {
        throw;
    } catch (...) {
        throw;
    }
}

APICALL EXPORT void PLUGIN_EXIT() {
    try {
        g_pluginShuttingDown = true;

        // Reset hook
        if (g_renderHook)
            g_renderHook.reset();

        // Clear state
        g_indicators.clear();
        g_cachedEmojiTexture.reset();
    } catch (...) {
        // Silently catch cleanup errors
    }
}
