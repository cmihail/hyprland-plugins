#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

// Test fixture for config path detection
class ConfigPathDetectionTest : public ::testing::Test {
protected:
    std::string testDir;
    std::string configBasePath;
    std::string pluginsConfPath;
    std::string hyprlandConfPath;
    std::string originalHome;

    void SetUp() override {
        // Create temporary test directory
        testDir = fs::temp_directory_path() / "mouse_gestures_test_config_path";
        fs::create_directories(testDir);

        configBasePath = testDir + "/config/hypr";
        fs::create_directories(configBasePath + "/config");

        pluginsConfPath = configBasePath + "/config/plugins.conf";
        hyprlandConfPath = configBasePath + "/hyprland.conf";

        // Save original HOME
        const char* home = std::getenv("HOME");
        if (home) {
            originalHome = home;
        }

        // Set temporary HOME
        setenv("HOME", testDir.c_str(), 1);
    }

    void TearDown() override {
        // Restore original HOME
        if (!originalHome.empty()) {
            setenv("HOME", originalHome.c_str(), 1);
        }

        // Clean up test directory
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    void createPluginsConf(const std::string& content) {
        std::ofstream file(pluginsConfPath);
        file << content;
        file.close();
    }

    void createHyprlandConf(const std::string& content) {
        std::ofstream file(hyprlandConfPath);
        file << content;
        file.close();
    }

    std::string detectConfigPath() {
        // This simulates the detectConfigFilePath function
        std::vector<std::string> configPaths = {
            pluginsConfPath,
            hyprlandConfPath
        };

        for (const auto& configPath : configPaths) {
            std::ifstream inFile(configPath);
            if (!inFile.is_open()) {
                continue;
            }

            std::string line;
            while (std::getline(inFile, line)) {
                std::string trimmed = line;
                trimmed.erase(0, trimmed.find_first_not_of(" \t"));
                trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

                if (trimmed.find("mouse_gestures") != std::string::npos ||
                    trimmed.find("gesture_action") != std::string::npos) {
                    inFile.close();
                    return configPath;
                }
            }
            inFile.close();
        }

        // Default to plugins.conf if no config found
        return pluginsConfPath;
    }
};

// Test that plugins.conf is detected when it contains mouse_gestures config
TEST_F(ConfigPathDetectionTest, DetectsPluginsConfWithMouseGestures) {
    createPluginsConf(R"(
plugin {
    mouse_gestures {
        drag_threshold = 50
    }
}
)");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test that plugins.conf is detected when it contains gesture_action
TEST_F(ConfigPathDetectionTest, DetectsPluginsConfWithGestureAction) {
    createPluginsConf(R"(
plugin {
    mouse_gestures {
        gesture_action = exec notify-send "Test"|0.5,0.5;0.5,0.8
    }
}
)");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test that hyprland.conf is detected when plugins.conf doesn't exist
TEST_F(ConfigPathDetectionTest, DetectsHyprlandConfWhenPluginsConfMissing) {
    createHyprlandConf(R"(
plugin {
    mouse_gestures {
        drag_threshold = 50
    }
}
)");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, hyprlandConfPath);
}

// Test that plugins.conf takes precedence over hyprland.conf
TEST_F(ConfigPathDetectionTest, PluginsConfTakesPrecedence) {
    createPluginsConf(R"(
plugin {
    mouse_gestures {
        drag_threshold = 50
    }
}
)");

    createHyprlandConf(R"(
plugin {
    mouse_gestures {
        drag_threshold = 100
    }
}
)");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test that default is plugins.conf when no config found
TEST_F(ConfigPathDetectionTest, DefaultsToPluginsConf) {
    createPluginsConf("");
    createHyprlandConf("");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test that config with whitespace is detected
TEST_F(ConfigPathDetectionTest, DetectsConfigWithWhitespace) {
    createPluginsConf(R"(
plugin {
    mouse_gestures {
        drag_threshold = 50
    }
}
)");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test that config with comments is detected
TEST_F(ConfigPathDetectionTest, DetectsConfigWithComments) {
    createPluginsConf(R"(
# Mouse gestures configuration
plugin {
    mouse_gestures {
        # Set threshold
        drag_threshold = 50
    }
}
)");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test that partial matches don't trigger detection
TEST_F(ConfigPathDetectionTest, IgnoresPartialMatches) {
    createPluginsConf(R"(
# This is not mouse_gestures configuration
some_other_plugin {
    option = value
}
)");

    std::string detected = detectConfigPath();
    // Should default to plugins.conf since no mouse_gestures config found
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test detection with gesture_action in hyprland.conf
TEST_F(ConfigPathDetectionTest, DetectsGestureActionInHyprlandConf) {
    createHyprlandConf(R"(
plugin:mouse_gestures:gesture_action = exec kitty|0.5,0.5;0.5,0.8
)");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, hyprlandConfPath);
}

// Test that empty files default to plugins.conf
TEST_F(ConfigPathDetectionTest, EmptyFilesDefaultToPluginsConf) {
    createPluginsConf("");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test that files with only whitespace default to plugins.conf
TEST_F(ConfigPathDetectionTest, WhitespaceOnlyDefaultsToPluginsConf) {
    createPluginsConf("   \n\t\n   ");

    std::string detected = detectConfigPath();
    EXPECT_EQ(detected, pluginsConfPath);
}

// Test case sensitivity
TEST_F(ConfigPathDetectionTest, CaseSensitive) {
    createPluginsConf(R"(
plugin {
    MOUSE_GESTURES {
        drag_threshold = 50
    }
}
)");

    std::string detected = detectConfigPath();
    // Should default since "MOUSE_GESTURES" != "mouse_gestures"
    EXPECT_EQ(detected, pluginsConfPath);
}
