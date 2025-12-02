# no-mouse

A Hyprland plugin that displays a 26x26 grid overlay for keyboard-based mouse navigation, similar to mouseless. Move your cursor to any screen position by typing two letters (to select a cell) and pressing SPACE or RETURN (to jump). Optionally, type a third letter for sub-column precision positioning within the selected cell.

## Features

- Registers a custom dispatcher called `no-mouse`
- Displays a 26x26 grid overlay with letter labels (configurable transparency)
- Navigate to any cell using 2-letter sequences + SPACE or RETURN key (e.g., AA + SPACE, BM + RETURN)
- **Precision mode**: Optional 3rd letter splits the cell into a 3x6 sub-grid (18 positions, A-R)
- **Visual feedback**: Selected cell is highlighted with a colored border before jumping
- **Sub-column visualization**: Sub-grid labels and highlighted sub-cell when using 3-letter sequences
- **Mistake correction**: Type new letters to change selection before pressing SPACE or RETURN
- Automatic letter key capture when overlay is active (no configuration needed)
- Keys are blocked from reaching active window while overlay is shown
- **Auto-exit**: Overlay automatically closes after jumping to a cell
- **Auto-focus**: Automatically focuses the window under the cursor after movement
- Toggle on/off with dispatcher or ESC key
- Configurable transparency, colors for overlay, grid lines, labels, and highlights

## Installation

### Building

```bash
cd no-mouse
make
```

### Installing

```bash
make install
```

This will install the plugin to `~/.local/share/hyprload/plugins/no-mouse/`.

### Loading the Plugin

Add to your Hyprland configuration:

```conf
plugin = ~/.local/share/hyprload/plugins/no-mouse/no-mouse.so
```

Or using hyprload:

```conf
plugin {
    hyprload {
        plugins {
            no-mouse
        }
    }
}
```

## Usage

The plugin registers a dispatcher called `no-mouse` that can be triggered via hyprctl:

### Toggle Overlay

```bash
hyprctl dispatch no-mouse toggle
```

This will:
- Show/hide a 26x26 grid overlay with letter labels (90% transparency)
- Each cell is labeled with two letters (row and column: AA, AB, AC... to ZZ)

### Mouse Navigation

With the overlay active:

1. **Toggle the overlay** - `hyprctl dispatch no-mouse toggle` or press your configured keybind
2. **Type two letters** - The first letter selects the row (A-Z), second letter selects the column (A-Z)
   - Letters are automatically captured when overlay is active
   - Keys are blocked from reaching the active window
   - After typing 2 letters, the selected cell is **highlighted** with a colored border (blue by default)
3. **Press SPACE or RETURN to jump** - Press the SPACE or RETURN key to move the cursor to the highlighted cell
   - Mouse cursor moves to the center of the selected cell
   - Window under the cursor gets focused automatically
   - Overlay closes after the jump

**Example workflow (coarse positioning):**
- Press `B` then `M` → Cell BM is highlighted with a colored border
- Press **SPACE** (or **RETURN**) → Mouse moves to the center of cell BM, overlay closes

#### Precision Mode (Optional 3rd Letter)

For fine-tuned positioning, you can type a **3rd letter** after selecting a cell to split it into a 3x6 sub-grid (18 positions, A-R):

1. **Type two letters** - Select a cell (e.g., `B` then `M` → Cell BM is highlighted)
2. **Type a third letter (A-R)** - The cell splits into a 3x6 sub-grid with 18 positions
   - The selected cell shows the sub-grid labels
   - The chosen sub-cell is highlighted in color
   - The mouse jumps automatically (no SPACE needed for 3-letter sequences)
3. **Movement happens immediately** - Mouse moves to the center of the sub-cell and overlay closes

**Example workflow (precision positioning):**
- Press `B` then `M` → Cell BM is highlighted with a colored border
- Press `K` → Mouse immediately jumps to sub-cell K within cell BM, overlay closes
- (No SPACE or RETURN needed for 3-letter sequences - auto-executes)

**Precision comparison:**
- **2 letters + SPACE/RETURN**: Jumps to one of **676 positions** (26×26 grid)
- **3 letters (auto-execute)**: Jumps to one of **12,168 positions** (26×26×18 grid)

**Correcting mistakes:**
- If you typed the wrong letters, just keep typing! The plugin tracks the **last 2 letters** you typed
- Example: Type `A`, `A` (wrong) → Cell AA is highlighted
- Type `B`, `M` (correct) → Cell BM is now highlighted instead
- Press SPACE or RETURN → Jumps to cell BM

**Note**: When the overlay is active, all letter keys (A-Z), SPACE, and RETURN are intercepted and won't be sent to the active window. This allows you to navigate without affecting your applications.

### Turning Off the Overlay

You can exit the overlay without jumping:

1. **Press ESC key** - Cancels any pending cell selection and closes the overlay
2. **Run toggle command again** - `hyprctl dispatch no-mouse toggle`

The overlay also automatically closes after you press SPACE or RETURN and jump to a cell.

## Keybinding Example

Add to your Hyprland configuration:

```conf
# Toggle overlay with SUPER + M
bind = SUPER, M, exec, hyprctl dispatch no-mouse toggle

# Or use submap for dedicated overlay mode
bind = SUPER, O, exec, hyprctl dispatch no-mouse toggle
```

## Configuration

You can customize the transparency of various overlay elements by adding configuration to your Hyprland config file (`~/.config/hypr/config/plugins.conf` or `~/.config/hypr/hyprland.conf`):

```conf
plugin {
  no_mouse {
    # Opacity settings
    opacity_background = 0.1   # Background overlay opacity (0.0 = fully transparent, 1.0 = opaque)
    opacity_grid_lines = 0.25  # Grid line opacity
    opacity_labels = 0.4       # Label text opacity

    # Color settings (hex format: 0xRRGGBB)
    border_color = 0x4C7FA6    # Border color around selected cell (default: blue)
    border_opacity = 0.8       # Border opacity
    highlight_color = 0x4C7FA6 # Highlight color for selected sub-cell (default: blue)
    highlight_opacity = 0.8    # Highlight opacity
  }
}
```

### Configuration Options

| Option | Default | Range/Format | Description |
|--------|---------|--------------|-------------|
| `opacity_background` | `0.1` | 0.0 - 1.0 | Background overlay opacity (0.1 = 90% transparent) |
| `opacity_grid_lines` | `0.25` | 0.0 - 1.0 | Grid lines opacity (0.25 = 75% transparent) |
| `opacity_labels` | `0.4` | 0.0 - 1.0 | Label text opacity (0.4 = 60% transparent) |
| `border_color` | `0x4C7FA6` | 0xRRGGBB | Border color around selected cell (hex RGB) |
| `border_opacity` | `0.8` | 0.0 - 1.0 | Border opacity (0.8 = 20% transparent) |
| `highlight_color` | `0x4C7FA6` | 0xRRGGBB | Highlight color for selected sub-cell (hex RGB) |
| `highlight_opacity` | `0.8` | 0.0 - 1.0 | Highlight opacity (0.8 = 20% transparent) |

**Note**: Opacity values are not percentages. A value of `0.1` means 10% opaque (90% transparent), while `1.0` means fully opaque (0% transparent).

### Example Configurations

**More visible (lighter background, brighter labels)**:
```conf
plugin {
  no_mouse {
    opacity_background = 0.3
    opacity_grid_lines = 0.5
    opacity_labels = 0.7
  }
}
```

**Minimal (very transparent, subtle overlay)**:
```conf
plugin {
  no_mouse {
    opacity_background = 0.05
    opacity_grid_lines = 0.15
    opacity_labels = 0.25
  }
}
```

**Custom colors (red highlights)**:
```conf
plugin {
  no_mouse {
    border_color = 0xFF0000       # Red border
    border_opacity = 0.9
    highlight_color = 0xFF4444    # Light red highlight
    highlight_opacity = 0.7
  }
}
```

**Match workspace-overview theme (default)**:
```conf
plugin {
  no_mouse {
    border_color = 0x4C7FA6       # Blue border (matches workspace-overview)
    border_opacity = 0.8
    highlight_color = 0x4C7FA6    # Blue highlight
    highlight_opacity = 0.8
  }
}
```

After modifying the configuration, reload your Hyprland config with `hyprctl reload` for changes to take effect.

## Visual Effect

When enabled, the overlay displays:
- **Background**: Semi-transparent black (default 90% transparency) covering the full screen
- **Grid**: 26x26 grid with white lines (default 75% transparency)
- **Labels**: Letter pairs in each cell (default 60% transparency, non-bold Sans font)

**After typing 2 letters:**
- **Border**: Colored border (3px) around the selected cell (default: blue, configurable)
- Waits for SPACE or RETURN to execute the jump

**After typing 3 letters (precision mode):**
- **Sub-grid labels**: 18 letter labels (A-R) in a 3x6 grid within the selected cell
- **Sub-cell highlight**: Selected sub-cell is highlighted with color (default: blue, configurable)
- **Auto-execute**: Mouse jumps immediately without requiring SPACE

**Customizable colors:**
- Border and highlight colors can be changed via config (hex RGB format: 0xRRGGBB)
- Default colors match the workspace-overview plugin theme (blue: 0x4C7FA6)

The plugin operates silently without any notifications for a clean, distraction-free experience.

## Requirements

- Hyprland (latest version)
- hyprland-devel package (for headers)
- C++23 compatible compiler
- Cairo and Pango libraries (for text rendering)
  - `pkg-config --cflags --libs pangocairo cairo`

## Use Cases

- **Keyboard-only navigation**: Control mouse cursor without touching the mouse
- **Accessibility**: Alternative mouse input method for users who prefer keyboard
- **Efficiency**: Quickly jump to any screen location with 2 keystrokes
- **Presentations**: Navigate without visible mouse movement
- **Similar tools**: Works like mouseless but integrated into Hyprland

## License

See LICENSE file in the repository root.
