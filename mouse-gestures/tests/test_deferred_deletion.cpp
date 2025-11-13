#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstdlib>

// Mock the global state from main.cpp (anonymous namespace to avoid multiple definition)
namespace {
std::vector<std::string> g_pendingGestureDeletions;
}

// Test helper: Create a temporary config file
class TempConfigFile {
public:
    std::string path;

    TempConfigFile() {
        path = "/tmp/test_gesture_config_" + std::to_string(rand()) + ".conf";
    }

    ~TempConfigFile() {
        std::filesystem::remove(path);
    }

    void writeContent(const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }

    std::string readContent() {
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        return content;
    }
};

// Mock implementation of batch delete function (simplified for testing)
bool mockBatchDeleteGesturesFromConfig(const std::vector<std::string>& strokesToDelete,
                                       const std::string& configPath) {
    if (strokesToDelete.empty()) {
        return true;
    }

    std::ifstream inFile(configPath);
    if (!inFile.is_open()) {
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    std::vector<int> linesToDelete;

    int currentLine = 0;
    while (std::getline(inFile, line)) {
        lines.push_back(line);

        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

        if (trimmed.find("gesture_action") == 0) {
            size_t equalPos = trimmed.find('=');
            if (equalPos != std::string::npos) {
                std::string value = trimmed.substr(equalPos + 1);
                size_t pipePos = value.rfind('|');
                if (pipePos != std::string::npos) {
                    std::string strokeData = value.substr(pipePos + 1);

                    // Trim whitespace
                    size_t start = strokeData.find_first_not_of(" \t\n\r");
                    size_t end = strokeData.find_last_not_of(" \t\n\r");
                    if (start != std::string::npos && end != std::string::npos) {
                        strokeData = strokeData.substr(start, end - start + 1);
                    }

                    // Check if this stroke matches any to delete
                    for (const auto& strokeToDelete : strokesToDelete) {
                        if (strokeData == strokeToDelete) {
                            linesToDelete.push_back(currentLine);
                            break;
                        }
                    }
                }
            }
        }
        currentLine++;
    }
    inFile.close();

    if (!linesToDelete.empty()) {
        // Delete lines in reverse order
        std::sort(linesToDelete.begin(), linesToDelete.end(), std::greater<int>());
        for (int idx : linesToDelete) {
            lines.erase(lines.begin() + idx);
        }

        // Write back
        std::ofstream outFile(configPath);
        if (!outFile.is_open()) {
            return false;
        }

        for (const auto& l : lines) {
            outFile << l << "\n";
        }
        outFile.close();
        return true;
    }

    return false;
}

class DeferredDeletionTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_pendingGestureDeletions.clear();
    }

    void TearDown() override {
        g_pendingGestureDeletions.clear();
    }
};

// Test 1: Verify pending deletions list starts empty
TEST_F(DeferredDeletionTest, InitialStateEmpty) {
    EXPECT_TRUE(g_pendingGestureDeletions.empty());
    EXPECT_EQ(g_pendingGestureDeletions.size(), 0);
}

// Test 2: Add single gesture to pending deletions
TEST_F(DeferredDeletionTest, AddSinglePendingDeletion) {
    std::string strokeData = "0.1,0.2;0.3,0.4;0.5,0.6;";

    g_pendingGestureDeletions.push_back(strokeData);

    EXPECT_EQ(g_pendingGestureDeletions.size(), 1);
    EXPECT_EQ(g_pendingGestureDeletions[0], strokeData);
}

// Test 3: Add multiple gestures to pending deletions
TEST_F(DeferredDeletionTest, AddMultiplePendingDeletions) {
    std::string stroke1 = "0.1,0.2;0.3,0.4;";
    std::string stroke2 = "0.5,0.6;0.7,0.8;";
    std::string stroke3 = "0.9,1.0;1.1,1.2;";

    g_pendingGestureDeletions.push_back(stroke1);
    g_pendingGestureDeletions.push_back(stroke2);
    g_pendingGestureDeletions.push_back(stroke3);

    EXPECT_EQ(g_pendingGestureDeletions.size(), 3);
    EXPECT_EQ(g_pendingGestureDeletions[0], stroke1);
    EXPECT_EQ(g_pendingGestureDeletions[1], stroke2);
    EXPECT_EQ(g_pendingGestureDeletions[2], stroke3);
}

// Test 4: Clear pending deletions
TEST_F(DeferredDeletionTest, ClearPendingDeletions) {
    g_pendingGestureDeletions.push_back("0.1,0.2;");
    g_pendingGestureDeletions.push_back("0.3,0.4;");

    EXPECT_EQ(g_pendingGestureDeletions.size(), 2);

    g_pendingGestureDeletions.clear();

    EXPECT_TRUE(g_pendingGestureDeletions.empty());
    EXPECT_EQ(g_pendingGestureDeletions.size(), 0);
}

// Test 5: Batch delete single gesture from config
TEST_F(DeferredDeletionTest, BatchDeleteSingleGesture) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = hyprctl dispatch workspace +1|0.1,0.2;0.3,0.4;
    gesture_action = hyprctl dispatch workspace -1|0.5,0.6;0.7,0.8;
  }
})";

    configFile.writeContent(configContent);

    std::vector<std::string> toDelete = {"0.1,0.2;0.3,0.4;"};
    bool success = mockBatchDeleteGesturesFromConfig(toDelete, configFile.path);

    EXPECT_TRUE(success);

    std::string result = configFile.readContent();
    EXPECT_TRUE(result.find("0.1,0.2;0.3,0.4;") == std::string::npos);
    EXPECT_TRUE(result.find("0.5,0.6;0.7,0.8;") != std::string::npos);
}

// Test 6: Batch delete multiple gestures from config
TEST_F(DeferredDeletionTest, BatchDeleteMultipleGestures) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = hyprctl dispatch workspace +1|0.1,0.2;0.3,0.4;
    gesture_action = hyprctl dispatch workspace -1|0.5,0.6;0.7,0.8;
    gesture_action = hyprctl dispatch fullscreen|0.9,1.0;1.1,1.2;
  }
})";

    configFile.writeContent(configContent);

    std::vector<std::string> toDelete = {"0.1,0.2;0.3,0.4;", "0.9,1.0;1.1,1.2;"};
    bool success = mockBatchDeleteGesturesFromConfig(toDelete, configFile.path);

    EXPECT_TRUE(success);

    std::string result = configFile.readContent();
    EXPECT_TRUE(result.find("0.1,0.2;0.3,0.4;") == std::string::npos);
    EXPECT_TRUE(result.find("0.9,1.0;1.1,1.2;") == std::string::npos);
    EXPECT_TRUE(result.find("0.5,0.6;0.7,0.8;") != std::string::npos);
}

// Test 7: Batch delete with empty list (no-op)
TEST_F(DeferredDeletionTest, BatchDeleteEmptyList) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = hyprctl dispatch workspace +1|0.1,0.2;0.3,0.4;
  }
})";

    configFile.writeContent(configContent);
    std::string originalContent = configFile.readContent();

    std::vector<std::string> toDelete = {};
    bool success = mockBatchDeleteGesturesFromConfig(toDelete, configFile.path);

    EXPECT_TRUE(success);

    std::string result = configFile.readContent();
    EXPECT_EQ(result, originalContent);
}

// Test 8: Batch delete non-existent gesture (no change)
TEST_F(DeferredDeletionTest, BatchDeleteNonExistentGesture) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = hyprctl dispatch workspace +1|0.1,0.2;0.3,0.4;
  }
})";

    configFile.writeContent(configContent);

    std::vector<std::string> toDelete = {"9.9,9.9;9.9,9.9;"};
    bool success = mockBatchDeleteGesturesFromConfig(toDelete, configFile.path);

    EXPECT_FALSE(success); // Should return false when nothing deleted

    std::string result = configFile.readContent();
    EXPECT_TRUE(result.find("0.1,0.2;0.3,0.4;") != std::string::npos);
}

// Test 9: Verify deletion order doesn't matter
TEST_F(DeferredDeletionTest, DeletionOrderIndependent) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = cmd1|0.1,0.2;
    gesture_action = cmd2|0.3,0.4;
    gesture_action = cmd3|0.5,0.6;
  }
})";

    configFile.writeContent(configContent);

    // Delete in reverse order (should still work correctly)
    std::vector<std::string> toDelete = {"0.5,0.6;", "0.1,0.2;"};
    bool success = mockBatchDeleteGesturesFromConfig(toDelete, configFile.path);

    EXPECT_TRUE(success);

    std::string result = configFile.readContent();
    EXPECT_TRUE(result.find("0.1,0.2;") == std::string::npos);
    EXPECT_TRUE(result.find("0.5,0.6;") == std::string::npos);
    EXPECT_TRUE(result.find("0.3,0.4;") != std::string::npos);
}

// Test 10: Batch delete all gestures
TEST_F(DeferredDeletionTest, BatchDeleteAllGestures) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = cmd1|0.1,0.2;
    gesture_action = cmd2|0.3,0.4;
  }
})";

    configFile.writeContent(configContent);

    std::vector<std::string> toDelete = {"0.1,0.2;", "0.3,0.4;"};
    bool success = mockBatchDeleteGesturesFromConfig(toDelete, configFile.path);

    EXPECT_TRUE(success);

    std::string result = configFile.readContent();
    EXPECT_TRUE(result.find("gesture_action") == std::string::npos);
    EXPECT_TRUE(result.find("mouse_gestures") != std::string::npos); // Section still exists
}

// Test 11: Pending deletions accumulate correctly
TEST_F(DeferredDeletionTest, PendingDeletionsAccumulate) {
    // Simulate multiple delete button clicks
    g_pendingGestureDeletions.push_back("stroke1");
    EXPECT_EQ(g_pendingGestureDeletions.size(), 1);

    g_pendingGestureDeletions.push_back("stroke2");
    EXPECT_EQ(g_pendingGestureDeletions.size(), 2);

    g_pendingGestureDeletions.push_back("stroke3");
    EXPECT_EQ(g_pendingGestureDeletions.size(), 3);

    // Verify all strokes are preserved
    EXPECT_EQ(g_pendingGestureDeletions[0], "stroke1");
    EXPECT_EQ(g_pendingGestureDeletions[1], "stroke2");
    EXPECT_EQ(g_pendingGestureDeletions[2], "stroke3");
}

// Test 12: Duplicate deletions are handled
TEST_F(DeferredDeletionTest, DuplicateDeletionsHandled) {
    std::string stroke = "0.1,0.2;0.3,0.4;";

    g_pendingGestureDeletions.push_back(stroke);
    g_pendingGestureDeletions.push_back(stroke);
    g_pendingGestureDeletions.push_back(stroke);

    EXPECT_EQ(g_pendingGestureDeletions.size(), 3);

    TempConfigFile configFile;
    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = cmd|0.1,0.2;0.3,0.4;
  }
})";

    configFile.writeContent(configContent);

    // Should only delete once even if requested multiple times
    bool success = mockBatchDeleteGesturesFromConfig(g_pendingGestureDeletions, configFile.path);

    EXPECT_TRUE(success);

    std::string result = configFile.readContent();
    EXPECT_TRUE(result.find("0.1,0.2;0.3,0.4;") == std::string::npos);
}
