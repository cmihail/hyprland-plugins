#include "RecordModePassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

extern HANDLE PHANDLE;

CRecordModePassElement::CRecordModePassElement(PHLMONITOR monitor) : pMonitor(monitor) {
    // Store the monitor this overlay is for
}

void CRecordModePassElement::draw(const CRegion& damage) {
    try {
        // Safety check: ensure OpenGL context is valid
        if (!g_pHyprOpenGL) {
            return;
        }

        // Get the monitor this overlay is bound to
        auto monitor = pMonitor.lock();
        if (!monitor) {
            return;
        }

        // Additional safety check: verify this is the currently rendering monitor
        auto currentMonitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
        if (!currentMonitor || currentMonitor != monitor) {
            return;
        }

        // Create a semi-transparent black overlay covering the entire monitor
        CBox overlayBox = CBox{{0, 0}, monitor->m_size};

        // Get configured dim opacity
        static auto* const PDIMOPACITY = (Hyprlang::FLOAT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE,
                "plugin:mouse_gestures:dim_opacity"
            )->getDataStaticPtr();

        float dimOpacity = 0.2f;  // Default fallback
        if (PDIMOPACITY && *PDIMOPACITY) {
            dimOpacity = static_cast<float>(**PDIMOPACITY);
            // Clamp opacity between 0.0 and 1.0
            if (dimOpacity < 0.0f) dimOpacity = 0.0f;
            if (dimOpacity > 1.0f) dimOpacity = 1.0f;
        }

        // Render the dimming overlay
        // Using a dark color to dim the screen while keeping windows visible
        CHyprColor dimColor{0.0, 0.0, 0.0, dimOpacity};

        // Create a damage region covering the entire screen
        CRegion fullDamage{0, 0, INT16_MAX, INT16_MAX};

        g_pHyprOpenGL->renderRect(overlayBox, dimColor, {.damage = &fullDamage});

    } catch (const std::exception& e) {
        // Silently catch and ignore errors to prevent compositor crash
    } catch (...) {
        // Catch any other unexpected errors
    }
}

bool CRecordModePassElement::needsLiveBlur() {
    try {
        return false;
    } catch (...) {
        return false;
    }
}

bool CRecordModePassElement::needsPrecomputeBlur() {
    try {
        return false;
    } catch (...) {
        return false;
    }
}

std::optional<CBox> CRecordModePassElement::boundingBox() {
    try {
        // Get the monitor this overlay is bound to
        auto monitor = pMonitor.lock();
        if (!monitor) {
            // Return a valid empty box instead of nullopt to avoid skipping render
            return CBox{{0, 0}, {0, 0}};
        }

        // Return the full monitor size as the bounding box
        return CBox{{0, 0}, monitor->m_size};

    } catch (const std::exception& e) {
        // Return a valid empty box on error
        return CBox{{0, 0}, {0, 0}};
    } catch (...) {
        // Return a valid empty box on error
        return CBox{{0, 0}, {0, 0}};
    }
}
