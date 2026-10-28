#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>

// Declare external PHANDLE from main.cpp
extern HANDLE PHANDLE;

// Config values, Lua: plugin.no_mouse.<name>
struct SNoMouseConfig {
    SP<Config::Values::CFloatValue> opacityBackground;
    SP<Config::Values::CFloatValue> opacityGridLines;
    SP<Config::Values::CFloatValue> opacityLabels;
    SP<Config::Values::CIntValue>   borderColor;
    SP<Config::Values::CFloatValue> borderOpacity;
    SP<Config::Values::CIntValue>   highlightColor;
    SP<Config::Values::CFloatValue> highlightOpacity;
};
extern SNoMouseConfig g_config;

// Declare external letter sequence state from main.cpp
extern std::string g_letterSequence;

// Declare external pending cell state from main.cpp
extern bool g_hasPendingCell;
extern int g_pendingRow;
extern int g_pendingCol;

// Declare external sub-column refinement state from main.cpp
extern bool g_hasSubColumn;
extern int g_subColumn;

class CNoMouseOverlay : public IPassElement {
  public:
    CNoMouseOverlay(PHLMONITOR monitor);
    virtual ~CNoMouseOverlay() = default;

    virtual std::vector<UP<IPassElement>> draw() override;
    virtual bool                          needsLiveBlur() override;
    virtual bool                          needsPrecomputeBlur() override;
    virtual ePassElementType              type() override { return EK_CUSTOM; }
    virtual std::string                   id();
    virtual const char*                   passName() override;

  private:
    PHLMONITOR m_pMonitor;
};
