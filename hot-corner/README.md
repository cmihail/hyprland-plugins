# Hot Corner Plugin for Hyprland

A Hyprland plugin that detects when the mouse cursor enters the top-left corner of a monitor and executes a configurable command after a delay.

## Configuration

Add the following to your Hyprland configuration file (`~/.config/hypr/hyprland.conf`):

```ini
plugin {
    hotcorner {
        # Command to execute when hot corner is activated (required)
        command = "notify-send 'Hot corner activated!'"

        # Delay in milliseconds before activation (default: 1000)
        delay_ms = 1000

        # Size of the hot corner area in pixels (default: 10)
        corner_size = 10
    }
}
```

## Example Commands

You can configure any shell command to be executed:

```ini
# Show notification
command = "notify-send 'Hot Corner' 'Corner activated!'"

# Launch application
command = "rofi -show drun"

# Execute Hyprland dispatch command
command = "hyprctl dispatch exec firefox"

# Run script
command = "/path/to/your/script.sh"

# Multiple commands (using shell)
command = "sh -c 'notify-send Hello && firefox'"
```

## Installation

1. Compile the plugin:
   ```bash
   make
   ```

2. Install the plugin:
   ```bash
   make install
   ```

3. Load the plugin in your Hyprland configuration:
   ```ini
   exec-once = hyprload load hot-corner
   ```

## How it works

- The plugin monitors mouse position continuously
- When the cursor enters the top-left corner (defined by `corner_size`), a timer starts
- After the specified `delay_ms`, the configured `command` is executed
- A cooldown period prevents rapid repeated activations