# Workspace Overview Plugin

A Hyprland plugin that provides a visual workspace overview similar to GNOME's Activities view. Shows workspace previews in a split-screen layout: 4 surrounding workspaces on the left (1/3 of screen), and the current workspace on the right (2/3 of screen). Click on any workspace to switch to it.

## Demo

https://github.com/user-attachments/assets/workspace-overview.mp4

## Features

- **Split-Screen Layout**:
  - Left panel (1/3): Shows 4 workspace previews (2 before + 2 after current)
  - Right panel (2/3): Shows the current workspace enlarged
- **Smart Workspace Selection**: Automatically adjusts workspace list for edge cases (workspace 1 and 2)
- **Click-to-Switch**: Click on any workspace preview to switch to it with smooth animation
- **Workspace Numbers**: Displays workspace number in the top-left corner of each preview
- **Smooth Animations**: Zoom animation when switching or exiting the overview
- **Mouse Click Exit**: Click outside workspaces to exit without switching
- **Live Previews**: Real-time rendering of workspace contents

## Prerequisites

On Fedora 42, install the required development packages:

```bash
sudo dnf install hyprland-devel hyprland-protocols-devel gcc-c++ make pkgconf-devel
```

For testing, also install:

```bash
sudo dnf install gtest-devel
```

## Building

1. Navigate to the plugin directory:
```bash
cd workspace-overview
```

2. Build the plugin:
```bash
make
```

This will create `workspace-overview.so` in the current directory.

## Loading the Plugin

### Method 1: Using hyprpm (Recommended)

1. Install the plugin using hyprpm:
```bash
hyprpm add /path/to/workspace-overview
hyprpm enable workspace-overview
```

2. Reload Hyprland configuration:
```bash
hyprctl reload
```

### Method 2: Manual Loading

1. Add to your Hyprland configuration (`~/.config/hypr/hyprland.conf`):
```
plugin = /path/to/workspace-overview/workspace-overview.so
```

2. Reload Hyprland:
```bash
hyprctl reload
```

## Usage

### Opening the Overview

Bind a key to open the workspace overview in your Hyprland config:

```conf
# Open/close workspace overview with Super+Tab
bind = SUPER, Tab, exec, hyprctl dispatch workspace-overview

# Or use toggle mode
bind = SUPER, Tab, exec, hyprctl dispatch workspace-overview toggle
```

### Interacting with the Overview

- **Click on Workspace**: Click any workspace preview to switch to that workspace with a smooth zoom animation
- **Click Outside**: Click outside any workspace to exit without switching
- **Keyboard**: Press the same keybind again (if using toggle mode) to exit

### Dispatcher Commands

```bash
# Open the overview
hyprctl dispatch workspace-overview

# Toggle the overview
hyprctl dispatch workspace-overview toggle

# Close the overview
hyprctl dispatch workspace-overview close
```

## How It Works

### Layout Logic

The plugin intelligently selects which workspaces to display based on your current workspace:

#### Workspace 1 (First workspace)
```
Left panel:  Workspaces 2, 3, 4, 5
Right panel: Workspace 1 (current)
```

#### Workspace 2 (Second workspace)
```
Left panel:  Workspaces 1, 3, 4, 5
Right panel: Workspace 2 (current)
```

#### Workspace 3+ (Normal case)
```
Left panel:  Current-2, Current-1, Current+1, Current+2
Right panel: Current workspace

Example for workspace 5:
Left:  3, 4, 6, 7
Right: 5
```

### Visual Behavior

1. **Overview Opens**: All workspace previews animate into view
2. **During Overview**:
   - Left side shows 4 workspace thumbnails stacked vertically
   - Right side shows the current workspace enlarged
   - All previews maintain aspect ratio
3. **Closing Animation**:
   - Zoom animation focuses on the active workspace
   - Non-active workspaces fade out
   - Smooth transition back to normal view

## Testing

Run the test suite to verify plugin functionality:

```bash
make test
```

This will compile and run all unit tests using Google Test framework. The tests validate:

- Workspace offset calculation for different workspace IDs
- Edge case handling (workspace 1 and 2)
- Layout box calculations
- Aspect ratio fitting algorithms
- Monitor resolution compatibility
- Spacing and padding consistency

Note: Tests are standalone unit tests that don't require the full Hyprland runtime environment.

## Configuration

The plugin works out of the box with sensible defaults. You can customize the appearance by adding these options to your Hyprland configuration (`~/.config/hypr/hyprland.conf`):

### Visual Customization

```conf
# Background image (optional)
plugin:workspace_overview:background_path = /path/to/your/background.png

# Active workspace border color (hex RGBA format: 0xRRGGBBAA)
plugin:workspace_overview:active_workspace_color = 0x4c7fa6ff  # Blue (default)

# Active workspace border thickness (pixels)
plugin:workspace_overview:border_size = 4.0

# Placeholder workspace plus sign color (hex RGBA format: 0xRRGGBBAA)
plugin:workspace_overview:placeholder_plus_color = 0xffffffcc  # White with 80% opacity (default)

# Placeholder workspace plus sign thickness (pixels)
plugin:workspace_overview:placeholder_plus_size = 8.0

# Drop zone indicator color for window drag-and-drop (hex RGBA format: 0xRRGGBBAA)
plugin:workspace_overview:drop_window_color = 0xffffffcc  # White with 80% opacity (default)

# Drop zone indicator color for workspace drag-and-drop (hex RGBA format: 0xRRGGBBAA)
plugin:workspace_overview:drop_workspace_color = 0xffffffcc  # White with 80% opacity (default)

# Number of placeholder (empty) workspaces to show
plugin:workspace_overview:placeholders_num = 5
```

### Mouse Button Configuration

```conf
# Mouse button for closing windows (optional, disabled by default)
# Click on a window with this button to close it
# Only closes if the click was not a drag (respects drag_threshold)
plugin:workspace_overview:kill_window_action_button = 273  # Right-click (example)

# Mouse button for dragging windows between workspaces
plugin:workspace_overview:drag_window_action_button = 272  # Left-click (default)

# Mouse button for dragging workspaces to reorder them
plugin:workspace_overview:drag_workspace_action_button = 274  # Middle-click (default)

# Mouse button for selecting a workspace (when not dragging)
plugin:workspace_overview:select_workspace_action_button = 272  # Left-click (default)

# Minimum pixel distance to initiate a drag operation
plugin:workspace_overview:drag_threshold = 50.0
```

**Mouse Button Codes**:
- 272 = Left mouse button (BTN_LEFT)
- 273 = Right mouse button (BTN_RIGHT)
- 274 = Middle mouse button (BTN_MIDDLE)
- Find other button codes by running `wev` or `xev` and pressing mouse buttons

**Note**: The `kill_window_action_button` is disabled by default. To enable window closing via mouse click, uncomment or add the configuration line with your preferred button code.

### Layout Constants

The following constants are defined in the code and control the layout:

```cpp
LEFT_WIDTH_RATIO = 0.33f;      // Left side takes 1/3 of screen
GAP_WIDTH = 10.0f;             // Gap between workspace previews
PADDING = 20.0f;               // Padding around edges
```

## Performance Considerations

- Workspace previews are rendered to framebuffers on overview open
- Framebuffers are cached during the overview session
- Resources are released when the overview closes
- Animation system uses Hyprland's native animation manager

## Troubleshooting

### Overview Not Opening
- Check that the plugin loaded successfully: `hyprctl plugin list`
- Verify the dispatcher command is correct
- Check Hyprland logs for error messages

### Black Workspace Previews
- This is expected for empty workspaces
- Workspaces with windows should show their contents
- Ensure windows are not hidden or minimized

### Stuttering Animation
- The plugin uses smooth zoom animations by default
- Check system performance and compositor settings
- Verify your GPU drivers are up to date

### Crash on Timeout
- This issue was fixed by using main-thread animations
- Ensure you're using the latest version of the plugin

## Development

### Building from Source

1. Clone or download the plugin source
2. Install development dependencies (see Prerequisites)
3. Run `make` to build the plugin
4. Load the plugin in Hyprland configuration

### Project Structure

```
workspace-overview/
├── globals.hpp              - Global state and handle
├── main.cpp                 - Plugin initialization and hooks
├── overview.hpp            - COverview class definition
├── overview.cpp            - Core overview logic and rendering
├── OverviewPassElement.hpp - Render pass element interface
├── OverviewPassElement.cpp - Render pass implementation
├── Makefile                - Build configuration
├── README.md               - This file
└── tests/                  - Unit tests
    ├── test_main.cpp       - Test runner
    ├── test_standalone.cpp - Core logic tests
    └── Makefile            - Test build configuration
```

### Key Components

- **COverview**: Main class managing the overview state, layout, and rendering
- **Function Hooks**: Intercepts `renderWorkspace` and damage reporting
- **Animation System**: Uses Hyprland's animation manager for smooth transitions
- **Mouse Handling**: Dynamic hook for mouse button events

## Implementation Details

### Rendering Pipeline

1. **Initialization**: Allocate framebuffers for each workspace
2. **Workspace Rendering**: Render each workspace to its framebuffer
3. **Layout Calculation**: Position boxes for left panel and right panel
4. **Aspect Ratio Fitting**: Scale framebuffers to fit boxes while maintaining aspect ratio
5. **Animation**: Apply zoom and position transformations during close
6. **Cleanup**: Release framebuffers when overview closes

### Animation System

The plugin uses two animated variables:
- **size**: Controls zoom level during open/close
- **pos**: Controls position offset for centering

These are synchronized with Hyprland's animation manager for smooth 60fps animations.

## Credits

Inspired by:
- [hyprexpo](https://github.com/hyprwm/hyprland-plugins/tree/main/hyprexpo) - Animation and rendering approach
- GNOME Activities - Layout concept

## License

This plugin is provided as-is for use with Hyprland.
