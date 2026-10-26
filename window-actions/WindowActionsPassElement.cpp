#include "WindowActionsPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include "WindowActionsBar.hpp"

using Render::GL::g_pHyprOpenGL;

CWindowActionsPassElement::CWindowActionsPassElement(const CWindowActionsPassElement::SWindowActionsData& data_) : data(data_) {
    ;
}

std::vector<UP<IPassElement>> CWindowActionsPassElement::draw() {
    data.deco->renderPass(g_pHyprRenderer->m_renderData.pMonitor.lock(), data.a);
    return {};
}

bool CWindowActionsPassElement::needsLiveBlur() {
    return false;
}

std::optional<CBox> CWindowActionsPassElement::boundingBox() {
    auto monitor = g_pHyprRenderer->m_renderData.pMonitor.lock();
    if (!monitor)
        return CBox{0, 0, 0, 0};
    return data.deco->assignedBoxGlobal().translate(-monitor->m_position).expand(5);
}

bool CWindowActionsPassElement::needsPrecomputeBlur() {
    return false;
}
