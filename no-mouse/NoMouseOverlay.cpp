#define WLR_USE_UNSTABLE

#include "NoMouseOverlay.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <fstream>
#include <unordered_map>

// Debug logging
static void debugLog(const std::string& msg) {
    std::ofstream logFile("/tmp/debug", std::ios::app);
    logFile << msg << std::endl;
    logFile.close();
}

// Global cache for text textures (key: "row,col", value: texture)
// Caching prevents recreating 676 Cairo surfaces every frame
static std::unordered_map<std::string, SP<CTexture>> g_labelTextureCache;

CNoMouseOverlay::CNoMouseOverlay(PHLMONITOR monitor) : m_pMonitor(monitor) {}

// Helper function to create a texture from Cairo surface
static SP<CTexture> createTextureFromCairoSurface(cairo_surface_t* surface, int width, int height) {
    const auto DATA = cairo_image_surface_get_data(surface);
    const auto STRIDE = cairo_image_surface_get_stride(surface);

    std::vector<uint8_t> pixelData(width * height * 4);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const auto SRC = DATA + y * STRIDE + x * 4;
            auto dst = pixelData.data() + (y * width + x) * 4;

            // Cairo uses BGRA, convert to RGBA
            dst[0] = SRC[2]; // R
            dst[1] = SRC[1]; // G
            dst[2] = SRC[0]; // B
            dst[3] = SRC[3]; // A
        }
    }

    const uint32_t drmFormat = DRM_FORMAT_ABGR8888;
    const uint32_t textureStride = width * 4;

    return makeShared<CTexture>(drmFormat, pixelData.data(), textureStride,
                                Vector2D{(double)width, (double)height}, true);
}

// Helper function to render text to a texture
static SP<CTexture> renderTextToTexture(const std::string& text, int fontSize,
                                        float r, float g, float b, float a) {
    // Create Cairo surface
    const int width = 60;
    const int height = 40;

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(surface);

    // Clear to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Setup Pango
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* fontDesc = pango_font_description_from_string("Sans");
    pango_font_description_set_size(fontDesc, fontSize * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_text(layout, text.c_str(), -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

    // Get text dimensions
    int textWidth, textHeight;
    pango_layout_get_pixel_size(layout, &textWidth, &textHeight);

    // Center the text
    cairo_move_to(cr, (width - textWidth) / 2.0, (height - textHeight) / 2.0);

    // Draw text
    cairo_set_source_rgba(cr, r, g, b, a);
    pango_cairo_show_layout(cr, layout);

    // Create texture
    auto texture = createTextureFromCairoSurface(surface, width, height);

    // Cleanup
    pango_font_description_free(fontDesc);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return texture;
}

void CNoMouseOverlay::draw(const CRegion& damage) {
    if (!m_pMonitor || !g_pHyprOpenGL) {
        return;
    }

    const auto monitorSize = m_pMonitor->m_size;

    // Read opacity config values
    static auto* const POPACITY_BG = (Hyprlang::FLOAT* const*)
        HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:no_mouse:opacity_background"
        )->getDataStaticPtr();
    static auto* const POPACITY_GRID = (Hyprlang::FLOAT* const*)
        HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:no_mouse:opacity_grid_lines"
        )->getDataStaticPtr();
    static auto* const POPACITY_LABELS = (Hyprlang::FLOAT* const*)
        HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:no_mouse:opacity_labels"
        )->getDataStaticPtr();

    const float bgOpacity = (POPACITY_BG && *POPACITY_BG) ? **POPACITY_BG : 0.1f;
    const float gridOpacity = (POPACITY_GRID && *POPACITY_GRID) ?
        **POPACITY_GRID : 0.25f;
    const float labelOpacity = (POPACITY_LABELS && *POPACITY_LABELS) ?
        **POPACITY_LABELS : 0.4f;

    // Draw background overlay
    const auto monitorBox = CBox{0, 0, monitorSize.x, monitorSize.y};
    const CHyprColor overlayColor = CHyprColor(0.0f, 0.0f, 0.0f, bgOpacity);
    g_pHyprOpenGL->renderRect(monitorBox, overlayColor, {});

    // Grid configuration
    constexpr int GRID_SIZE = 26;
    constexpr int SUB_GRID_ROWS = 3; // 3x6 sub-grid (18 positions, A-R)
    constexpr int SUB_GRID_COLS = 6;
    const float cellWidth = monitorSize.x / (float)GRID_SIZE;
    const float cellHeight = monitorSize.y / (float)GRID_SIZE;

    // Grid line color (white with configurable opacity)
    const CHyprColor gridLineColor = CHyprColor(1.0f, 1.0f, 1.0f, gridOpacity);
    const float lineThickness = 1.0f;

    // Draw vertical grid lines
    for (int i = 1; i < GRID_SIZE; i++) {
        const float x = i * cellWidth;
        CBox line = {x, 0, lineThickness, monitorSize.y};
        g_pHyprOpenGL->renderRect(line, gridLineColor, {});
    }

    // Draw horizontal grid lines
    for (int i = 1; i < GRID_SIZE; i++) {
        const float y = i * cellHeight;
        CBox line = {0, y, monitorSize.x, lineThickness};
        g_pHyprOpenGL->renderRect(line, gridLineColor, {});
    }

    // Draw labels for each cell (A-Z for columns, A-Z for rows)
    // We'll draw labels at the top-left corner of each cell
    for (int row = 0; row < GRID_SIZE; row++) {
        for (int col = 0; col < GRID_SIZE; col++) {
            // Skip drawing label for pending cell (sub-cells will be shown instead)
            if (g_hasPendingCell && row == g_pendingRow && col == g_pendingCol) {
                continue;
            }

            // Create label: first char is row (A-Z), three spaces, second char is column (A-Z)
            char label[6];
            label[0] = 'A' + row;
            label[1] = ' ';
            label[2] = ' ';
            label[3] = ' ';
            label[4] = 'A' + col;
            label[5] = '\0';

            // Calculate cell top-left corner with small padding
            const float cellX = col * cellWidth + 5.0f; // 5px padding from left
            const float cellY = row * cellHeight + 5.0f; // 5px padding from top

            // Check cache first to avoid recreating textures every frame
            std::string cacheKey = std::to_string(row) + "," + std::to_string(col);
            SP<CTexture> textTexture;

            auto it = g_labelTextureCache.find(cacheKey);
            if (it != g_labelTextureCache.end()) {
                // Use cached texture
                textTexture = it->second;
            } else {
                // Render and cache new texture
                textTexture = renderTextToTexture(
                    std::string(label), 11, 1.0f, 1.0f, 1.0f, labelOpacity
                );
                if (textTexture) {
                    g_labelTextureCache[cacheKey] = textTexture;
                }
            }

            if (textTexture) {
                // Position the text at the top-left corner
                CBox textBox = {cellX, cellY, 60.0f, 40.0f};
                g_pHyprOpenGL->renderTexture(textTexture, textBox, {});
            }
        }
    }

    // Highlight pending cell if user has typed 2 letters (waiting for SPACE)
    if (g_hasPendingCell && g_pendingRow >= 0 && g_pendingRow < GRID_SIZE &&
        g_pendingCol >= 0 && g_pendingCol < GRID_SIZE) {

        debugLog("RENDER: hasPendingCell=true, row=" + std::to_string(g_pendingRow) +
                 ", col=" + std::to_string(g_pendingCol) +
                 ", hasSubColumn=" + std::to_string(g_hasSubColumn) +
                 ", subColumn=" + std::to_string(g_subColumn));

        const float cellX = g_pendingCol * cellWidth;
        const float cellY = g_pendingRow * cellHeight;

        // Read border color config
        static auto* const PBORDER_COLOR = (Hyprlang::INT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE, "plugin:no_mouse:border_color"
            )->getDataStaticPtr();
        static auto* const PBORDER_OPACITY = (Hyprlang::FLOAT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE, "plugin:no_mouse:border_opacity"
            )->getDataStaticPtr();

        const int64_t borderColorHex = (PBORDER_COLOR && *PBORDER_COLOR) ?
            **PBORDER_COLOR : 0x4C7FA6;
        const float borderOpacity = (PBORDER_OPACITY && *PBORDER_OPACITY) ?
            **PBORDER_OPACITY : 0.8f;

        // Convert hex color (0xRRGGBB) to RGB floats
        const float borderR = ((borderColorHex >> 16) & 0xFF) / 255.0f;
        const float borderG = ((borderColorHex >> 8) & 0xFF) / 255.0f;
        const float borderB = (borderColorHex & 0xFF) / 255.0f;

        // Draw a border around the cell (configurable color)
        const CHyprColor borderColor = CHyprColor(borderR, borderG, borderB, borderOpacity);
        const float borderThickness = 3.0f;

        // Top border
        CBox topBorder = {cellX, cellY, cellWidth, borderThickness};
        g_pHyprOpenGL->renderRect(topBorder, borderColor, {});

        // Bottom border
        CBox bottomBorder = {cellX, cellY + cellHeight - borderThickness,
            cellWidth, borderThickness};
        g_pHyprOpenGL->renderRect(bottomBorder, borderColor, {});

        // Left border
        CBox leftBorder = {cellX, cellY, borderThickness, cellHeight};
        g_pHyprOpenGL->renderRect(leftBorder, borderColor, {});

        // Right border
        CBox rightBorder = {cellX + cellWidth - borderThickness, cellY,
            borderThickness, cellHeight};
        g_pHyprOpenGL->renderRect(rightBorder, borderColor, {});

        // Always show sub-cell labels when a cell is selected (immediately after 2 letters)
        const float subCellWidth = cellWidth / (float)SUB_GRID_COLS;
        const float subCellHeight = cellHeight / (float)SUB_GRID_ROWS;

        // Only draw highlight overlay if a sub-cell is selected (3rd letter typed)
        if (g_hasSubColumn && g_subColumn >= 0 && g_subColumn < SUB_GRID_ROWS * SUB_GRID_COLS) {
            debugLog("RENDER SUB-GRID: Drawing highlight overlay for subColumn=" +
                     std::to_string(g_subColumn));

            // Calculate sub-cell row and column from index (0-17 maps to 3x6 grid)
            const int subRow = g_subColumn / SUB_GRID_COLS; // 0-2
            const int subCol = g_subColumn % SUB_GRID_COLS; // 0-5

            // Read highlight color config
            static auto* const PHIGHLIGHT_COLOR = (Hyprlang::INT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE, "plugin:no_mouse:highlight_color"
                )->getDataStaticPtr();
            static auto* const PHIGHLIGHT_OPACITY = (Hyprlang::FLOAT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE, "plugin:no_mouse:highlight_opacity"
                )->getDataStaticPtr();

            const int64_t highlightColorHex = (PHIGHLIGHT_COLOR && *PHIGHLIGHT_COLOR) ?
                **PHIGHLIGHT_COLOR : 0x4C7FA6;
            const float highlightOpacity = (PHIGHLIGHT_OPACITY && *PHIGHLIGHT_OPACITY) ?
                **PHIGHLIGHT_OPACITY : 0.8f;

            // Convert hex color (0xRRGGBB) to RGB floats
            const float highlightR = ((highlightColorHex >> 16) & 0xFF) / 255.0f;
            const float highlightG = ((highlightColorHex >> 8) & 0xFF) / 255.0f;
            const float highlightB = (highlightColorHex & 0xFF) / 255.0f;

            // Highlight the selected sub-cell (configurable color)
            const float subCellX = cellX + subCol * subCellWidth;
            const float subCellY = cellY + subRow * subCellHeight;
            const CHyprColor subHighlightColor = CHyprColor(
                highlightR, highlightG, highlightB, highlightOpacity
            );
            CBox subCellBox = {subCellX, subCellY, subCellWidth, subCellHeight};
            g_pHyprOpenGL->renderRect(subCellBox, subHighlightColor, {});
        }

        debugLog("RENDER SUB-GRID: Drawing 18 letter labels (A-R) in 3x6 grid");

        // Draw all 18 sub-cell letter labels (A-R) in 3x6 grid layout
        for (int row = 0; row < SUB_GRID_ROWS; row++) {
            for (int col = 0; col < SUB_GRID_COLS; col++) {
                int index = row * SUB_GRID_COLS + col;
                if (index >= 18) break; // Only A-R (18 letters)

                char subLabel[2];
                subLabel[0] = 'A' + index;
                subLabel[1] = '\0';

                // Calculate position for this sub-cell label (center of sub-cell)
                const float subCellX = cellX + col * subCellWidth;
                const float subCellY = cellY + row * subCellHeight;
                const float subLabelX = subCellX + subCellWidth / 2.0f - 12.0f;
                const float subLabelY = subCellY + subCellHeight / 2.0f - 12.0f;

                // Render centered label at center of each sub-cell (11pt font)
                auto subTextTexture = renderTextToTexture(
                    std::string(subLabel), 11, 1.0f, 1.0f, 1.0f, 0.8f
                );

                if (subTextTexture) {
                    CBox subTextBox = {subLabelX, subLabelY, 24.0f, 24.0f};
                    g_pHyprOpenGL->renderTexture(subTextTexture, subTextBox, {});
                }
            }
        }
    }
}

bool CNoMouseOverlay::needsLiveBlur() {
    return false;
}

bool CNoMouseOverlay::needsPrecomputeBlur() {
    return false;
}

std::string CNoMouseOverlay::id() {
    return "CNoMouseOverlay";
}

const char* CNoMouseOverlay::passName() {
    return "CNoMouseOverlay";
}
