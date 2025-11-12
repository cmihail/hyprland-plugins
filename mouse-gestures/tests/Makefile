TEST_TARGET = mouse-gestures-tests

TEST_SOURCES = test_main.cpp test_standalone.cpp test_threading.cpp test_stroke.cpp test_config_parsing.cpp test_recording.cpp test_default_command.cpp test_dimming.cpp test_trail_animation.cpp test_shutdown.cpp test_overlay.cpp
PLUGIN_SOURCES =

COMPILE_FLAGS = -std=c++23 -Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing -Wno-pointer-arith
COMPILE_FLAGS += -I "../"
COMPILE_FLAGS += $(shell pkg-config --cflags gtest)
LINK_FLAGS = $(shell pkg-config --libs gtest) -pthread

HYPRLAND_HEADERS ?= /usr/include/hyprland

all: check_env $(TEST_TARGET)

check_env:
	@if ! pkg-config --exists gtest; then \
		echo "Error: gtest not found. Please install gtest-devel package."; \
		exit 1; \
	fi

$(TEST_TARGET): $(TEST_SOURCES) $(PLUGIN_SOURCES)
	g++ $(COMPILE_FLAGS) $(TEST_SOURCES) $(PLUGIN_SOURCES) $(LINK_FLAGS) -o $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TEST_TARGET)

.PHONY: all test clean check_env
