# Window Actions Plugin

A minimal Hyprland plugin for window actions.

## Prerequisites

On Fedora 42, install the required development packages:

```bash
sudo dnf install hyprland-devel hyprland-protocols-devel gcc-c++ make pkgconf-devel
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

## Development

This is a minimal plugin template. To add functionality, modify `main.cpp` and rebuild with `make`.

## Troubleshooting

- If build fails, ensure both `hyprland-devel` and `hyprland-protocols-devel` packages are installed
- Check that `/usr/include/hyprland` exists (Hyprland headers)
- For permission issues, ensure the plugin file is readable by your user