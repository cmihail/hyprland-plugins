#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

class CMouseGestureOverlay : public IPassElement {
  public:
    CMouseGestureOverlay(PHLMONITOR monitor);
    virtual ~CMouseGestureOverlay() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();

    virtual const char*         passName() {
        return "CMouseGestureOverlay";
    }

  private:
    WP<CMonitor> pMonitor;
};
