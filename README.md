# Hyprland Custom Plugins

A collection of custom plugins for Hyprland window manager, built for Hyprland v0.53.3.

## Plugins

### 1. Mouse Gestures
**Status**: ✅ Working
**Description**: Execute commands using mouse gestures with fuzzy pattern matching based on the easystroke algorithm

[Full Documentation](mouse-gestures/README.md)

### 2. No Mouse
**Status**: ✅ Working
**Description**: 26x26 grid overlay for keyboard-based mouse navigation. Move your cursor to any screen position using just two letter key presses, similar to mouseless

[Full Documentation](no-mouse/README.md)

### 3. Window Actions
**Status**: ✅ Working (fixed 2 crash bugs)
**Description**: Add customizable action buttons to window decorations with support for custom colors, icons, and state-dependent behavior

**Fixed Issues in v0.53.3**:
- Crash due to nullptr dereference when `getMonitorFromCursor()` returned null
- Crash due to bad_optional_access when calling `.value()` on `COverridableVar<bool>` without checking

[Full Documentation](window-actions/README.md)

### 4. Workspace Overview
**Status**: ✅ Working
**Description**: Visual workspace overview similar to GNOME's Activities view with split-screen layout and click-to-switch functionality

[Full Documentation](workspace-overview/README.md)

## Local Build Setup

All plugins are built against **Hyprland v0.53.3** headers installed locally at:
```
/home/cmihail/.local/include/hyprland/
```

See `/home/cmihail/configs/hypr/LOCAL_BUILD.md` for complete Hyprland build documentation.

## Building Plugins

### Prerequisites

Ensure you have Hyprland v0.53.3 headers installed:

```bash
ls /home/cmihail/.local/include/hyprland/src/
```

### Build All Plugins

```bash
cd /home/cmihail/hyprland-plugins
for plugin in mouse-gestures no-mouse window-actions workspace-overview; do
    echo "Building $plugin..."
    cd "$plugin"
    make clean && make
    cd ..
done
```

### Build Individual Plugin

```bash
cd /home/cmihail/hyprland-plugins/<plugin-name>
make clean
make
```

## Makefile Configuration

All plugin Makefiles use these settings for the local build:

```makefile
HYPRLAND_HEADERS ?= /home/cmihail/.local/include

COMPILE_FLAGS = -shared -fPIC --no-gnu-unique -g -std=c++23 -Wall -Wextra \
    -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers \
    -Wno-narrowing -Wno-pointer-arith
COMPILE_FLAGS += -I "/usr/include/pixman-1" -I "/usr/include/libdrm" \
    -I "/usr/include" -I "$(HYPRLAND_HEADERS)" \
    -I "$(HYPRLAND_HEADERS)/hyprland/protocols"
```

## Loading Plugins

Plugins are loaded via `~/configs/hypr/config/plugins.conf`:

```bash
# Load plugins
plugin = /home/cmihail/hyprland-plugins/mouse-gestures/mouse-gestures.so
plugin = /home/cmihail/hyprland-plugins/no-mouse/no-mouse.so
plugin = /home/cmihail/hyprland-plugins/window-actions/window-actions.so
plugin = /home/cmihail/hyprland-plugins/workspace-overview/workspace-overview.so
```

Or manually via hyprctl:

```bash
hyprctl plugin load /home/cmihail/hyprland-plugins/<plugin-name>/<plugin-name>.so
```

## Common Build Errors & Solutions

| Error | Cause | Solution |
|-------|-------|----------|
| Version mismatch | Plugin ABI doesn't match Hyprland | Rebuild with current headers |
| `m_lastMonitor` not found | API changed in v0.53.x | Use `getMonitorFromCursor()` |
| `Debug::log` not found | Logging API changed | Use `Log::logger->log()` |
| `RESERVED_EXTENTS` not declared | Namespace change | Add `Desktop::View::` prefix |
| Segfault on load | Nullptr dereference or bad_optional_access | Check for null, use `valueOrDefault()` |

## Testing Plugins

```bash
# Load plugin
hyprctl plugin load /home/cmihail/hyprland-plugins/<plugin-name>/<plugin-name>.so

# Verify loaded
hyprctl plugin list

# Check logs for errors
journalctl --user -u hyprland -f
```

## Maintenance

When updating Hyprland:

1. Update Hyprland and dependencies (see `/home/cmihail/configs/hypr/LOCAL_BUILD.md`)
2. Check Hyprland changelog for API changes
3. Update plugin code if needed
4. Rebuild all plugins
5. Test each plugin individually

## Resources

- **Hyprland Plugin API**: https://wiki.hyprland.org/Plugins/Development/Getting-Started/
- **Local Build Docs**: `/home/cmihail/configs/hypr/LOCAL_BUILD.md`
- **Hyprland Headers**: `/home/cmihail/.local/include/hyprland/`

## Notes for Future AI Sessions

- All plugins built against Hyprland v0.53.3 local headers
- window-actions had two critical crash bugs fixed (nullptr and bad_optional_access)
- Always rebuild plugins after Hyprland updates
- Use this migration guide when Hyprland API changes occur
- Check `/home/cmihail/configs/hypr/LOCAL_BUILD.md` for Hyprland build details
