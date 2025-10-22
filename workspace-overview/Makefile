PLUGIN_NAME = workspace-overview

SOURCE_FILES = main.cpp overview.cpp OverviewPassElement.cpp

COMPILE_FLAGS = -shared -fPIC --no-gnu-unique -g -std=c++23 -Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing -Wno-pointer-arith
COMPILE_FLAGS += -I "/usr/include/pixman-1" -I "/usr/include/libdrm" -I "$(HYPRLAND_HEADERS)" -I "$(HYPRLAND_HEADERS)/protocols"

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

test:
	$(MAKE) -C tests test

install: $(PLUGIN_NAME).so
	mkdir -p ~/.local/share/hyprload/plugins/$(PLUGIN_NAME)
	cp $(PLUGIN_NAME).so ~/.local/share/hyprload/plugins/$(PLUGIN_NAME)/

.PHONY: all clean install check_env test
