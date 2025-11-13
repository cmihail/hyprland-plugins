#include "WindowActionsBar.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/protocols/LayerShell.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <pango/pangocairo.h>
#include <linux/input-event-codes.h>

#include "globals.hpp"
#include "WindowActionsPassElement.hpp"

CWindowActionsBar::CWindowActionsBar(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;

    const auto PMONITOR = pWindow->m_monitor.lock();
    PMONITOR->m_scheduledRecalc = true;

    m_pMouseButtonCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseButton", [&](void* self, SCallbackInfo& info, std::any param) { onMouseButton(info, std::any_cast<IPointer::SButtonEvent>(param)); });
    m_pMouseMoveCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseMove", [&](void* self, SCallbackInfo& info, std::any param) { onMouseMove(std::any_cast<Vector2D>(param)); });
    m_pTouchDownCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchDown", [&](void* self, SCallbackInfo& info, std::any param) { onTouchDown(info, std::any_cast<ITouch::SDownEvent>(param)); });
    m_pTouchUpCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchUp", [&](void* self, SCallbackInfo& info, std::any param) { handleUpEvent(info); });

    // Initialize button textures based on config
    size_t buttonCount = g_pGlobalState->buttons.size();
    m_pButtonTextures.resize(buttonCount);
    for (size_t i = 0; i < buttonCount; i++) {
        m_pButtonTextures[i] = makeShared<CTexture>();
    }
}

CWindowActionsBar::~CWindowActionsBar() {
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseButtonCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseMoveCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchDownCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchUpCallback);
    std::erase(g_pGlobalState->bars, m_self);
}

float CWindowActionsBar::getButtonSize() const {
    auto* const PBUTTON_SIZE_VAL = HyprlandAPI::getConfigValue(PHANDLE, "plugin:window_actions:button_size");
    int buttonSize = 15; // default

    if (PBUTTON_SIZE_VAL) {
        buttonSize = std::any_cast<Hyprlang::INT>(PBUTTON_SIZE_VAL->getValue());
    }

    return static_cast<float>(buttonSize);
}

uint32_t CWindowActionsBar::getActionButton() const {
    auto* const PACTION_BUTTON_VAL = HyprlandAPI::getConfigValue(PHANDLE, "plugin:window_actions:action_button");
    int actionButton = 272; // default BTN_LEFT

    if (PACTION_BUTTON_VAL) {
        actionButton = std::any_cast<Hyprlang::INT>(PACTION_BUTTON_VAL->getValue());
    }

    return static_cast<uint32_t>(actionButton);
}

float CWindowActionsBar::getUnhoveredAlpha() const {
    auto* const PUNHOVERED_ALPHA_VAL = HyprlandAPI::getConfigValue(PHANDLE, "plugin:window_actions:unhovered_alpha");
    float unhovered_alpha = 1.0f; // default (no transparency)

    if (PUNHOVERED_ALPHA_VAL) {
        unhovered_alpha = std::any_cast<Hyprlang::FLOAT>(PUNHOVERED_ALPHA_VAL->getValue());
    }

    return unhovered_alpha;
}


SDecorationPositioningInfo CWindowActionsBar::getPositioningInfo() {
    SDecorationPositioningInfo info;
    info.policy         = m_hidden ? DECORATION_POSITION_ABSOLUTE : DECORATION_POSITION_ABSOLUTE;
    info.edges          = DECORATION_EDGE_TOP;
    info.priority       = 10000;
    info.reserved       = false;
    info.desiredExtents = {{0, 0}, {0, 0}};
    return info;
}

void CWindowActionsBar::onPositioningReply(const SDecorationPositioningReply& reply) {
    if (reply.assignedGeometry.size() != m_bAssignedBox.size())
        m_bWindowSizeChanged = true;

    m_bAssignedBox = reply.assignedGeometry;
}

std::string CWindowActionsBar::getDisplayName() {
    return "WindowActionsBar";
}

bool CWindowActionsBar::inputIsValid() {
    if (!m_pWindow->m_workspace || !m_pWindow->m_workspace->isVisible() || !g_pInputManager->m_exclusiveLSes.empty() ||
        (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(m_pWindow->m_wlSurface->resource())))
        return false;

    const auto WINDOWATCURSOR = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

    if (WINDOWATCURSOR != m_pWindow && m_pWindow != g_pCompositor->m_lastWindow)
        return false;

    auto     PMONITOR     = g_pCompositor->m_lastMonitor.lock();
    PHLLS    foundSurface = nullptr;
    Vector2D surfaceCoords;

    g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &foundSurface);

    if (foundSurface)
        return false;

    g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords,
                                        &foundSurface);

    if (foundSurface)
        return false;

    return true;
}

void CWindowActionsBar::onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e) {
    if (!inputIsValid())
        return;

    const auto PWINDOW = m_pWindow.lock();
    const auto COORDS = cursorRelativeToButton();

    if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        int buttonIndex = getButtonIndex(COORDS);
        if (buttonIndex >= 0) {
            // Handle configured buttons
            if (buttonIndex < (int)g_pGlobalState->buttons.size()) {
                const auto& button = g_pGlobalState->buttons[buttonIndex];

                if (e.button == getActionButton()) {
                    executeCommand(button.command);
                    info.cancelled = true;
                    return;
                }

                // For __movewindow__ command, also handle left-click
                if (e.button == BTN_LEFT && button.command == "__movewindow__") {
                    executeCommand(button.command);
                    info.cancelled = true;
                    return;
                }

                if (e.button == BTN_LEFT) {
                    return;
                }
            }
        }
        handleDownEvent(info, std::nullopt);
    } else {
        handleUpEvent(info);
    }
}

void CWindowActionsBar::onTouchDown(SCallbackInfo& info, ITouch::SDownEvent e) {
    if (!inputIsValid())
        return;

    auto PMONITOR = g_pCompositor->getMonitorFromName(!e.device->m_boundOutput.empty() ? e.device->m_boundOutput : "");
    PMONITOR      = PMONITOR ? PMONITOR : g_pCompositor->m_lastMonitor.lock();
    g_pCompositor->warpCursorTo({PMONITOR->m_position.x + e.pos.x * PMONITOR->m_size.x, PMONITOR->m_position.y + e.pos.y * PMONITOR->m_size.y}, true);

    handleDownEvent(info, e);
}

void CWindowActionsBar::onMouseMove(Vector2D coords) {
    if (!inputIsValid()) return;

    // Handle drag movement if pending
    if (m_bDragPending && !m_bTouchEv && validMapped(m_pWindow)) {
        m_bDragPending = false;
        handleMovement();
        return;
    }

    const auto COORDS = cursorRelativeToButton();
    const int newHoveredButton = getButtonIndex(COORDS);

    if (newHoveredButton != m_iHoveredButton) {
        m_iHoveredButton = newHoveredButton;
        damageEntire(); // Trigger redraw
    }
}

Vector2D CWindowActionsBar::cursorRelativeToButton() {
    const auto COORDS = g_pInputManager->getMouseCoordsInternal();
    const auto PWINDOW = m_pWindow.lock();
    return COORDS - PWINDOW->m_realPosition->value();
}

void CWindowActionsBar::handleDownEvent(SCallbackInfo& info, std::optional<ITouch::SDownEvent> touchEvent) {
    m_bTouchEv = touchEvent.has_value();

    const auto PWINDOW = m_pWindow.lock();
    const auto COORDS = cursorRelativeToButton();

    if (!validMapped(PWINDOW))
        return;

    if (getButtonIndex(COORDS) >= 0) {
        info.cancelled = true;
        m_bCancelledDown = true;
        return;
    }
}

void CWindowActionsBar::handleUpEvent(SCallbackInfo& info) {
    if (m_pWindow.lock() != g_pCompositor->m_lastWindow.lock())
        return;

    if (m_bDraggingThis) {
        g_pKeybindManager->m_dispatchers["mouse"]("0movewindow");
        m_bDraggingThis = false;
        damageEntire(); // Trigger redraw to restore inactive icon and normal opacity
    }

    if (m_bCancelledDown)
        info.cancelled = true;

    m_bCancelledDown = false;
    m_bDragPending = false;
}

void CWindowActionsBar::handleMovement() {
    g_pKeybindManager->m_dispatchers["mouse"]("1movewindow");
    m_bDraggingThis = true;
    damageEntire(); // Trigger redraw to show active icon and full opacity
    Debug::log(LOG, "[window-actions] Dragging initiated");
}

int CWindowActionsBar::getButtonIndex(Vector2D coords) {
    const float buttonSize = getButtonSize();
    const size_t buttonCount = g_pGlobalState->buttons.size();

    for (size_t i = 0; i < buttonCount; i++) {
        float startX = i * (buttonSize + BUTTON_SPACING);
        if (coords.x >= startX && coords.x <= startX + buttonSize &&
            coords.y >= 0 && coords.y <= buttonSize) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CWindowActionsBar::executeCommand(const std::string& command) {
    if (command.empty())
        return;

    Debug::log(LOG, "[window-actions] Executing command: {}", command);

    // Check for special movewindow command
    if (command == "__movewindow__") {
        Debug::log(LOG, "[window-actions] Initiating window move");
        m_bDragPending = true;
        return;
    }

    // Focus the window before executing any command to ensure it targets the correct window
    const auto PWINDOW = m_pWindow.lock();
    if (PWINDOW) {
        g_pCompositor->focusWindow(PWINDOW);
    }

    // Use the exec dispatcher to execute the command
    if (g_pKeybindManager && g_pKeybindManager->m_dispatchers.contains("exec")) {
        g_pKeybindManager->m_dispatchers["exec"](command);
    } else {
        Debug::log(ERR, "[window-actions] exec dispatcher not found");
    }
}

bool CWindowActionsBar::getWindowState(const std::string& condition) {
    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return false;

    if (condition.empty())
        return false;

    if (condition == "fullscreen") {
        return PWINDOW->isFullscreen();
    } else if (condition == "grouped") {
        return PWINDOW->m_groupData.pNextWindow != nullptr;
    } else if (condition == "floating") {
        return PWINDOW->m_isFloating;
    } else if (condition == "maximized") {
        return PWINDOW->m_fullscreenState.internal == FSMODE_MAXIMIZED;
    } else if (condition == "focused") {
        return g_pCompositor->m_lastWindow.lock() == PWINDOW;
    } else if (condition == "pinned") {
        return PWINDOW->m_pinned;
    }

    return false;
}

void CWindowActionsBar::renderText(SP<CTexture> out, const std::string& text, const CHyprColor& color, const Vector2D& bufferSize, const float scale, const int fontSize) {
    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    PangoLayout*          layout = pango_cairo_create_layout(CAIRO);
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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize.x, bufferSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

void CWindowActionsBar::renderButtonTexts(const Vector2D& bufferSize, const float scale) {
    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW)
        return;

    // Ensure we have the right number of button textures
    if (m_pButtonTextures.size() != g_pGlobalState->buttons.size()) {
        m_pButtonTextures.resize(g_pGlobalState->buttons.size());
        for (size_t i = 0; i < g_pGlobalState->buttons.size(); i++) {
            if (!m_pButtonTextures[i]) {
                m_pButtonTextures[i] = makeShared<CTexture>();
            }
        }
    }

    // Render buttons based on configuration
    for (size_t i = 0; i < g_pGlobalState->buttons.size(); i++) {
        const auto& button = g_pGlobalState->buttons[i];

        // Choose icon based on window state or drag state
        std::string icon;
        if (button.command == "__movewindow__" && m_bDraggingThis) {
            // Show active icon while dragging
            icon = button.icon_active;
        } else if (!button.condition.empty() && getWindowState(button.condition)) {
            icon = button.icon_active;
        } else {
            icon = button.icon_inactive;
        }

        // Clear existing texture to force re-render (for state changes)
        m_pButtonTextures[i]->destroyTexture();
        renderText(m_pButtonTextures[i], icon, button.text_color, bufferSize, scale, getButtonSize() * 0.6);
    }
}

void CWindowActionsBar::draw(PHLMONITOR pMonitor, const float& a) {
    if (m_hidden || !validMapped(m_pWindow))
        return;

    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW->m_windowData.decorate.valueOrDefault())
        return;

    auto data = CWindowActionsPassElement::SWindowActionsData{this, a};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CWindowActionsPassElement>(data));
}

void CWindowActionsBar::renderPass(PHLMONITOR pMonitor, const float& a) {
    const auto PWINDOW = m_pWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto PWORKSPACE      = PWINDOW->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    const float buttonSize = getButtonSize();
    const Vector2D bufferSize = {buttonSize * pMonitor->m_scale, buttonSize * pMonitor->m_scale};
    renderButtonTexts(bufferSize, pMonitor->m_scale);

    // Render all configured buttons
    for (size_t i = 0; i < g_pGlobalState->buttons.size() && i < m_pButtonTextures.size(); i++) {
        const auto& button = g_pGlobalState->buttons[i];

        CBox buttonBox = {PWINDOW->m_realPosition->value().x - pMonitor->m_position.x + static_cast<float>(i) * (buttonSize + BUTTON_SPACING),
                          PWINDOW->m_realPosition->value().y - pMonitor->m_position.y,
                          buttonSize, buttonSize};

        buttonBox.translate(PWINDOW->m_floatingOffset).translate(WORKSPACEOFFSET).scale(pMonitor->m_scale).round();

        if (buttonBox.w < 1 || buttonBox.h < 1)
            continue;

        // Apply hover opacity: unhovered_alpha when not hovered, full opacity when hovered or dragging
        float unhovered_alpha = getUnhoveredAlpha();
        bool isDraggingMoveButton = (button.command == "__movewindow__" && m_bDraggingThis);
        float buttonAlpha = (m_iHoveredButton == static_cast<int>(i) || isDraggingMoveButton) ? a : a * unhovered_alpha;

        // Use button's configured background color with hover-adjusted alpha
        CHyprColor bgColor = button.bg_color;
        bgColor.a *= buttonAlpha;

        g_pHyprOpenGL->renderRect(buttonBox, bgColor, {.round = 3.0f * pMonitor->m_scale});

        if (m_pButtonTextures[i] && m_pButtonTextures[i]->m_texID != 0)
            g_pHyprOpenGL->renderTexture(m_pButtonTextures[i], buttonBox, {.a = buttonAlpha});
    }

    m_bWindowSizeChanged = false;
}

eDecorationType CWindowActionsBar::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CWindowActionsBar::updateWindow(PHLWINDOW pWindow) {
    damageEntire();
}

void CWindowActionsBar::damageEntire() {
    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return;

    const float buttonSize = getButtonSize();
    const int buttonCount = static_cast<int>(g_pGlobalState->buttons.size());
    if (buttonCount > 0) {
        CBox damageBox = {PWINDOW->m_realPosition->value().x,
                          PWINDOW->m_realPosition->value().y,
                          buttonCount * buttonSize + (buttonCount - 1) * BUTTON_SPACING, buttonSize};
        g_pHyprRenderer->damageBox(damageBox);
    }
}

eDecorationLayer CWindowActionsBar::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

uint64_t CWindowActionsBar::getDecorationFlags() {
    return 0;
}

PHLWINDOW CWindowActionsBar::getOwner() {
    return m_pWindow.lock();
}

CBox CWindowActionsBar::assignedBoxGlobal() {
    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return {};

    const float buttonSize = getButtonSize();
    const int buttonCount = static_cast<int>(g_pGlobalState->buttons.size());
    if (buttonCount == 0)
        return {};
    return CBox{PWINDOW->m_realPosition->value().x,
                PWINDOW->m_realPosition->value().y,
                buttonCount * buttonSize + (buttonCount - 1) * BUTTON_SPACING, buttonSize};
}