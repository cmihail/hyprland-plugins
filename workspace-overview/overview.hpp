#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <vector>

class CMonitor;

class COverview {
  public:
    COverview(PHLWORKSPACE startedOn_);
    ~COverview();

    void render();
    void damage();
    void onDamageReported();
    void onPreRender();

    void close();
    void selectWorkspaceAtPosition(const Vector2D& pos);

    bool blockOverviewRendering = false;
    bool blockDamageReporting   = false;

    PHLMONITORREF pMonitor;

  private:
    void redrawID(int id, bool forcelowres = false);
    void redrawAll(bool forcelowres = false);
    void fullRender();

    // Helper functions for constructor
    void setupWorkspaceIDs(int currentID);
    void calculateLayoutBoxes(const Vector2D& monitorSize);
    void renderWorkspacesToFramebuffers(PHLMONITOR monitor,
                                        PHLWORKSPACE openSpecial);
    void setupAnimations(const Vector2D& monitorSize);
    void setupEventHooks();

    // Helper functions for drag and drop
    int         findWorkspaceIndexAtPosition(const Vector2D& pos);
    PHLWINDOW   findWindowAtPosition(const Vector2D& pos, int workspaceIndex);
    void        moveWindowToWorkspace(PHLWINDOW window, int targetWorkspaceIndex);

    // Layout constants
    static constexpr int   LEFT_WORKSPACES     = 4;  // Number of workspaces in left list
    static constexpr float LEFT_WIDTH_RATIO    = 0.33f;  // Left side takes 1/3
    static constexpr float GAP_WIDTH           = 10.0f;
    static constexpr float PADDING             = 20.0f;
    CHyprColor             BG_COLOR            = CHyprColor{0.1, 0.1, 0.1, 1.0};

    bool damageDirty = false;

    struct SWorkspaceImage {
        CFramebuffer fb;
        int64_t      workspaceID = -1;
        PHLWORKSPACE pWorkspace;
        CBox         box;
        bool         isActive = false;  // true if this is the active workspace on right side
    };

    int                          activeIndex = -1;  // Index of active workspace in images
    int                          selectedIndex = -1;  // Workspace to switch to on close
    std::vector<SWorkspaceImage> images;
    PHLWORKSPACE                 startedOn;

    PHLANIMVAR<Vector2D> size;
    PHLANIMVAR<Vector2D> pos;

    bool     closing = false;
    Vector2D lastMousePosLocal = Vector2D{};

    // Drag detection
    static constexpr float DRAG_THRESHOLD = 50.0f;
    bool                   mouseButtonPressed = false;
    Vector2D               mouseDownPos = Vector2D{};
    bool                   isDragging = false;
    PHLWINDOW              draggedWindow = nullptr;
    int                    sourceWorkspaceIndex = -1;

    // Event hooks
    SP<HOOK_CALLBACK_FN> mouseButtonHook;
    SP<HOOK_CALLBACK_FN> mouseMoveHook;

    friend class COverviewPassElement;
    friend void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable>);
};

inline std::unique_ptr<COverview> g_pOverview;
