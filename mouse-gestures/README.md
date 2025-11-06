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

3. **Check the recorded data**:
   ```bash
   cat /tmp/hyprland-mouse-gesture-recorded
   ```

4. **Add to your config**:
   ```conf
   plugin {
       mouse_gestures {
           gesture_action = hyprctl dispatch workspace +1|<paste_stroke_data_here>
       }
   }
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
        # Then paste the stroke data from /tmp/hyprland-mouse-gesture-recorded

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
- **Testing**: Draw gestures to see match results in `/tmp/hyprland-mouse` (includes point count and stroke data)

## Debugging

The plugin writes detailed debug logs to `/tmp/hyprland-debug` to help troubleshoot matching issues.

### What's Logged

The debug log includes:
- **Config loading**: When gesture actions are added/cleared from config
- **Gesture detection**: When gestures are detected and their path size
- **Stroke creation**: Input stroke data and point coordinates
- **Pattern comparison**:
  - Each configured pattern being checked
  - Comparison costs for each pattern
  - Match threshold value
  - Which pattern (if any) matched
- **Command execution**: When commands are executed

### How to Debug Matching Issues

1. **Clear the log** before testing:
   ```bash
   rm /tmp/hyprland-debug
   ```

2. **Reload config** to see what gestures are loaded:
   ```bash
   hyprctl reload
   ```

3. **Check what was loaded**:
   ```bash
   tail -20 /tmp/hyprland-debug
   ```
   Look for lines like:
   ```
   >>> Config: Added gesture action <<<
     Name: YourGestureName
     Command: your command
     Stroke points: 50
     Stroke data: x,y;x,y;...
   ```

4. **Draw a gesture** and check the matching process:
   ```bash
   tail -100 /tmp/hyprland-debug
   ```

5. **Look for**:
   - `>>> GESTURE DETECTED <<<` - Shows path size
   - `Match threshold:` - Current threshold value (default: 0.15)
   - `Comparing against N configured gestures` - How many patterns loaded
   - `Comparison cost:` - Cost for each pattern comparison
   - `FINAL MATCH:` or `NO MATCH FOUND` - Result

### Common Issues

**Issue: "Comparing against 0 configured gestures"**
- Your gesture actions aren't loading from config
- Check config syntax (must use `{ }` block format)
- Look for error notifications when reloading

**Issue: All comparison costs > threshold**
- Gestures are too different from recorded patterns
- Try increasing `match_threshold` (e.g., to 0.25)
- Re-record the gesture for better accuracy

**Issue: Stroke has very few points**
- Drawing gesture too fast
- Try drawing slower or increase `drag_threshold`

### Example Debug Output

```
[1699123456789] >>> Config: Added gesture action <<<
[1699123456789]   Name: SwipeRight
[1699123456789]   Command: hyprctl dispatch workspace +1
[1699123456789]   Stroke points: 45
[1699123456789]   Stroke data: 100.5,200.3;150.7,200.8;...

[1699123456999] >>> GESTURE DETECTED <<<
[1699123456999] Path size: 52
[1699123456999] === findMatchingGestureAction START ===
[1699123456999] Match threshold: 0.150000
[1699123456999] Input stroke created successfully with 52 points
[1699123456999] Comparing against 1 configured gestures
[1699123456999]   Checking gesture: SwipeRight
[1699123456999]     Pattern has 45 points
[1699123456999]     Comparison cost: 0.087234
[1699123456999]     Current best cost: 0.150000
[1699123456999]     NEW BEST MATCH! Cost: 0.087234
[1699123456999] FINAL MATCH: SwipeRight with cost 0.087234
[1699123456999] EXECUTING COMMAND: hyprctl dispatch workspace +1
```

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
