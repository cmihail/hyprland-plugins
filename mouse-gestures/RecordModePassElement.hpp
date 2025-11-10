#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

class CRecordModePassElement : public IPassElement {
  public:
    CRecordModePassElement(PHLMONITOR monitor);
    virtual ~CRecordModePassElement() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();

    virtual const char*         passName() {
        return "CRecordModePassElement";
    }

  private:
    WP<CMonitor> pMonitor;
};
