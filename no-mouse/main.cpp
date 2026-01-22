#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <unordered_map>
#include <fstream>
#include <linux/input-event-codes.h>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>

#include "NoMouseOverlay.hpp"

// Debug logging
static void debugLog(const std::string& msg) {
    std::ofstream logFile("/tmp/debug", std::ios::app);
    logFile << msg << std::endl;
    logFile.close();
}

inline HANDLE PHANDLE = nullptr;

// Global state to track toggle
static bool g_toggleState = false;
static bool g_selectMode = false; // Track if overlay is in select mode (press/release mouse button)
static bool g_pluginShuttingDown = false;
static PHLWORKSPACE g_activeWorkspace = nullptr;

// Letter sequence tracking
static std::string g_letterSequence = "";
static constexpr int GRID_SIZE = 26;
static constexpr int SUB_GRID_ROWS = 3; // 3x6 sub-grid (18 positions, A-R)
static constexpr int SUB_GRID_COLS = 6;
static bool g_keybindsRegistered = false;

// Pending cell selection (waiting for SPACE key)
bool g_hasPendingCell = false;
int g_pendingRow = -1;
int g_pendingCol = -1;

// Sub-cell refinement (optional 3rd letter for precision in 3x6 grid)
bool g_hasSubColumn = false;
int g_subColumn = -1; // 0-17 (A-R)

// Hook handles
static SP<HOOK_CALLBACK_FN> g_renderHook;
static SP<HOOK_CALLBACK_FN> g_keyboardHook;

// Forward declarations
static void damageAllMonitors();
static void registerLetterKeybinds();
static void unregisterLetterKeybinds();
static void disableOverlay();

// Map Linux keycode to letter (A-Z)
// Keycodes follow QWERTY layout, not alphabetical
static char keycodeToLetter(uint32_t keycode) {
    // QWERTY top row: Q W E R T Y U I O P
    if (keycode == KEY_Q) return 'Q';
    if (keycode == KEY_W) return 'W';
    if (keycode == KEY_E) return 'E';
    if (keycode == KEY_R) return 'R';
    if (keycode == KEY_T) return 'T';
    if (keycode == KEY_Y) return 'Y';
    if (keycode == KEY_U) return 'U';
    if (keycode == KEY_I) return 'I';
    if (keycode == KEY_O) return 'O';
    if (keycode == KEY_P) return 'P';

    // QWERTY middle row: A S D F G H J K L
    if (keycode == KEY_A) return 'A';
    if (keycode == KEY_S) return 'S';
    if (keycode == KEY_D) return 'D';
    if (keycode == KEY_F) return 'F';
    if (keycode == KEY_G) return 'G';
    if (keycode == KEY_H) return 'H';
    if (keycode == KEY_J) return 'J';
    if (keycode == KEY_K) return 'K';
    if (keycode == KEY_L) return 'L';

    // QWERTY bottom row: Z X C V B N M
    if (keycode == KEY_Z) return 'Z';
    if (keycode == KEY_X) return 'X';
    if (keycode == KEY_C) return 'C';
    if (keycode == KEY_V) return 'V';
    if (keycode == KEY_B) return 'B';
    if (keycode == KEY_N) return 'N';
    if (keycode == KEY_M) return 'M';

    return '\0'; // Not a letter key
}

// Helper function to damage all monitors
static void damageAllMonitors() {
    try {
        if (!g_pCompositor || !g_pHyprRenderer) {
            return;
        }

        for (auto& monitor : g_pCompositor->m_monitors) {
            if (!monitor) {
                continue;
            }

            try {
                g_pHyprRenderer->damageMonitor(monitor);
                g_pCompositor->scheduleFrameForMonitor(monitor);
            } catch (...) {
                // Silently catch per-monitor errors
            }
        }
    } catch (...) {
        // Silently catch to prevent crashes
    }
}

// Function to move mouse to specific cell (optionally with sub-column refinement)
static void moveMouseToCell(int row, int col, int subColumn = -1) {
    try {
        if (!g_pCompositor || !g_pCompositor->m_lastMonitor) {
            return;
        }

        auto monitor = g_pCompositor->m_lastMonitor;
        const auto monitorSize = monitor->m_size;

        // Calculate cell dimensions
        const float cellWidth = monitorSize.x / (float)GRID_SIZE;
        const float cellHeight = monitorSize.y / (float)GRID_SIZE;

        float cellCenterX;
        float cellCenterY;

        if (subColumn >= 0 && subColumn < SUB_GRID_ROWS * SUB_GRID_COLS) {
            // Sub-cell refinement: split cell into 3x5 sub-grid (15 positions)
            const float subCellWidth = cellWidth / (float)SUB_GRID_COLS;
            const float subCellHeight = cellHeight / (float)SUB_GRID_ROWS;

            // Calculate sub-cell row and column from index (0-14 maps to 3x5 grid)
            // Index layout: A=0, B=1, C=2, D=3, E=4, F=5, G=6, ...
            const int subRow = subColumn / SUB_GRID_COLS; // 0-2
            const int subCol = subColumn % SUB_GRID_COLS; // 0-4

            // Calculate center of sub-cell
            cellCenterX = col * cellWidth + subCol * subCellWidth + subCellWidth / 2.0f;
            cellCenterY = row * cellHeight + subRow * subCellHeight + subCellHeight / 2.0f;
        } else {
            // No sub-cell: use cell center
            cellCenterX = col * cellWidth + cellWidth / 2.0f;
            cellCenterY = row * cellHeight + cellHeight / 2.0f;
        }

        // Calculate absolute position (monitor position + cell center)
        const Vector2D absolutePos = monitor->m_position + Vector2D{cellCenterX, cellCenterY};

        // Warp the cursor
        g_pCompositor->warpCursorTo(absolutePos, true);

        // Simulate mouse movement to trigger hover states in applications (e.g., Chrome)
        if (g_pInputManager) {
            g_pInputManager->simulateMouseMovement();
        }

        // Focus the window under the cursor
        try {
            const auto windowUnderCursor = g_pCompositor->vectorToWindowUnified(
                absolutePos, RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);
            if (windowUnderCursor) {
                g_pCompositor->focusWindow(windowUnderCursor);
            }
        } catch (...) {
            // Silently catch focus errors
        }

    } catch (const std::exception& e) {
        Debug::log(ERR, "[no-mouse] Error moving mouse: {}", e.what());
    } catch (...) {
        Debug::log(ERR, "[no-mouse] Unknown error moving mouse");
    }
}

// Helper function to simulate mouse button press
static void simulateMouseButtonPress() {
    try {
        if (!g_pSeatManager) {
            return;
        }

        // Simulate left mouse button press using SeatManager
        g_pSeatManager->sendPointerButton(0, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
        g_pSeatManager->sendPointerFrame();

    } catch (const std::exception& e) {
        Debug::log(ERR, "[no-mouse] Error pressing mouse button: {}", e.what());
    } catch (...) {
        Debug::log(ERR, "[no-mouse] Unknown error pressing mouse button");
    }
}

// Helper function to simulate mouse button release
static void simulateMouseButtonRelease() {
    try {
        if (!g_pSeatManager) {
            return;
        }

        // Simulate left mouse button release using SeatManager
        g_pSeatManager->sendPointerButton(0, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
        g_pSeatManager->sendPointerFrame();

    } catch (const std::exception& e) {
        Debug::log(ERR, "[no-mouse] Error releasing mouse button: {}", e.what());
    } catch (...) {
        Debug::log(ERR, "[no-mouse] Unknown error releasing mouse button");
    }
}

// Function to process letter sequence and set pending cell
static void processLetterSequence() {
    if (g_letterSequence.length() == 2) {
        // Convert letters to grid coordinates
        char firstLetter = g_letterSequence[0];
        char secondLetter = g_letterSequence[1];

        if (firstLetter >= 'A' && firstLetter <= 'Z' &&
            secondLetter >= 'A' && secondLetter <= 'Z') {

            int row = firstLetter - 'A';
            int col = secondLetter - 'A';

            // Store as pending cell (for showing sub-grid)
            g_hasPendingCell = true;
            g_pendingRow = row;
            g_pendingCol = col;

            // Move mouse immediately to the cell center
            moveMouseToCell(row, col);

            // Trigger redraw to show highlight and sub-grid
            damageAllMonitors();
        }
    }
}

// Dispatcher function for individual letters (not used anymore, kept for compatibility)
static SDispatchResult letterDispatcher(std::string arg) {
    if (!g_toggleState) {
        return {};
    }

    try {
        if (arg.length() == 1) {
            char letter = arg[0];

            // Ensure uppercase
            if (letter >= 'a' && letter <= 'z') {
                letter = letter - 'a' + 'A';
            }

            if (letter >= 'A' && letter <= 'Z') {
                // Add to sequence
                g_letterSequence += letter;

                // Limit to 2 characters
                if (g_letterSequence.length() > 2) {
                    g_letterSequence = g_letterSequence.substr(g_letterSequence.length() - 2);
                }

                // Process sequence if we have 2 letters
                processLetterSequence();
            }
        }
    } catch (...) {
        // Silently catch errors
    }

    return {};
}

// Register keybinds for all letters A-Z (kept for compatibility but not used)
static void registerLetterKeybinds() {
    if (g_keybindsRegistered) {
        return;
    }

    try {
        for (char letter = 'A'; letter <= 'Z'; letter++) {
            std::string dispatcherName = "no-mouse-letter-";
            dispatcherName += letter;

            // Register dispatcher for this letter
            HyprlandAPI::addDispatcherV2(PHANDLE, dispatcherName,
                [letter](std::string) -> SDispatchResult {
                    return letterDispatcher(std::string(1, letter));
                }
            );
        }

        g_keybindsRegistered = true;
        Debug::log(LOG, "[no-mouse] Registered letter keybinds");
    } catch (const std::exception& e) {
        Debug::log(ERR, "[no-mouse] Error registering keybinds: {}", e.what());
    } catch (...) {
        Debug::log(ERR, "[no-mouse] Unknown error registering keybinds");
    }
}

// Unregister all letter keybinds
static void unregisterLetterKeybinds() {
    if (!g_keybindsRegistered) {
        return;
    }

    try {
        for (char letter = 'A'; letter <= 'Z'; letter++) {
            std::string dispatcherName = "no-mouse-letter-";
            dispatcherName += letter;

            // Remove dispatcher
            HyprlandAPI::removeDispatcher(PHANDLE, dispatcherName);
        }

        g_keybindsRegistered = false;
        Debug::log(LOG, "[no-mouse] Unregistered letter keybinds");
    } catch (const std::exception& e) {
        Debug::log(ERR, "[no-mouse] Error unregistering keybinds: {}", e.what());
    } catch (...) {
        Debug::log(ERR, "[no-mouse] Unknown error unregistering keybinds");
    }
}

// Function to turn off the overlay
static void disableOverlay() {
    if (g_toggleState) {
        // Release mouse button if in select mode
        if (g_selectMode) {
            simulateMouseButtonRelease();
            g_selectMode = false;
        }

        g_toggleState = false;
        g_activeWorkspace = nullptr;
        g_letterSequence.clear();

        // Clear pending cell state
        g_hasPendingCell = false;
        g_pendingRow = -1;
        g_pendingCol = -1;

        // Clear sub-column state
        g_hasSubColumn = false;
        g_subColumn = -1;

        // Unregister keybinds
        unregisterLetterKeybinds();

        damageAllMonitors();
    }
}

// Dispatcher function for no-mouse
static SDispatchResult noMouseDispatch(std::string arg) {
    try {
        if (arg == "move" || arg == "select") {
            // Determine if we're entering select mode
            bool enteringSelectMode = (arg == "select");

            // Toggle the state
            g_toggleState = !g_toggleState;

            if (g_toggleState) {
                // Set select mode flag
                g_selectMode = enteringSelectMode;

                // Capture the active workspace when turning on
                if (g_pCompositor && g_pCompositor->m_lastMonitor) {
                    g_activeWorkspace = g_pCompositor->m_lastMonitor->m_activeWorkspace;
                }
                // Clear letter sequence when turning on
                g_letterSequence.clear();

                // Register keybinds for letter capture
                registerLetterKeybinds();

                // Press mouse button if entering select mode
                if (g_selectMode) {
                    simulateMouseButtonPress();
                }
            } else {
                // Release mouse button if exiting select mode
                if (g_selectMode) {
                    simulateMouseButtonRelease();
                    g_selectMode = false;
                }

                // Clear when turning off
                g_activeWorkspace = nullptr;
                g_letterSequence.clear();

                // Unregister keybinds
                unregisterLetterKeybinds();
            }

            // Damage all monitors to trigger redraw
            damageAllMonitors();
        }
    } catch (...) {
        // Silently catch errors
    }

    return {};
}

// Setup render hook to display overlay
static void setupRenderHook() {
    try {
        auto onRender = [](void* self, SCallbackInfo& info, std::any param) {
            try {
                // Don't render if plugin is shutting down or toggle is off
                if (g_pluginShuttingDown || !g_toggleState) {
                    return;
                }

                // Safety checks
                if (!g_pHyprOpenGL || !g_pHyprRenderer) {
                    return;
                }

                // Get the current monitor being rendered
                auto monitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
                if (!monitor) {
                    return;
                }

                // Only render on the workspace where overlay was activated
                if (g_activeWorkspace && monitor->m_activeWorkspace != g_activeWorkspace) {
                    return;
                }

                // Add the overlay to the render pass
                g_pHyprRenderer->m_renderPass.add(
                    makeUnique<CNoMouseOverlay>(monitor)
                );

                // Schedule next frame if overlay is active
                if (g_toggleState) {
                    try {
                        if (g_pCompositor) {
                            g_pCompositor->scheduleFrameForMonitor(monitor);
                        }
                    } catch (...) {
                        // Silently catch scheduling errors
                    }
                }

            } catch (...) {
                // Silently catch to prevent compositor crash
            }
        };

        g_renderHook = g_pHookSystem->hookDynamic("render", onRender);

    } catch (...) {
        // Failed to set up render hook
    }
}

// Setup keyboard hook to detect ESC key and show notifications for all key presses
static void setupKeyboardHook() {
    try {
        auto onKeyPress = [](void* self, SCallbackInfo& info, std::any param) {
            try {
                // Only process if overlay is active
                if (!g_toggleState) {
                    return;
                }

                // Extract the keyboard event from the map
                auto eventMap = std::any_cast<std::unordered_map<std::string, std::any>>(param);
                auto e = std::any_cast<IKeyboard::SKeyEvent>(eventMap["event"]);

                // Only process key press events
                if (e.state != WL_KEYBOARD_KEY_STATE_PRESSED) {
                    return;
                }

                // Check for ESC key press (KEY_ESC = 1)
                if (e.keycode == KEY_ESC) {
                    // Turn off the overlay
                    disableOverlay();
                    info.cancelled = true;
                    return;
                }

                // Check for SPACE or RETURN key press (KEY_SPACE = 57, KEY_ENTER = 28)
                if (e.keycode == KEY_SPACE || e.keycode == KEY_ENTER) {
                    // SPACE/RETURN now just exits the overlay
                    disableOverlay();
                    info.cancelled = true;
                    return;
                }

                // Check for letter keys (A-Z)
                char letter = keycodeToLetter(e.keycode);
                if (letter != '\0') {
                    debugLog("Letter pressed: " + std::string(1, letter) +
                             ", hasPending=" + std::to_string(g_hasPendingCell) +
                             ", seqLen=" + std::to_string(g_letterSequence.length()) +
                             ", seq=" + g_letterSequence);

                    // If we already have a pending cell, this letter selects a sub-cell in 3x6 grid
                    if (g_hasPendingCell && g_letterSequence.length() == 2) {
                        int subIndex = letter - 'A';

                        // Only accept A-R (0-17) for 3x6 grid (18 positions)
                        if (subIndex >= 0 && subIndex < SUB_GRID_ROWS * SUB_GRID_COLS) {
                            g_hasSubColumn = true;
                            g_subColumn = subIndex;

                            debugLog("SUB-CELL MODE: hasSubColumn=" +
                                     std::to_string(g_hasSubColumn) +
                                     ", subColumn=" + std::to_string(g_subColumn) +
                                     " (letter " + std::string(1, letter) +
                                     ") - moving to sub-cell");

                            // Move to sub-cell immediately but stay in overlay
                            moveMouseToCell(g_pendingRow, g_pendingCol, g_subColumn);

                            // Clear all state after moving so user can select another cell
                            g_hasSubColumn = false;
                            g_subColumn = -1;
                            g_hasPendingCell = false;
                            g_pendingRow = -1;
                            g_pendingCol = -1;
                            g_letterSequence.clear();

                            // Trigger redraw to hide sub-grid
                            damageAllMonitors();

                            info.cancelled = true;
                            return;
                        } else {
                            debugLog("SUB-CELL MODE: Ignoring letter " + std::string(1, letter) +
                                     " (only A-R supported for 3x6 grid)");
                        }
                    } else {
                        // Otherwise, add to sequence for cell selection
                        g_letterSequence += letter;

                        // Limit to 2 characters
                        if (g_letterSequence.length() > 2) {
                            g_letterSequence =
                                g_letterSequence.substr(g_letterSequence.length() - 2);
                        }

                        // Clear sub-column if user is changing cell selection
                        g_hasSubColumn = false;
                        g_subColumn = -1;

                        debugLog("CELL MODE: seq=" + g_letterSequence + ", cleared subColumn");

                        // Process sequence if we have 2 letters
                        processLetterSequence();
                    }

                    // Block the key from reaching the active window
                    info.cancelled = true;
                    return;
                }

            } catch (...) {
                // Silently catch errors
            }
        };

        g_keyboardHook = g_pHookSystem->hookDynamic("keyPress", onKeyPress);

    } catch (...) {
        // Failed to set up keyboard hook
    }
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[no-mouse] Failure in initialization: Version mismatch "
            "(headers ver is not equal to running hyprland ver)",
            CHyprColor{1.0f, 0.2f, 0.2f, 1.0f},
            5000
        );
        throw std::runtime_error("[no-mouse] Version mismatch");
    }

    // Register config values with default values
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:no_mouse:opacity_background",
        Hyprlang::FLOAT{0.1}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:no_mouse:opacity_grid_lines",
        Hyprlang::FLOAT{0.25}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:no_mouse:opacity_labels",
        Hyprlang::FLOAT{0.4}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:no_mouse:border_color",
        Hyprlang::INT{0x4C7FA6}  // Default: blue RGB(0.3, 0.5, 0.7)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:no_mouse:border_opacity",
        Hyprlang::FLOAT{0.8}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:no_mouse:highlight_color",
        Hyprlang::INT{0x4C7FA6}  // Default: blue RGB(0.3, 0.5, 0.7)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:no_mouse:highlight_opacity",
        Hyprlang::FLOAT{0.8}
    );

    // Register the dispatcher
    HyprlandAPI::addDispatcherV2(PHANDLE, "no-mouse", noMouseDispatch);

    // Setup hooks
    setupRenderHook();
    setupKeyboardHook();

    return {"no-mouse", "Overlay plugin with keyboard control", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    try {
        // Set shutdown flag first
        g_pluginShuttingDown = true;

        // Turn off overlay
        g_toggleState = false;

        // Unregister keybinds if registered
        unregisterLetterKeybinds();

        // Unhook all event handlers
        g_renderHook.reset();
        g_keyboardHook.reset();

    } catch (...) {
        // Silently catch any errors during cleanup
    }
}
