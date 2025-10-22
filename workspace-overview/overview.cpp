#include "overview.hpp"
#include <any>
#define private public
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#undef private
#include "OverviewPassElement.hpp"

static void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview->damage();
}

static void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
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

    // Get workspaces relative to current
    int currentID = pMonitor->activeWorkspaceID();

    // Determine offsets based on current workspace ID
    int offsets[4];
    if (currentID == 1) {
        // First workspace: show next 4 workspaces (2, 3, 4, 5)
        offsets[0] = +1;
        offsets[1] = +2;
        offsets[2] = +3;
        offsets[3] = +4;
    } else if (currentID == 2) {
        // Second workspace: show previous 1 and next 3 (1, 3, 4, 5)
        offsets[0] = -1;
        offsets[1] = +1;
        offsets[2] = +2;
        offsets[3] = +3;
    } else {
        // Normal case: show 2 before and 2 after (-2, -1, +1, +2)
        offsets[0] = -2;
        offsets[1] = -1;
        offsets[2] = +1;
        offsets[3] = +2;
    }

    // Populate left side workspaces
    for (size_t i = 0; i < LEFT_WORKSPACES; ++i) {
        auto& image = images[i];
        int offset = offsets[i];

        if (offset < 0) {
            image.workspaceID = getWorkspaceIDNameFromString("r" + std::to_string(offset)).id;
        } else {
            image.workspaceID = getWorkspaceIDNameFromString("r+" + std::to_string(offset)).id;
        }
        image.isActive = false;
    }

    // Last image is for the active workspace (right side)
    images[LEFT_WORKSPACES].workspaceID = currentID;
    images[LEFT_WORKSPACES].isActive    = true;
    activeIndex                         = LEFT_WORKSPACES;

    g_pHyprRenderer->makeEGLCurrent();

    // Calculate layout
    const Vector2D monitorSize       = pMonitor->m_size;
    const float    leftWidth         = monitorSize.x * LEFT_WIDTH_RATIO;
    const float    rightWidth        = monitorSize.x - leftWidth;
    const float    availableHeight   = monitorSize.y - (2 * PADDING);
    const float    totalGaps         = (LEFT_WORKSPACES - 1) * GAP_WIDTH;
    const float    leftPreviewHeight = (availableHeight - totalGaps) / LEFT_WORKSPACES;

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
        g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

        g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(image.workspaceID);

        if (PWORKSPACE) {
            image.pWorkspace            = PWORKSPACE;
            PMONITOR->m_activeWorkspace = PWORKSPACE;
            g_pDesktopAnimationManager->startAnimation(PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
            PWORKSPACE->m_visible = true;

            if (PWORKSPACE == startedOn)
                PMONITOR->m_activeSpecialWorkspace = openSpecial;

            g_pHyprRenderer->renderWorkspace(PMONITOR, PWORKSPACE, Time::steadyNow(), monbox);

            PWORKSPACE->m_visible = false;
            g_pDesktopAnimationManager->startAnimation(PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);

            if (PWORKSPACE == startedOn)
                PMONITOR->m_activeSpecialWorkspace.reset();
        } else
            g_pHyprRenderer->renderWorkspace(PMONITOR, PWORKSPACE, Time::steadyNow(), monbox);

        // Calculate box positions for rendering
        if (image.isActive) {
            // Right side - active workspace
            image.box = {leftWidth + PADDING, PADDING, rightWidth - (2 * PADDING), monitorSize.y - (2 * PADDING)};
        } else {
            // Left side - workspace list
            image.box = {PADDING, PADDING + i * (leftPreviewHeight + GAP_WIDTH), leftWidth - (2 * PADDING), leftPreviewHeight};
        }

        g_pHyprOpenGL->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
    }

    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    PMONITOR->m_activeSpecialWorkspace = openSpecial;
    PMONITOR->m_activeWorkspace        = startedOn;
    startedOn->m_visible               = true;
    g_pDesktopAnimationManager->startAnimation(startedOn, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);

    // Setup animations for zoom effect
    g_pAnimationManager->createAnimation(pMonitor->m_size, size, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
    g_pAnimationManager->createAnimation(Vector2D{0, 0}, pos, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);

    size->setUpdateCallback(damageMonitor);
    pos->setUpdateCallback(damageMonitor);

    *size = pMonitor->m_size;
    *pos  = {0, 0};

    size->setCallbackOnEnd([this](auto) { redrawAll(true); });

    // Setup mouse button hook to close on click
    auto onMouseButton = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled = true;
        close();
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
    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

    const auto   PWORKSPACE  = image.pWorkspace;
    PHLWORKSPACE openSpecial = pMonitor->m_activeSpecialWorkspace;

    if (openSpecial)
        pMonitor->m_activeSpecialWorkspace.reset();

    startedOn->m_visible = false;

    if (PWORKSPACE) {
        pMonitor->m_activeWorkspace = PWORKSPACE;
        g_pDesktopAnimationManager->startAnimation(PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
        PWORKSPACE->m_visible = true;

        if (PWORKSPACE == startedOn)
            pMonitor->m_activeSpecialWorkspace = openSpecial;

        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE, Time::steadyNow(), monbox);

        PWORKSPACE->m_visible = false;
        g_pDesktopAnimationManager->startAnimation(PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);

        if (PWORKSPACE == startedOn)
            pMonitor->m_activeSpecialWorkspace.reset();
    } else
        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE, Time::steadyNow(), monbox);

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    pMonitor->m_activeSpecialWorkspace = openSpecial;
    pMonitor->m_activeWorkspace        = startedOn;
    startedOn->m_visible               = true;
    g_pDesktopAnimationManager->startAnimation(startedOn, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);

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

void COverview::close() {
    if (closing)
        return;

    // Get the active workspace box (always on the right side)
    const auto& activeBox = images[activeIndex].box;

    // Calculate zoom animation to focus on active workspace
    // We want to zoom from current overview to just the active workspace filling the screen
    const Vector2D monitorSize = pMonitor->m_size;

    // Calculate the scale factor needed to zoom the active box to fill the screen
    const float scaleX = monitorSize.x / activeBox.w;
    const float scaleY = monitorSize.y / activeBox.h;
    const float scale = std::min(scaleX, scaleY);

    // Target size is scaled up from current
    *size = monitorSize * scale;

    // Target position: we need to move so the active workspace center aligns with screen center
    const Vector2D activeCenter = Vector2D{activeBox.x + activeBox.w / 2.0f, activeBox.y + activeBox.h / 2.0f};
    const Vector2D screenCenter = monitorSize / 2.0f;
    *pos = (screenCenter - activeCenter) * scale;

    size->setCallbackOnEnd(removeOverview);
    closing = true;
    redrawAll();
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

    // Render each workspace
    for (size_t i = 0; i < images.size(); ++i) {
        auto& image = images[i];

        // Get the pre-calculated box from constructor
        CBox texbox = image.box;

        // Scale the workspace framebuffer to fit in the box while maintaining aspect ratio
        const float fbAspect  = (float)image.fb.m_size.x / image.fb.m_size.y;
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

        // Apply zoom animation transformations
        if (closing) {
            // During closing animation, scale and translate all boxes
            scaledBox.x = scaledBox.x * zoomScale;
            scaledBox.y = scaledBox.y * zoomScale;
            scaledBox.w = scaledBox.w * zoomScale;
            scaledBox.h = scaledBox.h * zoomScale;

            // Apply position offset
            scaledBox.x += currentPos.x;
            scaledBox.y += currentPos.y;
        }

        scaledBox.scale(monScale);
        scaledBox.round();

        CRegion damage{0, 0, INT16_MAX, INT16_MAX};

        // Fade out non-active workspaces during closing
        float alpha = 1.0f;
        if (closing && i != (size_t)activeIndex) {
            alpha = 1.0f - size->getPercent();  // Fade out as we zoom
        }

        g_pHyprOpenGL->renderTextureInternal(image.fb.getTexture(), scaledBox, {.damage = &damage, .a = alpha});
    }
}
