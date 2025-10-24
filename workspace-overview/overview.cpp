#include "overview.hpp"
#include <algorithm>
#include <any>
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

static void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview->damage();
}

void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview.reset();
}

COverview::~COverview() {
    g_pHyprRenderer->makeEGLCurrent();
    images.clear(); // otherwise we get a vram leak
    g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
}

COverview::COverview(PHLWORKSPACE startedOn_) : startedOn(startedOn_) {
    const auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
    pMonitor            = PMONITOR;

    // We need LEFT_WORKSPACES + 1 (for the active workspace on the right)
    images.resize(LEFT_WORKSPACES + 1);

    // Get current workspace ID
    int currentID = pMonitor->activeWorkspaceID();

    // Get all workspaces and filter by monitor
    auto allWorkspaces = g_pCompositor->getWorkspacesCopy();
    std::vector<int64_t> monitorWorkspaceIDs;

    for (const auto& ws : allWorkspaces) {
        if (!ws || ws->m_isSpecialWorkspace)
            continue;

        auto wsMonitor = ws->m_monitor.lock();

        // Include workspaces that belong to this monitor OR are unassigned
        if (!wsMonitor || wsMonitor == pMonitor) {
            monitorWorkspaceIDs.push_back(ws->m_id);
        }
    }

    // Sort workspace IDs
    std::sort(monitorWorkspaceIDs.begin(), monitorWorkspaceIDs.end());

    // Determine how many workspaces to show on the left
    size_t numExisting = monitorWorkspaceIDs.size();
    size_t numToShow;

    // If 0-3 existing workspaces, show them all plus 1 with plus sign
    if (numExisting < LEFT_WORKSPACES) {
        numToShow = numExisting + 1;  // Show existing + 1 with plus
    }
    // If 4 or more existing workspaces, show first 4
    else {
        numToShow = LEFT_WORKSPACES;
    }

    // Populate left side workspaces
    size_t numExistingToShow = std::min(numToShow, numExisting);

    for (size_t i = 0; i < numExistingToShow; ++i) {
        auto& image = images[i];
        image.workspaceID = monitorWorkspaceIDs[i];
        // Mark if this is the active workspace
        image.isActive = (monitorWorkspaceIDs[i] == currentID);
    }

    // Fill remaining slots with new workspace IDs (not assigned to any monitor)
    if (numExistingToShow < numToShow) {
        // Collect all workspace IDs already in use (on any monitor)
        std::vector<int64_t> allUsedIDs;
        for (const auto& ws : allWorkspaces) {
            if (!ws || ws->m_isSpecialWorkspace)
                continue;
            allUsedIDs.push_back(ws->m_id);
        }
        std::sort(allUsedIDs.begin(), allUsedIDs.end());

        // Find new workspace IDs that aren't in use
        int nextID = 1;
        for (size_t i = numExistingToShow; i < numToShow; ++i) {
            // Find an ID that's not in use
            while (std::find(allUsedIDs.begin(), allUsedIDs.end(), nextID) !=
                   allUsedIDs.end()) {
                nextID++;
            }

            auto& image = images[i];
            image.workspaceID = nextID;
            image.isActive = false;

            // Mark this ID as used so we don't reuse it
            allUsedIDs.push_back(nextID);
            std::sort(allUsedIDs.begin(), allUsedIDs.end());
            nextID++;
        }
    }

    // Keep numToShow for left side + 1 for right side active workspace
    images.resize(numToShow + 1);

    // Last image is for the active workspace (right side) - for real-time updates
    images[numToShow].workspaceID = currentID;
    images[numToShow].isActive    = true;
    activeIndex                   = numToShow;

    // Note: The active workspace appears both in the left side (in its proper position)
    // and on the right side (for real-time updates). The right side will be rendered
    // from activeIndex, while the left side shows all workspaces including active.

    g_pHyprRenderer->makeEGLCurrent();

    // Calculate layout with equal margins on left, top, and bottom for left workspaces
    const Vector2D monitorSize       = pMonitor->m_size;
    const float    availableHeight   = monitorSize.y - (2 * PADDING);
    const float    totalGaps         = (LEFT_WORKSPACES - 1) * GAP_WIDTH;
    const float    leftPreviewHeight = (availableHeight - totalGaps) / LEFT_WORKSPACES;

    // Left workspaces: calculate width so that left margin = top/bottom margin = PADDING
    // The aspect ratio should match the monitor's aspect ratio for proper scaling
    const float monitorAspectRatio = monitorSize.x / monitorSize.y;
    const float leftWorkspaceWidth = leftPreviewHeight * monitorAspectRatio;

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

        g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(image.workspaceID);

        if (PWORKSPACE) {
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
        } else
            g_pHyprRenderer->renderWorkspace(PMONITOR, PWORKSPACE, Time::steadyNow(), monbox);

        // Calculate box positions for rendering
        if (i == (size_t)activeIndex) {
            // Right side - active workspace (maximized with consistent margins)
            // This is the last element, used for real-time updates
            image.box = {activeX, PADDING, activeMaxWidth, activeMaxHeight};
        } else {
            // Left side - workspace list (left margin = PADDING, same as top)
            float yPos = PADDING + i * (leftPreviewHeight + GAP_WIDTH);
            image.box = {PADDING, yPos, leftWorkspaceWidth, leftPreviewHeight};
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

    // Setup mouse move hook to track cursor position and detect dragging
    auto onMouseMove = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        lastMousePosLocal =
            g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

        // Trigger redraw during drag to update preview position
        if (isDragging) {
            damage();
        }

        // Check if we're dragging (similar to Hyprland's drag detection)
        if (mouseButtonPressed && !isDragging) {
            const float distanceX = std::abs(lastMousePosLocal.x - mouseDownPos.x);
            const float distanceY = std::abs(lastMousePosLocal.y - mouseDownPos.y);

            if (distanceX > DRAG_THRESHOLD || distanceY > DRAG_THRESHOLD) {
                isDragging = true;

                // Render drag preview if we have a window
                if (draggedWindow) {
                    const Vector2D windowSize = draggedWindow->m_realSize->value();

                    // Allocate framebuffer for drag preview at window size
                    if (dragPreviewFB.m_size != windowSize) {
                        dragPreviewFB.release();
                        dragPreviewFB.alloc(windowSize.x, windowSize.y,
                                           pMonitor->m_output->state->state().drmFormat);
                    }

                    // Render window preview to framebuffer
                    g_pHyprRenderer->makeEGLCurrent();

                    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
                    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage,
                                                RENDER_MODE_FULL_FAKE, nullptr, &dragPreviewFB);

                    // Clear with semi-transparent background
                    g_pHyprOpenGL->clear(CHyprColor{0.1, 0.1, 0.1, 0.8});

                    // Render a placeholder colored rectangle
                    CBox placeholderBox = {10, 10, windowSize.x - 20, windowSize.y - 20};
                    g_pHyprOpenGL->renderRect(placeholderBox, CHyprColor{0.3, 0.5, 0.7, 1.0}, {});

                    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
                    g_pHyprRenderer->endRender();
                }
            }
        }
    };
    mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onMouseMove);

    // Setup mouse button hook to detect workspace clicks vs drags
    auto onMouseButton = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled = true;

        auto e = std::any_cast<IPointer::SButtonEvent>(param);

        // Any button except left click: consume event without doing anything
        if (e.button != BTN_LEFT) {
            return;
        }

        if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
            // Mouse button pressed - record position and find window
            mouseButtonPressed = true;
            mouseDownPos = lastMousePosLocal;
            isDragging = false;

            // Find which workspace was clicked
            sourceWorkspaceIndex = findWorkspaceIndexAtPosition(mouseDownPos);

            // Find window at click position in that workspace
            draggedWindow = findWindowAtPosition(mouseDownPos, sourceWorkspaceIndex);
        } else {
            // Mouse button released
            mouseButtonPressed = false;

            if (isDragging) {
                // Dragging detected - handle window move
                if (draggedWindow) {
                    // Find target workspace at release position
                    int targetIndex = findWorkspaceIndexAtPosition(lastMousePosLocal);

                    if (targetIndex >= 0 && targetIndex != sourceWorkspaceIndex) {
                        // Move window to different workspace
                        moveWindowToWorkspace(draggedWindow, targetIndex);
                    } else if (targetIndex == sourceWorkspaceIndex) {
                        HyprlandAPI::addNotification(PHANDLE, "Same workspace",
                                                     CHyprColor{0.8, 0.6, 0.2, 1.0}, 2000);
                    } else {
                        HyprlandAPI::addNotification(PHANDLE, "Outside workspace area",
                                                     CHyprColor{0.8, 0.6, 0.2, 1.0}, 2000);
                    }
                } else {
                    HyprlandAPI::addNotification(PHANDLE, "Drag (no window)",
                                                 CHyprColor{0.8, 0.4, 0.2, 1.0}, 2000);
                }
            } else {
                // Click (not drag) - select workspace and close
                selectWorkspaceAtPosition(lastMousePosLocal);
                close();
            }

            // Reset drag state
            draggedWindow = nullptr;
            sourceWorkspaceIndex = -1;
            isDragging = false;

            // Release drag preview framebuffer
            if (dragPreviewFB.m_size.x > 0) {
                dragPreviewFB.release();
            }
        }
    };

    mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onMouseButton);
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

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

    const auto   PWORKSPACE  = image.pWorkspace;
    PHLWORKSPACE openSpecial = pMonitor->m_activeSpecialWorkspace;

    if (openSpecial)
        pMonitor->m_activeSpecialWorkspace.reset();

    startedOn->m_visible = false;

    if (PWORKSPACE) {
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
    } else
        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE,
                                         Time::steadyNow(), monbox);

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

    size->setCallbackOnEnd(removeOverview);
    closing = true;
    redrawAll();

    // Switch workspace after starting the closing animation (exactly like hyprexpo)
    if (selectedIndex >= 0 && selectedIndex < (int)images.size()) {
        const auto& targetImage = images[selectedIndex];

        if (targetImage.workspaceID != pMonitor->activeWorkspaceID()) {
            pMonitor->setSpecialWorkspace(nullptr);

            const auto NEWIDWS =
                g_pCompositor->getWorkspaceByID(targetImage.workspaceID);
            const auto OLDWS = pMonitor->m_activeWorkspace;

            if (!NEWIDWS)
                g_pKeybindManager->changeworkspace(
                    std::to_string(targetImage.workspaceID));
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

void COverview::onPreRender() {
    if (damageDirty) {
        damageDirty = false;
        redrawID(activeIndex);
    }
}

void COverview::render() {
    g_pHyprRenderer->m_renderPass.add(makeUnique<COverviewPassElement>());
}

void COverview::fullRender() {
    g_pHyprOpenGL->clear(BG_COLOR.stripA());

    const Vector2D monitorSize = pMonitor->m_size;
    const float    monScale    = pMonitor->m_scale;

    // Get animation values
    const Vector2D currentSize = size->value();
    const Vector2D currentPos  = pos->value();

    // Calculate zoom scale based on animation
    const float zoomScale = currentSize.x / monitorSize.x;

    // Fill empty workspace slots with background color
    // This happens when we have fewer than LEFT_WORKSPACES displayed
    size_t numLeftWorkspaces = images.size() - 1;  // Exclude active workspace
    if (numLeftWorkspaces < LEFT_WORKSPACES) {
        const float availableHeight   = monitorSize.y - (2 * PADDING);
        const float totalGaps         = (LEFT_WORKSPACES - 1) * GAP_WIDTH;
        const float leftPreviewHeight = (availableHeight - totalGaps) / LEFT_WORKSPACES;
        const float monitorAspectRatio = monitorSize.x / monitorSize.y;
        const float leftWorkspaceWidth = leftPreviewHeight * monitorAspectRatio;

        // Fill slots after the displayed workspaces
        for (size_t i = numLeftWorkspaces; i < LEFT_WORKSPACES; ++i) {
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
    for (size_t i = 0; i < images.size(); ++i) {
        auto& image = images[i];

        // During closing animation, if a different workspace was selected,
        // render the selected workspace in the active workspace position
        CBox texbox = image.box;
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
            const float borderThickness = 4.0f;  // Half of plus sign thickness
            const CHyprColor borderColor = CHyprColor{0.3, 0.5, 0.7, 1.0};  // Same as drag preview

            // Top border
            CBox topBorder = {scaledBox.x, scaledBox.y, scaledBox.w, borderThickness};
            g_pHyprOpenGL->renderRect(topBorder, borderColor, {.damage = &damage});

            // Bottom border
            CBox bottomBorder = {scaledBox.x, scaledBox.y + scaledBox.h - borderThickness,
                                scaledBox.w, borderThickness};
            g_pHyprOpenGL->renderRect(bottomBorder, borderColor, {.damage = &damage});

            // Left border
            CBox leftBorder = {scaledBox.x, scaledBox.y, borderThickness, scaledBox.h};
            g_pHyprOpenGL->renderRect(leftBorder, borderColor, {.damage = &damage});

            // Right border
            CBox rightBorder = {scaledBox.x + scaledBox.w - borderThickness, scaledBox.y,
                               borderThickness, scaledBox.h};
            g_pHyprOpenGL->renderRect(rightBorder, borderColor, {.damage = &damage});
        }

        // Render workspace indicator (number or plus sign for new workspaces)
        bool isNewWorkspace = !image.pWorkspace;

        // For new workspaces, draw a plus sign in the center
        if (isNewWorkspace) {
            const float plusSize = std::min(scaledBox.w, scaledBox.h) * 0.5f;
            const float lineThickness = 8.0f;

            const float centerX = scaledBox.x + scaledBox.w / 2.0f;
            const float centerY = scaledBox.y + scaledBox.h / 2.0f;

            // Horizontal line of the plus
            CBox hLine = {
                centerX - plusSize / 2.0f,
                centerY - lineThickness / 2.0f,
                plusSize,
                lineThickness
            };

            // Vertical line of the plus
            CBox vLine = {
                centerX - lineThickness / 2.0f,
                centerY - plusSize / 2.0f,
                lineThickness,
                plusSize
            };

            g_pHyprOpenGL->renderRect(hLine, CHyprColor{1.0, 1.0, 1.0, 0.8}, {.damage = &damage});
            g_pHyprOpenGL->renderRect(vLine, CHyprColor{1.0, 1.0, 1.0, 0.8}, {.damage = &damage});
        } else {
            // For existing workspaces, show the workspace number in top-left corner
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

    // Render drag preview if dragging
    if (isDragging && draggedWindow && dragPreviewFB.m_size.x > 0) {
        const float monScale = pMonitor->m_scale;

        // Scale down the preview size
        const Vector2D fullSize = dragPreviewFB.m_size;
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
        g_pHyprOpenGL->renderTextureInternal(dragPreviewFB.getTexture(),
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
        const auto& box = images[i].box;

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

    // Apply same transformation as in fullRender()
    CBox transformedBox = image.box;
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

    // Create workspace if it doesn't exist yet
    if (!targetImage.pWorkspace) {
        const int64_t workspaceID = targetImage.workspaceID;
        const auto monitorID = pMonitor->m_id;

        // Create the workspace on the overview's monitor
        targetImage.pWorkspace = g_pCompositor->createNewWorkspace(workspaceID, monitorID);

        if (!targetImage.pWorkspace) {
            HyprlandAPI::addNotification(PHANDLE, "Failed to create workspace",
                                        CHyprColor{0.8, 0.2, 0.2, 1.0}, 3000);
            return;
        }

        // Trigger redraw to change plus sign to number
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

    // Exit fullscreen if needed
    const bool wasFullscreen = window->isFullscreen();
    if (wasFullscreen) {
        g_pCompositor->setWindowFullscreenInternal(window, FSMODE_NONE);
    }

    // Store floating state
    const bool wasFloating = window->m_isFloating;

    // Remove from source layout
    if (wasFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowRemovedFloating(window);
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowRemovedTiling(window);
    }

    // Move window to target workspace
    window->moveToWorkspace(targetImage.pWorkspace);

    // Add to target workspace (preserve floating state)
    if (wasFloating) {
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(window);
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedTiling(window);
    }

    // Recalculate layouts for both workspaces
    if (sourceWorkspace) {
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(
            sourceWorkspace->monitorID());
    }
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(
        targetImage.pWorkspace->monitorID());

    // Schedule repeating redraws for affected non-active workspaces
    // We need to refresh both source and target if they're not the active workspace
    struct TimerData {
        std::vector<int> workspaceIndices;
        int tickCount;
        wl_event_source* timerSource;
    };

    std::vector<int> workspacesToRefresh;

    // Add source workspace if it's not the active workspace
    if (sourceIndex >= 0 && sourceIndex != activeIndex) {
        workspacesToRefresh.push_back(sourceIndex);
    }

    // Add target workspace if it's not the active workspace and different from source
    if (targetWorkspaceIndex != activeIndex && targetWorkspaceIndex != sourceIndex) {
        workspacesToRefresh.push_back(targetWorkspaceIndex);
    }

    if (!workspacesToRefresh.empty()) {
        auto* timerData = new TimerData{workspacesToRefresh, 0, nullptr};

        auto* timer = wl_event_loop_add_timer(
            wl_display_get_event_loop(g_pCompositor->m_wlDisplay),
            [](void* data) -> int {
                auto* td = static_cast<TimerData*>(data);
                if (g_pOverview) {
                    // Redraw all affected workspaces
                    for (int workspaceIndex : td->workspaceIndices) {
                        g_pOverview->redrawID(workspaceIndex);
                    }
                    g_pOverview->damage();

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
            timerData);

        timerData->timerSource = timer;
        wl_event_source_timer_update(timer, 50);
    }
}
