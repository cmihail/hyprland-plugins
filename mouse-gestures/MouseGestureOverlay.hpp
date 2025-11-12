#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

class CMouseGestureOverlay : public IPassElement {
  public:
    CMouseGestureOverlay(PHLMONITOR monitor);
    virtual ~CMouseGestureOverlay();

    virtual void                draw(const CRegion& damage);
    virtual bool                needsLiveBlur();
    virtual bool                needsPrecomputeBlur();
    virtual std::optional<CBox> boundingBox();

    virtual const char*         passName() {
        return "CMouseGestureOverlay";
    }

  private:
    WP<CMonitor> pMonitor;

    // Helper methods for rendering
    void renderBackground(PHLMONITOR monitor, float monScale);
    void renderRecordModeUI(PHLMONITOR monitor);
    void renderRecordSquare(const Vector2D& pos, const Vector2D& size,
                           const CRegion& damage);
    void renderGestureSquare(float x, float y, float size, size_t gestureIndex,
                            const CRegion& damage);
    void renderGestureTrail(PHLMONITOR monitor, const Vector2D& monitorSize);

    struct TrailConfig {
        float circleRadius;
        int fadeDurationMs;
        float r, g, b;
    };
    TrailConfig getTrailConfig();
};
