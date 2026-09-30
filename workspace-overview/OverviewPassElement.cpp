#include "OverviewPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "overview.hpp"

COverviewPassElement::COverviewPassElement(COverview* overview) : pOverview(overview) {
    ;
}

void COverviewPassElement::draw(const CRegion& damage) {
    if (pOverview)
        pOverview->fullRender();
}

bool COverviewPassElement::needsLiveBlur() {
    return false;
}

bool COverviewPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> COverviewPassElement::boundingBox() {
    if (!pOverview || !pOverview->pMonitor)
        return std::nullopt;

    auto monitor = pOverview->pMonitor.lock();
    if (!monitor)
        return std::nullopt;

    return CBox{{}, monitor->m_size};
}

CRegion COverviewPassElement::opaqueRegion() {
    if (!pOverview || !pOverview->pMonitor)
        return CRegion{};

    auto monitor = pOverview->pMonitor.lock();
    if (!monitor)
        return CRegion{};

    return CBox{{}, monitor->m_size};
}
