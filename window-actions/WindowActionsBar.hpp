#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/desktop/WindowRule.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include "globals.hpp"

#define private public
#include <hyprland/src/managers/input/InputManager.hpp>
#undef private

class CWindowActionsBar : public IHyprWindowDecoration {
  public:
    CWindowActionsBar(PHLWINDOW);
    virtual ~CWindowActionsBar();

    virtual SDecorationPositioningInfo getPositioningInfo();
    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);
    virtual void                       draw(PHLMONITOR, float const& a);
    virtual eDecorationType            getDecorationType();
    virtual void                       updateWindow(PHLWINDOW);
    virtual void                       damageEntire();
    virtual eDecorationLayer           getDecorationLayer();
    virtual uint64_t                   getDecorationFlags();
    virtual std::string                getDisplayName();

    PHLWINDOW                          getOwner();
    void                               renderPass(PHLMONITOR, float const& a);
    CBox                               assignedBoxGlobal();

    WP<CWindowActionsBar>              m_self;

  private:
    SBoxExtents               m_seExtents;
    PHLWINDOWREF              m_pWindow;
    CBox                      m_bAssignedBox;
    SP<CTexture>              m_pCloseButtonTex;
    SP<CTexture>              m_pFullscreenButtonTex;
    SP<CTexture>              m_pGroupButtonTex;
    SP<CTexture>              m_pFloatingButtonTex;
    bool                      m_bWindowSizeChanged = false;
    bool                      m_hidden             = false;

    const float               BUTTON_SPACING = 2.0f;
    const int                 NUM_BUTTONS = 4;

    float                     getButtonSize() const;
    uint32_t                  getActionButton() const;

    Vector2D                  cursorRelativeToButton();
    void                      renderButtonTexts(const Vector2D& bufferSize, const float scale);
    void                      renderText(SP<CTexture> out, const std::string& text, const CHyprColor& color, const Vector2D& bufferSize, const float scale, const int fontSize);

    bool                      inputIsValid();
    void                      onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e);
    void                      onTouchDown(SCallbackInfo& info, ITouch::SDownEvent e);

    void                      handleDownEvent(SCallbackInfo& info, std::optional<ITouch::SDownEvent> touchEvent);
    void                      handleUpEvent(SCallbackInfo& info);
    bool                      isOnCloseButton(Vector2D coords);
    bool                      isOnFullscreenButton(Vector2D coords);
    bool                      isOnGroupButton(Vector2D coords);
    bool                      isOnFloatingButton(Vector2D coords);
    int                       getButtonIndex(Vector2D coords);

    SP<HOOK_CALLBACK_FN>      m_pMouseButtonCallback;
    SP<HOOK_CALLBACK_FN>      m_pTouchDownCallback;
    SP<HOOK_CALLBACK_FN>      m_pTouchUpCallback;

    bool                      m_bTouchEv = false;
    bool                      m_bCancelledDown = false;

    friend class CWindowActionsPassElement;
};