#include <gtest/gtest.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#include <chrono>

namespace fs = std::filesystem;

class AtomicWritesTest : public ::testing::Test {
protected:
    std::string testDir;
    std::string testFilePath;

    void SetUp() override {
        // Create temp test directory
        testDir = fs::temp_directory_path() / "atomic_writes_test";
        fs::create_directories(testDir);
        testFilePath = testDir + "/test_config.conf";
    }

    void TearDown() override {
        // Clean up test directory
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    // Simulate atomic write operation
    bool atomicWrite(const std::string& path, const std::string& content) {
        std::string tempPath = path + ".tmp";

        // Write to temp file
        std::ofstream outFile(tempPath);
        if (!outFile.is_open()) {
            return false;
        }
        outFile << content;
        outFile.close();

        // Atomically rename
        if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
            std::remove(tempPath.c_str());
            return false;
        }
        return true;
    }

    // Simulate non-atomic write (for comparison)
    bool directWrite(const std::string& path, const std::string& content) {
        std::ofstream outFile(path);
        if (!outFile.is_open()) {
            return false;
        }
        outFile << content;
        outFile.close();
        return true;
    }

    std::string readFile(const std::string& path) {
        std::ifstream inFile(path);
        if (!inFile.is_open()) {
            return "";
        }
        std::string content((std::istreambuf_iterator<char>(inFile)),
                           std::istreambuf_iterator<char>());
        return content;
    }
};

// Test 1: Atomic write creates temp file then renames
TEST_F(AtomicWritesTest, AtomicWriteCreatesAndRenames) {
    std::string content = "test content line 1\ntest content line 2\n";

    EXPECT_TRUE(atomicWrite(testFilePath, content));
    EXPECT_TRUE(fs::exists(testFilePath));
    EXPECT_FALSE(fs::exists(testFilePath + ".tmp"));  // Temp file removed

    std::string readContent = readFile(testFilePath);
    EXPECT_EQ(readContent, content);
}

// Test 2: Atomic write overwrites existing file
TEST_F(AtomicWritesTest, AtomicWriteOverwritesExisting) {
    // Create initial file
    directWrite(testFilePath, "old content\n");
    EXPECT_EQ(readFile(testFilePath), "old content\n");

    // Overwrite with atomic write
    std::string newContent = "new content\n";
    EXPECT_TRUE(atomicWrite(testFilePath, newContent));
    EXPECT_EQ(readFile(testFilePath), newContent);
}

// Test 3: Temp file cleaned up on failure
TEST_F(AtomicWritesTest, TempFileCleanedUpOnFailure) {
    std::string tempPath = testFilePath + ".tmp";

    // Create temp file manually to simulate partial write
    directWrite(tempPath, "temp content\n");
    EXPECT_TRUE(fs::exists(tempPath));

    // Try atomic write to non-existent directory (should fail)
    std::string badPath = testDir + "/nonexistent/file.conf";
    std::string badTempPath = badPath + ".tmp";

    directWrite(badTempPath, "test\n");
    if (std::rename(badTempPath.c_str(), badPath.c_str()) != 0) {
        std::remove(badTempPath.c_str());
    }

    // Temp file should be cleaned up
    EXPECT_FALSE(fs::exists(badTempPath));
}

// Test 4: Concurrent atomic writes don't corrupt file
TEST_F(AtomicWritesTest, ConcurrentAtomicWritesSafe) {
    const int numThreads = 5;
    const int numWrites = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Each thread performs multiple atomic writes
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, numWrites, &successCount]() {
            for (int i = 0; i < numWrites; ++i) {
                std::string content = "thread_" + std::to_string(t) +
                                    "_write_" + std::to_string(i) + "\n";
                if (atomicWrite(testFilePath, content)) {
                    successCount++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // File should exist and be readable
    EXPECT_TRUE(fs::exists(testFilePath));
    std::string finalContent = readFile(testFilePath);
    EXPECT_FALSE(finalContent.empty());

    // At least some writes should have succeeded
    EXPECT_GT(successCount.load(), 0);

    // Content should be valid (from one of the threads)
    EXPECT_TRUE(finalContent.find("thread_") != std::string::npos);
    EXPECT_TRUE(finalContent.find("write_") != std::string::npos);
}

// Test 5: Atomic write preserves file content integrity
TEST_F(AtomicWritesTest, AtomicWritePreservesIntegrity) {
    std::string longContent;
    for (int i = 0; i < 1000; ++i) {
        longContent += "line_" + std::to_string(i) + "\n";
    }

    EXPECT_TRUE(atomicWrite(testFilePath, longContent));
    std::string readContent = readFile(testFilePath);

    // Content should match exactly
    EXPECT_EQ(readContent.size(), longContent.size());
    EXPECT_EQ(readContent, longContent);
}

// Test 6: Multiple atomic writes in sequence
TEST_F(AtomicWritesTest, SequentialAtomicWrites) {
    std::vector<std::string> contents = {
        "first write\n",
        "second write\n",
        "third write\n"
    };

    for (const auto& content : contents) {
        EXPECT_TRUE(atomicWrite(testFilePath, content));
        EXPECT_EQ(readFile(testFilePath), content);
        EXPECT_FALSE(fs::exists(testFilePath + ".tmp"));
    }
}

// Test 7: Atomic write with empty content
TEST_F(AtomicWritesTest, AtomicWriteEmptyContent) {
    // Write non-empty first
    directWrite(testFilePath, "some content\n");

    // Overwrite with empty
    EXPECT_TRUE(atomicWrite(testFilePath, ""));
    EXPECT_EQ(readFile(testFilePath), "");
}

// Test 8: Rename operation is atomic (no partial content visible)
TEST_F(AtomicWritesTest, RenameIsAtomic) {
    std::string content = "complete content that should appear atomically\n";

    // Multiple readers trying to read during write
    std::atomic<bool> writing{true};
    std::atomic<int> partialReads{0};
    std::atomic<int> emptyReads{0};
    std::atomic<int> completeReads{0};

    std::thread writer([this, &content, &writing]() {
        for (int i = 0; i < 20; ++i) {
            atomicWrite(testFilePath, content);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        writing = false;
    });

    std::vector<std::thread> readers;
    for (int i = 0; i < 3; ++i) {
        readers.emplace_back([this, &content, &writing, &partialReads,
                             &emptyReads, &completeReads]() {
            while (writing) {
                std::string read = readFile(testFilePath);
                if (read.empty()) {
                    emptyReads++;
                } else if (read == content) {
                    completeReads++;
                } else {
                    partialReads++;  // Should never happen with atomic writes
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    writer.join();
    for (auto& reader : readers) {
        reader.join();
    }

    // With atomic writes, we should never see partial content
    EXPECT_EQ(partialReads.load(), 0);
    EXPECT_GT(completeReads.load(), 0);
}
