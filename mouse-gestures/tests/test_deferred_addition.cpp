#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstdlib>

// Mock the global state from main.cpp (anonymous namespace to avoid multiple definition)
namespace {
std::vector<std::string> g_pendingGestureAdditions;
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

// Helper to find mouse_gestures section in config
int findMouseGesturesSectionEnd(const std::vector<std::string>& lines) {
    int mouseGesturesSectionEnd = -1;
    int currentLine = 0;
    int braceDepth = 0;
    bool foundMouseGestures = false;

    for (const auto& line : lines) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

        if (trimmed.find("mouse_gestures") == 0 && trimmed.find("{") != std::string::npos) {
            foundMouseGestures = true;
            braceDepth = 1;
        } else if (foundMouseGestures && braceDepth > 0) {
            for (char c : trimmed) {
                if (c == '{') braceDepth++;
                else if (c == '}') braceDepth--;
            }
            if (braceDepth == 0) {
                mouseGesturesSectionEnd = currentLine;
                break;
            }
        }
        currentLine++;
    }
    return mouseGesturesSectionEnd;
}

// Helper to write lines to config file
bool writeLinesToFile(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream outFile(path);
    if (!outFile.is_open()) {
        return false;
    }
    for (const auto& l : lines) {
        outFile << l << "\n";
    }
    outFile.close();
    return true;
}

// Mock implementation of addGestureToConfig (simplified for testing)
bool mockAddGestureToConfig(const std::string& strokeData, const std::string& configPath) {
    std::ifstream inFile(configPath);
    if (!inFile.is_open()) {
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    int sectionEnd = findMouseGesturesSectionEnd(lines);
    if (sectionEnd > 0) {
        std::string gestureAction = "    gesture_action = test_cmd|" + strokeData;
        lines.insert(lines.begin() + sectionEnd, gestureAction);
        return writeLinesToFile(configPath, lines);
    }

    return false;
}

// Mock implementation of normalizeStrokeData
std::string normalizeStrokeData(const std::string& stroke) {
    std::string normalized = stroke;
    size_t pos = 0;
    while ((pos = normalized.find("-0.000000", pos)) != std::string::npos) {
        normalized.replace(pos, 9, "0.000000");
        pos += 8;
    }
    return normalized;
}

class DeferredAdditionTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_pendingGestureAdditions.clear();
        g_pendingGestureDeletions.clear();
    }

    void TearDown() override {
        g_pendingGestureAdditions.clear();
        g_pendingGestureDeletions.clear();
    }
};

// Test 1: Verify pending additions list starts empty
TEST_F(DeferredAdditionTest, InitialStateEmpty) {
    EXPECT_TRUE(g_pendingGestureAdditions.empty());
    EXPECT_EQ(g_pendingGestureAdditions.size(), 0);
}

// Test 2: Add single gesture to pending additions
TEST_F(DeferredAdditionTest, AddSinglePendingAddition) {
    std::string strokeData = "0.1,0.2;0.3,0.4;0.5,0.6;";

    g_pendingGestureAdditions.push_back(strokeData);

    EXPECT_EQ(g_pendingGestureAdditions.size(), 1);
    EXPECT_EQ(g_pendingGestureAdditions[0], strokeData);
}

// Test 3: Add multiple gestures to pending additions
TEST_F(DeferredAdditionTest, AddMultiplePendingAdditions) {
    std::string stroke1 = "0.1,0.2;0.3,0.4;";
    std::string stroke2 = "0.5,0.6;0.7,0.8;";
    std::string stroke3 = "0.9,1.0;1.1,1.2;";

    g_pendingGestureAdditions.push_back(stroke1);
    g_pendingGestureAdditions.push_back(stroke2);
    g_pendingGestureAdditions.push_back(stroke3);

    EXPECT_EQ(g_pendingGestureAdditions.size(), 3);
    EXPECT_EQ(g_pendingGestureAdditions[0], stroke1);
    EXPECT_EQ(g_pendingGestureAdditions[1], stroke2);
    EXPECT_EQ(g_pendingGestureAdditions[2], stroke3);
}

// Test 4: Clear pending additions
TEST_F(DeferredAdditionTest, ClearPendingAdditions) {
    g_pendingGestureAdditions.push_back("0.1,0.2;");
    g_pendingGestureAdditions.push_back("0.3,0.4;");

    EXPECT_EQ(g_pendingGestureAdditions.size(), 2);

    g_pendingGestureAdditions.clear();

    EXPECT_TRUE(g_pendingGestureAdditions.empty());
    EXPECT_EQ(g_pendingGestureAdditions.size(), 0);
}

// Test 5: Add single gesture to config file
TEST_F(DeferredAdditionTest, AddSingleGestureToConfig) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = hyprctl dispatch workspace +1|0.1,0.2;0.3,0.4;
  }
})";

    configFile.writeContent(configContent);

    std::string newStroke = "0.5,0.6;0.7,0.8;";
    bool success = mockAddGestureToConfig(newStroke, configFile.path);

    EXPECT_TRUE(success);

    std::string result = configFile.readContent();
    EXPECT_TRUE(result.find("0.5,0.6;0.7,0.8;") != std::string::npos);
    EXPECT_TRUE(result.find("0.1,0.2;0.3,0.4;") != std::string::npos); // Original still there
}

// Test 6: Add multiple gestures to config file
TEST_F(DeferredAdditionTest, AddMultipleGesturesToConfig) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = original|0.1,0.2;
  }
})";

    configFile.writeContent(configContent);

    // Add first gesture
    mockAddGestureToConfig("0.3,0.4;", configFile.path);

    // Read back and add second gesture
    std::string intermediate = configFile.readContent();
    configFile.writeContent(intermediate);
    mockAddGestureToConfig("0.5,0.6;", configFile.path);

    std::string result = configFile.readContent();
    EXPECT_TRUE(result.find("0.1,0.2;") != std::string::npos);
    EXPECT_TRUE(result.find("0.3,0.4;") != std::string::npos);
    EXPECT_TRUE(result.find("0.5,0.6;") != std::string::npos);
}

// Test 7: Pending additions accumulate correctly
TEST_F(DeferredAdditionTest, PendingAdditionsAccumulate) {
    // Simulate recording multiple gestures in record mode
    g_pendingGestureAdditions.push_back("stroke1");
    EXPECT_EQ(g_pendingGestureAdditions.size(), 1);

    g_pendingGestureAdditions.push_back("stroke2");
    EXPECT_EQ(g_pendingGestureAdditions.size(), 2);

    g_pendingGestureAdditions.push_back("stroke3");
    EXPECT_EQ(g_pendingGestureAdditions.size(), 3);

    // Verify all strokes are preserved
    EXPECT_EQ(g_pendingGestureAdditions[0], "stroke1");
    EXPECT_EQ(g_pendingGestureAdditions[1], "stroke2");
    EXPECT_EQ(g_pendingGestureAdditions[2], "stroke3");
}

// Test 8: Conflict resolution - add then delete same gesture
TEST_F(DeferredAdditionTest, AddThenDeleteSameGesture) {
    std::string stroke = "0.1,0.2;0.3,0.4;";

    // Add gesture
    g_pendingGestureAdditions.push_back(stroke);
    EXPECT_EQ(g_pendingGestureAdditions.size(), 1);

    // Delete same gesture
    g_pendingGestureDeletions.push_back(stroke);
    EXPECT_EQ(g_pendingGestureDeletions.size(), 1);

    // Simulate conflict resolution
    std::vector<std::string> filteredAdditions;
    for (const auto& addition : g_pendingGestureAdditions) {
        bool foundInDeletions = false;
        for (size_t i = 0; i < g_pendingGestureDeletions.size(); ++i) {
            if (normalizeStrokeData(addition) == normalizeStrokeData(g_pendingGestureDeletions[i])) {
                g_pendingGestureDeletions.erase(g_pendingGestureDeletions.begin() + i);
                foundInDeletions = true;
                break;
            }
        }
        if (!foundInDeletions) {
            filteredAdditions.push_back(addition);
        }
    }
    g_pendingGestureAdditions = filteredAdditions;

    // Both lists should be empty after conflict resolution
    EXPECT_TRUE(g_pendingGestureAdditions.empty());
    EXPECT_TRUE(g_pendingGestureDeletions.empty());
}

// Test 9: Conflict resolution - add multiple, delete some
TEST_F(DeferredAdditionTest, AddMultipleDeleteSome) {
    g_pendingGestureAdditions.push_back("stroke1");
    g_pendingGestureAdditions.push_back("stroke2");
    g_pendingGestureAdditions.push_back("stroke3");

    // Delete stroke2
    g_pendingGestureDeletions.push_back("stroke2");

    // Simulate conflict resolution
    std::vector<std::string> filteredAdditions;
    for (const auto& addition : g_pendingGestureAdditions) {
        bool foundInDeletions = false;
        for (size_t i = 0; i < g_pendingGestureDeletions.size(); ++i) {
            if (normalizeStrokeData(addition) == normalizeStrokeData(g_pendingGestureDeletions[i])) {
                g_pendingGestureDeletions.erase(g_pendingGestureDeletions.begin() + i);
                foundInDeletions = true;
                break;
            }
        }
        if (!foundInDeletions) {
            filteredAdditions.push_back(addition);
        }
    }
    g_pendingGestureAdditions = filteredAdditions;

    // Should have stroke1 and stroke3 remaining
    EXPECT_EQ(g_pendingGestureAdditions.size(), 2);
    EXPECT_EQ(g_pendingGestureAdditions[0], "stroke1");
    EXPECT_EQ(g_pendingGestureAdditions[1], "stroke3");
    EXPECT_TRUE(g_pendingGestureDeletions.empty());
}

// Test 10: Normalize stroke data handles -0.000000
TEST_F(DeferredAdditionTest, NormalizeStrokeData) {
    std::string stroke1 = "0.1,-0.000000;0.3,0.4;";
    std::string stroke2 = "0.1,0.000000;0.3,0.4;";

    std::string normalized1 = normalizeStrokeData(stroke1);
    std::string normalized2 = normalizeStrokeData(stroke2);

    EXPECT_EQ(normalized1, normalized2);
    EXPECT_EQ(normalized1, "0.1,0.000000;0.3,0.4;");
}

// Test 11: Conflict resolution with normalized strokes
TEST_F(DeferredAdditionTest, ConflictResolutionWithNormalization) {
    std::string stroke1 = "0.1,-0.000000;0.3,0.4;";
    std::string stroke2 = "0.1,0.000000;0.3,0.4;";

    g_pendingGestureAdditions.push_back(stroke1);
    g_pendingGestureDeletions.push_back(stroke2);

    // Should match after normalization
    std::vector<std::string> filteredAdditions;
    for (const auto& addition : g_pendingGestureAdditions) {
        bool foundInDeletions = false;
        for (size_t i = 0; i < g_pendingGestureDeletions.size(); ++i) {
            if (normalizeStrokeData(addition) == normalizeStrokeData(g_pendingGestureDeletions[i])) {
                g_pendingGestureDeletions.erase(g_pendingGestureDeletions.begin() + i);
                foundInDeletions = true;
                break;
            }
        }
        if (!foundInDeletions) {
            filteredAdditions.push_back(addition);
        }
    }
    g_pendingGestureAdditions = filteredAdditions;

    EXPECT_TRUE(g_pendingGestureAdditions.empty());
    EXPECT_TRUE(g_pendingGestureDeletions.empty());
}

// Test 12: Empty additions list with no-op
TEST_F(DeferredAdditionTest, EmptyAdditionsNoOp) {
    TempConfigFile configFile;

    std::string configContent = R"(plugin {
  mouse_gestures {
    gesture_action = original|0.1,0.2;
  }
})";

    configFile.writeContent(configContent);
    std::string originalContent = configFile.readContent();

    // Process empty additions (should be no-op)
    for (const auto& strokeData : g_pendingGestureAdditions) {
        mockAddGestureToConfig(strokeData, configFile.path);
    }

    std::string result = configFile.readContent();
    EXPECT_EQ(result, originalContent);
}

// Test 13: Verify gesture order is preserved
TEST_F(DeferredAdditionTest, GestureOrderPreserved) {
    g_pendingGestureAdditions.push_back("first");
    g_pendingGestureAdditions.push_back("second");
    g_pendingGestureAdditions.push_back("third");

    EXPECT_EQ(g_pendingGestureAdditions[0], "first");
    EXPECT_EQ(g_pendingGestureAdditions[1], "second");
    EXPECT_EQ(g_pendingGestureAdditions[2], "third");

    // Order matters for when they're written to config
    std::vector<std::string> processingOrder;
    for (const auto& addition : g_pendingGestureAdditions) {
        processingOrder.push_back(addition);
    }

    EXPECT_EQ(processingOrder[0], "first");
    EXPECT_EQ(processingOrder[1], "second");
    EXPECT_EQ(processingOrder[2], "third");
}
