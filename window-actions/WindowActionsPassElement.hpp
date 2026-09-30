#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>

class CWindowActionsBar;

class CWindowActionsPassElement : public IPassElement {
  public:
    struct SWindowActionsData {
        CWindowActionsBar* deco = nullptr;
        float              a    = 1.F;
    };

    CWindowActionsPassElement(const SWindowActionsData& data_);
    virtual ~CWindowActionsPassElement() = default;

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();

    virtual const char*         passName() {
        return "CWindowActionsPassElement";
    }

  private:
    SWindowActionsData data;
};