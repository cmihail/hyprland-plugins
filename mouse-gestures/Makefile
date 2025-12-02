PLUGIN_NAME = mouse-gestures

SOURCE_FILES = main.cpp MouseGestureOverlay.cpp

COMPILE_FLAGS = -shared -fPIC --no-gnu-unique -g -std=c++23 -Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing -Wno-pointer-arith
COMPILE_FLAGS += -I "/usr/include/pixman-1" -I "/usr/include/libdrm" -I "/usr/include" -I "$(HYPRLAND_HEADERS)" -I "$(HYPRLAND_HEADERS)/protocols"
COMPILE_FLAGS += $(shell pkg-config --cflags --libs gdk-pixbuf-2.0 pangocairo cairo)

HYPRLAND_HEADERS ?= /usr/include/hyprland

all: check_env $(PLUGIN_NAME).so

check_env:
	@if [ ! -d "$(HYPRLAND_HEADERS)" ]; then \
		echo "Error: HYPRLAND_HEADERS directory '$(HYPRLAND_HEADERS)' not found."; \
		echo "Please install hyprland-devel package or set HYPRLAND_HEADERS to your Hyprland source directory."; \
		exit 1; \
	fi

$(PLUGIN_NAME).so: $(SOURCE_FILES)
	g++ $(COMPILE_FLAGS) $(SOURCE_FILES) -o $(PLUGIN_NAME).so

clean:
	rm -f $(PLUGIN_NAME).so
	$(MAKE) -C tests clean

install: $(PLUGIN_NAME).so
	mkdir -p ~/.local/share/hyprload/plugins/$(PLUGIN_NAME)
	cp $(PLUGIN_NAME).so ~/.local/share/hyprload/plugins/$(PLUGIN_NAME)/

test:
	$(MAKE) -C tests test

.PHONY: all clean install check_env test
