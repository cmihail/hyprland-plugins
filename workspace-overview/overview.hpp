#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <vector>
#include <unordered_map>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo/cairo.h>

class CMonitor;
class COverview;

// Global drag state shared across all monitor overviews
struct GlobalDragState {
    bool isDragging = false;
    bool mouseButtonPressed = false;  // Shared across all monitors
    bool isWorkspaceDrag = false;     // true if dragging whole workspace (middle-click)
    PHLWINDOW draggedWindow = nullptr;
    int sourceWorkspaceIndex = -1;
    COverview* sourceOverview = nullptr;
    CFramebuffer dragPreviewFB;
    Vector2D mouseDownPos = Vector2D{};

    void reset() {
        isDragging = false;
        mouseButtonPressed = false;
        isWorkspaceDrag = false;
        draggedWindow = nullptr;
        sourceWorkspaceIndex = -1;
        sourceOverview = nullptr;
        mouseDownPos = Vector2D{};
        if (dragPreviewFB.m_size.x > 0) {
            dragPreviewFB.release();
        }
    }
};

inline GlobalDragState g_dragState;

class COverview {
  public:
    COverview(PHLWORKSPACE startedOn_, bool skipAnimation_ = false);
    ~COverview();

    void render();
    void damage();
    void onDamageReported();
    void onPreRender();

    void close();
    void startCloseAnimation();
    void selectWorkspaceAtPosition(const Vector2D& pos);

    bool blockOverviewRendering = false;
    bool blockDamageReporting   = false;

    PHLMONITORREF pMonitor;

  private:
    void redrawID(int id, bool forcelowres = false);
    void redrawAll(bool forcelowres = false);
    void fullRender();

    // Helper functions for fullRender
    void renderBackgroundImage(const Vector2D& monitorSize, float monScale);
    void renderEmptyWorkspaceSlots(const Vector2D& monitorSize,
                                    const Vector2D& currentSize,
                                    const Vector2D& currentPos, float monScale);
    void renderWorkspace(size_t i, const Vector2D& monitorSize, float monScale,
                         float zoomScale, const Vector2D& currentPos,
                         int dropZoneAbove, int dropZoneBelow,
                         int firstPlaceholderIndex, int windowDragTargetIndex);
    void renderWorkspaceIndicator(const CBox& scaledBox, size_t i, float alpha,
                                   CRegion& damage);
    void renderDropZoneIndicator(int dropZoneAbove, int dropZoneBelow);
    void renderDragPreviewAtCursor(float monScale);

    // Helper functions for constructor
    void setupWorkspaceIDs(int currentID);
    void calculateLayoutBoxes(const Vector2D& monitorSize);
    void setInitialScrollPosition(float availableHeight);
    void adjustScrollForEqualPartialVisibility(float availableHeight);
    void renderWorkspacesToFramebuffers(PHLMONITOR monitor,
                                        PHLWORKSPACE openSpecial);
    void setupAnimations(const Vector2D& monitorSize);
    void setupEventHooks();
    void renderBackgroundForLeftPanel(const CBox& monbox, float leftPreviewHeight);
    void setupMouseMoveHook();
    void setupMouseButtonHook();
    void handleSelectWorkspaceButton(uint32_t state,
                                      const SP<CMonitor>& clickedMonitor);
    void setupMouseAxisHook();
    void setupMonitorHooks();
    void setupWorkspaceChangeHook();
    void setupWindowEventHooks();

    // Helper functions for drag and drop
    int         findWorkspaceIndexAtPosition(const Vector2D& pos);
    bool        isMiddleClickWorkspaceDragAllowed(int clickedWorkspaceIndex) const;
    void        setupWorkspaceDragOnMiddleClick(int clickedWorkspaceIndex, const Vector2D& mousePos);
    PHLWINDOW   findWindowAtPosition(const Vector2D& pos, int workspaceIndex);
    Vector2D    convertPreviewToWorkspaceCoords(const Vector2D& pos, int workspaceIndex);
    int         calculateDropDirection(PHLWINDOW targetWindow, const Vector2D& cursorPos);
    void        moveWindowToWorkspace(PHLWINDOW window, int targetWorkspaceIndex,
                                      const Vector2D& cursorPos);
    void        renderDragPreview();
    std::pair<int, int> findDropZoneBetweenWorkspaces(const Vector2D& pos);
    void        renderDropZoneAboveFirst();
    void        renderDropZoneBelowLast(int lastIndex);
    void        renderDropZoneBetween(int above, int below);
    void        handleWorkspaceReordering();
    void        handleSameMonitorDrop(int sourceIdx);
    void        handleCrossMonitorDrop(int sourceIdx);
    void        handleMiddleThirdDrop(COverview* targetOverview, int sourceIdx,
                                       int targetIdx, bool isCrossMonitor);
    void        handleCrossMonitorPlaceholderDrop(COverview* targetOverview,
                                                    int sourceIdx, int targetIdx);
    int         findFirstPlaceholderIndex() const;
    void        reorderWorkspace(int sourceIdx, int targetIdx);
    void        mergeWorkspace(int sourceIdx, int targetIdx);
    void        moveWorkspaceToPlaceholder(int sourceIdx, int placeholderIdx);
    void        removeWorkspaceAndShiftWindows(int workspaceIdx);
    int         calculateTargetIndexFromDropZone(int sourceIdx, int dropZoneAbove,
                                                  int dropZoneBelow);
    void        collectWorkspaceWindowsForReorder(
                    int sourceIdx, int targetIdx,
                    std::vector<std::pair<int, std::vector<PHLWINDOW>>>& workspaceWindows);
    void        moveWindowsForReorder(
                    int sourceIdx, int targetIdx,
                    const std::vector<std::pair<int, std::vector<PHLWINDOW>>>& workspaceWindows);
    void        scheduleWorkspaceRefreshes(int sourceIdx, int targetIdx);
    static void moveCrossMonitorWorkspace(COverview* sourceOverview, int sourceIdx,
                                          COverview* targetOverview, int targetIdx);
    static void mergeCrossMonitorWorkspace(COverview* sourceOverview, int sourceIdx,
                                           COverview* targetOverview, int targetIdx);
    static void moveSourceMonitorWindowsUp(COverview* sourceOverview, int sourceIdx);
    static void moveTargetMonitorWindowsDown(COverview* targetOverview, int targetIdx);
    void        recalculateMaxScrollOffset();

    // Cross-monitor helpers
    static std::pair<COverview*, int> findWorkspaceAtGlobalPosition(
        const Vector2D& globalPos
    );
    static void setupSourceWorkspaceRefreshTimer(
        COverview* sourceOverview,
        const std::vector<int>& workspaceIndices,
        int durationMs = 1000  // Default 1 second, can be reduced for same-workspace drops
    );
    static void setupSingleWorkspaceRefresh(
        COverview* sourceOverview,
        const std::vector<int>& workspaceIndices,
        int delayMs = 50  // Single refresh after delay
    );
    static void refreshSourceWorkspacesAfterCrossMonitorMove(
        COverview* sourceOverview,
        int sourceWorkspaceIndex
    );
    static int64_t findFirstAvailableWorkspaceID();
    static void closeAllOverviews();

    // Layout constants
    static constexpr float LEFT_WIDTH_RATIO    = 0.33f;  // Left side takes 1/3
    static constexpr float GAP_WIDTH           = 10.0f;
    static constexpr float PADDING             = 20.0f;
    CHyprColor             BG_COLOR            = CHyprColor{0.1, 0.1, 0.1, 1.0};

    // Dynamic per-monitor workspace count
    size_t leftWorkspaceCount = 0;  // Number of workspaces in left list (existing + configured placeholders)

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
    PHLANIMVAR<float> scrollOffset;

    bool     closing = false;
    Vector2D lastMousePosLocal = Vector2D{};

    // Scroll offset for workspace list
    float maxScrollOffset = 0.0f;
    float leftPreviewHeight = 0.0f;

    // Drag preview scale
    static constexpr float DRAG_PREVIEW_SCALE = 0.10f;  // Scale factor for drag preview

    // Event hooks
    SP<HOOK_CALLBACK_FN> mouseButtonHook;
    SP<HOOK_CALLBACK_FN> mouseMoveHook;
    SP<HOOK_CALLBACK_FN> mouseAxisHook;
    SP<HOOK_CALLBACK_FN> monitorAddedHook;
    SP<HOOK_CALLBACK_FN> monitorRemovedHook;
    SP<HOOK_CALLBACK_FN> workspaceChangeHook;
    SP<HOOK_CALLBACK_FN> openWindowHook;
    SP<HOOK_CALLBACK_FN> closeWindowHook;
    SP<HOOK_CALLBACK_FN> moveWindowHook;

    friend class COverviewPassElement;
    friend void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable>, PHLMONITOR);
    friend void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable>);
};

inline std::unordered_map<PHLMONITOR, std::unique_ptr<COverview>> g_pOverviews;

// Background image loading functions
void loadBackgroundImage(const std::string& path);
std::vector<uint8_t> convertPixelDataToRGBA(const guchar* pixels, int width,
                                              int height, int channels, int stride);
bool createTextureFromPixelData(const std::vector<uint8_t>& pixelData,
                                 int width, int height);
