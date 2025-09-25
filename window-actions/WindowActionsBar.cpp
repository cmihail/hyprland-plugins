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
    m_pTouchDownCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchDown", [&](void* self, SCallbackInfo& info, std::any param) { onTouchDown(info, std::any_cast<ITouch::SDownEvent>(param)); });
    m_pTouchUpCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchUp", [&](void* self, SCallbackInfo& info, std::any param) { handleUpEvent(info); });

    m_pCloseButtonTex = makeShared<CTexture>();
    m_pFullscreenButtonTex = makeShared<CTexture>();
    m_pGroupButtonTex = makeShared<CTexture>();
    m_pFloatingButtonTex = makeShared<CTexture>();
}

CWindowActionsBar::~CWindowActionsBar() {
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseButtonCallback);
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
            if (e.button == getActionButton()) {
                switch (buttonIndex) {
                    case 0: // Close button
                        g_pCompositor->closeWindow(PWINDOW);
                        break;
                    case 1: // Fullscreen button
                        g_pCompositor->setWindowFullscreenState(PWINDOW, {.internal = PWINDOW->isFullscreen() ? FSMODE_NONE : FSMODE_MAXIMIZED});
                        break;
                    case 2: // Group button
                        if (PWINDOW->m_groupData.pNextWindow) {
                            PWINDOW->destroyGroup();
                        } else {
                            PWINDOW->createGroup();
                        }
                        break;
                    case 3: // Floating button
                        PWINDOW->m_isFloating = !PWINDOW->m_isFloating;
                        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(PWINDOW);
                        // Focus the window to bring it to front
                        g_pCompositor->focusWindow(PWINDOW);
                        break;
                }
                info.cancelled = true;
                return;
            }
            if (e.button == BTN_LEFT) {
                return;
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

    if (m_bCancelledDown)
        info.cancelled = true;

    m_bCancelledDown = false;
}

bool CWindowActionsBar::isOnCloseButton(Vector2D coords) {
    const float buttonSize = getButtonSize();
    return coords.x >= 0 && coords.x <= buttonSize && coords.y >= 0 && coords.y <= buttonSize;
}

bool CWindowActionsBar::isOnFullscreenButton(Vector2D coords) {
    const float buttonSize = getButtonSize();
    float startX = buttonSize + BUTTON_SPACING;
    return coords.x >= startX && coords.x <= startX + buttonSize && coords.y >= 0 && coords.y <= buttonSize;
}

bool CWindowActionsBar::isOnGroupButton(Vector2D coords) {
    const float buttonSize = getButtonSize();
    float startX = 2 * (buttonSize + BUTTON_SPACING);
    return coords.x >= startX && coords.x <= startX + buttonSize && coords.y >= 0 && coords.y <= buttonSize;
}

bool CWindowActionsBar::isOnFloatingButton(Vector2D coords) {
    const float buttonSize = getButtonSize();
    float startX = 3 * (buttonSize + BUTTON_SPACING);
    return coords.x >= startX && coords.x <= startX + buttonSize && coords.y >= 0 && coords.y <= buttonSize;
}

int CWindowActionsBar::getButtonIndex(Vector2D coords) {
    if (isOnCloseButton(coords)) return 0;
    if (isOnFullscreenButton(coords)) return 1;
    if (isOnGroupButton(coords)) return 2;
    if (isOnFloatingButton(coords)) return 3;
    return -1;
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
    const CHyprColor color = CHyprColor(0.9, 0.9, 0.9, 1.0);
    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW)
        return;

    // Always render close button - state doesn't change
    if (m_pCloseButtonTex->m_texID == 0) {
        renderText(m_pCloseButtonTex, "⨯", color, bufferSize, scale, getButtonSize() * 0.6);
    }

    // Fullscreen button - show different icons based on fullscreen state
    bool isFullscreen = PWINDOW->isFullscreen();
    std::string fullscreenIcon = isFullscreen ? "⬋" : "⬈";
    m_pFullscreenButtonTex->destroyTexture(); // Clear existing texture to force re-render
    renderText(m_pFullscreenButtonTex, fullscreenIcon, color, bufferSize, scale, getButtonSize() * 0.6);

    // Group button - show different icons based on group state
    bool isInGroup = PWINDOW->m_groupData.pNextWindow != nullptr;
    std::string groupIcon = isInGroup ? "⊞" : "⊟";
    m_pGroupButtonTex->destroyTexture(); // Clear existing texture to force re-render
    renderText(m_pGroupButtonTex, groupIcon, color, bufferSize, scale, getButtonSize() * 0.6);

    // Floating button - show different icons based on floating state
    bool isFloating = PWINDOW->m_isFloating;
    std::string floatingIcon = isFloating ? "⠶" : "•";
    m_pFloatingButtonTex->destroyTexture(); // Clear existing texture to force re-render
    renderText(m_pFloatingButtonTex, floatingIcon, color, bufferSize, scale, getButtonSize() * 0.6);
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

    CHyprColor bgColor = CHyprColor(0.2, 0.2, 0.2, 0.8);
    bgColor.a *= a;

    // Render all buttons
    SP<CTexture> buttonTextures[] = {m_pCloseButtonTex, m_pFullscreenButtonTex, m_pGroupButtonTex, m_pFloatingButtonTex};

    for (int i = 0; i < NUM_BUTTONS; i++) {
        CBox buttonBox = {PWINDOW->m_realPosition->value().x - pMonitor->m_position.x + i * (buttonSize + BUTTON_SPACING),
                          PWINDOW->m_realPosition->value().y - pMonitor->m_position.y,
                          buttonSize, buttonSize};

        buttonBox.translate(PWINDOW->m_floatingOffset).translate(WORKSPACEOFFSET).scale(pMonitor->m_scale).round();

        if (buttonBox.w < 1 || buttonBox.h < 1)
            continue;

        g_pHyprOpenGL->renderRect(buttonBox, bgColor, {.round = 3.0f * pMonitor->m_scale});

        if (buttonTextures[i]->m_texID != 0)
            g_pHyprOpenGL->renderTexture(buttonTextures[i], buttonBox, {.a = a});
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
    CBox damageBox = {PWINDOW->m_realPosition->value().x,
                      PWINDOW->m_realPosition->value().y,
                      NUM_BUTTONS * buttonSize + (NUM_BUTTONS - 1) * BUTTON_SPACING, buttonSize};
    g_pHyprRenderer->damageBox(damageBox);
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
    return CBox{PWINDOW->m_realPosition->value().x,
                PWINDOW->m_realPosition->value().y,
                NUM_BUTTONS * buttonSize + (NUM_BUTTONS - 1) * BUTTON_SPACING, buttonSize};
}