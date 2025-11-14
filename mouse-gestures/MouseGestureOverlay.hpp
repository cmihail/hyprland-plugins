#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/render/Texture.hpp>
#include "stroke.hpp"

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
    void renderDeleteButton(float x, float y, float size, const CRegion& damage,
                           float alpha = 1.0f);
    void renderGestureTrail(PHLMONITOR monitor, const Vector2D& monitorSize);
    void renderBoxBorders(float x, float y, float size, const CHyprColor& color,
                         float borderSize, const CRegion& damage);
    void renderText(SP<CTexture> out, const std::string& text,
                   const CHyprColor& color, const Vector2D& bufferSize,
                   float scale, int fontSize);

    struct TrailConfig {
        float circleRadius;
        int fadeDurationMs;
        CHyprColor startColor;
        CHyprColor endColor;
    };

    void renderGesturePattern(float x, float y, float size,
                             const std::vector<Point>& points,
                             const TrailConfig& config, const CRegion& damage);
    TrailConfig getTrailConfig();
    CHyprColor interpolateColor(const CHyprColor& start, const CHyprColor& end,
                               float t);
};
