#pragma once

#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

// Declare external PHANDLE from main.cpp
extern HANDLE PHANDLE;

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

    virtual void draw(const CRegion& damage);
    virtual bool needsLiveBlur();
    virtual bool needsPrecomputeBlur();
    virtual std::string id();
    virtual const char* passName();

  private:
    PHLMONITOR m_pMonitor;
};
