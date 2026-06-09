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

    virtual std::vector<UP<IPassElement>> draw() override;
    virtual bool                          needsLiveBlur() override;
    virtual bool                          needsPrecomputeBlur() override;
    virtual ePassElementType              type() override { return EK_CUSTOM; }
    virtual std::optional<CBox>           boundingBox() override;

    virtual const char* passName() override {
        return "CWindowActionsPassElement";
    }

  private:
    SWindowActionsData data;
};