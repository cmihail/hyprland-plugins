# Copy Indicator Plugin

A Hyprland plugin that displays a clipboard emoji (📋) at the mouse cursor position when you copy text, replacing traditional desktop notifications with a lightweight visual indicator.

## Features

- Shows a 📋 emoji icon at the exact mouse cursor position when copying
- Smooth fade in/out animation over 1 second
- No notification daemon required
- Works across all monitors
- Triggered via custom dispatcher for precise control

## Installation

### Prerequisites

- Hyprland v0.53.3 or compatible version
- Hyprland headers installed at `~/.local/include/hyprland/`
- Cairo and Pango libraries

### Building

```bash
cd /home/cmihail/hyprland-plugins/copy-indicator
make clean
make
```

### Loading the Plugin

Add to your Hyprland plugins configuration (`~/configs/hypr/config/plugins.conf`):

```bash
plugin = /home/cmihail/hyprland-plugins/copy-indicator/copy-indicator.so
```

Or load manually:

```bash
hyprctl plugin load /home/cmihail/hyprland-plugins/copy-indicator/copy-indicator.so
```

## Usage

### Keybinding Setup

Replace your copy notification script with the plugin dispatcher. In `~/configs/hypr/config/bindings.conf`:

```bash
# Old (with notification):
# bind = , XF86Copy, exec, ~/.config/hypr/scripts/copy_with_notification.sh

# New (with copy indicator):
bind = , XF86Copy, exec, ~/.config/hypr/scripts/smart_key.sh "ctrl+c" "ctrl+shift+c" && hyprctl dispatch copyindicator:show
```

Or use directly in any keybinding:

```bash
bind = $mainMod, C, exec, wl-copy && hyprctl dispatch copyindicator:show
```

### Manual Trigger

You can trigger the indicator manually via:

```bash
hyprctl dispatch copyindicator:show
```

## Configuration

The plugin currently has the following settings hardcoded:

- **Icon**: 📋 (clipboard emoji)
- **Size**: 48x48 pixels
- **Duration**: 1000ms (1 second total)
  - Fade in: 0-500ms
  - Fade out: 500-1000ms
- **Position**: Centered at mouse cursor

Future versions may add configuration options via Hyprland's config system.

## How It Works

1. Plugin registers a custom dispatcher `copyindicator:show`
2. When triggered, it captures the current mouse cursor position
3. Creates a Cairo surface with the emoji rendered via Pango
4. Converts to OpenGL texture and renders it on screen
5. Uses Hyprland's animation system for smooth fade in/out
6. Automatically cleans up after 1 second

## Troubleshooting

### Plugin fails to load

Check version compatibility:
```bash
hyprctl version
```

Rebuild against your current Hyprland headers.

### Icon doesn't appear

1. Verify the plugin is loaded:
   ```bash
   hyprctl plugin list
   ```

2. Check Hyprland logs:
   ```bash
   journalctl --user -u hyprland -f
   ```

3. Test the dispatcher manually:
   ```bash
   hyprctl dispatch copyindicator:show
   ```

### Emoji appears as box/square

Your system may be missing emoji font support. Install:
```bash
sudo dnf install google-noto-emoji-fonts
```

## Performance

The plugin is designed to be lightweight:
- Only renders when active
- Uses hardware-accelerated OpenGL textures
- Minimal CPU usage (~0.1% during animation)
- Per-monitor overlay instances

## Credits

Created by cmihail as part of the Hyprland custom plugins collection.

## License

See the main LICENSE file in the hyprland-plugins repository.
