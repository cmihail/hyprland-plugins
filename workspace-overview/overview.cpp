#include "overview.hpp"
#include <algorithm>
#include <any>
#include <fstream>
#include <wayland-server.h>
#define private public
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#undef private
#include "OverviewPassElement.hpp"

void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    // Find the overview that owns this animation variable
    for (auto& [monitor, overview] : g_pOverviews) {
        if (!overview)
            continue;

        bool isSizeAnim = overview->size.get() == thisptr.get();
        bool isPosAnim = overview->pos.get() == thisptr.get();

        if (isSizeAnim || isPosAnim) {
            overview->damage();
            return;
        }
    }
}

void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr, PHLMONITOR monitor) {
    g_pOverviews.erase(monitor);
}

COverview::~COverview() {
    g_pHyprRenderer->makeEGLCurrent();
    images.clear(); // otherwise we get a vram leak
    g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
}

COverview::COverview(PHLWORKSPACE startedOn_, bool skipAnimation) : startedOn(startedOn_) {
    const auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
    pMonitor            = PMONITOR;

    // Get current workspace ID
    int currentID = pMonitor->activeWorkspaceID();

    // Get all workspaces and filter by monitor
    auto allWorkspaces = g_pCompositor->getWorkspacesCopy();
    std::vector<int64_t> monitorWorkspaceIDs;

    for (const auto& ws : allWorkspaces) {
        if (!ws || ws->m_isSpecialWorkspace)
            continue;

        auto wsMonitor = ws->m_monitor.lock();

        // Include only workspaces that belong to this monitor
        if (wsMonitor == pMonitor) {
            monitorWorkspaceIDs.push_back(ws->m_id);
        }
    }

    // Sort workspace IDs
    std::sort(monitorWorkspaceIDs.begin(), monitorWorkspaceIDs.end());

    // Calculate dynamic workspace count: existing workspaces + configured placeholders
    leftWorkspaceCount = monitorWorkspaceIDs.size() + g_placeholdersNum;

    // We need leftWorkspaceCount + 1 (for the active workspace on the right)
    images.resize(leftWorkspaceCount + 1);

    // Populate left side workspaces (up to leftWorkspaceCount)
    size_t numToShow = std::min((size_t)leftWorkspaceCount, monitorWorkspaceIDs.size());

    for (size_t i = 0; i < numToShow; ++i) {
        auto& image = images[i];
        image.workspaceID = monitorWorkspaceIDs[i];
        // Mark if this is the active workspace
        image.isActive = (monitorWorkspaceIDs[i] == currentID);
    }

    // Fill remaining left side slots as placeholders for new workspaces
    for (size_t i = numToShow; i < leftWorkspaceCount; ++i) {
        auto& image = images[i];
        image.workspaceID = -1;  // Placeholder
        image.isActive = false;
    }

    // Last image is for the active workspace (right side) - for real-time updates
    images[leftWorkspaceCount].workspaceID = currentID;
    images[leftWorkspaceCount].isActive    = true;
    activeIndex                            = leftWorkspaceCount;

    // Note: The active workspace appears both in the left side (in its proper position)
    // and on the right side (for real-time updates). The right side will be rendered
    // from activeIndex, while the left side shows all workspaces including active.

    g_pHyprRenderer->makeEGLCurrent();

    // Calculate layout with equal margins on left, top, and bottom for left workspaces
    const Vector2D monitorSize = pMonitor->m_size;
    const float availableHeight = monitorSize.y - (2 * PADDING);

    // Calculate workspace height based on 4 workspaces (first 4 shown fully)
    // We calculate for 4 workspaces even though we support up to 8
    const int VISIBLE_WORKSPACES = 4;
    const float totalGaps = (VISIBLE_WORKSPACES - 1) * GAP_WIDTH;
    const float baseHeight = (availableHeight - totalGaps) / VISIBLE_WORKSPACES;

    // Reduce height by 10% to make room for 5th workspace to peek at the bottom
    this->leftPreviewHeight = baseHeight * 0.9f;

    // Calculate max scroll offset based on existing workspaces + first placeholder
    // Count how many non-placeholder workspaces exist on the left side
    size_t numExistingWorkspaces = 0;
    for (size_t i = 0; i < leftWorkspaceCount; ++i) {
        if (images[i].workspaceID != -1) {
            numExistingWorkspaces++;
        }
    }

    // Include the first placeholder (with +) in scrolling calculation
    // This allows users to scroll to see the + sign to create a new workspace
    size_t numWorkspacesToShow = numExistingWorkspaces;
    if (numExistingWorkspaces < leftWorkspaceCount) {
        numWorkspacesToShow++; // Add 1 for the first placeholder
    }

    // Only allow scrolling if there are more than 4 workspaces to show
    if (numWorkspacesToShow <= 4) {
        this->maxScrollOffset = 0.0f;
    } else {
        // Calculate total height needed for workspaces + first placeholder
        float totalWorkspacesHeight = numWorkspacesToShow * this->leftPreviewHeight +
                                      (numWorkspacesToShow - 1) * GAP_WIDTH;
        this->maxScrollOffset = std::max(0.0f, totalWorkspacesHeight - availableHeight);
    }

    // Set initial scroll position to center active workspace
    setInitialScrollPosition(availableHeight);

    // Adjust scroll position for equal partial visibility at top and bottom
    adjustScrollForEqualPartialVisibility(availableHeight);

    // Left workspaces: calculate width so that left margin = top/bottom margin = PADDING
    // The aspect ratio should match the monitor's aspect ratio for proper scaling
    const float monitorAspectRatio = monitorSize.x / monitorSize.y;
    const float leftWorkspaceWidth = this->leftPreviewHeight * monitorAspectRatio;

    // Active workspace: starts right after left section with PADDING gap
    const float activeX = PADDING + leftWorkspaceWidth + PADDING;  // left margin + left width + gap
    const float activeMaxWidth = monitorSize.x - activeX - PADDING;  // Leave PADDING on right edge
    const float activeMaxHeight = monitorSize.y - (2 * PADDING);

    CBox monbox = {{0, 0}, pMonitor->m_pixelSize};

    PHLWORKSPACE openSpecial = PMONITOR->m_activeSpecialWorkspace;
    if (openSpecial)
        PMONITOR->m_activeSpecialWorkspace.reset();

    g_pHyprRenderer->m_bBlockSurfaceFeedback = true;
    startedOn->m_visible                     = false;

    // Render all workspaces to framebuffers
    for (size_t i = 0; i < images.size(); ++i) {
        auto& image = images[i];
        image.fb.alloc(monbox.w, monbox.h, PMONITOR->m_output->state->state().drmFormat);

        CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
        g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE,
                                      nullptr, &image.fb);

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(image.workspaceID);

        if (PWORKSPACE) {
            g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

            image.pWorkspace            = PWORKSPACE;
            PMONITOR->m_activeWorkspace = PWORKSPACE;
            g_pDesktopAnimationManager->startAnimation(
                PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
            PWORKSPACE->m_visible = true;

            if (PWORKSPACE == startedOn)
                PMONITOR->m_activeSpecialWorkspace = openSpecial;

            g_pHyprRenderer->renderWorkspace(PMONITOR, PWORKSPACE,
                                             Time::steadyNow(), monbox);

            PWORKSPACE->m_visible = false;
            g_pDesktopAnimationManager->startAnimation(
                PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);

            if (PWORKSPACE == startedOn)
                PMONITOR->m_activeSpecialWorkspace.reset();
        } else {
            // Render background image for placeholder workspaces
            renderBackgroundForLeftPanel(monbox, this->leftPreviewHeight);
        }

        // Calculate box positions for rendering
        if (i == (size_t)activeIndex) {
            // Right side - active workspace (maximized with consistent margins)
            // This is the last element, used for real-time updates
            image.box = {activeX, PADDING, activeMaxWidth, activeMaxHeight};
        } else {
            // Left side - workspace list (left margin = PADDING, same as top)
            // Apply scroll offset to shift workspaces up/down
            float yPos = PADDING + i * (this->leftPreviewHeight + GAP_WIDTH) -
                         scrollOffset;
            image.box = {PADDING, yPos, leftWorkspaceWidth, this->leftPreviewHeight};
        }

        g_pHyprOpenGL->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
    }

    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    PMONITOR->m_activeSpecialWorkspace = openSpecial;
    PMONITOR->m_activeWorkspace        = startedOn;
    startedOn->m_visible               = true;
    g_pDesktopAnimationManager->startAnimation(
        startedOn, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);

    // Setup animations for zoom effect
    auto animConfig = g_pConfigManager->getAnimationPropertyConfig("windowsMove");
    g_pAnimationManager->createAnimation(pMonitor->m_size, size, animConfig,
                                         AVARDAMAGE_NONE);
    g_pAnimationManager->createAnimation(Vector2D{0, 0}, pos, animConfig,
                                         AVARDAMAGE_NONE);

    size->setUpdateCallback(damageMonitor);
    pos->setUpdateCallback(damageMonitor);

    if (skipAnimation) {
        // No animation - directly set to normal view
        size->setValue(monitorSize);
        pos->setValue(Vector2D{0, 0});
        *size = monitorSize;
        *pos  = Vector2D{0, 0};
    } else {
        // Animate from zoomed in to normal view
        // Start with the view zoomed into the active workspace (right side)
        const auto& activeBox = images[activeIndex].box;

        // Calculate scale needed to zoom the active box to fill the screen
        const float scaleX = monitorSize.x / activeBox.w;
        const float scaleY = monitorSize.y / activeBox.h;
        const float scale = std::min(scaleX, scaleY);

        // Starting position: zoomed into active workspace
        const Vector2D activeCenter = Vector2D{activeBox.x + activeBox.w / 2.0f,
                                               activeBox.y + activeBox.h / 2.0f};
        const Vector2D screenCenter = monitorSize / 2.0f;

        // Set initial value (zoomed in)
        size->setValue(monitorSize * scale);
        pos->setValue((screenCenter - activeCenter) * scale);

        // Set goal (normal view) - this starts the animation
        *size = monitorSize;
        *pos  = Vector2D{0, 0};

        size->setCallbackOnEnd([this](auto) { redrawAll(true); });
    }

    setupEventHooks();
}

void COverview::setupEventHooks() {
    setupMouseMoveHook();
    setupMouseButtonHook();
    setupMouseAxisHook();
    setupMonitorHooks();
    setupWorkspaceChangeHook();
}

void COverview::setupMouseMoveHook() {
    auto onMouseMove = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        const Vector2D globalMousePos = g_pInputManager->getMouseCoordsInternal();
        lastMousePosLocal = globalMousePos - pMonitor->m_position;

        // Consume mouse move events during dragging to prevent interference
        if (g_dragState.isDragging) {
            info.cancelled = true;
            damage();
        }

        // Check if we're dragging (only the overview where button was pressed)
        if (g_dragState.mouseButtonPressed && !g_dragState.isDragging &&
            g_dragState.sourceOverview == this) {
            const float distanceX = std::abs(
                lastMousePosLocal.x - g_dragState.mouseDownPos.x
            );
            const float distanceY = std::abs(
                lastMousePosLocal.y - g_dragState.mouseDownPos.y
            );

            if (distanceX > g_dragThreshold || distanceY > g_dragThreshold) {
                g_dragState.isDragging = true;
                // Consume the event when drag is initiated
                info.cancelled = true;
                if (g_dragState.isWorkspaceDrag) {
                    // Render whole workspace preview for workspace drag
                    renderDragPreview();
                } else if (g_dragState.draggedWindow) {
                    // Render window preview for window drag
                    renderDragPreview();
                }
            }
        }
    };
    mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onMouseMove);
}

void COverview::setupMouseButtonHook() {
    auto onMouseButton = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        // Check if click is on a different monitor than the overview
        const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();
        const auto clickedMonitor = g_pCompositor->getMonitorFromVector(mousePos);

        auto e = std::any_cast<IPointer::SButtonEvent>(param);

        // IMPORTANT: Always reset mouseButtonPressed on button release for ALL overviews
        // This must happen before any early returns to avoid stuck drag states
        if (e.state != WL_POINTER_BUTTON_STATE_PRESSED) {
            g_dragState.mouseButtonPressed = false;
        }

        // If clicked on different monitor and not dragging, pass through
        if (clickedMonitor && clickedMonitor != pMonitor.lock() &&
            !g_dragState.isDragging) {
            return;
        }

        // Consume events for buttons we handle (workspace drag, window drag/select)
        if (e.button == g_dragWorkspaceActionButton ||
            e.button == g_dragWindowActionButton ||
            e.button == g_selectWorkspaceActionButton) {
            info.cancelled = true;
        }

        // Handle workspace drag button (default middle-click)
        if (e.button == g_dragWorkspaceActionButton) {
            if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
                // Find which workspace was clicked and setup drag if allowed
                int clickedWsIndex = findWorkspaceIndexAtPosition(lastMousePosLocal);
                setupWorkspaceDragOnMiddleClick(clickedWsIndex, lastMousePosLocal);
            } else {
                // Workspace drag button released - handle workspace drop
                // Only the overview where the mouse currently is should handle the drop
                if (clickedMonitor == pMonitor.lock()) {
                    if (g_dragState.isDragging && g_dragState.isWorkspaceDrag) {
                        handleWorkspaceReordering();
                    }

                    // Reset global drag state
                    g_dragState.reset();
                }
            }
            return;
        }

        // Handle window drag button (default left-click)
        if (e.button == g_dragWindowActionButton) {
            if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
                // Mouse button pressed - record position and find window
                // Calculate fresh mouse position relative to this monitor
                const Vector2D currentMousePos = mousePos - pMonitor->m_position;

                g_dragState.mouseButtonPressed = true;
                g_dragState.mouseDownPos = currentMousePos;
                g_dragState.sourceOverview = this;  // Set source overview when button is pressed
                g_dragState.isWorkspaceDrag = false;  // Window drag

                // Find which workspace was clicked
                int clickedWsIndex = findWorkspaceIndexAtPosition(
                    currentMousePos
                );

                // Find window at click position in that workspace
                PHLWINDOW clickedWindow = findWindowAtPosition(
                    currentMousePos, clickedWsIndex
                );

                g_dragState.sourceWorkspaceIndex = clickedWsIndex;
                g_dragState.draggedWindow = clickedWindow;
            } else {
                // Mouse button released (already set to false at the start)

                // Only overview where mouse is positioned handles release
                if (clickedMonitor != pMonitor.lock()) {
                    return;
                }

                if (g_dragState.isDragging) {
                    // Dragging detected - handle window move
                    if (g_dragState.draggedWindow) {
                        // Find target workspace at release position
                        auto [targetOverview, targetIndex] =
                            findWorkspaceAtGlobalPosition(mousePos);

                        if (targetOverview && targetIndex >= 0) {
                            bool sameWs = (
                                targetOverview == g_dragState.sourceOverview &&
                                targetIndex == g_dragState.sourceWorkspaceIndex
                            );

                            if (!sameWs) {
                                // Move window to target workspace
                                auto draggedWin = g_dragState.draggedWindow;
                                targetOverview->moveWindowToWorkspace(
                                    draggedWin, targetIndex
                                );

                                // For cross-monitor moves, redraw source
                                bool isCrossMonitor = (
                                    g_dragState.sourceOverview &&
                                    g_dragState.sourceOverview != targetOverview
                                );

                                if (isCrossMonitor) {
                                    refreshSourceWorkspacesAfterCrossMonitorMove(
                                        g_dragState.sourceOverview,
                                        g_dragState.sourceWorkspaceIndex
                                    );
                                }
                            }
                        }
                    }

                    // Reset global drag state
                    g_dragState.reset();
                } else if (e.button == g_selectWorkspaceActionButton) {
                    // Click (not drag) on select button - select workspace and close
                    selectWorkspaceAtPosition(lastMousePosLocal);
                    close();
                }
            }
            return;
        }

        // Handle select workspace button if it's different from drag button
        if (e.button == g_selectWorkspaceActionButton &&
            e.button != g_dragWindowActionButton) {
            handleSelectWorkspaceButton(e.state, clickedMonitor);
            return;
        }
    };

    mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onMouseButton);
}

void COverview::handleSelectWorkspaceButton(
    uint32_t state,
    const SP<CMonitor>& clickedMonitor
) {
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        // Button pressed - just wait for release
        return;
    }

    // Button released - select workspace if on this monitor
    if (clickedMonitor != pMonitor.lock()) {
        return;
    }

    selectWorkspaceAtPosition(lastMousePosLocal);
    close();
}

void COverview::setupMouseAxisHook() {
    auto onMouseAxis = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing) {
            return;
        }

        try {
            // Extract event from map
            auto eventMap = std::any_cast<
                std::unordered_map<std::string, std::any>
            >(param);
            auto e = std::any_cast<IPointer::SAxisEvent>(eventMap["event"]);

            // Check if mouse is on this monitor
            const Vector2D globalMousePos = g_pInputManager->getMouseCoordsInternal();
            const auto currentMonitor = g_pCompositor->getMonitorFromVector(globalMousePos);

            // Only handle scroll if mouse is on this monitor
            if (!currentMonitor || currentMonitor != pMonitor.lock()) {
                return;
            }

            // Update lastMousePosLocal with current position
            lastMousePosLocal = globalMousePos - pMonitor->m_position;

            // Check if mouse is over the left side workspace area
            const Vector2D monitorSize = pMonitor->m_size;
            const float monitorAspectRatio = monitorSize.x / monitorSize.y;
            const float leftWorkspaceWidth = this->leftPreviewHeight * monitorAspectRatio;
            const float leftSideEndX = PADDING + leftWorkspaceWidth;

            bool isOverLeftSide = (lastMousePosLocal.x >= PADDING &&
                                   lastMousePosLocal.x <= leftSideEndX);

            // Only scroll if mouse is over the left side
            if (isOverLeftSide) {
                // Adjust scroll offset based on scroll direction
                const float SCROLL_SPEED = 30.0f;
                scrollOffset += e.delta * SCROLL_SPEED;

                // Clamp scroll offset to valid range [0, maxScrollOffset]
                scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);

                // Update box positions for all left-side workspaces to reflect new scroll
                for (size_t i = 0; i < images.size(); ++i) {
                    if (i != (size_t)activeIndex) {
                        float yPos = PADDING + i * (this->leftPreviewHeight + GAP_WIDTH)
                                     - scrollOffset;
                        images[i].box = {PADDING, yPos, leftWorkspaceWidth,
                                         this->leftPreviewHeight};
                    }
                }

                // Trigger redraw
                damage();
            }

        } catch (const std::exception& e) {
            Debug::log(ERR, "[Overview] Exception in scroll handler: {}", e.what());
        }

        // Consume the scroll event to prevent it from propagating
        info.cancelled = true;
    };

    mouseAxisHook = g_pHookSystem->hookDynamic("mouseAxis", onMouseAxis);
}

void COverview::closeAllOverviews() {
    std::vector<PHLMONITOR> monitorsToClose;
    for (const auto& [mon, overview] : g_pOverviews) {
        monitorsToClose.push_back(mon);
    }
    for (const auto& mon : monitorsToClose) {
        auto monIt = g_pOverviews.find(mon);
        if (monIt != g_pOverviews.end())
            monIt->second->close();
    }
}

void COverview::setupMonitorHooks() {
    auto onMonitorAdded = [](void* self, SCallbackInfo& info, std::any param) {
        closeAllOverviews();
    };

    auto onMonitorRemoved = [](void* self, SCallbackInfo& info, std::any param) {
        closeAllOverviews();
    };

    monitorAddedHook = g_pHookSystem->hookDynamic("monitorAdded", onMonitorAdded);
    monitorRemovedHook = g_pHookSystem->hookDynamic("monitorRemoved", onMonitorRemoved);
}

void COverview::setupWorkspaceChangeHook() {
    auto onWorkspaceChange = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        try {
            auto newWorkspace = std::any_cast<PHLWORKSPACE>(param);

            // Only update if the workspace change is on this monitor
            auto workspaceMonitor = newWorkspace ? newWorkspace->m_monitor.lock() : nullptr;
            if (!newWorkspace || !workspaceMonitor)
                return;

            auto monitor = pMonitor.lock();
            if (!monitor || workspaceMonitor != monitor)
                return;

            // Recreate the overview with the new workspace
            // This is simpler and more reliable than trying to update indices
            // Skip animation for instant recreation
            g_pOverviews.erase(monitor);
            g_pOverviews[monitor] = std::make_unique<COverview>(newWorkspace, true);
        } catch (const std::exception& e) {
            Debug::log(ERR, "[Overview] Exception in workspace change handler: {}", e.what());
        }
    };

    workspaceChangeHook = g_pHookSystem->hookDynamic("workspace", onWorkspaceChange);
}

void COverview::setInitialScrollPosition(float availableHeight) {
    // Find the active workspace on the left side
    int activeLeftIndex = -1;
    for (size_t i = 0; i < leftWorkspaceCount; ++i) {
        if (images[i].isActive) {
            activeLeftIndex = i;
            break;
        }
    }

    if (activeLeftIndex < 0) {
        // Active workspace not found on left side, keep scrollOffset at 0
        return;
    }

    // Calculate scroll offset to center the active workspace
    float panelCenter = availableHeight / 2.0f;
    float workspaceTopWithoutScroll = activeLeftIndex * (this->leftPreviewHeight + GAP_WIDTH);
    float workspaceCenterOffset = this->leftPreviewHeight / 2.0f;

    scrollOffset = workspaceTopWithoutScroll + workspaceCenterOffset - panelCenter;

    // Clamp to valid range to ensure we don't scroll beyond boundaries
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);
}

void COverview::renderBackgroundForLeftPanel(const CBox& monbox, float leftPreviewHeight) {
    if (!g_pBackgroundTexture || g_pBackgroundTexture->m_texID == 0) {
        // No background image loaded, just clear to black
        g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});
        return;
    }

    // Clear first
    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

    const Vector2D texSize = g_pBackgroundTexture->m_size;

    // Scale background to cover the entire framebuffer while maintaining aspect ratio
    const float monboxAspect = (float)monbox.w / monbox.h;
    const float textureAspect = texSize.x / texSize.y;

    CBox bgBox = monbox;

    if (textureAspect > monboxAspect) {
        // Texture is wider, scale by height
        const float scale = monbox.h / texSize.y;
        const float scaledWidth = texSize.x * scale;
        bgBox.x = -(scaledWidth - monbox.w) / 2.0f;
        bgBox.w = scaledWidth;
    } else {
        // Texture is taller, scale by width
        const float scale = monbox.w / texSize.x;
        const float scaledHeight = texSize.y * scale;
        bgBox.y = -(scaledHeight - monbox.h) / 2.0f;
        bgBox.h = scaledHeight;
    }

    bgBox.round();

    g_pHyprOpenGL->renderTexture(g_pBackgroundTexture, bgBox, {});
}

void COverview::adjustScrollForEqualPartialVisibility(float availableHeight) {
    // Only adjust if we're not at the very top or very bottom
    if (scrollOffset <= 0.0f || scrollOffset >= maxScrollOffset) {
        return;
    }

    // Count total number of workspaces (including placeholders) to display
    size_t numWorkspacesToShow = 0;
    for (size_t i = 0; i < leftWorkspaceCount; ++i) {
        if (images[i].workspaceID != -1) {
            numWorkspacesToShow = i + 1;
        }
    }

    // Add 1 for first placeholder if not all slots are filled
    if (numWorkspacesToShow < leftWorkspaceCount) {
        numWorkspacesToShow++;
    }

    // Only adjust if there are more than 4 workspaces (scrolling is enabled)
    if (numWorkspacesToShow <= 4) {
        return;
    }

    // Calculate positions of first and last workspaces
    size_t firstIndex = 0;
    size_t lastIndex = numWorkspacesToShow - 1;

    float firstYPos = PADDING + firstIndex * (this->leftPreviewHeight + GAP_WIDTH) - scrollOffset;
    float lastYPos = PADDING + lastIndex * (this->leftPreviewHeight + GAP_WIDTH) - scrollOffset;

    // Check if both first and last are partially visible
    // Partially visible means: cut off but still showing some part
    bool firstPartiallyVisible = (firstYPos < PADDING) &&
                                  ((firstYPos + this->leftPreviewHeight) > PADDING);
    bool lastPartiallyVisible =
        ((lastYPos + this->leftPreviewHeight) > (PADDING + availableHeight)) &&
        (lastYPos < (PADDING + availableHeight));

    if (!firstPartiallyVisible || !lastPartiallyVisible) {
        return;
    }

    // Calculate how much of each is visible
    float topPartial = (firstYPos + this->leftPreviewHeight) - PADDING;
    float bottomPartial = (PADDING + availableHeight) - lastYPos;

    // Adjust scroll offset to equalize the partial visibility
    float difference = bottomPartial - topPartial;
    scrollOffset -= difference / 2.0f;

    // Clamp again after adjustment
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);
}

void COverview::redrawID(int id, bool forcelowres) {
    blockOverviewRendering = true;

    g_pHyprRenderer->makeEGLCurrent();

    id = std::clamp(id, 0, (int)images.size() - 1);

    CBox monbox = {{0, 0}, pMonitor->m_pixelSize};

    if (!forcelowres && (size->value() != pMonitor->m_size || closing))
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    auto& image = images[id];

    if (image.fb.m_size != monbox.size()) {
        image.fb.release();
        image.fb.alloc(monbox.w, monbox.h, pMonitor->m_output->state->state().drmFormat);
    }

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage,
                                  RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

    const auto   PWORKSPACE  = image.pWorkspace;
    PHLWORKSPACE openSpecial = pMonitor->m_activeSpecialWorkspace;

    if (openSpecial)
        pMonitor->m_activeSpecialWorkspace.reset();

    startedOn->m_visible = false;

    if (PWORKSPACE) {
        g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

        pMonitor->m_activeWorkspace = PWORKSPACE;
        g_pDesktopAnimationManager->startAnimation(
            PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
        PWORKSPACE->m_visible = true;

        if (PWORKSPACE == startedOn)
            pMonitor->m_activeSpecialWorkspace = openSpecial;

        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE,
                                         Time::steadyNow(), monbox);

        PWORKSPACE->m_visible = false;
        g_pDesktopAnimationManager->startAnimation(
            PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);

        if (PWORKSPACE == startedOn)
            pMonitor->m_activeSpecialWorkspace.reset();
    } else {
        // Render background image for placeholder workspaces
        renderBackgroundForLeftPanel(monbox, this->leftPreviewHeight);
    }

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    pMonitor->m_activeSpecialWorkspace = openSpecial;
    pMonitor->m_activeWorkspace        = startedOn;
    startedOn->m_visible               = true;
    g_pDesktopAnimationManager->startAnimation(
        startedOn, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);

    blockOverviewRendering = false;
}

void COverview::redrawAll(bool forcelowres) {
    for (size_t i = 0; i < images.size(); ++i) {
        redrawID(i, forcelowres);
    }
}

void COverview::damage() {
    blockDamageReporting = true;
    g_pHyprRenderer->damageMonitor(pMonitor.lock());
    blockDamageReporting = false;
}

void COverview::onDamageReported() {
    damageDirty = true;
    damage();
    g_pCompositor->scheduleFrameForMonitor(pMonitor.lock());
}

void COverview::selectWorkspaceAtPosition(const Vector2D& pos) {
    // Reset selection - if nothing is clicked, stay on active workspace
    selectedIndex = -1;

    // Check if click is within any workspace box
    for (size_t i = 0; i < images.size(); ++i) {
        const auto& box = images[i].box;

        // Check if position is within this workspace's box
        if (pos.x >= box.x && pos.x <= box.x + box.w &&
            pos.y >= box.y && pos.y <= box.y + box.h) {
            selectedIndex = i;
            break;
        }
    }
}

void COverview::close() {
    if (closing)
        return;

    // Close all overviews on all monitors
    // Make a copy of all monitors to close, excluding this one
    std::vector<PHLMONITOR> otherMonitorsToClose;
    for (const auto& [mon, overview] : g_pOverviews) {
        if (overview.get() != this && overview && !overview->closing) {
            otherMonitorsToClose.push_back(mon);
        }
    }

    // Close other overviews first (without workspace switching)
    for (const auto& mon : otherMonitorsToClose) {
        auto it = g_pOverviews.find(mon);
        if (it != g_pOverviews.end() && it->second) {
            it->second->closing = true;
            it->second->startCloseAnimation();
        }
    }

    // Now close this overview (with workspace switching if needed)
    // Always animate towards the active workspace position (right side)
    // This ensures a consistent zoom animation regardless of which workspace was clicked
    const auto& activeBox = images[activeIndex].box;

    // Calculate zoom animation to focus on active workspace position
    const Vector2D monitorSize = pMonitor->m_size;

    // Calculate the scale factor to zoom the active box to fill screen
    const float scaleX = monitorSize.x / activeBox.w;
    const float scaleY = monitorSize.y / activeBox.h;
    const float scale = std::min(scaleX, scaleY);

    // Target size is scaled up from current
    *size = monitorSize * scale;

    // Target position: move active workspace center to screen center
    const Vector2D activeCenter = Vector2D{activeBox.x + activeBox.w / 2.0f,
                                           activeBox.y + activeBox.h / 2.0f};
    const Vector2D screenCenter = monitorSize / 2.0f;
    *pos = (screenCenter - activeCenter) * scale;

    auto monitor = pMonitor.lock();
    size->setCallbackOnEnd([monitor](auto var) { removeOverview(var, monitor); });
    closing = true;
    redrawAll();

    // Switch workspace after starting the closing animation (exactly like hyprexpo)
    if (selectedIndex >= 0 && selectedIndex < (int)images.size()) {
        auto& targetImage = images[selectedIndex];
        int64_t targetWorkspaceID = targetImage.workspaceID;

        // If this is a placeholder, create a new workspace
        if (targetWorkspaceID == -1) {
            targetWorkspaceID = findFirstAvailableWorkspaceID();
            targetImage.workspaceID = targetWorkspaceID;
        }

        if (targetWorkspaceID > 0 &&
            targetWorkspaceID != pMonitor->activeWorkspaceID()) {
            pMonitor->setSpecialWorkspace(nullptr);

            const auto NEWIDWS = g_pCompositor->getWorkspaceByID(targetWorkspaceID);
            const auto OLDWS = pMonitor->m_activeWorkspace;

            if (!NEWIDWS)
                g_pKeybindManager->changeworkspace(std::to_string(targetWorkspaceID));
            else
                g_pKeybindManager->changeworkspace(NEWIDWS->getConfigName());

            // Start animations for workspace transition (like hyprexpo)
            g_pDesktopAnimationManager->startAnimation(
                pMonitor->m_activeWorkspace,
                CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
            g_pDesktopAnimationManager->startAnimation(
                OLDWS, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);
            startedOn = pMonitor->m_activeWorkspace;
        }
    }
}

void COverview::startCloseAnimation() {
    // Start the closing animation without workspace switching
    const auto& activeBox = images[activeIndex].box;
    const Vector2D monitorSize = pMonitor->m_size;

    const float scaleX = monitorSize.x / activeBox.w;
    const float scaleY = monitorSize.y / activeBox.h;
    const float scale = std::min(scaleX, scaleY);

    *size = monitorSize * scale;

    const Vector2D activeCenter = Vector2D{activeBox.x + activeBox.w / 2.0f,
                                           activeBox.y + activeBox.h / 2.0f};
    const Vector2D screenCenter = monitorSize / 2.0f;
    *pos = (screenCenter - activeCenter) * scale;

    auto monitor = pMonitor.lock();
    size->setCallbackOnEnd([monitor](auto var) { removeOverview(var, monitor); });
    redrawAll();
}

void COverview::onPreRender() {
    if (damageDirty) {
        damageDirty = false;
        redrawID(activeIndex);
    }
}

void COverview::render() {
    g_pHyprRenderer->m_renderPass.add(makeUnique<COverviewPassElement>(this));
}

void COverview::fullRender() {
    g_pHyprOpenGL->clear(BG_COLOR.stripA());

    const Vector2D monitorSize = pMonitor->m_size;
    const float    monScale    = pMonitor->m_scale;

    // Render background image if loaded
    if (g_pBackgroundTexture && g_pBackgroundTexture->m_texID != 0) {
        const Vector2D texSize = g_pBackgroundTexture->m_size;
        CBox bgBox = {{0, 0}, monitorSize};

        // Scale background to cover the entire monitor while maintaining aspect ratio
        const float monitorAspect = monitorSize.x / monitorSize.y;
        const float textureAspect = texSize.x / texSize.y;

        if (textureAspect > monitorAspect) {
            // Texture is wider, scale by height
            const float scale = monitorSize.y / texSize.y;
            const float scaledWidth = texSize.x * scale;
            bgBox.x = -(scaledWidth - monitorSize.x) / 2.0f;
            bgBox.w = scaledWidth;
        } else {
            // Texture is taller, scale by width
            const float scale = monitorSize.x / texSize.x;
            const float scaledHeight = texSize.y * scale;
            bgBox.y = -(scaledHeight - monitorSize.y) / 2.0f;
            bgBox.h = scaledHeight;
        }

        bgBox.scale(monScale);
        bgBox.round();

        g_pHyprOpenGL->renderTexture(g_pBackgroundTexture, bgBox, {});
    }

    // Get animation values
    const Vector2D currentSize = size->value();
    const Vector2D currentPos  = pos->value();

    // Calculate zoom scale based on animation
    const float zoomScale = currentSize.x / monitorSize.x;

    // Fill empty workspace slots with background color
    // This happens when we have fewer than leftWorkspaceCount displayed
    size_t numLeftWorkspaces = images.size() - 1;  // Exclude active workspace
    if (numLeftWorkspaces < leftWorkspaceCount) {
        const float availableHeight   = monitorSize.y - (2 * PADDING);
        const float totalGaps         = (leftWorkspaceCount - 1) * GAP_WIDTH;
        const float leftPreviewHeight = (availableHeight - totalGaps) / leftWorkspaceCount;
        const float monitorAspectRatio = monitorSize.x / monitorSize.y;
        const float leftWorkspaceWidth = leftPreviewHeight * monitorAspectRatio;

        // Fill slots after the displayed workspaces
        for (size_t i = numLeftWorkspaces; i < leftWorkspaceCount; ++i) {
            float yPos = PADDING + i * (leftPreviewHeight + GAP_WIDTH);
            CBox emptyBox = {PADDING, yPos, leftWorkspaceWidth, leftPreviewHeight};

            // Apply zoom animation transformations
            emptyBox.x = emptyBox.x * zoomScale + currentPos.x;
            emptyBox.y = emptyBox.y * zoomScale + currentPos.y;
            emptyBox.w = emptyBox.w * zoomScale;
            emptyBox.h = emptyBox.h * zoomScale;

            emptyBox.scale(monScale);
            emptyBox.round();

            // Render background color for empty slot
            CRegion damage{0, 0, INT16_MAX, INT16_MAX};
            g_pHyprOpenGL->renderRect(emptyBox, BG_COLOR, {.damage = &damage});
        }
    }

    // Render each workspace
    // Track first placeholder for determining which ones to hide
    int firstPlaceholderIndex = -1;
    for (size_t i = 0; i < images.size() && i < (size_t)activeIndex; ++i) {
        if (!images[i].pWorkspace) {
            firstPlaceholderIndex = i;
            break;
        }
    }

    for (size_t i = 0; i < images.size(); ++i) {
        auto& image = images[i];

        // Skip rendering non-interactive placeholder workspaces
        bool isNonInteractivePlaceholder = (
            !image.pWorkspace &&
            i < (size_t)activeIndex &&
            firstPlaceholderIndex >= 0 &&
            (int)i > firstPlaceholderIndex
        );
        if (isNonInteractivePlaceholder) {
            continue;
        }

        // During closing animation, if a different workspace was selected,
        // render the selected workspace in the active workspace position
        CBox texbox = image.box;

        // For left side workspaces, recalculate position with scroll offset
        if (i != (size_t)activeIndex) {
            const float monitorAspectRatio = monitorSize.x / monitorSize.y;
            const float leftWorkspaceWidth = this->leftPreviewHeight * monitorAspectRatio;
            float yPos = PADDING + i * (this->leftPreviewHeight + GAP_WIDTH) -
                         scrollOffset;
            texbox = {PADDING, yPos, leftWorkspaceWidth, this->leftPreviewHeight};
        }

        auto* fbToRender = &image.fb;

        if (closing && selectedIndex >= 0 && selectedIndex != activeIndex) {
            if (i == (size_t)activeIndex) {
                // Render selected workspace's content in active workspace position
                fbToRender = &images[selectedIndex].fb;
            } else if (i == (size_t)selectedIndex) {
                // Don't render the selected workspace in its original position
                continue;
            }
        }

        // Scale the workspace framebuffer to fit in the box while maintaining aspect ratio
        const float fbAspect  = (float)fbToRender->m_size.x / fbToRender->m_size.y;
        const float boxAspect = texbox.w / texbox.h;

        CBox scaledBox = texbox;
        if (fbAspect > boxAspect) {
            // Framebuffer is wider, fit to width
            const float newHeight = texbox.w / fbAspect;
            scaledBox.y           = texbox.y + (texbox.h - newHeight) / 2.0f;
            scaledBox.h           = newHeight;
        } else {
            // Framebuffer is taller, fit to height
            const float newWidth = texbox.h * fbAspect;
            scaledBox.x          = texbox.x + (texbox.w - newWidth) / 2.0f;
            scaledBox.w          = newWidth;
        }

        // Apply zoom animation transformations (for both opening and closing)
        // During opening: animate from zoomed-in to normal view
        // During closing: animate from normal view to zoomed-in
        scaledBox.x = scaledBox.x * zoomScale;
        scaledBox.y = scaledBox.y * zoomScale;
        scaledBox.w = scaledBox.w * zoomScale;
        scaledBox.h = scaledBox.h * zoomScale;

        // Apply position offset
        scaledBox.x += currentPos.x;
        scaledBox.y += currentPos.y;

        scaledBox.scale(monScale);
        scaledBox.round();

        CRegion damage{0, 0, INT16_MAX, INT16_MAX};

        // Fade in/out non-active workspaces during animation
        float alpha = 1.0f;
        if (i != (size_t)activeIndex) {
            if (closing) {
                alpha = 1.0f - size->getPercent();  // Fade out as we zoom in
            } else {
                alpha = size->getPercent();  // Fade in as we zoom out
            }
        }

        g_pHyprOpenGL->renderTextureInternal(fbToRender->getTexture(),
                                              scaledBox,
                                              {.damage = &damage, .a = alpha});

        // Draw border around active workspace on left side
        if (i != (size_t)activeIndex && image.isActive) {
            // Top border
            CBox topBorder = {scaledBox.x, scaledBox.y, scaledBox.w, g_activeBorderSize};
            g_pHyprOpenGL->renderRect(topBorder, g_activeBorderColor, {.damage = &damage});

            // Bottom border
            CBox bottomBorder = {scaledBox.x, scaledBox.y + scaledBox.h - g_activeBorderSize,
                                scaledBox.w, g_activeBorderSize};
            g_pHyprOpenGL->renderRect(bottomBorder, g_activeBorderColor, {.damage = &damage});

            // Left border
            CBox leftBorder = {scaledBox.x, scaledBox.y, g_activeBorderSize, scaledBox.h};
            g_pHyprOpenGL->renderRect(leftBorder, g_activeBorderColor, {.damage = &damage});

            // Right border
            CBox rightBorder = {scaledBox.x + scaledBox.w - g_activeBorderSize, scaledBox.y,
                               g_activeBorderSize, scaledBox.h};
            g_pHyprOpenGL->renderRect(rightBorder, g_activeBorderColor, {.damage = &damage});
        }

        // Render workspace indicator (number or plus sign for new workspaces)
        bool isNewWorkspace = !image.pWorkspace;

        // For new workspaces, draw a plus sign in the center
        if (isNewWorkspace) {
            const float plusSize = std::min(scaledBox.w, scaledBox.h) * 0.5f;

            const float centerX = scaledBox.x + scaledBox.w / 2.0f;
            const float centerY = scaledBox.y + scaledBox.h / 2.0f;

            // Horizontal line of the plus
            CBox hLine = {
                centerX - plusSize / 2.0f,
                centerY - g_placeholderPlusSize / 2.0f,
                plusSize,
                g_placeholderPlusSize
            };

            // Vertical line of the plus
            CBox vLine = {
                centerX - g_placeholderPlusSize / 2.0f,
                centerY - plusSize / 2.0f,
                g_placeholderPlusSize,
                plusSize
            };

            g_pHyprOpenGL->renderRect(hLine, g_placeholderPlusColor, {.damage = &damage});
            g_pHyprOpenGL->renderRect(vLine, g_placeholderPlusColor, {.damage = &damage});
        } else if (image.workspaceID > 0 && i != (size_t)activeIndex) {
            // Show workspace number in top-left corner for left panel workspaces
            int workspaceNum = image.workspaceID;
            if (closing && selectedIndex >= 0 && selectedIndex != activeIndex &&
                i == (size_t)activeIndex) {
                // If rendering selected workspace in active position, show its number
                workspaceNum = images[selectedIndex].workspaceID;
            }

            std::string numberText = std::to_string(workspaceNum);
            auto textTexture = g_pHyprOpenGL->renderText(
                numberText, CHyprColor{1.0, 1.0, 1.0, 1.0}, 16, false);

            if (textTexture) {
                // Create a perfect circle background
                const float bgPadding = 4.0f;

                // Make the background a perfect circle using larger dimension
                const float circleSize = std::max(textTexture->m_size.x,
                                                  textTexture->m_size.y) +
                                         bgPadding * 2;

                CBox bgBox;
                bgBox.x = scaledBox.x;
                bgBox.y = scaledBox.y;
                bgBox.w = circleSize;
                bgBox.h = circleSize;

                // Render circular background (round = half makes perfect circle)
                g_pHyprOpenGL->renderRect(bgBox, CHyprColor{0.0, 0.0, 0.0, 0.7},
                                          {.damage = &damage,
                                           .round = (int)(circleSize / 2)});

                // Center text within the circular background
                CBox textBox;
                textBox.x = bgBox.x + (circleSize - textTexture->m_size.x) / 2;
                textBox.y = bgBox.y + (circleSize - textTexture->m_size.y) / 2;
                textBox.w = textTexture->m_size.x;
                textBox.h = textTexture->m_size.y;

                g_pHyprOpenGL->renderTexture(textTexture, textBox,
                                             {.damage = &damage, .a = alpha});
            }
        }
    }

    // Render drop zone indicator when dragging workspace
    if (g_dragState.isDragging && g_dragState.isWorkspaceDrag) {
        auto [dropZoneAbove, dropZoneBelow] =
            findDropZoneBetweenWorkspaces(lastMousePosLocal);

        bool isAdjacentToSource = false;
        int sourceIdx = g_dragState.sourceWorkspaceIndex;

        if (sourceIdx >= 0 && g_dragState.sourceOverview == this) {
            if ((dropZoneAbove == sourceIdx && dropZoneBelow >= 0) ||
                (dropZoneBelow == sourceIdx && dropZoneAbove >= 0) ||
                (dropZoneAbove >= 0 && dropZoneBelow == sourceIdx) ||
                (dropZoneBelow >= 0 && dropZoneAbove == sourceIdx)) {
                isAdjacentToSource = true;
            }

            if (sourceIdx == 0 &&
                dropZoneAbove == -2 && dropZoneBelow == 0) {
                isAdjacentToSource = true;
            }

            int lastLeftIndex = activeIndex - 1;
            if (sourceIdx == lastLeftIndex &&
                dropZoneAbove == lastLeftIndex && dropZoneBelow == -3) {
                isAdjacentToSource = true;
            }
        }

        bool isAfterPlaceholder = false;
        if (dropZoneAbove >= 0 &&
            dropZoneAbove < (int)images.size()) {
            if (!images[dropZoneAbove].pWorkspace) {
                isAfterPlaceholder = true;
            }
        }

        if (!isAdjacentToSource && !isAfterPlaceholder) {
            if (dropZoneAbove == -2 && dropZoneBelow == 0) {
                renderDropZoneAboveFirst();
            } else if (dropZoneBelow == -3 && dropZoneAbove >= 0) {
                renderDropZoneBelowLast(dropZoneAbove);
            } else if (dropZoneAbove >= 0 && dropZoneBelow >= 0) {
                renderDropZoneBetween(dropZoneAbove, dropZoneBelow);
            }
        }
    }

    // Render drag preview if dragging
    bool shouldRenderDragPreview = (
        g_dragState.isDragging &&
        g_dragState.dragPreviewFB.m_size.x > 0 &&
        (g_dragState.draggedWindow || g_dragState.isWorkspaceDrag)
    );
    if (shouldRenderDragPreview) {
        const float monScale = pMonitor->m_scale;

        // Scale down the preview size
        const Vector2D fullSize = g_dragState.dragPreviewFB.m_size;
        const Vector2D previewSize = fullSize * DRAG_PREVIEW_SCALE;

        // Position preview at cursor location (centered on cursor)
        CBox previewBox = {
            lastMousePosLocal.x - previewSize.x / 2.0f,
            lastMousePosLocal.y - previewSize.y / 2.0f,
            previewSize.x,
            previewSize.y
        };

        previewBox.scale(monScale);
        previewBox.round();

        CRegion damage{0, 0, INT16_MAX, INT16_MAX};
        g_pHyprOpenGL->renderTextureInternal(g_dragState.dragPreviewFB.getTexture(),
                                            previewBox,
                                            {.damage = &damage, .a = 0.9f});
    }
}

int COverview::findWorkspaceIndexAtPosition(const Vector2D& pos) {
    // Get animation values to transform boxes the same way as in rendering
    const Vector2D monitorSize = pMonitor->m_size;
    const Vector2D currentSize = size->value();
    const Vector2D currentPos  = this->pos->value();
    const float    zoomScale   = currentSize.x / monitorSize.x;

    // Check if position is within any workspace box (with animation applied)
    for (size_t i = 0; i < images.size(); ++i) {
        CBox box = images[i].box;

        // For left side workspaces, recalculate position with scroll offset
        if (i != (size_t)activeIndex) {
            const float monitorAspectRatio = monitorSize.x / monitorSize.y;
            const float leftWorkspaceWidth = this->leftPreviewHeight * monitorAspectRatio;
            float yPos = PADDING + i * (this->leftPreviewHeight + GAP_WIDTH) -
                         scrollOffset;
            box = {PADDING, yPos, leftWorkspaceWidth, this->leftPreviewHeight};
        }

        // Apply same transformation as in fullRender()
        CBox transformedBox = box;
        transformedBox.x = transformedBox.x * zoomScale + currentPos.x;
        transformedBox.y = transformedBox.y * zoomScale + currentPos.y;
        transformedBox.w = transformedBox.w * zoomScale;
        transformedBox.h = transformedBox.h * zoomScale;

        if (pos.x >= transformedBox.x &&
            pos.x <= transformedBox.x + transformedBox.w &&
            pos.y >= transformedBox.y &&
            pos.y <= transformedBox.y + transformedBox.h) {
            return i;
        }
    }
    return -1;
}

bool COverview::isMiddleClickWorkspaceDragAllowed(int clickedWorkspaceIndex) const {
    // Middle-click workspace dragging is only allowed for left side workspaces
    // (not the active workspace displayed on the right side)
    return clickedWorkspaceIndex >= 0 && clickedWorkspaceIndex != activeIndex;
}

void COverview::setupWorkspaceDragOnMiddleClick(int clickedWorkspaceIndex, const Vector2D& mousePos) {
    // Only setup workspace drag if clicking on a left side workspace
    if (!isMiddleClickWorkspaceDragAllowed(clickedWorkspaceIndex)) {
        return;
    }

    // Middle-click pressed on left side workspace - setup for workspace drag
    g_dragState.mouseButtonPressed = true;
    g_dragState.mouseDownPos = mousePos;
    g_dragState.sourceOverview = this;
    g_dragState.isWorkspaceDrag = true;
    g_dragState.sourceWorkspaceIndex = clickedWorkspaceIndex;
    g_dragState.draggedWindow = nullptr;  // No window for workspace drag
}

void COverview::renderDropZoneAboveFirst() {
    const Vector2D monitorSize = pMonitor->m_size;
    const Vector2D currentSize = size->value();
    const Vector2D currentPos  = pos->value();
    const float    zoomScale   = currentSize.x / monitorSize.x;

    const float monitorAspectRatio = monitorSize.x / monitorSize.y;
    const float leftWorkspaceWidth =
        this->leftPreviewHeight * monitorAspectRatio;

    float yPos0Untransformed =
        PADDING + 0 * (this->leftPreviewHeight + GAP_WIDTH) -
        scrollOffset;
    float yPos0Transformed = yPos0Untransformed * zoomScale + currentPos.y;

    float leftPanelX = PADDING * zoomScale + currentPos.x;
    float gapHeightTransformed = GAP_WIDTH * zoomScale;

    CBox dropZoneBox = {
        leftPanelX,
        yPos0Transformed - gapHeightTransformed,
        leftWorkspaceWidth * zoomScale,
        gapHeightTransformed
    };

    const float monScale = pMonitor->m_scale;
    dropZoneBox.scale(monScale);
    dropZoneBox.round();

    CRegion damage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprOpenGL->renderRect(dropZoneBox, g_dropZoneColor,
                               {.damage = &damage});
}

void COverview::renderDropZoneBelowLast(int lastIndex) {
    const Vector2D monitorSize = pMonitor->m_size;
    const Vector2D currentSize = size->value();
    const Vector2D currentPos  = pos->value();
    const float    zoomScale   = currentSize.x / monitorSize.x;

    const float monitorAspectRatio = monitorSize.x / monitorSize.y;
    const float leftWorkspaceWidth =
        this->leftPreviewHeight * monitorAspectRatio;

    float yPosLastUntransformed =
        PADDING + lastIndex * (this->leftPreviewHeight + GAP_WIDTH) -
        scrollOffset;
    float yPosLastTransformed =
        yPosLastUntransformed * zoomScale + currentPos.y;
    float yBottomLastTransformed =
        yPosLastTransformed + (this->leftPreviewHeight * zoomScale);

    float leftPanelX = PADDING * zoomScale + currentPos.x;
    float gapHeightTransformed = GAP_WIDTH * zoomScale;

    CBox dropZoneBox = {
        leftPanelX,
        yBottomLastTransformed,
        leftWorkspaceWidth * zoomScale,
        gapHeightTransformed
    };

    const float monScale = pMonitor->m_scale;
    dropZoneBox.scale(monScale);
    dropZoneBox.round();

    CRegion damage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprOpenGL->renderRect(dropZoneBox, g_dropZoneColor,
                               {.damage = &damage});
}

void COverview::renderDropZoneBetween(int above, int below) {
    const Vector2D monitorSize = pMonitor->m_size;
    const Vector2D currentSize = size->value();
    const Vector2D currentPos  = pos->value();
    const float    zoomScale   = currentSize.x / monitorSize.x;

    const float monitorAspectRatio = monitorSize.x / monitorSize.y;
    const float leftWorkspaceWidth =
        this->leftPreviewHeight * monitorAspectRatio;

    float yPosAbove =
        PADDING + above * (this->leftPreviewHeight + GAP_WIDTH) -
        scrollOffset;
    CBox boxAbove = {
        PADDING, yPosAbove,
        leftWorkspaceWidth, this->leftPreviewHeight
    };

    boxAbove.x = boxAbove.x * zoomScale + currentPos.x;
    boxAbove.y = boxAbove.y * zoomScale + currentPos.y;
    boxAbove.w = boxAbove.w * zoomScale;
    boxAbove.h = boxAbove.h * zoomScale;

    float yPosBelow =
        PADDING + below * (this->leftPreviewHeight + GAP_WIDTH) -
        scrollOffset;
    CBox boxBelow = {
        PADDING, yPosBelow,
        leftWorkspaceWidth, this->leftPreviewHeight
    };

    boxBelow.x = boxBelow.x * zoomScale + currentPos.x;
    boxBelow.y = boxBelow.y * zoomScale + currentPos.y;
    boxBelow.w = boxBelow.w * zoomScale;
    boxBelow.h = boxBelow.h * zoomScale;

    CBox dropZoneBox = {
        boxAbove.x,
        boxAbove.y + boxAbove.h,
        boxAbove.w,
        boxBelow.y - (boxAbove.y + boxAbove.h)
    };

    const float monScale = pMonitor->m_scale;
    dropZoneBox.scale(monScale);
    dropZoneBox.round();

    CRegion damage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprOpenGL->renderRect(dropZoneBox, g_dropZoneColor,
                               {.damage = &damage});
}

std::pair<int, int> COverview::findDropZoneBetweenWorkspaces(const Vector2D& pos) {
    // Returns a pair of workspace indices where the drop zone is between them
    // Returns {-1, -1} if not in the left panel
    // Returns {-2, 0} if above the first workspace
    // Returns {lastIndex, -3} if below the last workspace
    // Returns {i, i+1} if between workspaces i and i+1
    //
    // Logic: When cursor is over a workspace, check if it's in the top or bottom third:
    // - Top third: select drop zone ABOVE the workspace
    // - Bottom third: select drop zone BELOW the workspace

    const Vector2D monitorSize = pMonitor->m_size;
    const Vector2D currentSize = size->value();
    const Vector2D currentPos  = this->pos->value();
    const float    zoomScale   = currentSize.x / monitorSize.x;

    const float monitorAspectRatio = monitorSize.x / monitorSize.y;
    const float leftWorkspaceWidth = this->leftPreviewHeight * monitorAspectRatio;

    // Check if cursor is in the left panel area horizontally
    CBox leftPanelBox = {PADDING, PADDING, leftWorkspaceWidth,
                         monitorSize.y - 2 * PADDING};

    // Apply zoom transformation to left panel box
    leftPanelBox.x = leftPanelBox.x * zoomScale + currentPos.x;
    leftPanelBox.y = leftPanelBox.y * zoomScale + currentPos.y;
    leftPanelBox.w = leftPanelBox.w * zoomScale;
    leftPanelBox.h = leftPanelBox.h * zoomScale;

    // Check if cursor is within the left panel area horizontally
    if (pos.x < leftPanelBox.x || pos.x > leftPanelBox.x + leftPanelBox.w) {
        return {-1, -1};
    }

    if (activeIndex <= 0) {
        return {-1, -1};
    }

    // Find which workspace the cursor is over (or closest to)
    // We check each workspace and determine if cursor is in top or bottom half
    for (int i = 0; i < activeIndex; ++i) {
        float yPosUntransformed = PADDING + i * (this->leftPreviewHeight + GAP_WIDTH) - scrollOffset;
        float yPosTransformed = yPosUntransformed * zoomScale + currentPos.y;
        float workspaceHeightTransformed = this->leftPreviewHeight * zoomScale;
        float yBottomTransformed = yPosTransformed + workspaceHeightTransformed;

        // Check if cursor is within this workspace\'s bounds
        if (pos.y >= yPosTransformed && pos.y <= yBottomTransformed) {
            float yTopThird = yPosTransformed + workspaceHeightTransformed / 3.0f;
            float yBottomThird = yPosTransformed + workspaceHeightTransformed * 2.0f / 3.0f;

            if (pos.y < yTopThird) {
                // Cursor in TOP third - select drop zone ABOVE this workspace
                if (i == 0) {
                    // First workspace - drop zone is above it
                    return {-2, 0};
                } else {
                    // Drop zone is between workspace i-1 and i
                    return {i - 1, i};
                }
            } else if (pos.y > yBottomThird) {
                // Cursor in BOTTOM third - select drop zone BELOW this workspace
                if (i == activeIndex - 1) {
                    // Last workspace - drop zone is below it
                    return {i, -3};
                } else {
                    // Drop zone is between workspace i and i+1
                    return {i, i + 1};
                }
            }
        }
    }

    // Cursor is not over any workspace - check if it's above first or below last
    float yPos0Untransformed = PADDING - scrollOffset;
    float yPos0Transformed = yPos0Untransformed * zoomScale + currentPos.y;

    if (pos.y < yPos0Transformed) {
        return {-2, 0};
    }

    int lastIndex = activeIndex - 1;
    float yPosLastUntransformed = PADDING + lastIndex * (this->leftPreviewHeight + GAP_WIDTH) - scrollOffset;
    float yPosLastTransformed = yPosLastUntransformed * zoomScale + currentPos.y;
    float yBottomLastTransformed = yPosLastTransformed + (this->leftPreviewHeight * zoomScale);

    if (pos.y > yBottomLastTransformed) {
        return {lastIndex, -3};
    }

    // Cursor is in a gap between workspaces - find which gap
    for (int i = 0; i < activeIndex - 1; ++i) {
        float yPos1Untransformed = PADDING + i * (this->leftPreviewHeight + GAP_WIDTH) - scrollOffset;
        float yPos1Transformed = yPos1Untransformed * zoomScale + currentPos.y;
        float yBottom1Transformed = yPos1Transformed + (this->leftPreviewHeight * zoomScale);

        float yPos2Untransformed = PADDING + (i + 1) * (this->leftPreviewHeight + GAP_WIDTH) - scrollOffset;
        float yPos2Transformed = yPos2Untransformed * zoomScale + currentPos.y;

        if (pos.y >= yBottom1Transformed && pos.y <= yPos2Transformed) {
            return {i, i + 1};
        }
    }

    return {-1, -1};
}

std::pair<COverview*, int> COverview::findWorkspaceAtGlobalPosition(
    const Vector2D& globalPos
) {
    // Find which monitor the position is on
    const auto monitor = g_pCompositor->getMonitorFromVector(globalPos);
    if (!monitor)
        return {nullptr, -1};

    // Check if that monitor has an overview
    auto it = g_pOverviews.find(monitor);
    if (it == g_pOverviews.end() || !it->second)
        return {nullptr, -1};

    COverview* overview = it->second.get();

    // Convert global position to monitor-local position
    Vector2D localPos = globalPos - monitor->m_position;

    // Find workspace at that local position
    int workspaceIndex = overview->findWorkspaceIndexAtPosition(localPos);

    if (workspaceIndex >= 0)
        return {overview, workspaceIndex};

    return {nullptr, -1};
}

void COverview::setupSourceWorkspaceRefreshTimer(
    COverview* sourceOverview,
    const std::vector<int>& workspaceIndices
) {
    if (!sourceOverview || workspaceIndices.empty())
        return;

    struct TimerData {
        std::vector<int> workspaceIndices;
        int tickCount;
        wl_event_source* timerSource;
        PHLMONITOR monitor;
    };

    auto srcMon = sourceOverview->pMonitor.lock();
    auto* timerData = new TimerData{workspaceIndices, 0, nullptr, srcMon};

    auto* timer = wl_event_loop_add_timer(
        wl_display_get_event_loop(g_pCompositor->m_wlDisplay),
        [](void* data) -> int {
            auto* td = static_cast<TimerData*>(data);

            // Check if overview still exists for this monitor
            auto it = g_pOverviews.find(td->monitor);
            if (it != g_pOverviews.end() && it->second) {
                // Redraw source workspace
                for (int wsIdx : td->workspaceIndices) {
                    it->second->redrawID(wsIdx);
                }
                it->second->damage();

                td->tickCount++;
                // Redraw every 50ms for up to 1 second (20 ticks)
                if (td->tickCount < 20) {
                    wl_event_source_timer_update(td->timerSource, 50);
                    return 0;
                }
            }
            // Clean up after 20 ticks or if overview is closed
            delete td;
            return 0;
        },
        timerData
    );

    timerData->timerSource = timer;
    wl_event_source_timer_update(timer, 50);
}

void COverview::refreshSourceWorkspacesAfterCrossMonitorMove(
    COverview* sourceOverview,
    int sourceWorkspaceIndex
) {
    if (!sourceOverview || sourceWorkspaceIndex < 0)
        return;

    std::vector<int> workspacesToRefresh;
    workspacesToRefresh.push_back(sourceWorkspaceIndex);

    // If moving from active workspace, also refresh left-side
    if (sourceWorkspaceIndex == sourceOverview->activeIndex) {
        int leftSideActiveIndex = -1;
        for (size_t i = 0; i < (size_t)sourceOverview->activeIndex; ++i) {
            if (sourceOverview->images[i].isActive) {
                leftSideActiveIndex = i;
                break;
            }
        }
        if (leftSideActiveIndex >= 0) {
            workspacesToRefresh.push_back(leftSideActiveIndex);
        }
    }

    setupSourceWorkspaceRefreshTimer(sourceOverview, workspacesToRefresh);
}

void COverview::renderDragPreview() {
    int srcIdx = g_dragState.sourceWorkspaceIndex;
    auto* srcOverview = g_dragState.sourceOverview;

    if (!srcOverview)
        return;

    if (srcIdx < 0 || srcIdx >= (int)srcOverview->images.size())
        return;

    auto& sourceImage = srcOverview->images[srcIdx];

    // Handle workspace drag - render entire workspace
    if (g_dragState.isWorkspaceDrag) {
        // Use entire workspace framebuffer as preview
        Vector2D previewSize = sourceImage.fb.m_size;

        if (g_dragState.dragPreviewFB.m_size != previewSize) {
            g_dragState.dragPreviewFB.release();
            g_dragState.dragPreviewFB.alloc(
                previewSize.x, previewSize.y,
                pMonitor->m_output->state->state().drmFormat
            );
        }

        g_pHyprRenderer->makeEGLCurrent();

        CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
        auto& fb = g_dragState.dragPreviewFB;
        g_pHyprRenderer->beginRender(
            pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &fb
        );

        g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 0});

        // Render entire workspace texture
        CBox destBox = {
            0,
            0,
            (double)sourceImage.fb.m_size.x,
            (double)sourceImage.fb.m_size.y
        };

        g_pHyprOpenGL->renderTexturePrimitive(
            sourceImage.fb.getTexture(), destBox
        );

        g_pHyprOpenGL->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
        return;
    }

    // Handle window drag - render only window region
    const Vector2D winPos =
        g_dragState.draggedWindow->m_realPosition->value();
    const Vector2D winSize =
        g_dragState.draggedWindow->m_realSize->value();

    // Calculate window position in workspace framebuffer coordinates
    auto srcMon = g_dragState.sourceOverview->pMonitor.lock();
    float relX = (winPos.x - srcMon->m_position.x) / srcMon->m_size.x;
    float relY = (winPos.y - srcMon->m_position.y) / srcMon->m_size.y;
    float relW = winSize.x / srcMon->m_size.x;
    float relH = winSize.y / srcMon->m_size.y;

    // Convert to framebuffer pixel coordinates
    CBox sourceRegion = {
        relX * sourceImage.fb.m_size.x,
        relY * sourceImage.fb.m_size.y,
        relW * sourceImage.fb.m_size.x,
        relH * sourceImage.fb.m_size.y
    };

    // Clamp to FB bounds
    sourceRegion.x = std::max(0.0, sourceRegion.x);
    sourceRegion.y = std::max(0.0, sourceRegion.y);
    sourceRegion.w = std::min(
        sourceRegion.w, sourceImage.fb.m_size.x - sourceRegion.x
    );
    sourceRegion.h = std::min(
        sourceRegion.h, sourceImage.fb.m_size.y - sourceRegion.y
    );

    // Allocate FB at the exact size of window in workspace preview
    Vector2D previewSize = {sourceRegion.w, sourceRegion.h};
    if (g_dragState.dragPreviewFB.m_size != previewSize) {
        g_dragState.dragPreviewFB.release();
        g_dragState.dragPreviewFB.alloc(
            previewSize.x, previewSize.y,
            pMonitor->m_output->state->state().drmFormat
        );
    }

    g_pHyprRenderer->makeEGLCurrent();

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    auto& fb = g_dragState.dragPreviewFB;
    g_pHyprRenderer->beginRender(
        pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &fb
    );

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 0});

    // Render workspace texture with offset to show only window region
    CBox destBox = {
        -sourceRegion.x,
        -sourceRegion.y,
        (double)sourceImage.fb.m_size.x,
        (double)sourceImage.fb.m_size.y
    };

    g_pHyprOpenGL->renderTexturePrimitive(
        sourceImage.fb.getTexture(), destBox
    );

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();
}

int64_t COverview::findFirstAvailableWorkspaceID() {
    // Collect all workspace IDs currently in use across all monitors
    std::vector<int64_t> allUsedIDs;

    // Check all existing workspaces
    for (const auto& ws : g_pCompositor->m_workspaces) {
        if (!ws || ws->m_isSpecialWorkspace)
            continue;
        allUsedIDs.push_back(ws->m_id);
    }

    // Check all workspace IDs that are planned in overview slots
    for (const auto& [monitor, overview] : g_pOverviews) {
        if (!overview)
            continue;
        for (const auto& image : overview->images) {
            if (image.workspaceID > 0) {
                allUsedIDs.push_back(image.workspaceID);
            }
        }
    }

    // Sort and find first available ID starting from 1
    std::sort(allUsedIDs.begin(), allUsedIDs.end());
    allUsedIDs.erase(std::unique(allUsedIDs.begin(), allUsedIDs.end()),
                     allUsedIDs.end());

    int64_t nextID = 1;
    while (std::find(allUsedIDs.begin(), allUsedIDs.end(), nextID) !=
           allUsedIDs.end()) {
        nextID++;
    }

    return nextID;
}

PHLWINDOW COverview::findWindowAtPosition(const Vector2D& pos, int workspaceIndex) {
    if (workspaceIndex < 0 || workspaceIndex >= (int)images.size())
        return nullptr;

    const auto& image = images[workspaceIndex];
    if (!image.pWorkspace)
        return nullptr;

    // Get animation values to transform box the same way as in rendering
    const Vector2D monitorSize = pMonitor->m_size;
    const Vector2D currentSize = size->value();
    const Vector2D currentPos  = this->pos->value();
    const float    zoomScale   = currentSize.x / monitorSize.x;

    // Get the box, recalculating for left-side workspaces with scroll offset
    CBox box = image.box;
    if (workspaceIndex != activeIndex) {
        const float monitorAspectRatio = monitorSize.x / monitorSize.y;
        const float leftWorkspaceWidth = this->leftPreviewHeight * monitorAspectRatio;
        float yPos = PADDING + workspaceIndex * (this->leftPreviewHeight + GAP_WIDTH) -
                     scrollOffset;
        box = {PADDING, yPos, leftWorkspaceWidth, this->leftPreviewHeight};
    }

    // Apply same transformation as in fullRender()
    CBox transformedBox = box;
    transformedBox.x = transformedBox.x * zoomScale + currentPos.x;
    transformedBox.y = transformedBox.y * zoomScale + currentPos.y;
    transformedBox.w = transformedBox.w * zoomScale;
    transformedBox.h = transformedBox.h * zoomScale;

    // Apply aspect ratio scaling (same as in fullRender())
    const float fbAspect  = monitorSize.x / monitorSize.y;
    const float boxAspect = transformedBox.w / transformedBox.h;

    CBox scaledBox = transformedBox;
    if (fbAspect > boxAspect) {
        // Framebuffer is wider, fit to width
        const float newHeight = transformedBox.w / fbAspect;
        scaledBox.y           = transformedBox.y + (transformedBox.h - newHeight) / 2.0f;
        scaledBox.h           = newHeight;
    } else {
        // Framebuffer is taller, fit to height
        const float newWidth = transformedBox.h * fbAspect;
        scaledBox.x          = transformedBox.x + (transformedBox.w - newWidth) / 2.0f;
        scaledBox.w          = newWidth;
    }

    // Check if click is within the scaled box (not in black bars)
    if (pos.x < scaledBox.x || pos.x > scaledBox.x + scaledBox.w ||
        pos.y < scaledBox.y || pos.y > scaledBox.y + scaledBox.h) {
        return nullptr;  // Click is in black bar area
    }

    // Convert click position to workspace-relative coordinates
    // Account for the scaling of the preview
    float relX = (pos.x - scaledBox.x) / scaledBox.w;
    float relY = (pos.y - scaledBox.y) / scaledBox.h;

    // Convert to global coordinates (windows are positioned globally)
    Vector2D wsPos = {pMonitor->m_position.x + relX * monitorSize.x,
                      pMonitor->m_position.y + relY * monitorSize.y};

    // Find all windows at this position, then return the topmost one
    PHLWINDOW topmostWindow = nullptr;

    for (auto& w : g_pCompositor->m_windows) {
        if (!w->m_workspace || w->m_workspace != image.pWorkspace)
            continue;
        if (w->isHidden() || !w->m_isMapped)
            continue;

        // Use real position/size which works for both tiled and floating windows
        const Vector2D wPos = w->m_realPosition->value();
        const Vector2D wSize = w->m_realSize->value();

        if (wsPos.x >= wPos.x && wsPos.x <= wPos.x + wSize.x &&
            wsPos.y >= wPos.y && wsPos.y <= wPos.y + wSize.y) {
            // Found a window at this position
            // Prioritize: fullscreen > floating > tiled
            if (!topmostWindow) {
                topmostWindow = w;
            } else {
                // Check if this window should be on top
                if (w->isFullscreen() && !topmostWindow->isFullscreen()) {
                    topmostWindow = w;
                } else if (w->m_isFloating && !topmostWindow->m_isFloating &&
                           !topmostWindow->isFullscreen()) {
                    topmostWindow = w;
                }
            }
        }
    }

    return topmostWindow;
}

void COverview::moveWindowToWorkspace(PHLWINDOW window, int targetWorkspaceIndex) {
    if (!window || targetWorkspaceIndex < 0 || targetWorkspaceIndex >= (int)images.size())
        return;

    auto& targetImage = images[targetWorkspaceIndex];

    // Check if this is a non-interactive placeholder workspace
    // Only the first placeholder workspace (with +) should be interactive
    if (!targetImage.pWorkspace) {
        // Count how many placeholder workspaces come before this one
        int placeholderCount = 0;
        for (int i = 0; i < targetWorkspaceIndex && i < activeIndex; ++i) {
            if (!images[i].pWorkspace) {
                placeholderCount++;
            }
        }

        // If this is not the first placeholder, ignore the move
        if (placeholderCount > 0) {
            return;
        }
    }

    // Create workspace if it doesn't exist yet
    if (!targetImage.pWorkspace) {
        // Find first available workspace ID across all monitors
        const int64_t workspaceID = findFirstAvailableWorkspaceID();
        const auto monitorID = pMonitor->m_id;

        // Update the target image with the new workspace ID
        targetImage.workspaceID = workspaceID;

        // Create the workspace on the overview's monitor
        targetImage.pWorkspace = g_pCompositor->createNewWorkspace(workspaceID, monitorID);

        if (!targetImage.pWorkspace) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "Failed to create workspace",
                CHyprColor{0.8, 0.2, 0.2, 1.0},
                3000
            );
            return;
        }

        // Recalculate max scroll offset to allow scrolling to the new workspace + placeholder
        // Count how many non-placeholder workspaces exist on the left side
        size_t numExistingWorkspaces = 0;
        for (size_t i = 0; i < leftWorkspaceCount; ++i) {
            if (images[i].workspaceID != -1) {
                numExistingWorkspaces++;
            }
        }

        const Vector2D monitorSize = pMonitor->m_size;
        const float availableHeight = monitorSize.y - (2 * PADDING);

        // Include the first placeholder (with +) in scrolling calculation
        size_t numWorkspacesToShow = numExistingWorkspaces;
        if (numExistingWorkspaces < leftWorkspaceCount) {
            numWorkspacesToShow++; // Add 1 for the first placeholder
        }

        // Only allow scrolling if there are more than 4 workspaces to show
        if (numWorkspacesToShow <= 4) {
            this->maxScrollOffset = 0.0f;
        } else {
            // Calculate total height needed for workspaces + first placeholder
            float totalWorkspacesHeight = numWorkspacesToShow * this->leftPreviewHeight +
                                          (numWorkspacesToShow - 1) * GAP_WIDTH;
            this->maxScrollOffset = std::max(0.0f, totalWorkspacesHeight - availableHeight);
        }

        // Clamp current scroll offset to new bounds
        scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);

        // Update box positions for all left-side workspaces to reflect new scroll
        const float monitorAspectRatio = monitorSize.x / monitorSize.y;
        const float leftWorkspaceWidth = this->leftPreviewHeight * monitorAspectRatio;

        for (size_t i = 0; i < images.size(); ++i) {
            if (i != (size_t)activeIndex) {
                float yPos = PADDING + i * (this->leftPreviewHeight + GAP_WIDTH) - scrollOffset;
                images[i].box = {PADDING, yPos, leftWorkspaceWidth, this->leftPreviewHeight};
            }
        }

        // Trigger redraw
        damage();
    }

    // Don't move if it's already in the target workspace
    if (window->m_workspace == targetImage.pWorkspace)
        return;

    // Store source workspace for redrawing
    const auto sourceWorkspace = window->m_workspace;
    int sourceIndex = -1;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].pWorkspace == sourceWorkspace) {
            sourceIndex = i;
            break;
        }
    }

    // Use Hyprland's safe window move function
    // This handles all the layout management, fullscreen, floating state, etc.
    g_pCompositor->moveWindowToWorkspaceSafe(window, targetImage.pWorkspace);

    // Schedule repeating redraws for affected non-active workspaces
    // We need to refresh both source and target if they're not the active workspace
    std::vector<int> workspacesToRefresh;

    // Find the left-side index of the active workspace
    int leftSideActiveIndex = -1;
    for (size_t i = 0; i < (size_t)activeIndex; ++i) {
        if (images[i].isActive) {
            leftSideActiveIndex = i;
            break;
        }
    }

    // Add source workspace if it's not the active workspace
    if (sourceIndex >= 0 && sourceIndex != activeIndex) {
        workspacesToRefresh.push_back(sourceIndex);
    }

    // Add target workspace if it's not the active workspace and different from source
    if (targetWorkspaceIndex != activeIndex && targetWorkspaceIndex != sourceIndex) {
        workspacesToRefresh.push_back(targetWorkspaceIndex);
    }

    // If we moved to/from the active workspace, also refresh its left-side representation
    if (leftSideActiveIndex >= 0 &&
        (sourceIndex == activeIndex || targetWorkspaceIndex == activeIndex)) {
        workspacesToRefresh.push_back(leftSideActiveIndex);
    }

    if (!workspacesToRefresh.empty()) {
        setupSourceWorkspaceRefreshTimer(this, workspacesToRefresh);
    }
}

std::vector<uint8_t> convertPixelDataToRGBA(const guchar* pixels, int width, int height,
                                             int channels, int stride) {
    std::vector<uint8_t> pixelData(width * height * 4);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const guchar* src = pixels + y * stride + x * channels;
            uint8_t* dst = pixelData.data() + (y * width + x) * 4;

            if (channels == 4) {
                // RGBA format
                dst[0] = src[0];  // R
                dst[1] = src[1];  // G
                dst[2] = src[2];  // B
                dst[3] = src[3];  // A
            } else if (channels == 3) {
                // RGB format - add alpha channel
                dst[0] = src[0];  // R
                dst[1] = src[1];  // G
                dst[2] = src[2];  // B
                dst[3] = 255;     // A (fully opaque)
            }
        }
    }

    return pixelData;
}

bool createTextureFromPixelData(const std::vector<uint8_t>& pixelData,
                                 int width, int height) {
    const uint32_t drmFormat = DRM_FORMAT_ABGR8888;
    const uint32_t textureStride = width * 4;

    try {
        auto* pixels = const_cast<uint8_t*>(pixelData.data());
        g_pBackgroundTexture = makeShared<CTexture>(drmFormat, pixels, textureStride,
                                                     Vector2D{(double)width, (double)height},
                                                     true);
        return true;
    } catch (const std::exception& e) {
        Debug::log(ERR, "[workspace-overview] Failed to create texture: {}", e.what());
        g_pBackgroundTexture.reset();
        return false;
    }
}

void COverview::handleWorkspaceReordering() {
    if (!g_dragState.sourceOverview)
        return;

    int sourceIdx = g_dragState.sourceWorkspaceIndex;

    if (sourceIdx < 0 || sourceIdx >= g_dragState.sourceOverview->activeIndex)
        return;

    // Don't allow reordering placeholders
    if (!g_dragState.sourceOverview->images[sourceIdx].pWorkspace)
        return;

    // Check if this is a cross-monitor drop
    bool isCrossMonitor = (g_dragState.sourceOverview != this);

    if (isCrossMonitor) {
        // Cross-monitor workspace drag - need to find target monitor first
        auto mousePos = g_pInputManager->getMouseCoordsInternal();

        const auto monitor = g_pCompositor->getMonitorFromVector(mousePos);
        if (!monitor)
            return;

        auto it = g_pOverviews.find(monitor);
        if (it == g_pOverviews.end() || !it->second)
            return;

        COverview* targetOverview = it->second.get();
        Vector2D localPos = mousePos - monitor->m_position;

        // Find drop zone on target monitor
        auto [dropZoneAbove, dropZoneBelow] = targetOverview->findDropZoneBetweenWorkspaces(localPos);

        if (dropZoneAbove < 0 && dropZoneBelow < 0)
            return;

        // Calculate target index from drop zone (use -1 for sourceIdx since it's cross-monitor)
        int targetIdx = targetOverview->calculateTargetIndexFromDropZone(-1, dropZoneAbove, dropZoneBelow);

        if (targetIdx < 0)
            return;

        // Perform cross-monitor workspace move
        moveCrossMonitorWorkspace(g_dragState.sourceOverview, sourceIdx, targetOverview, targetIdx);
    } else {
        // Same-monitor workspace reordering
        auto [dropZoneAbove, dropZoneBelow] = findDropZoneBetweenWorkspaces(lastMousePosLocal);

        if (dropZoneAbove < 0 && dropZoneBelow < 0)
            return;

        int targetIdx = calculateTargetIndexFromDropZone(sourceIdx, dropZoneAbove, dropZoneBelow);

        if (targetIdx < 0 || sourceIdx == targetIdx)
            return;

        reorderWorkspace(sourceIdx, targetIdx);
    }
}

int COverview::calculateTargetIndexFromDropZone(int sourceIdx, int dropZoneAbove,
                                                 int dropZoneBelow) {
    if (dropZoneAbove == -2 && dropZoneBelow == 0) {
        // Dropping above first workspace
        return 0;
    } else if (dropZoneBelow == -3 && dropZoneAbove >= 0) {
        // Dropping below last workspace
        // Don't allow placement after a placeholder
        if (dropZoneAbove < (int)images.size() && !images[dropZoneAbove].pWorkspace)
            return -1;

        // For cross-monitor (sourceIdx < 0), return dropZoneAbove + 1
        if (sourceIdx < 0)
            return dropZoneAbove + 1;
        // For same-monitor, return dropZoneAbove (original logic)
        return dropZoneAbove;
    } else if (dropZoneAbove >= 0 && dropZoneBelow >= 0) {
        // Dropping between two workspaces
        // Don't allow placement after a placeholder
        if (dropZoneAbove < (int)images.size() && !images[dropZoneAbove].pWorkspace)
            return -1;

        if (sourceIdx < 0) {
            // Cross-monitor drop: always use dropZoneBelow
            return dropZoneBelow;
        } else if (sourceIdx < dropZoneBelow) {
            // Same-monitor moving down: target is one position before dropZoneBelow
            return dropZoneBelow - 1;
        } else {
            // Same-monitor moving up: target is dropZoneBelow
            return dropZoneBelow;
        }
    }
    return -1;
}

void COverview::reorderWorkspace(int sourceIdx, int targetIdx) {
    if (sourceIdx < 0 || sourceIdx >= activeIndex ||
        targetIdx < 0 || targetIdx >= activeIndex)
        return;

    // Don't allow reordering placeholders
    if (!images[sourceIdx].pWorkspace)
        return;

    // Collect windows, move them, then schedule refreshes
    std::vector<std::pair<int, std::vector<PHLWINDOW>>> workspaceWindows;
    collectWorkspaceWindowsForReorder(sourceIdx, targetIdx, workspaceWindows);
    moveWindowsForReorder(sourceIdx, targetIdx, workspaceWindows);
    scheduleWorkspaceRefreshes(sourceIdx, targetIdx);
}

void COverview::collectWorkspaceWindowsForReorder(
    int sourceIdx, int targetIdx,
    std::vector<std::pair<int, std::vector<PHLWINDOW>>>& workspaceWindows) {

    int startIdx = sourceIdx < targetIdx ? sourceIdx : targetIdx;
    int endIdx = sourceIdx < targetIdx ? targetIdx : sourceIdx;

    for (int i = startIdx; i <= endIdx; ++i) {
        if (!images[i].pWorkspace)
            continue;

        std::vector<PHLWINDOW> windows;
        for (auto& w : g_pCompositor->m_windows) {
            if (w->m_workspace == images[i].pWorkspace &&
                !w->isHidden() && w->m_isMapped) {
                windows.push_back(w);
            }
        }
        workspaceWindows.push_back({i, windows});
    }
}

void COverview::moveWindowsForReorder(
    int sourceIdx, int targetIdx,
    const std::vector<std::pair<int, std::vector<PHLWINDOW>>>& workspaceWindows) {

    bool movingDown = sourceIdx < targetIdx;

    for (const auto& [wsIdx, windows] : workspaceWindows) {
        int targetWsIdx;
        if (wsIdx == sourceIdx) {
            targetWsIdx = targetIdx;
        } else {
            targetWsIdx = movingDown ? wsIdx - 1 : wsIdx + 1;
        }

        if (targetWsIdx >= 0 && targetWsIdx < activeIndex &&
            images[targetWsIdx].pWorkspace) {
            for (auto& win : windows) {
                g_pCompositor->moveWindowToWorkspaceSafe(
                    win, images[targetWsIdx].pWorkspace
                );
            }
        }
    }
}

void COverview::scheduleWorkspaceRefreshes(int sourceIdx, int targetIdx) {
    std::vector<int> workspacesToRefresh;
    int minIdx = std::min(sourceIdx, targetIdx);
    int maxIdx = std::max(sourceIdx, targetIdx);

    for (int i = minIdx; i <= maxIdx; ++i) {
        if (i != activeIndex) {
            workspacesToRefresh.push_back(i);
        }
    }

    // Also refresh left-side active workspace if affected
    int leftSideActiveIndex = -1;
    for (size_t i = 0; i < (size_t)activeIndex; ++i) {
        if (images[i].isActive) {
            leftSideActiveIndex = i;
            break;
        }
    }

    if (leftSideActiveIndex >= 0 &&
        leftSideActiveIndex >= minIdx && leftSideActiveIndex <= maxIdx) {
        if (std::find(workspacesToRefresh.begin(), workspacesToRefresh.end(),
                     leftSideActiveIndex) == workspacesToRefresh.end()) {
            workspacesToRefresh.push_back(leftSideActiveIndex);
        }
    }

    if (!workspacesToRefresh.empty()) {
        setupSourceWorkspaceRefreshTimer(this, workspacesToRefresh);
    }
}

void COverview::moveCrossMonitorWorkspace(COverview* sourceOverview, int sourceIdx,
                                          COverview* targetOverview, int targetIdx) {
    if (!sourceOverview || !targetOverview || sourceIdx < 0 || targetIdx < 0)
        return;

    if (sourceIdx >= sourceOverview->activeIndex || targetIdx >= targetOverview->activeIndex)
        return;

    auto& sourceImage = sourceOverview->images[sourceIdx];
    if (!sourceImage.pWorkspace)
        return;

    // Move all windows from dragged workspace to target workspace
    std::vector<PHLWINDOW> draggedWindows;
    for (auto& w : g_pCompositor->m_windows) {
        if (w->m_workspace == sourceImage.pWorkspace && !w->isHidden() && w->m_isMapped) {
            draggedWindows.push_back(w);
        }
    }

    // Move windows from source monitor workspaces with indices > sourceIdx up
    moveSourceMonitorWindowsUp(sourceOverview, sourceIdx);

    // Move windows from target monitor workspaces with indices >= targetIdx down
    // IMPORTANT: Do this BEFORE creating the target workspace to avoid moving it too
    moveTargetMonitorWindowsDown(targetOverview, targetIdx);

    // Get or create target workspace
    auto& targetImage = targetOverview->images[targetIdx];
    if (!targetImage.pWorkspace) {
        const int64_t workspaceID = targetOverview->findFirstAvailableWorkspaceID();
        const auto monitorID = targetOverview->pMonitor->m_id;
        targetImage.workspaceID = workspaceID;
        targetImage.pWorkspace = g_pCompositor->createNewWorkspace(workspaceID, monitorID, "");
    }

    // Move dragged windows to target workspace
    for (auto& win : draggedWindows) {
        g_pCompositor->moveWindowToWorkspaceSafe(win, targetImage.pWorkspace);
    }

    // Schedule refreshes for both monitors
    std::vector<int> sourceRefreshIndices;
    for (int i = sourceIdx; i < sourceOverview->activeIndex; ++i) {
        if (i != sourceOverview->activeIndex) {
            sourceRefreshIndices.push_back(i);
        }
    }
    if (!sourceRefreshIndices.empty()) {
        setupSourceWorkspaceRefreshTimer(sourceOverview, sourceRefreshIndices);
    }

    std::vector<int> targetRefreshIndices;
    for (int i = targetIdx; i < targetOverview->activeIndex; ++i) {
        if (i != targetOverview->activeIndex) {
            targetRefreshIndices.push_back(i);
        }
    }
    if (!targetRefreshIndices.empty()) {
        setupSourceWorkspaceRefreshTimer(targetOverview, targetRefreshIndices);
    }

    // Recalculate max scroll offset for both monitors
    sourceOverview->recalculateMaxScrollOffset();
    targetOverview->recalculateMaxScrollOffset();
}

void COverview::moveSourceMonitorWindowsUp(COverview* sourceOverview, int sourceIdx) {
    if (!sourceOverview || sourceIdx < 0 || sourceIdx >= sourceOverview->activeIndex)
        return;

    // Move windows from workspaces with indices > sourceIdx up by one position
    for (int i = sourceIdx + 1; i < sourceOverview->activeIndex; ++i) {
        auto& currentImage = sourceOverview->images[i];
        if (!currentImage.pWorkspace)
            continue;

        int targetIdx = i - 1;
        if (targetIdx < 0 || targetIdx >= sourceOverview->activeIndex)
            continue;

        auto& targetImage = sourceOverview->images[targetIdx];

        // Collect windows from current workspace
        std::vector<PHLWINDOW> windows;
        for (auto& w : g_pCompositor->m_windows) {
            if (w->m_workspace == currentImage.pWorkspace && !w->isHidden() && w->m_isMapped) {
                windows.push_back(w);
            }
        }

        // Create target workspace if needed
        if (!targetImage.pWorkspace) {
            const int64_t workspaceID = sourceOverview->findFirstAvailableWorkspaceID();
            const auto monitorID = sourceOverview->pMonitor->m_id;
            targetImage.workspaceID = workspaceID;
            targetImage.pWorkspace = g_pCompositor->createNewWorkspace(workspaceID, monitorID, "");
        }

        // Move windows to target workspace
        for (auto& win : windows) {
            g_pCompositor->moveWindowToWorkspaceSafe(win, targetImage.pWorkspace);
        }
    }
}

void COverview::moveTargetMonitorWindowsDown(COverview* targetOverview, int targetIdx) {
    if (!targetOverview || targetIdx < 0)
        return;

    int maxIdx = targetOverview->activeIndex - 1;

    if (targetIdx > maxIdx)
        return;

    // Move windows from workspaces with indices >= targetIdx down by one position
    // Process in reverse order to avoid overwriting
    for (int i = maxIdx; i >= targetIdx; --i) {
        auto& currentImage = targetOverview->images[i];
        if (!currentImage.pWorkspace)
            continue;

        int newIdx = i + 1;

        if (newIdx >= (int)targetOverview->images.size())
            continue;

        auto& newImage = targetOverview->images[newIdx];

        // Collect windows from current workspace
        std::vector<PHLWINDOW> windows;
        for (auto& w : g_pCompositor->m_windows) {
            if (w->m_workspace == currentImage.pWorkspace && !w->isHidden() && w->m_isMapped) {
                windows.push_back(w);
            }
        }

        // Create new workspace if needed
        if (!newImage.pWorkspace) {
            const int64_t workspaceID = targetOverview->findFirstAvailableWorkspaceID();
            const auto monitorID = targetOverview->pMonitor->m_id;
            newImage.workspaceID = workspaceID;
            newImage.pWorkspace = g_pCompositor->createNewWorkspace(workspaceID, monitorID, "");
        }

        // Move windows to new workspace
        for (auto& win : windows) {
            g_pCompositor->moveWindowToWorkspaceSafe(win, newImage.pWorkspace);
        }
    }
}

void COverview::recalculateMaxScrollOffset() {
    // Count how many non-placeholder workspaces exist on the left side
    size_t numExistingWorkspaces = 0;
    for (size_t i = 0; i < (size_t)activeIndex; ++i) {
        if (images[i].pWorkspace) {
            numExistingWorkspaces++;
        }
    }

    const Vector2D monitorSize = pMonitor->m_size;
    const float availableHeight = monitorSize.y - (2 * PADDING);

    // Include the first placeholder (with +) in scrolling calculation
    size_t numWorkspacesToShow = numExistingWorkspaces;
    if (numExistingWorkspaces < (size_t)activeIndex) {
        numWorkspacesToShow++; // Add 1 for the first placeholder
    }

    // Only allow scrolling if there are more than 4 workspaces to show
    if (numWorkspacesToShow <= 4) {
        this->maxScrollOffset = 0.0f;
    } else {
        // Calculate total height needed for workspaces + first placeholder
        float totalWorkspacesHeight = numWorkspacesToShow * this->leftPreviewHeight +
                                      (numWorkspacesToShow - 1) * GAP_WIDTH;
        this->maxScrollOffset = std::max(0.0f, totalWorkspacesHeight - availableHeight);
    }

    // Clamp current scroll offset to new bounds
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScrollOffset);
}

void loadBackgroundImage(const std::string& path) {
    if (path.empty()) {
        g_pBackgroundTexture.reset();
        return;
    }

    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(path.c_str(), &error);

    if (!pixbuf) {
        Debug::log(ERR, "[workspace-overview] Failed to load background image: {}",
                   error ? error->message : "unknown error");
        if (error)
            g_error_free(error);
        g_pBackgroundTexture.reset();
        return;
    }

    const int width = gdk_pixbuf_get_width(pixbuf);
    const int height = gdk_pixbuf_get_height(pixbuf);
    const int channels = gdk_pixbuf_get_n_channels(pixbuf);

    if (channels != 3 && channels != 4) {
        Debug::log(ERR, "[workspace-overview] Unsupported image channel count: {}",
                   channels);
        g_object_unref(pixbuf);
        g_pBackgroundTexture.reset();
        return;
    }

    const int stride = gdk_pixbuf_get_rowstride(pixbuf);
    const guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);

    auto pixelData = convertPixelDataToRGBA(pixels, width, height, channels, stride);
    g_object_unref(pixbuf);

    if (createTextureFromPixelData(pixelData, width, height)) {
        Debug::log(LOG, "[workspace-overview] Loaded background image: {} ({}x{})",
                   path, width, height);
    }
}
