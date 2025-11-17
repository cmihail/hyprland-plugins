#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

// Mock the global synchronization primitives
std::atomic<int> g_fileWriteCount{0};
std::mutex g_fileWriteMutex;

// Mock pending lists
std::vector<std::string> g_pendingGestureAdditions;
std::vector<std::string> g_pendingGestureDeletions;

// Mock config write function
void addGestureToConfig(const std::string& strokeData) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void batchDeleteGesturesFromConfig(const std::vector<std::string>& strokes) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Function under test: processPendingGestureAdditions
void processPendingGestureAdditions() {
    if (g_pendingGestureAdditions.empty()) {
        return;
    }
    std::vector<std::string> additionsToProcess = g_pendingGestureAdditions;
    g_pendingGestureAdditions.clear();
    g_fileWriteCount++;
    std::thread([additionsToProcess]() {
        try {
            std::lock_guard<std::mutex> lock(g_fileWriteMutex);
            for (const auto& strokeData : additionsToProcess) {
                addGestureToConfig(strokeData);
            }
        } catch (...) {
            // Silently catch errors
        }
        g_fileWriteCount--;
    }).detach();
}

// Function under test: processPendingGestureDeletions
void processPendingGestureDeletions() {
    if (g_pendingGestureDeletions.empty()) {
        return;
    }
    std::vector<std::string> deletionsToProcess = g_pendingGestureDeletions;
    g_pendingGestureDeletions.clear();
    g_fileWriteCount++;
    std::thread([deletionsToProcess]() {
        try {
            std::lock_guard<std::mutex> lock(g_fileWriteMutex);
            batchDeleteGesturesFromConfig(deletionsToProcess);
        } catch (...) {
            // Silently catch errors
        }
        g_fileWriteCount--;
    }).detach();
}

// Function under test: wait for file writes before entering record mode
bool waitForFileWrites() {
    constexpr int MAX_WAIT_MS = 5000;
    constexpr int SLEEP_MS = 10;
    int totalWaitMs = 0;
    while (g_fileWriteCount > 0 && totalWaitMs < MAX_WAIT_MS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
        totalWaitMs += SLEEP_MS;
    }
    return g_fileWriteCount == 0;
}

TEST(FileWriteSyncTest, CounterIncrementsOnWrite) {
    g_fileWriteCount = 0;
    g_pendingGestureAdditions = {"stroke1"};

    processPendingGestureAdditions();

    // Should increment immediately
    EXPECT_EQ(g_fileWriteCount.load(), 1);

    // Wait for completion
    waitForFileWrites();
    EXPECT_EQ(g_fileWriteCount.load(), 0);
}

TEST(FileWriteSyncTest, WaitBlocksUntilWriteCompletes) {
    g_fileWriteCount = 0;
    g_pendingGestureAdditions = {"stroke1", "stroke2"};

    auto start = std::chrono::steady_clock::now();

    processPendingGestureAdditions();

    // Wait should block until write completes
    bool completed = waitForFileWrites();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    EXPECT_TRUE(completed);
    EXPECT_EQ(g_fileWriteCount.load(), 0);
    EXPECT_GE(duration, 50);  // Should wait at least 50ms for the mock write
}

TEST(FileWriteSyncTest, MultipleWritesTrackedCorrectly) {
    g_fileWriteCount = 0;
    g_pendingGestureAdditions = {"stroke1"};
    g_pendingGestureDeletions = {"stroke2"};

    processPendingGestureAdditions();
    processPendingGestureDeletions();

    // Should have 2 concurrent writes
    EXPECT_EQ(g_fileWriteCount.load(), 2);

    // Wait for both to complete
    waitForFileWrites();
    EXPECT_EQ(g_fileWriteCount.load(), 0);
}

TEST(FileWriteSyncTest, TimeoutPreventsInfiniteWait) {
    g_fileWriteCount = 1;  // Simulate a stuck write

    auto start = std::chrono::steady_clock::now();

    // This should timeout after 5 seconds
    bool completed = waitForFileWrites();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    EXPECT_FALSE(completed);
    EXPECT_GE(duration, 5000);
    EXPECT_LT(duration, 5500);  // Should not wait much longer than timeout

    // Reset for other tests
    g_fileWriteCount = 0;
}

TEST(FileWriteSyncTest, EmptyPendingListsDoNotIncrementCounter) {
    g_fileWriteCount = 0;
    g_pendingGestureAdditions.clear();
    g_pendingGestureDeletions.clear();

    processPendingGestureAdditions();
    processPendingGestureDeletions();

    EXPECT_EQ(g_fileWriteCount.load(), 0);
}
