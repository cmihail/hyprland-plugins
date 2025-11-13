#include "MouseGestureOverlay.hpp"
#include "stroke.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <chrono>
#include <cmath>

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
                                               const CRegion& damage) {
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

    // Red-ish background color
    CHyprColor deleteButtonColor{0.85, 0.2, 0.2, 0.9}; // Brighter red, more opaque
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
            g_pHyprOpenGL->renderRect(pointBox, CHyprColor{1.0, 1.0, 1.0, 1.0},
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
            g_pHyprOpenGL->renderRect(pointBox, CHyprColor{1.0, 1.0, 1.0, 1.0},
                                     {.damage = &damage});
        }
    }
}

void CMouseGestureOverlay::renderGestureSquare(float x, float y, float size,
                                                size_t gestureIndex,
                                                const CRegion& damage) {
    CHyprColor rectBgColor{0.2, 0.2, 0.2, 1.0};
    CHyprColor borderColor{0.4, 0.4, 0.4, 1.0};
    constexpr float BORDER_SIZE = 2.0f;
    constexpr float INNER_PADDING = 10.0f;

    // Get trail config for color and radius
    auto config = getTrailConfig();
    const CHyprColor gestureColor = config.color;
    const float pointRadius = config.circleRadius;

    // Render background
    CBox gestureBox = CBox{{x, y}, {size, size}};
    g_pHyprOpenGL->renderRect(gestureBox, rectBgColor, {.damage = &damage});

    // Render borders
    CBox topBorder = {x, y, size, BORDER_SIZE};
    g_pHyprOpenGL->renderRect(topBorder, borderColor, {.damage = &damage});

    CBox bottomBorder = {x, y + size - BORDER_SIZE, size, BORDER_SIZE};
    g_pHyprOpenGL->renderRect(bottomBorder, borderColor, {.damage = &damage});

    CBox leftBorder = {x, y, BORDER_SIZE, size};
    g_pHyprOpenGL->renderRect(leftBorder, borderColor, {.damage = &damage});

    CBox rightBorder = {x + size - BORDER_SIZE, y, BORDER_SIZE, size};
    g_pHyprOpenGL->renderRect(rightBorder, borderColor, {.damage = &damage});

    // Render gesture pattern if it exists
    if (gestureIndex >= g_gestureActions.size())
        return;

    const auto& gesture = g_gestureActions[gestureIndex];
    if (!gesture.pattern.isFinished())
        return;

    const auto& points = gesture.pattern.getPoints();
    const float drawWidth = size - 2 * INNER_PADDING;
    const float drawHeight = size - 2 * INNER_PADDING;

    for (const auto& point : points) {
        float px = x + INNER_PADDING + point.x * drawWidth;
        float py = y + INNER_PADDING + point.y * drawHeight;

        CBox pointBox = CBox{{px - pointRadius, py - pointRadius},
                            {pointRadius * 2, pointRadius * 2}};

        g_pHyprOpenGL->renderRect(pointBox, gestureColor,
                                 {.damage = &damage,
                                  .round = static_cast<int>(pointRadius)});
    }

    // Render delete button on top-right corner (only in record mode)
    if (g_recordMode) {
        renderDeleteButton(x, y, size, damage);
    }
}

void CMouseGestureOverlay::renderRecordModeUI(PHLMONITOR monitor) {
    constexpr float PADDING = 20.0f;
    constexpr float GAP_WIDTH = 10.0f;
    constexpr int VISIBLE_GESTURES = 3;

    const Vector2D monitorSize = monitor->m_size;
    CRegion fullDamage{0, 0, INT16_MAX, INT16_MAX};

    // Calculate layout dimensions
    const float verticalSpace = monitorSize.y - (2.0f * PADDING);
    const float totalGaps = (VISIBLE_GESTURES - 1) * GAP_WIDTH;
    const float baseHeight = (verticalSpace - totalGaps) / VISIBLE_GESTURES;
    const float gestureRectHeight = baseHeight * 0.9f;
    const float gestureRectWidth = gestureRectHeight;
    const float recordSquareSize = verticalSpace;
    const float totalWidth = gestureRectWidth + recordSquareSize;
    const float horizontalMargin = (monitorSize.x - totalWidth) / 3.0f;

    // Render right record square
    const Vector2D recordPos = {
        horizontalMargin + gestureRectWidth + horizontalMargin,
        PADDING
    };
    const Vector2D recordSize = {recordSquareSize, recordSquareSize};
    renderRecordSquare(recordPos, recordSize, fullDamage);

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

    // Render gesture squares
    for (size_t i = 0; i < totalGestures; ++i) {
        const float yPos = PADDING + i * (gestureRectHeight + GAP_WIDTH) -
                          scrollOffset;

        if (yPos + gestureRectHeight < 0 || yPos > monitorSize.y)
            continue;

        renderGestureSquare(horizontalMargin, yPos, gestureRectWidth, i,
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

    for (const auto& point : g_gestureState.timestampedPath) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - point.timestamp
        ).count();

        if (age > config.fadeDurationMs)
            continue;

        float alpha = 1.0f - (static_cast<float>(age) / config.fadeDurationMs);
        CHyprColor color = config.color;
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

    CHyprColor trailColor = (PTRAILCOLOR && *PTRAILCOLOR) ?
        CHyprColor{(uint32_t)**PTRAILCOLOR} :
        CHyprColor{0x4C7FA6FF};

    return {
        .circleRadius = (PCIRCLERADIUS && *PCIRCLERADIUS) ?
                       static_cast<float>(**PCIRCLERADIUS) : 8.0f,
        .fadeDurationMs = (PFADEDURATION && *PFADEDURATION) ?
                         **PFADEDURATION : 300,
        .color = trailColor
    };
}
