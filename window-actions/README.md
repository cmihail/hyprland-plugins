# Window Actions Plugin

A configurable Hyprland plugin that adds customizable action buttons to window decorations. Supports dynamic button configuration with custom colors, icons, commands, and state-dependent behavior.

## Prerequisites

On Fedora 42, install the required development packages:

```bash
sudo dnf install hyprland-devel hyprland-protocols-devel gcc-c++ make pkgconf-devel pango-devel
```

For testing, also install:

```bash
sudo dnf install gtest-devel
```

## Building

1. Navigate to the plugin directory:
```bash
cd window-actions
```

2. Build the plugin:
```bash
make
```

This will create `window-actions.so` in the current directory.

## Loading the Plugin

### Method 1: Using hyprpm (Recommended)

1. Install the plugin using hyprpm:
```bash
hyprpm add /path/to/window-actions
hyprpm enable window-actions
```

2. Reload Hyprland configuration:
```bash
hyprctl reload
```

### Method 2: Manual Loading

1. Add to your Hyprland configuration (`~/.config/hypr/hyprland.conf`):
```
plugin = /path/to/window-actions/window-actions.so
```

2. Reload Hyprland:
```bash
hyprctl reload
```

## Configuration

Add configuration options to your Hyprland config file (`~/.config/hypr/hyprland.conf`):

### Basic Plugin Settings

```conf
plugin {
    window_actions {
        button_size = 15                    # Size of buttons in pixels (default: 15)
        action_button = 272                 # Mouse button for actions (default: 272 = BTN_LEFT)
        unhovered_alpha = 1.0               # Opacity when not hovered (default: 1.0 = full opacity)
    }
}
```

### Configurable Action Buttons

Define custom buttons with the `window_actions_button` keyword:

```conf
# Format: window_actions_button = text_color, bg_color, inactive_icon, active_icon, command, [condition]

# Close button - red text on dark background
window_actions_button = rgb(ff4040), rgb(333333), ⨯, ⨯, hyprctl dispatch killactive,

# Fullscreen button - yellow text on gray background, state-aware icons
window_actions_button = rgb(eeee11), rgb(444444), ⬈, ⬋, hyprctl dispatch fullscreen 1, fullscreen

# Group button - blue text on darker background, state-aware
window_actions_button = rgb(4040ff), rgb(555555), ⊟, ⊞, hyprctl dispatch togglegroup, grouped

# Floating button - green text on custom background, state-aware
window_actions_button = rgb(40ff40), rgb(666666), •, ⠶, hyprctl dispatch togglefloating, floating

# Default colors example (empty color fields use defaults)
window_actions_button = , , ⚙, ⚙, notify-send "Window Info" "$(hyprctl activewindow)",
```

### Configuration Parameters

#### Button Configuration Format
Each `window_actions_button` line follows this format:
- **text_color**: Button text color in `rgb(rrggbb)` format (empty = default rgba(e6e6e6ff))
- **bg_color**: Button background color in `rgb(rrggbb)` format (empty = default rgba(333333dd))
- **inactive_icon**: Unicode icon shown when condition is false/unset
- **active_icon**: Unicode icon shown when condition is true
- **command**: Shell command to execute when button is clicked
- **condition** (optional): Window state to check for icon switching

#### Plugin Settings
- **button_size**: Size of buttons in pixels (default: 15)
- **action_button**: Mouse button code for triggering actions (default: 272 = BTN_LEFT)
- **unhovered_alpha**: Opacity multiplier when buttons are not hovered (default: 1.0 = full opacity)
  - Set to `0.1` for 10% opacity when not hovered
  - Set to `0.5` for 50% opacity when not hovered
  - Set to `1.0` for no transparency effect (default)

#### Available Conditions
- `fullscreen` - Window is in fullscreen mode
- `grouped` - Window is part of a group
- `floating` - Window is in floating mode
- `maximized` - Window is maximized
- `focused` - Window is currently focused
- `pinned` - Window is pinned

#### Color Formats
Button colors support multiple formats:
- `rgb(ff4040)` - Hex RGB
- `rgb(255, 64, 64)` - Decimal RGB
- `rgba(255, 64, 64, 0.8)` - RGBA with alpha
- `0xffff4040` - Hex RGBA
- `` (empty) - Use default colors

### Example Configurations

#### Minimal Setup
```conf
# Simple close button with default colors
window_actions_button = , , ✕, ✕, hyprctl dispatch killactive,
```

#### Advanced Setup
```conf
plugin {
    window_actions {
        button_size = 18
        action_button = 272
        unhovered_alpha = 0.3        # Buttons fade to 30% opacity when not hovered
    }
}

# Close button with custom red text and dark background
window_actions_button = rgb(e74c3c), rgb(2c1810), ✕, ✕, hyprctl dispatch killactive,

# Minimize button with orange text and brown background
window_actions_button = rgb(f39c12), rgb(2c1f10), ▽, ▽, hyprctl dispatch movetoworkspacesilent special,

# Fullscreen toggle with green text and custom background
window_actions_button = rgb(2ecc71), rgb(0f1f0f), ⛶, ⛷, hyprctl dispatch fullscreen 1, fullscreen

# Info button with purple text and dark purple background
window_actions_button = rgb(9b59b6), rgb(1f0f2c), ⚙, ⚙, notify-send "Window Info" "$(hyprctl activewindow)",
```

#### Custom Commands
Buttons can execute any shell command with individual styling:
```conf
# Terminal button - blue text on dark blue background
window_actions_button = rgb(3498db), rgb(0f1a2c), ⚡, ⚡, kitty --working-directory="$(pwd)",

# Screenshot button - orange text on brown background
window_actions_button = rgb(e67e22), rgb(2c1510), 📷, 📷, grim -g "$(hyprctl activewindow -j | jq -r '"\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])"')" ~/window.png,

# Workspace button - purple text on dark purple background
window_actions_button = rgb(8e44ad), rgb(1a0f1f), →, →, hyprctl dispatch movetoworkspace +1,

# Mixed styling example - some with custom colors, some with defaults
window_actions_button = rgb(ff6b6b), rgb(330000), 🔥, 🔥, systemctl --user restart myapp,
window_actions_button = , , 🔧, 🔧, code .,
```

## Testing

Run the test suite to verify plugin functionality:

```bash
make test
```

This will compile and run all unit tests using Google Test framework. The tests focus on core logic validation including:

- Button position detection algorithms
- State management logic
- Pass element functionality
- Boundary condition testing

Note: Tests are designed as standalone unit tests that don't require the full Hyprland runtime environment.

## Features

- **Dynamic Button Configuration**: Define any number of buttons with custom icons and commands
- **State-Aware Icons**: Icons change based on window state (fullscreen, floating, grouped, etc.)
- **Customizable Colors**: Set individual colors for each button's text/icons and global background
- **Flexible Commands**: Execute any shell command or Hyprland dispatcher
- **Mouse Button Configuration**: Choose which mouse button triggers actions
- **Unicode Icon Support**: Use any Unicode characters as button icons

## Development

### Building from Source

1. Clone or download the plugin source
2. Install development dependencies (see Prerequisites)
3. Run `make` to build the plugin
4. Load the plugin in Hyprland configuration

### Extending the Plugin

Key files:
- `main.cpp` - Plugin initialization and configuration parsing
- `WindowActionsBar.cpp` - Button rendering and interaction logic
- `globals.hpp` - Data structures and shared state

## Troubleshooting

- If build fails, ensure both `hyprland-devel` and `hyprland-protocols-devel` packages are installed
- Check that `/usr/include/hyprland` exists (Hyprland headers)
- For permission issues, ensure the plugin file is readable by your user