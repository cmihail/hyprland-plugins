# Monitor Workspace Plugin

A Hyprland plugin that automatically manages workspace placement based on connected monitors. Define different workspace layouts for different monitor configurations, and the plugin will automatically apply the correct layout when monitors are connected or disconnected.

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
cd monitor-workspace
```

2. Build the plugin:
```bash
make
```

This will create `monitor-workspace.so` in the current directory.

## Loading the Plugin

### Method 1: Using hyprpm (Recommended)

1. Install the plugin using hyprpm:
```bash
hyprpm add /path/to/monitor-workspace
hyprpm enable monitor-workspace
```

2. Reload Hyprland configuration:
```bash
hyprctl reload
```

### Method 2: Manual Loading

1. Add to your Hyprland configuration (`~/.config/hypr/hyprland.conf`):
```
plugin = /path/to/monitor-workspace/monitor-workspace.so
```

2. Reload Hyprland:
```bash
hyprctl reload
```

## Configuration

Add configuration options to your Hyprland config file (`~/.config/hypr/hyprland.conf`):

### Basic Configuration

```conf
plugin {
    monitor_workspace {
        # Define layout for 1 monitor
        layout_1_monitors = m1:1,2,3,4,5,6,7,8,9,10

        # Define layout for 2 monitors
        layout_2_monitors = m1:2,5,6;m2:1,3,4

        # Define layout for 3 monitors
        layout_3_monitors = m1:7;m2:2,5,6;m3:1,3,4
    }
}
```

### Configuration Format

Each layout follows this pattern:
```
layout_N_monitors = m1:workspaces;m2:workspaces;m3:workspaces
```

Where:
- `N` is the number of monitors (1-10)
- `m1`, `m2`, `m3`, etc. are relative monitor names
- `workspaces` is a comma-separated list of workspace IDs

### Monitor Ordering

Monitors are automatically sorted by position:
- **Primary sort**: X position (left to right)
- **Secondary sort**: Y position (top to bottom, if X is equal)

This means:
- `m1` = leftmost monitor
- `m2` = second monitor from the left
- `m3` = third monitor from the left
- etc.

### Example Configurations

#### Single Monitor Setup
```conf
plugin {
    monitor_workspace {
        # All workspaces on one monitor
        layout_1_monitors = m1:1,2,3,4,5,6,7,8,9,10
    }
}
```

#### Dual Monitor Setup
```conf
plugin {
    monitor_workspace {
        # Single monitor: all workspaces
        layout_1_monitors = m1:1,2,3,4,5,6,7,8,9,10

        # Dual monitor: split workspaces
        layout_2_monitors = m1:1,2,3,4,5;m2:6,7,8,9,10
    }
}
```

#### Triple Monitor Setup
```conf
plugin {
    monitor_workspace {
        # Single monitor
        layout_1_monitors = m1:1,2,3,4,5,6,7,8,9,10

        # Dual monitor
        layout_2_monitors = m1:1,3,5,7,9;m2:2,4,6,8,10

        # Triple monitor
        layout_3_monitors = m1:1,4,7,10;m2:2,5,8;m3:3,6,9
    }
}
```

#### Asymmetric Layouts
```conf
plugin {
    monitor_workspace {
        # Primary monitor gets more workspaces
        layout_2_monitors = m1:1,2,3,4,5,6,7;m2:8,9,10

        # Middle monitor for main work, sides for auxiliary
        layout_3_monitors = m1:9,10;m2:1,2,3,4,5;m3:6,7,8
    }
}
```

## Features

- **Automatic Layout Switching**: Layouts change automatically when monitors are connected/disconnected
- **Position-Based Ordering**: Monitors are identified by physical position, not by name
- **Workspace Management**: Workspaces are automatically created and moved to the correct monitors
- **Persistent Configuration**: Layouts survive monitor changes and Hyprland restarts
- **Multiple Monitor Configurations**: Support for 1-10 monitor setups
- **Auto-Focus**: Each monitor automatically switches to its first configured workspace

## How It Works

1. **Monitor Detection**: When a monitor is added or removed, the plugin detects the change
2. **Layout Lookup**: The plugin finds the layout configuration for the current monitor count
3. **Monitor Sorting**: Physical monitors are sorted by position (left to right, top to bottom)
4. **Monitor Mapping**: Relative monitor names (m1, m2, m3) are mapped to actual monitor names
5. **Workspace Assignment**: Each workspace is created or moved to its assigned monitor
6. **Auto-Focus**: Each monitor switches to its first configured workspace

## Testing

Run the test suite to verify plugin functionality:

```bash
make test
```

This will compile and run all unit tests using Google Test framework.

## Troubleshooting

### No Layout Applied
- Ensure you have a `layout_N_monitors` entry for your current monitor count
- Check that workspace IDs are valid integers
- Verify the configuration syntax matches the format shown above

### Workspaces on Wrong Monitors
- Check that your monitor physical positions match your expectations
- Use `hyprctl monitors` to see current monitor order
- Remember that m1 is the leftmost monitor based on X position

### Configuration Errors
The plugin shows notifications for:
- Invalid layout format (missing colon or semicolon)
- Invalid workspace numbers (non-numeric values)

## Development

### Building from Source

1. Clone or download the plugin source
2. Install development dependencies (see Prerequisites)
3. Run `make` to build the plugin
4. Load the plugin in Hyprland configuration

### Project Structure

- `main.cpp` - Plugin implementation
- `Makefile` - Build configuration
- `tests/` - Unit tests (if implemented)

## License

This plugin is provided as-is for use with Hyprland.