# Mouse Gestures Plugin for Hyprland

A Hyprland plugin that adds mouse gesture support using the easystroke pattern matching algorithm.

## Features

- **Easystroke Algorithm**: Uses the same sophisticated pattern matching algorithm as easystroke
- **Fuzzy Matching**: Gestures don't need to be pixel-perfect - the algorithm handles variations in speed and slight deviations
- **Easy Recording**: Built-in gesture recording mode to capture your own gestures
- **Configurable**: Threshold-based matching, custom action button, drag threshold

## Configuration

Add these settings to your Hyprland config:

```conf
plugin {
    mouse_gestures {
        # Plugin configuration (optional - these are the defaults)
        drag_threshold = 50          # Pixels to move before detecting gesture
        action_button = 273          # BTN_RIGHT (right mouse button)
        match_threshold = 0.15       # Lower = stricter matching

        # Define gesture actions using pipe-delimited format
        # Format: gesture_action = <command>|<stroke_data>
        # The pipe | separates command from stroke data
        # Stroke data can contain commas and semicolons freely!

        gesture_action = your command here|x1,y1;x2,y2;x3,y3;...
    }
}

# Alternative: Use full path at top level (outside plugin blocks)
plugin:mouse_gestures:gesture_action = command|x1,y1;x2,y2;...
```

## Recording Gestures

To create gesture patterns:

1. **Enable recording mode**:
   ```bash
   hyprctl dispatch mouse-gestures record
   ```
   You'll see a notification that recording mode is enabled.

2. **Draw your gesture** using the action button (right mouse button by default).
   - If the gesture matches an existing one, you'll see an orange notification showing which command it duplicates (no new entry will be added)
   - If the gesture is unique, it will be automatically added to your Hyprland configuration file

3. **Edit the recorded gesture** (if a new gesture was added):
   The plugin will add a placeholder gesture_action to your config file (in `~/.config/hypr/config/plugins.conf` if it exists, or `~/.config/hypr/hyprland.conf` otherwise):
   ```conf
   gesture_action = hyprctl notify -1 2000 "rgb(ff0000)" "modify me in config file <path>"|<stroke_data>
   ```
   The notification will indicate which config file was modified.

4. **Customize the command**:
   Edit the config file and replace the placeholder command with your desired action:
   ```conf
   gesture_action = hyprctl dispatch workspace +1|<stroke_data>
   ```

5. **Reload config**:
   ```bash
   hyprctl reload
   ```

## Example Configuration

```conf
plugin {
    mouse_gestures {
        # Increase drag threshold for less sensitive detection
        drag_threshold = 75

        # Tighten match threshold for more precise matching
        match_threshold = 0.10

        # Record some gestures first using: hyprctl dispatch mouse-gestures record
        # The stroke data will be automatically added to your config file

        # Example: Swipe right to go to next workspace
        gesture_action = hyprctl dispatch workspace +1|0.123456,0.500000;0.234567,0.500000;0.345678,0.500000;

        # Example: Swipe left to go to previous workspace
        gesture_action = hyprctl dispatch workspace -1|0.876543,0.500000;0.765432,0.500000;0.654321,0.500000;

        # Example: Swipe down to close window
        gesture_action = hyprctl dispatch killactive|0.500000,0.123456;0.500000,0.234567;0.500000,0.345678;

        # Another example with a notification
        gesture_action = notify-send "Custom gesture!"|0.5,0.3;0.6,0.4;0.7,0.5;
    }
}
```

## How It Works

### Pattern Matching Algorithm

The plugin uses the easystroke algorithm, which:

1. **Normalizes** gesture coordinates to [0,1] range centered at 0.5
2. **Calculates** arc-length parametrization for time-independence
3. **Computes** angles between consecutive points (range [-1,1] normalized by π)
4. **Compares** using dynamic programming to minimize angle differences
5. **Returns** a cost metric (lower = better match)

### Stroke Data Format

Stroke data is serialized as semicolon-separated coordinate pairs, where each pair is comma-separated x,y values:
```
x1,y1;x2,y2;x3,y3;...
```

The coordinates are the raw screen positions when you draw the gesture. The algorithm normalizes them internally.

**Note:** The pipe delimiter `|` separates command from stroke data, allowing commas and semicolons in the stroke data without conflicts.

## Tips

- **Consistent gestures**: Try to draw gestures consistently for best recognition
- **Match threshold**: Lower values (0.05-0.10) = stricter, higher values (0.15-0.25) = more lenient
- **Drag threshold**: Increase if gestures trigger too easily, decrease for more sensitivity

## Debugging

The plugin shows notifications for matched and unmatched gestures to help troubleshoot matching issues.

### Troubleshooting

**Issue: Gestures not being recognized**
- Check config syntax (must use `{ }` block format)
- Look for error notifications when reloading config with `hyprctl reload`
- Try increasing `match_threshold` (e.g., to 0.25) for more lenient matching
- Re-record the gesture for better accuracy

**Issue: Gestures trigger too easily**
- Increase `drag_threshold` (e.g., to 75 or 100)
- Decrease `match_threshold` for stricter matching

**Issue: Gestures require very precise drawing**
- Drawing gesture too fast may result in few points
- Try drawing slower or decrease `drag_threshold`

## Differences from Old Version

The previous version used discrete 8-direction matching (UP, DOWN, LEFT, RIGHT, etc.). This new version:

- ✅ Uses continuous angle-based matching (more accurate)
- ✅ Handles variations in gesture speed
- ✅ Provides fuzzy matching with configurable threshold
- ✅ Compatible with easystroke gesture data
- ❌ No longer supports simple direction patterns like "UP DOWN"

## Building

```bash
make
```

## Error Handling

The plugin includes comprehensive error handling to prevent crashes:

- **Try-catch blocks** around all operations that could throw exceptions
- **Null pointer checks** before accessing config values and managers
- **Bounds checking** in stroke comparison algorithm
- **Validation** of deserialized stroke data
- **Safe file operations** with exception handling
- **Hook safety** - all hook callbacks are wrapped to prevent exceptions from propagating to Hyprland

If an error occurs during gesture processing, the plugin will:
1. Show a notification (when appropriate)
2. Reset gesture state
3. Continue running without crashing your session

## License

Based on easystroke's stroke matching algorithm (ISC License, Copyright 2009 Thomas Jaeger).
