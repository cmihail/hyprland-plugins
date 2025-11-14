#include "MouseGestureOverlay.hpp"
#include "stroke.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

extern HANDLE PHANDLE;

// Forward declarations from main.cpp
struct PathPoint {
    Vector2D position;
    std::chrono::steady_clock::time_point timestamp;
};

struct MouseGestureState {
    bool rightButtonPressed;
    Vector2D mouseDownPos;
    bool dragDetected;
    std::vector<Vector2D> path;
    std::vector<PathPoint> timestampedPath;
    std::chrono::steady_clock::time_point pressTime;
    uint32_t pressButton;
    uint32_t pressTimeMs;
};

extern MouseGestureState g_gestureState;
extern bool g_recordMode;
extern bool g_pluginShuttingDown;

// Forward declaration from main.cpp
struct GestureAction {
    Stroke pattern;
    std::string command;
    std::string name;
};

extern std::vector<GestureAction> g_gestureActions;
extern std::unordered_map<PHLMONITOR, float> g_scrollOffsets;
extern std::unordered_map<PHLMONITOR, float> g_maxScrollOffsets;
extern std::string g_configFilePath;

// Animation state for record mode entry
extern std::unordered_map<PHLMONITOR, PHLANIMVAR<Vector2D>> g_recordAnimSize;
extern std::unordered_map<PHLMONITOR, PHLANIMVAR<Vector2D>> g_recordAnimPos;
extern std::unordered_map<PHLMONITOR, bool> g_recordModeClosing;

// Animation state for individual gesture add/remove
extern std::unordered_map<size_t, PHLANIMVAR<float>> g_gestureScaleAnims;
extern std::unordered_map<size_t, PHLANIMVAR<float>> g_gestureAlphaAnims;
extern std::unordered_set<size_t> g_gesturesPendingRemoval;

// Background texture
extern SP<CTexture> g_pBackgroundTexture;

CMouseGestureOverlay::CMouseGestureOverlay(PHLMONITOR monitor) : pMonitor(monitor) {
    // Store the monitor this overlay is for
}

CMouseGestureOverlay::~CMouseGestureOverlay() {
    // Default destructor
}

void CMouseGestureOverlay::draw(const CRegion& damage) {
    try {
        // Safety checks
        if (g_pluginShuttingDown || !g_pHyprOpenGL)
            return;

        auto monitor = pMonitor.lock();
        if (!monitor)
            return;

        auto currentMonitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
        if (!currentMonitor || currentMonitor != monitor)
            return;

        const Vector2D monitorSize = monitor->m_size;
        const float monScale = monitor->m_scale;

        // Render record mode UI
        if (g_recordMode) {
            renderBackground(monitor, monScale);
            renderRecordModeUI(monitor);
        }

        // Render gesture trail
        renderGestureTrail(monitor, monitorSize);

    } catch (const std::bad_alloc&) {
        // Handle memory allocation failures
    } catch (...) {
        // Catch any other unexpected errors
    }
}

bool CMouseGestureOverlay::needsLiveBlur() {
    try {
        return false;
    } catch (...) {
        return false;
    }
}

bool CMouseGestureOverlay::needsPrecomputeBlur() {
    try {
        return false;
    } catch (...) {
        return false;
    }
}

std::optional<CBox> CMouseGestureOverlay::boundingBox() {
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

void CMouseGestureOverlay::renderBackground(PHLMONITOR monitor, float monScale) {
    // Clear to single background color (like workspace-overview)
    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

    // Render background image if loaded
    if (!g_pBackgroundTexture || g_pBackgroundTexture->m_texID == 0)
        return;

    const Vector2D monitorSize = monitor->m_size;
    const Vector2D texSize = g_pBackgroundTexture->m_size;
    CBox bgBox = {{0, 0}, monitorSize};

    const float monitorAspect = monitorSize.x / monitorSize.y;
    const float textureAspect = texSize.x / texSize.y;

    if (textureAspect > monitorAspect) {
        const float scale = monitorSize.y / texSize.y;
        const float scaledWidth = texSize.x * scale;
        bgBox.x = -(scaledWidth - monitorSize.x) / 2.0f;
        bgBox.w = scaledWidth;
    } else {
        const float scale = monitorSize.x / texSize.x;
        const float scaledHeight = texSize.y * scale;
        bgBox.y = -(scaledHeight - monitorSize.y) / 2.0f;
        bgBox.h = scaledHeight;
    }

    bgBox.scale(monScale);
    bgBox.round();

    g_pHyprOpenGL->renderTexture(g_pBackgroundTexture, bgBox, {});
}

CHyprColor CMouseGestureOverlay::interpolateColor(const CHyprColor& start,
                                                  const CHyprColor& end,
                                                  float t) {
    CHyprColor result;
    result.r = start.r + (end.r - start.r) * t;
    result.g = start.g + (end.g - start.g) * t;
    result.b = start.b + (end.b - start.b) * t;
    result.a = start.a + (end.a - start.a) * t;
    return result;
}

void CMouseGestureOverlay::renderBoxBorders(float x, float y, float size,
                                            const CHyprColor& color,
                                            float borderSize,
                                            const CRegion& damage) {
    CBox topBorder = {x, y, size, borderSize};
    g_pHyprOpenGL->renderRect(topBorder, color, {.damage = &damage});

    CBox bottomBorder = {x, y + size - borderSize, size, borderSize};
    g_pHyprOpenGL->renderRect(bottomBorder, color, {.damage = &damage});

    CBox leftBorder = {x, y, borderSize, size};
    g_pHyprOpenGL->renderRect(leftBorder, color, {.damage = &damage});

    CBox rightBorder = {x + size - borderSize, y, borderSize, size};
    g_pHyprOpenGL->renderRect(rightBorder, color, {.damage = &damage});
}

void CMouseGestureOverlay::renderGesturePattern(float x, float y, float size,
                                                const std::vector<Point>& points,
                                                const TrailConfig& config,
                                                const CRegion& damage) {
    constexpr float INNER_PADDING = 10.0f;
    const float drawWidth = size - 2 * INNER_PADDING;
    const float drawHeight = size - 2 * INNER_PADDING;
    const size_t numPoints = points.size();
    const float pointRadius = config.circleRadius;

    for (size_t i = 0; i < numPoints; ++i) {
        const auto& point = points[i];
        float px = x + INNER_PADDING + point.x * drawWidth;
        float py = y + INNER_PADDING + point.y * drawHeight;

        float pathPosition = (numPoints > 1) ?
            static_cast<float>(i) / static_cast<float>(numPoints - 1) : 0.0f;

        CHyprColor gestureColor = interpolateColor(config.startColor,
                                                   config.endColor,
                                                   pathPosition);
        gestureColor.a = 1.0f;

        CBox pointBox = CBox{{px - pointRadius, py - pointRadius},
                            {pointRadius * 2, pointRadius * 2}};

        g_pHyprOpenGL->renderRect(pointBox, gestureColor,
                                 {.damage = &damage,
                                  .round = static_cast<int>(pointRadius)});
    }
}

void CMouseGestureOverlay::renderRecordSquare(const Vector2D& pos,
                                               const Vector2D& size,
                                               const CRegion& damage) {
    CHyprColor rectBgColor{0.2, 0.2, 0.2, 1.0};
    CHyprColor borderColor{0.4, 0.4, 0.4, 1.0};
    constexpr float BORDER_SIZE = 2.0f;

    // Render background
    CBox recordRect = CBox{pos, size};
    g_pHyprOpenGL->renderRect(recordRect, rectBgColor, {.damage = &damage});

    // Render borders
    CBox topBorder = {pos.x, pos.y, size.x, BORDER_SIZE};
    g_pHyprOpenGL->renderRect(topBorder, borderColor, {.damage = &damage});

    CBox bottomBorder = {pos.x, pos.y + size.y - BORDER_SIZE,
                        size.x, BORDER_SIZE};
    g_pHyprOpenGL->renderRect(bottomBorder, borderColor, {.damage = &damage});

    CBox leftBorder = {pos.x, pos.y, BORDER_SIZE, size.y};
    g_pHyprOpenGL->renderRect(leftBorder, borderColor, {.damage = &damage});

    CBox rightBorder = {pos.x + size.x - BORDER_SIZE, pos.y,
                       BORDER_SIZE, size.y};
    g_pHyprOpenGL->renderRect(rightBorder, borderColor, {.damage = &damage});
}

void CMouseGestureOverlay::renderDeleteButton(float x, float y, float size,
                                               const CRegion& damage,
                                               float alpha) {
    // Safety check - only render in record mode
    if (!g_recordMode || g_pluginShuttingDown) {
        return;
    }

    // Create a larger circular button with red-ish color and X marker
    const float circleSize = 36.0f; // Larger button for better visibility
    const float margin = 4.0f; // Margin from top and right edges

    CBox bgBox;
    bgBox.x = x + size - circleSize - margin; // Top-right corner with margin
    bgBox.y = y + margin;
    bgBox.w = circleSize;
    bgBox.h = circleSize;

    // Red-ish background color with applied alpha
    CHyprColor deleteButtonColor{0.85, 0.2, 0.2, 0.9 * alpha}; // Brighter red, more opaque
    g_pHyprOpenGL->renderRect(bgBox, deleteButtonColor,
                              {.damage = &damage,
                               .round = static_cast<int>(circleSize / 2)});

    // Draw X marker with smoother lines
    const float xSize = circleSize * 0.5f; // Larger X relative to button
    const float centerX = bgBox.x + circleSize / 2.0f;
    const float centerY = bgBox.y + circleSize / 2.0f;
    const float lineWidth = 1.75f; // Half of previous thickness
    const float lineLength = xSize;

    // First diagonal: top-left to bottom-right (45 degrees)
    // Draw as a rotated rectangle for smoother appearance
    const float halfWidth = lineWidth / 2.0f;
    const float halfLength = lineLength / 2.0f;

    // Calculate the four corners of the first diagonal line
    const float cos45 = 0.7071067811865476f; // cos(45°)
    const float sin45 = 0.7071067811865476f; // sin(45°)

    // First diagonal line (top-left to bottom-right)
    for (float offset = -halfWidth; offset <= halfWidth; offset += 0.5f) {
        for (float t = -halfLength; t <= halfLength; t += 0.5f) {
            float px = centerX + t * cos45 - offset * sin45;
            float py = centerY + t * sin45 + offset * cos45;
            CBox pointBox = CBox{{px - 0.5f, py - 0.5f}, {1.0f, 1.0f}};
            g_pHyprOpenGL->renderRect(pointBox, CHyprColor{1.0, 1.0, 1.0, alpha},
                                     {.damage = &damage});
        }
    }

    // Second diagonal line (top-right to bottom-left)
    const float cos135 = -0.7071067811865476f; // cos(135°)
    const float sin135 = 0.7071067811865476f;  // sin(135°)

    for (float offset = -halfWidth; offset <= halfWidth; offset += 0.5f) {
        for (float t = -halfLength; t <= halfLength; t += 0.5f) {
            float px = centerX + t * cos135 - offset * sin135;
            float py = centerY + t * sin135 + offset * cos135;
            CBox pointBox = CBox{{px - 0.5f, py - 0.5f}, {1.0f, 1.0f}};
            g_pHyprOpenGL->renderRect(pointBox, CHyprColor{1.0, 1.0, 1.0, alpha},
                                     {.damage = &damage});
        }
    }
}

// Helper to get animation values for a gesture
static void getGestureAnimationValues(size_t gestureIndex, float& scale,
                                       float& alpha) {
    scale = 1.0f;
    alpha = 1.0f;

    try {
        if (g_gestureScaleAnims.count(gestureIndex) &&
            g_gestureScaleAnims[gestureIndex]) {
            scale = std::clamp(g_gestureScaleAnims[gestureIndex]->value(),
                              0.0f, 1.0f);
        }
    } catch (...) {
        scale = 1.0f;
    }

    try {
        if (g_gestureAlphaAnims.count(gestureIndex) &&
            g_gestureAlphaAnims[gestureIndex]) {
            alpha = std::clamp(g_gestureAlphaAnims[gestureIndex]->value(),
                              0.0f, 1.0f);
        }
    } catch (...) {
        alpha = 1.0f;
    }
}

void CMouseGestureOverlay::renderGestureSquare(float x, float y, float size,
                                                size_t gestureIndex,
                                                const CRegion& damage) {
    try {
        if (gestureIndex >= g_gestureActions.size())
            return;

        float scale, alpha;
        getGestureAnimationValues(gestureIndex, scale, alpha);

        // Skip rendering if fully transparent or invisible
        if (alpha <= 0.01f || scale <= 0.01f)
            return;

        // Apply scale transform from center
        const float centerX = x + size / 2.0f;
        const float centerY = y + size / 2.0f;
        const float scaledSize = size * scale;
        const float scaledX = centerX - scaledSize / 2.0f;
        const float scaledY = centerY - scaledSize / 2.0f;

        // Apply alpha to colors
        CHyprColor rectBgColor{0.2, 0.2, 0.2, alpha};
        CHyprColor borderColor{0.4, 0.4, 0.4, alpha};
        constexpr float BORDER_SIZE = 2.0f;

        // Render background with scaled box
        CBox gestureBox = CBox{{scaledX, scaledY}, {scaledSize, scaledSize}};
        g_pHyprOpenGL->renderRect(gestureBox, rectBgColor, {.damage = &damage});

        // Render borders with scaled box
        const float scaledBorderSize = BORDER_SIZE * scale;
        renderBoxBorders(scaledX, scaledY, scaledSize, borderColor, scaledBorderSize, damage);

        // Double-check gesture still exists
        if (gestureIndex >= g_gestureActions.size())
            return;

        const auto& gesture = g_gestureActions[gestureIndex];
        if (!gesture.pattern.isFinished())
            return;

        auto config = getTrailConfig();

        // Apply alpha to trail colors
        config.startColor.a *= alpha;
        config.endColor.a *= alpha;

        renderGesturePattern(scaledX, scaledY, scaledSize,
                             gesture.pattern.getPoints(), config, damage);

        // Render delete button on top-right corner (only in record mode)
        if (g_recordMode) {
            renderDeleteButton(scaledX, scaledY, scaledSize, damage, alpha);
        }
    } catch (const std::exception& e) {
        // Silently fail to avoid crashing Hyprland
    } catch (...) {
        // Catch everything to avoid crashing Hyprland
    }
}

void CMouseGestureOverlay::renderRecordModeUI(PHLMONITOR monitor) {
    constexpr float PADDING = 20.0f;
    constexpr float GAP_WIDTH = 10.0f;
    constexpr int VISIBLE_GESTURES = 3;
    constexpr float TEXT_HEIGHT = 80.0f;
    constexpr float TEXT_GAP = 20.0f;
    constexpr float BOTTOM_MARGIN = 20.0f;

    const Vector2D monitorSize = monitor->m_size;
    CRegion fullDamage{0, 0, INT16_MAX, INT16_MAX};

    // Check if we have animation for this monitor
    Vector2D currentSize = monitorSize;
    Vector2D currentPos = {0, 0};
    float zoomScale = 1.0f;

    if (g_recordAnimSize.count(monitor) && g_recordAnimPos.count(monitor)) {
        auto& sizeVar = g_recordAnimSize[monitor];
        auto& posVar = g_recordAnimPos[monitor];
        if (sizeVar && posVar) {
            currentSize = sizeVar->value();
            currentPos = posVar->value();
            zoomScale = currentSize.x / monitorSize.x;
        }
    }

    // Calculate layout dimensions
    const float verticalSpace = monitorSize.y - (2.0f * PADDING);
    const float totalGaps = (VISIBLE_GESTURES - 1) * GAP_WIDTH;
    const float baseHeight = (verticalSpace - totalGaps) / VISIBLE_GESTURES;
    const float gestureRectHeight = baseHeight * 0.9f;
    const float gestureRectWidth = gestureRectHeight;
    // Use original vertical space for layout calculation
    const float recordSquareLayoutSize = verticalSpace;
    const float totalWidth = gestureRectWidth + recordSquareLayoutSize;
    const float horizontalMargin = (monitorSize.x - totalWidth) / 3.0f;
    // Rectangle extends from below the text to the bottom margin
    const float maxVerticalSize = monitorSize.y - (PADDING + TEXT_HEIGHT +
                                                    TEXT_GAP) - BOTTOM_MARGIN;
    // Also ensure it fits horizontally
    const float maxHorizontalSize = monitorSize.x - (horizontalMargin +
                                     gestureRectWidth + horizontalMargin);
    const float recordSquareSize = std::min(maxVerticalSize, maxHorizontalSize);

    // Apply animation transform to record square
    Vector2D recordPos = {
        horizontalMargin + gestureRectWidth + horizontalMargin,
        PADDING + TEXT_HEIGHT + TEXT_GAP
    };
    Vector2D recordSize = {recordSquareSize, recordSquareSize};

    // Transform the position and size based on animation
    Vector2D transformedRecordPos = {
        recordPos.x * zoomScale + currentPos.x,
        recordPos.y * zoomScale + currentPos.y
    };
    Vector2D transformedRecordSize = {
        recordSize.x * zoomScale,
        recordSize.y * zoomScale
    };

    // Render text above the record square
    const float textX = recordPos.x;
    const float textY = PADDING;
    const float textWidth = recordSquareSize;

    // Create textures for the two text lines
    SP<CTexture> line1Texture = makeShared<CTexture>();
    SP<CTexture> line2Texture = makeShared<CTexture>();

    // Render first line: "Register a new gesture."
    const std::string line1Text = "Register a new gesture.";
    Vector2D line1BufferSize = {textWidth, TEXT_HEIGHT / 2.0f};
    renderText(line1Texture, line1Text, CHyprColor{1.0, 1.0, 1.0, 1.0},
              line1BufferSize, monitor->m_scale, 18);

    // Render second line: "Config file: <path>"
    std::string line2Text = "Config file: ";
    if (!g_configFilePath.empty()) {
        line2Text += g_configFilePath;
    } else {
        line2Text += "not set";
    }
    Vector2D line2BufferSize = {textWidth, TEXT_HEIGHT / 2.0f};
    renderText(line2Texture, line2Text, CHyprColor{0.8, 0.8, 0.8, 1.0},
              line2BufferSize, monitor->m_scale, 14);

    // Transform text positions based on animation
    Vector2D transformedTextPos1 = {
        textX * zoomScale + currentPos.x,
        textY * zoomScale + currentPos.y
    };
    Vector2D transformedTextPos2 = {
        textX * zoomScale + currentPos.x,
        (textY + TEXT_HEIGHT / 2.0f) * zoomScale + currentPos.y
    };

    // Render the text textures
    if (line1Texture && line1Texture->m_texID != 0) {
        CBox line1Box = {transformedTextPos1,
                        {textWidth * zoomScale, (TEXT_HEIGHT / 2.0f) * zoomScale}};
        g_pHyprOpenGL->renderTexture(line1Texture, line1Box, {});
    }
    if (line2Texture && line2Texture->m_texID != 0) {
        CBox line2Box = {transformedTextPos2,
                        {textWidth * zoomScale, (TEXT_HEIGHT / 2.0f) * zoomScale}};
        g_pHyprOpenGL->renderTexture(line2Texture, line2Box, {});
    }

    renderRecordSquare(transformedRecordPos, transformedRecordSize, fullDamage);

    // Calculate scroll offset for this monitor
    const size_t totalGestures = g_gestureActions.size();
    float& maxScrollOffset = g_maxScrollOffsets[monitor];
    float& scrollOffset = g_scrollOffsets[monitor];

    if (totalGestures > VISIBLE_GESTURES) {
        const float totalHeight = totalGestures * (gestureRectHeight + GAP_WIDTH);
        maxScrollOffset = totalHeight - verticalSpace;
    } else {
        maxScrollOffset = 0.0f;
    }

    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);

    // Render gesture squares with animation transform
    for (size_t i = 0; i < totalGestures; ++i) {
        const float yPos = PADDING + i * (gestureRectHeight + GAP_WIDTH) -
                          scrollOffset;

        if (yPos + gestureRectHeight < 0 || yPos > monitorSize.y)
            continue;

        // Transform the position and size
        float transformedX = horizontalMargin * zoomScale + currentPos.x;
        float transformedY = yPos * zoomScale + currentPos.y;
        float transformedSize = gestureRectWidth * zoomScale;

        renderGestureSquare(transformedX, transformedY, transformedSize, i,
                           fullDamage);
    }
}

void CMouseGestureOverlay::renderGestureTrail(PHLMONITOR monitor,
                                               const Vector2D& monitorSize) {
    // Only render trail if there are points and drag was detected
    if (g_gestureState.timestampedPath.empty() ||
        (!g_gestureState.dragDetected && g_gestureState.rightButtonPressed))
        return;

    auto config = getTrailConfig();
    auto now = std::chrono::steady_clock::now();
    const size_t numPoints = g_gestureState.timestampedPath.size();

    for (size_t i = 0; i < numPoints; ++i) {
        const auto& point = g_gestureState.timestampedPath[i];
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - point.timestamp
        ).count();

        if (age > config.fadeDurationMs)
            continue;

        // Calculate position along the path (0.0 at start, 1.0 at end)
        float pathPosition = (numPoints > 1) ?
            static_cast<float>(i) / static_cast<float>(numPoints - 1) : 0.0f;

        // Interpolate between start and end colors
        CHyprColor color = interpolateColor(config.startColor,
                                           config.endColor,
                                           pathPosition);

        // Apply alpha fade based on time
        float alpha = 1.0f - (static_cast<float>(age) / config.fadeDurationMs);
        color.a = alpha;

        Vector2D localPos = point.position - monitor->m_position;
        CBox circleBox = CBox{
            {localPos.x - config.circleRadius, localPos.y - config.circleRadius},
            {config.circleRadius * 2, config.circleRadius * 2}
        };

        g_pHyprOpenGL->renderRect(circleBox, color, {
            .round = static_cast<int>(config.circleRadius)
        });
    }
}

void CMouseGestureOverlay::renderText(SP<CTexture> out,
                                       const std::string& text,
                                       const CHyprColor& color,
                                       const Vector2D& bufferSize,
                                       float scale,
                                       int fontSize) {
    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                         bufferSize.x,
                                                         bufferSize.y);
    const auto CAIRO = cairo_create(CAIROSURFACE);

    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    PangoFontDescription* fontDesc = pango_font_description_from_string("Sans");

    pango_font_description_set_size(fontDesc, fontSize * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_text(layout, text.c_str(), -1);

    int textW, textH;
    pango_layout_get_size(layout, &textW, &textH);
    textW /= PANGO_SCALE;
    textH /= PANGO_SCALE;

    cairo_set_source_rgba(CAIRO, color.r, color.g, color.b, color.a);
    cairo_move_to(CAIRO, (bufferSize.x - textW) / 2.0, (bufferSize.y - textH) / 2.0);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);
    pango_font_description_free(fontDesc);

    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    out->allocate();
    glBindTexture(GL_TEXTURE_2D, out->m_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize.x, bufferSize.y, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

CMouseGestureOverlay::TrailConfig CMouseGestureOverlay::getTrailConfig() {
    static auto* const PCIRCLERADIUS = (Hyprlang::FLOAT* const*)
        HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:mouse_gestures:drag_trail_circle_radius"
        )->getDataStaticPtr();

    static auto* const PFADEDURATION = (Hyprlang::INT* const*)
        HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:mouse_gestures:trail_fade_duration_ms"
        )->getDataStaticPtr();

    static auto* const PTRAILCOLOR = (Hyprlang::INT* const*)
        HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:mouse_gestures:drag_trail_color"
        )->getDataStaticPtr();

    static auto* const PTRAILENDCOLOR = (Hyprlang::INT* const*)
        HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:mouse_gestures:drag_trail_end_color"
        )->getDataStaticPtr();

    CHyprColor startColor = (PTRAILCOLOR && *PTRAILCOLOR) ?
        CHyprColor{(uint32_t)**PTRAILCOLOR} :
        CHyprColor{0x4C7FA6FF};

    CHyprColor endColor = (PTRAILENDCOLOR && *PTRAILENDCOLOR) ?
        CHyprColor{(uint32_t)**PTRAILENDCOLOR} :
        CHyprColor{0xA64C7FFF};

    return {
        .circleRadius = (PCIRCLERADIUS && *PCIRCLERADIUS) ?
                       static_cast<float>(**PCIRCLERADIUS) : 8.0f,
        .fadeDurationMs = (PFADEDURATION && *PFADEDURATION) ?
                         **PFADEDURATION : 300,
        .startColor = startColor,
        .endColor = endColor
    };
}
