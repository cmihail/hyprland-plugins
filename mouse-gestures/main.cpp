#define WLR_USE_UNSTABLE

#include <any>
#include <string>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <wayland-server.h>
#include <wayland-server-protocol.h>
#include <linux/input-event-codes.h>
#include <hyprland/src/plugins/PluginAPI.hpp>
#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#undef private

#include "stroke.hpp"

inline HANDLE PHANDLE = nullptr;

// Gesture action configuration
struct GestureAction {
    Stroke pattern;
    std::string command;
    std::string name;  // Optional name for display
};

// Mouse gesture state
struct MouseGestureState {
    bool rightButtonPressed = false;
    Vector2D mouseDownPos = {0, 0};
    bool dragDetected = false;
    std::vector<Vector2D> path;
    std::chrono::steady_clock::time_point pressTime;
    uint32_t pressButton = 0;
    uint32_t pressTimeMs = 0;

    void reset() {
        rightButtonPressed = false;
        mouseDownPos = {0, 0};
        dragDetected = false;
        path.clear();
        pressButton = 0;
        pressTimeMs = 0;
    }
};

MouseGestureState g_gestureState;
std::vector<GestureAction> g_gestureActions;
bool g_recordMode = false;

// Hook handles
SP<HOOK_CALLBACK_FN> g_mouseButtonHook;
SP<HOOK_CALLBACK_FN> g_mouseMoveHook;
SP<HOOK_CALLBACK_FN> g_renderHook;

// Generate ASCII art representation of a gesture
static std::vector<std::string> generateGestureAsciiArt(
    const std::string& strokeData
) {
    std::vector<std::string> result;

    try {
        // Deserialize stroke to get normalized points
        Stroke stroke = Stroke::deserialize(strokeData);
        if (!stroke.isFinished() || stroke.size() < 2) {
            return result;
        }

        const auto& points = stroke.getPoints();

        // Find bounding box (points are already normalized [0, 1])
        double minX = 1.0, maxX = 0.0, minY = 1.0, maxY = 0.0;
        for (const auto& p : points) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }

        // Grid dimensions - maintain aspect ratio
        const int maxHeight = 4;
        const int maxWidth = 70;

        double rangeX = maxX - minX;
        double rangeY = maxY - minY;

        // Avoid division by zero
        if (rangeX < 0.001) rangeX = 0.001;
        if (rangeY < 0.001) rangeY = 0.001;

        // Calculate aspect ratio (width/height in normalized coords)
        double aspect = rangeX / rangeY;

        int height, width;

        // Character aspect ratio compensation (terminal chars are ~2.5x taller)
        const double charAspect = 2.5;

        if (aspect * charAspect > 1.0) {
            // Wider gesture - use max width, calculate height
            width = maxWidth;
            height = std::max(1, static_cast<int>(width / (aspect * charAspect)));
            height = std::min(height, maxHeight);
        } else {
            // Taller gesture - use max height, calculate width
            height = maxHeight;
            width = std::max(1, static_cast<int>(height * aspect * charAspect));
            width = std::min(width, maxWidth);
        }

        // Create grid
        std::vector<std::vector<char>> grid(
            height, std::vector<char>(width, ' ')
        );

        // Draw continuous path
        for (size_t i = 0; i < points.size() - 1; i++) {
            // Map normalized coords to grid
            int x1 = static_cast<int>(
                (points[i].x - minX) / (maxX - minX + 0.01) * (width - 1)
            );
            int y1 = static_cast<int>(
                (points[i].y - minY) / (maxY - minY + 0.01) * (height - 1)
            );
            int x2 = static_cast<int>(
                (points[i + 1].x - minX) / (maxX - minX + 0.01) * (width - 1)
            );
            int y2 = static_cast<int>(
                (points[i + 1].y - minY) / (maxY - minY + 0.01) * (height - 1)
            );

            // Clamp to grid bounds
            x1 = std::max(0, std::min(x1, width - 1));
            y1 = std::max(0, std::min(y1, height - 1));
            x2 = std::max(0, std::min(x2, width - 1));
            y2 = std::max(0, std::min(y2, height - 1));

            // Draw line using Bresenham-like algorithm
            int dx = std::abs(x2 - x1);
            int dy = std::abs(y2 - y1);
            int sx = x1 < x2 ? 1 : -1;
            int sy = y1 < y2 ? 1 : -1;
            int err = dx - dy;

            int x = x1, y = y1;
            while (true) {
                // Determine character based on direction to next point
                char c = '*';
                bool isStart = (i == 0 && x == x1 && y == y1);
                bool isEnd = (i == points.size() - 2 && x == x2 && y == y2);

                if (isStart) {
                    c = 'S';  // Start
                } else if (isEnd) {
                    c = 'E';  // End
                } else if (grid[y][x] != ' ' && grid[y][x] != '*') {
                    c = grid[y][x];  // Keep existing
                } else {
                    // Calculate direction to next point
                    int nextX = x, nextY = y;
                    if (x != x2 || y != y2) {
                        int e2 = 2 * err;
                        if (e2 > -dy) nextX += sx;
                        if (e2 < dx) nextY += sy;
                    }

                    int dirX = nextX - x;
                    int dirY = nextY - y;

                    // Choose character based on movement direction
                    if (dirY == 0 && dirX != 0) {
                        c = '-';  // Horizontal
                    } else if (dirX == 0 && dirY != 0) {
                        c = '|';  // Vertical
                    } else if (dirX * dirY > 0) {
                        c = '\\';  // Diagonal down-right or up-left
                    } else if (dirX * dirY < 0) {
                        c = '/';  // Diagonal down-left or up-right
                    } else {
                        // Fallback based on overall direction
                        if (dx > dy * 2) {
                            c = '-';
                        } else if (dy > dx * 2) {
                            c = '|';
                        } else if ((sx > 0 && sy > 0) || (sx < 0 && sy < 0)) {
                            c = '\\';
                        } else {
                            c = '/';
                        }
                    }
                }

                grid[y][x] = c;

                if (x == x2 && y == y2) break;

                int e2 = 2 * err;
                if (e2 > -dy) {
                    err -= dy;
                    x += sx;
                    if (x < 0 || x >= width) break;
                }
                if (e2 < dx) {
                    err += dx;
                    y += sy;
                    if (y < 0 || y >= height) break;
                }
            }
        }

        // Convert grid to strings with comment prefix
        for (int y = 0; y < height; y++) {
            std::string line = "    # ";
            for (int x = 0; x < width; x++) {
                line += grid[y][x];
            }
            // Trim trailing spaces
            size_t end = line.find_last_not_of(' ');
            if (end != std::string::npos) {
                line = line.substr(0, end + 1);
            }
            result.push_back(line);
        }

    } catch (const std::exception&) {
        result.clear();
    }

    return result;
}

// Helper function to add gesture to Hyprland config file
static bool addGestureToConfig(const std::string& strokeData) {
    const char* home = std::getenv("HOME");
    if (!home) {
        return false;
    }

    std::string configBasePath = std::string(home) + "/.config/hypr";
    std::vector<std::string> configPaths = {
        configBasePath + "/config/plugins.conf",
        configBasePath + "/hyprland.conf"
    };

    // Step 1: Try to find existing mouse_gestures section
    for (const auto& configPath : configPaths) {
        std::ifstream inFile(configPath);
        if (!inFile.is_open()) continue;

        std::vector<std::string> lines;
        std::string line;
        bool foundMouseGestures = false;
        int mouseGesturesSectionEnd = -1;
        int currentLine = 0;
        int braceDepth = 0;
        int pluginBraceDepth = 0;
        bool inPlugin = false;

        // Read the file and find mouse_gestures section
        while (std::getline(inFile, line)) {
            lines.push_back(line);

            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

            if (trimmed.find("plugin") == 0 && trimmed.find("{") != std::string::npos) {
                inPlugin = true;
                pluginBraceDepth = 1;
            } else if (inPlugin) {
                for (char c : trimmed) {
                    if (c == '{') pluginBraceDepth++;
                    else if (c == '}') pluginBraceDepth--;
                }
                if (pluginBraceDepth == 0) inPlugin = false;
            }

            if (trimmed.find("mouse_gestures") == 0 && trimmed.find("{") != std::string::npos) {
                foundMouseGestures = true;
                braceDepth = 1;
            } else if (foundMouseGestures && braceDepth > 0) {
                for (char c : trimmed) {
                    if (c == '{') braceDepth++;
                    else if (c == '}') braceDepth--;
                }
                if (braceDepth == 0) {
                    mouseGesturesSectionEnd = currentLine;
                }
            }
            currentLine++;
        }
        inFile.close();

        if (foundMouseGestures && mouseGesturesSectionEnd > 0) {
            // Generate ASCII art
            std::vector<std::string> asciiArt = generateGestureAsciiArt(
                strokeData
            );

            // Add ASCII art and gesture_action before the closing brace
            std::string gestureAction = "    gesture_action = hyprctl notify -1 2000 \"rgb(ff0000)\" \"modify me in config file " + configPath + "\"|" + strokeData;

            // Insert in correct order: ASCII art first, then gesture_action
            int insertPos = mouseGesturesSectionEnd;
            for (const auto& artLine : asciiArt) {
                lines.insert(lines.begin() + insertPos, artLine);
                insertPos++;
            }
            lines.insert(lines.begin() + insertPos, gestureAction);

            // Write back to file
            std::ofstream outFile(configPath);
            if (!outFile.is_open()) return false;

            for (const auto& l : lines) {
                outFile << l << "\n";
            }
            outFile.close();
            return true;
        }
    }

    // Step 2: If no mouse_gestures section found, try to find plugin section
    for (const auto& configPath : configPaths) {
        std::ifstream inFile(configPath);
        if (!inFile.is_open()) continue;

        std::vector<std::string> lines;
        std::string line;
        int pluginSectionEnd = -1;
        int currentLine = 0;
        int braceDepth = 0;
        bool inPluginSection = false;

        while (std::getline(inFile, line)) {
            lines.push_back(line);

            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

            if (trimmed.find("plugin") == 0 && trimmed.find("{") != std::string::npos) {
                inPluginSection = true;
                braceDepth = 1;
            } else if (inPluginSection && braceDepth > 0) {
                for (char c : trimmed) {
                    if (c == '{') braceDepth++;
                    else if (c == '}') braceDepth--;
                }
                if (braceDepth == 0) {
                    pluginSectionEnd = currentLine;
                }
            }
            currentLine++;
        }
        inFile.close();

        if (pluginSectionEnd > 0) {
            // Generate ASCII art
            std::vector<std::string> asciiArt = generateGestureAsciiArt(
                strokeData
            );

            // Add mouse_gestures section before the closing brace of plugin section
            std::vector<std::string> newSection = {
                "",
                "  mouse_gestures {"
            };

            // Add ASCII art
            for (const auto& line : asciiArt) {
                newSection.push_back(line);
            }

            // Add gesture action
            newSection.push_back("    gesture_action = hyprctl notify -1 2000 \"rgb(ff0000)\" \"modify me in config file " + configPath + "\"|" + strokeData);
            newSection.push_back("  }");

            for (int i = newSection.size() - 1; i >= 0; i--) {
                lines.insert(lines.begin() + pluginSectionEnd, newSection[i]);
            }

            // Write back to file
            std::ofstream outFile(configPath);
            if (!outFile.is_open()) return false;

            for (const auto& l : lines) {
                outFile << l << "\n";
            }
            outFile.close();
            return true;
        }
    }

    // Step 3: If no plugin section found, add to hyprland.conf
    std::string hyprlandConf = configBasePath + "/hyprland.conf";
    std::ofstream outFile(hyprlandConf, std::ios::app);
    if (!outFile.is_open()) return false;

    // Generate ASCII art
    std::vector<std::string> asciiArt = generateGestureAsciiArt(strokeData);

    outFile << "\nplugin {\n";
    outFile << "  mouse_gestures {\n";

    // Write ASCII art
    for (const auto& line : asciiArt) {
        outFile << line << "\n";
    }

    outFile << "    gesture_action = hyprctl notify -1 2000 \"rgb(ff0000)\" \"modify me in config file " << hyprlandConf << "\"|" << strokeData << "\n";
    outFile << "  }\n";
    outFile << "}\n";
    outFile.close();

    return true;
}







// Convert path to stroke and find best matching gesture action
static const GestureAction* findMatchingGestureAction(const std::vector<Vector2D>& path) {

    if (path.size() < 2) {
        return nullptr;
    }

    try {
        // Get match threshold from config
        static auto* const PMATCHTHRESHOLD = (Hyprlang::FLOAT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE,
                "plugin:mouse_gestures:match_threshold"
            )->getDataStaticPtr();

        if (!PMATCHTHRESHOLD || !*PMATCHTHRESHOLD) {
            return nullptr;
        }

        const double matchThreshold = static_cast<double>(**PMATCHTHRESHOLD);

        // Create stroke from path
        Stroke inputStroke;
        for (size_t i = 0; i < path.size(); i++) {
            if (!inputStroke.addPoint(path[i].x, path[i].y)) {
                return nullptr;
            }
        }

        if (!inputStroke.finish()) {
            return nullptr;
        }

        // Find best matching gesture
        if (g_gestureActions.empty()) {
            return nullptr;
        }

        const GestureAction* bestMatch = nullptr;
        double bestCost = matchThreshold;

        for (size_t i = 0; i < g_gestureActions.size(); i++) {
            const auto& action = g_gestureActions[i];

            std::string actionName = action.name.empty() ?
                "Action_" + std::to_string(i) : action.name;


            if (!action.pattern.isFinished()) {
                continue;
            }


            double cost = inputStroke.compare(action.pattern);

            if (cost < bestCost) {
                bestCost = cost;
                bestMatch = &action;
            }
        }

        if (bestMatch) {
            std::string matchName = bestMatch->name.empty() ? "unnamed" : bestMatch->name;
        } else {
        }

        return bestMatch;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

// Handle detected gesture - analyze path and display result
static void handleGestureDetected() {

    if (g_gestureState.path.size() <= 1) {
        return;
    }

    try {
        // Create stroke from path
        Stroke inputStroke;
        for (const auto& point : g_gestureState.path) {
            if (!inputStroke.addPoint(point.x, point.y)) {
                return;
            }
        }

        if (!inputStroke.finish()) {
            return;
        }

        // If in record mode, save the stroke data
        if (g_recordMode) {

            try {
                // First check if this gesture matches any existing gesture
                const GestureAction* matchingAction = findMatchingGestureAction(g_gestureState.path);

                if (matchingAction) {
                    // Gesture already exists, notify user
                    std::string message = "[mouse-gestures] Duplicated gesture with command: " + matchingAction->command;
                    HyprlandAPI::addNotification(
                        PHANDLE,
                        message,
                        {1, 0.65, 0, 1},  // Orange color for warning
                        5000
                    );
                } else {
                    // No match, add to config file
                    std::string strokeData = inputStroke.serialize();

                    if (addGestureToConfig(strokeData)) {
                        HyprlandAPI::addNotification(
                            PHANDLE,
                            "[mouse-gestures] Gesture added to config",
                            {0, 0.5, 1, 1},
                            3000
                        );
                    } else {
                        HyprlandAPI::addNotification(
                            PHANDLE,
                            "[mouse-gestures] Failed to add gesture to config",
                            {1, 0, 0, 1},
                            3000
                        );
                    }
                }
            } catch (const std::exception&) {
                HyprlandAPI::addNotification(
                    PHANDLE,
                    "[mouse-gestures] Error recording gesture",
                    {1, 0, 0, 1},
                    3000
                );
            }

            g_recordMode = false;
            return;
        }

        // Check for matching gesture action
        const GestureAction* matchingAction = findMatchingGestureAction(g_gestureState.path);

        if (matchingAction) {
            std::string actionName = matchingAction->name.empty() ?
                matchingAction->command : matchingAction->name;

            try {
                // Execute the command in a new thread to avoid blocking
                std::thread([command = matchingAction->command]() {
                    try {
                        system(command.c_str());
                    } catch (...) {
                        // Ignore command execution errors
                    }
                }).detach();
            } catch (const std::exception&) {
            }
        } else {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Gesture not matched",
                {0, 1, 0, 1},
                3000
            );
        }
    } catch (const std::exception& e) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Error processing gesture",
            {1, 0, 0, 1},
            3000
        );
    }
}

// Handle no gesture detected - replay press and release events
static void replayButtonEvents(uint32_t releaseTimeMs) {
    if (!g_pSeatManager) {
        return;
    }

    g_pSeatManager->sendPointerButton(
        g_gestureState.pressTimeMs,
        g_gestureState.pressButton,
        WL_POINTER_BUTTON_STATE_PRESSED
    );
    g_pSeatManager->sendPointerButton(
        releaseTimeMs,
        g_gestureState.pressButton,
        WL_POINTER_BUTTON_STATE_RELEASED
    );
}

// Check if drag threshold is exceeded within time window
static bool checkDragThresholdExceeded(const Vector2D& mousePos) {
    try {
        // Calculate time elapsed since button press
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - g_gestureState.pressTime
        ).count();

        // Only detect gesture if within 100ms window
        if (elapsed > 100) {
            return false;
        }

        // Get configured drag threshold (default: 50.0)
        static auto* const PDRAGTHRESHOLD = (Hyprlang::INT* const*)
            HyprlandAPI::getConfigValue(
                PHANDLE,
                "plugin:mouse_gestures:drag_threshold"
            )->getDataStaticPtr();

        if (!PDRAGTHRESHOLD || !*PDRAGTHRESHOLD) {
            return false;
        }

        const float dragThreshold = static_cast<float>(**PDRAGTHRESHOLD);

        const float distanceX = std::abs(
            mousePos.x - g_gestureState.mouseDownPos.x
        );
        const float distanceY = std::abs(
            mousePos.y - g_gestureState.mouseDownPos.y
        );

        return (distanceX > dragThreshold || distanceY > dragThreshold);
    } catch (const std::exception&) {
        return false;
    }
}

// Config handler for gesture_action keyword
// NEW FORMAT: gesture_action = <command>|<stroke_data>
// The pipe character | separates command from stroke data
// Stroke data contains x,y;x,y;... (commas and semicolons are preserved)
static Hyprlang::CParseResult onGestureAction(const char* COMMAND, const char* VALUE) {

    try {
        if (!VALUE) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] gesture_action value is null",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        std::string value(VALUE);

        // Find the LAST pipe delimiter (to support pipes in command)
        size_t pipePos = value.rfind('|');
        if (pipePos == std::string::npos) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Format: command|stroke_data",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }


        // Extract command and stroke data (swapped order!)
        std::string command = value.substr(0, pipePos);
        std::string strokeData = value.substr(pipePos + 1);

        // Trim whitespace
        size_t start = strokeData.find_first_not_of(" \t\n\r");
        size_t end = strokeData.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            strokeData = strokeData.substr(start, end - start + 1);
        } else {
            strokeData = "";  // All whitespace, make it empty
        }

        start = command.find_first_not_of(" \t\n\r");
        end = command.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            command = command.substr(start, end - start + 1);
        } else {
            command = "";  // All whitespace, make it empty
        }


        // Validate
        if (strokeData.empty()) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] stroke_data cannot be empty",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        if (command.empty()) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] command cannot be empty",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        // Create action
        GestureAction action;
        action.name = "";  // No name in simple format
        action.command = command;
        action.pattern = Stroke::deserialize(strokeData);

        if (!action.pattern.isFinished() || action.pattern.size() < 2) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Invalid stroke data in gesture_action",
                {1, 0, 0, 1},
                5000
            );
            return Hyprlang::CParseResult{};
        }

        g_gestureActions.push_back(action);


    } catch (const std::exception& e) {
        HyprlandAPI::addNotification(
            PHANDLE,
            "[mouse-gestures] Error parsing gesture_action",
            {1, 0, 0, 1},
            5000
        );
    }

    return Hyprlang::CParseResult{};
}

// Handler to clear gesture actions on config reload
static void onPreConfigReload() {
    g_gestureActions.clear();
}

static SDispatchResult mouseGesturesDispatch(std::string arg) {
    if (arg == "record") {
        g_recordMode = !g_recordMode;
        if (g_recordMode) {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Recording mode enabled - draw gesture with action button",
                {0, 0.5, 1, 1},
                5000
            );
        } else {
            HyprlandAPI::addNotification(
                PHANDLE,
                "[mouse-gestures] Recording mode disabled",
                {0, 0.5, 1, 1},
                3000
            );
        }
    }
    return {};
}

static void setupMouseButtonHook() {
    auto onMouseButton = [](void* self, SCallbackInfo& info, std::any param) {
        try {
            auto e = std::any_cast<IPointer::SButtonEvent>(param);

            // Get configured action button (default: BTN_RIGHT = 273)
            static auto* const PACTIONBUTTON = (Hyprlang::INT* const*)
                HyprlandAPI::getConfigValue(
                    PHANDLE,
                    "plugin:mouse_gestures:action_button"
                )->getDataStaticPtr();

            if (!PACTIONBUTTON || !*PACTIONBUTTON) {
                return;
            }

            const uint32_t actionButton = static_cast<uint32_t>(**PACTIONBUTTON);

            // Only handle configured action button
            if (e.button != actionButton)
                return;

            if (e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
                // Action button pressed - record position and time
                if (!g_pInputManager) {
                    return;
                }

                const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();
                g_gestureState.rightButtonPressed = true;
                g_gestureState.mouseDownPos = mousePos;
                g_gestureState.dragDetected = false;
                g_gestureState.path.clear();
                g_gestureState.path.push_back(mousePos);
                g_gestureState.pressTime = std::chrono::steady_clock::now();
                g_gestureState.pressButton = actionButton;
                g_gestureState.pressTimeMs = e.timeMs;

                // Consume the press - will replay on release if no drag detected
                info.cancelled = true;
            } else {
                // Action button released
                if (g_gestureState.dragDetected) {
                    handleGestureDetected();
                } else {
                    replayButtonEvents(e.timeMs);
                }

                // Always consume the release event
                info.cancelled = true;

                // Reset state
                g_gestureState.reset();
            }
        } catch (const std::exception&) {
            // Catch all exceptions to prevent crashing Hyprland
            // Reset state on error
            g_gestureState.reset();
        }
    };

    g_mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onMouseButton);
}

static void setupMouseMoveHook() {
    auto onMouseMove = [](void* self, SCallbackInfo& info, std::any param) {
        try {
            if (!g_gestureState.rightButtonPressed)
                return;

            if (!g_pInputManager) {
                return;
            }

            const Vector2D mousePos = g_pInputManager->getMouseCoordsInternal();

            // Check if drag threshold exceeded (if not yet detected)
            if (!g_gestureState.dragDetected) {
                if (checkDragThresholdExceeded(mousePos)) {
                    g_gestureState.dragDetected = true;
                }
            }

            // Record path point if drag is active (but don't consume move events)
            if (g_gestureState.dragDetected) {
                g_gestureState.path.push_back(mousePos);
            }
        } catch (const std::exception&) {
            // Catch all exceptions to prevent crashing Hyprland
        }
    };

    g_mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onMouseMove);
}



APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;


    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        throw std::runtime_error("[mouse-gestures] Version mismatch");
    }

    // Register configuration values
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:drag_threshold",
        Hyprlang::INT{50}
    );
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:action_button",
        Hyprlang::INT{273}
    ); // BTN_RIGHT
    HyprlandAPI::addConfigValue(
        PHANDLE,
        "plugin:mouse_gestures:match_threshold",
        Hyprlang::FLOAT{0.15}
    );

    // Register config keyword for gesture_action
    HyprlandAPI::addConfigKeyword(
        PHANDLE,
        "plugin:mouse_gestures:gesture_action",
        onGestureAction,
        Hyprlang::SHandlerOptions{}
    );

    // Register preConfigReload handler
    static auto preConfigReloadHook = HyprlandAPI::registerCallbackDynamic(
        PHANDLE,
        "preConfigReload",
        [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); }
    );

    // Register configReloaded handler
    static auto configReloadedHook = HyprlandAPI::registerCallbackDynamic(
        PHANDLE,
        "configReloaded",
        [&](void* self, SCallbackInfo& info, std::any data) {}
    );

    // Reload config to apply registered values
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addDispatcherV2(PHANDLE, "mouse-gestures", mouseGesturesDispatch);

    // Setup mouse event hooks
    setupMouseButtonHook();
    setupMouseMoveHook();


    return {"mouse-gestures", "Mouse gestures for Hyprland", "cmihail", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {

}
