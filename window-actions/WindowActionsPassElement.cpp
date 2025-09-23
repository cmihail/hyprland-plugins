#include "WindowActionsPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "WindowActionsBar.hpp"

CWindowActionsPassElement::CWindowActionsPassElement(const CWindowActionsPassElement::SWindowActionsData& data_) : data(data_) {
    ;
}

void CWindowActionsPassElement::draw(const CRegion& damage) {
    data.deco->renderPass(g_pHyprOpenGL->m_renderData.pMonitor.lock(), data.a);
}

bool CWindowActionsPassElement::needsLiveBlur() {
    return false;
}

std::optional<CBox> CWindowActionsPassElement::boundingBox() {
    return data.deco->assignedBoxGlobal().translate(-g_pHyprOpenGL->m_renderData.pMonitor->m_position).expand(5);
}

bool CWindowActionsPassElement::needsPrecomputeBlur() {
    return false;
}