#include <gtest/gtest.h>
#include <filesystem>
#include <unistd.h>
#include "lua_gesture_file.hpp"

namespace fs = std::filesystem;
using namespace LuaGestureFile;

static const std::string STROKE_A = "0.1,0.2;0.3,0.4;";
static const std::string STROKE_B = "0.5,0.5;0.9,0.9;";

TEST(LuaGestureFileQuoteTest, RoundTripsSpecialCharacters) {
    const std::string value = "notify-send \"hi\" \\ done|0.1,0.2;\ttab\nline";
    const auto        quoted = quote(value);
    EXPECT_EQ(quoted.front(), '"');
    EXPECT_EQ(quoted.find('\n'), std::string::npos);
    EXPECT_EQ(unquote(quoted), std::optional<std::string>{value});
}

TEST(LuaGestureFileQuoteTest, KeepsUtf8AsIs) {
    EXPECT_EQ(quote("⨯|0,0;"), "\"⨯|0,0;\"");
}

TEST(LuaGestureFileQuoteTest, UnquoteHandlesLuaEscapes) {
    EXPECT_EQ(unquote(R"("a\65\x42\'")"), std::optional<std::string>{"aAB'"});
    EXPECT_FALSE(unquote(R"("unterminated)").has_value());
    EXPECT_FALSE(unquote("no quotes").has_value());
}

TEST(LuaGestureFileEntryTest, ParsesEntryLines) {
    EXPECT_EQ(entryOnLine("    \"cmd|" + STROKE_A + "\","), std::optional<std::string>{"cmd|" + STROKE_A});
    EXPECT_EQ(entryOnLine("\"cmd|" + STROKE_A + "\""), std::optional<std::string>{"cmd|" + STROKE_A});
    EXPECT_EQ(entryOnLine("  \"cmd|x\", -- trailing comment"), std::optional<std::string>{"cmd|x"});
}

TEST(LuaGestureFileEntryTest, IgnoresNonEntryLines) {
    EXPECT_FALSE(entryOnLine("return {").has_value());
    EXPECT_FALSE(entryOnLine("}").has_value());
    EXPECT_FALSE(entryOnLine("    -- \"cmd|" + STROKE_A + "\",").has_value());
    EXPECT_FALSE(entryOnLine("    \"a\" .. \"b\",").has_value());
}

TEST(LuaGestureFileEntryTest, DistinguishesPlainCommentsFromCommentedEntries) {
    EXPECT_TRUE(isPlainComment("    -- ascii art"));
    EXPECT_FALSE(isPlainComment("    -- \"cmd|" + STROKE_A + "\","));
    EXPECT_FALSE(isPlainComment("    \"cmd|" + STROKE_A + "\","));
}

TEST(LuaGestureFileEntryTest, ConvertsAsciiArtToLuaComments) {
    EXPECT_EQ(toLuaComment("#  ..*.."), "--  ..*..");
    EXPECT_EQ(toLuaComment("plain"), "-- plain");
}

TEST(LuaGestureFileEntryTest, StrokeOfUsesLastPipe) {
    EXPECT_EQ(strokeOf("a | b|" + STROKE_A), STROKE_A);
    EXPECT_EQ(strokeOf("no pipe"), "");
}

TEST(LuaGestureFileEditTest, InsertsBeforeClosingBrace) {
    auto lines = emptyFileLines();
    ASSERT_TRUE(insertEntry(lines, "cmd|" + STROKE_A));
    ASSERT_TRUE(insertEntry(lines, "other|" + STROKE_B, {"-- art"}));

    const std::vector<std::string> expected = {
        "return {",
        "    \"cmd|" + STROKE_A + "\",",
        "    -- art",
        "    \"other|" + STROKE_B + "\",",
        "}",
    };
    EXPECT_EQ(lines, expected);
}

TEST(LuaGestureFileEditTest, InsertFailsWithoutClosingBrace) {
    std::vector<std::string> lines = {"return {"};
    EXPECT_FALSE(insertEntry(lines, "cmd|" + STROKE_A));
}

TEST(LuaGestureFileEditTest, RemovesMatchingEntryAndItsArtButKeepsComments) {
    std::vector<std::string> lines = {
        "-- header comment",
        "return {",
        "    \"keep|" + STROKE_B + "\",",
        "    -- \"commented out|" + STROKE_A + "\",",
        "    -- art line 1",
        "    -- art line 2",
        "    \"drop|" + STROKE_A + "\",",
        "}",
    };

    EXPECT_EQ(removeEntries(lines, {STROKE_A}), 1u);

    const std::vector<std::string> expected = {
        "-- header comment",
        "return {",
        "    \"keep|" + STROKE_B + "\",",
        "    -- \"commented out|" + STROKE_A + "\",",
        "}",
    };
    EXPECT_EQ(lines, expected);
}

TEST(LuaGestureFileEditTest, RemoveMatchesNormalizedNegativeZero) {
    std::vector<std::string> lines = {"return {", "    \"cmd|-0.000000,0.5;1.0,0.5;\",", "}"};
    EXPECT_EQ(removeEntries(lines, {"0.000000,0.5;1.0,0.5;"}), 1u);
    EXPECT_EQ(lines, emptyFileLines());
}

TEST(LuaGestureFileEditTest, RemoveWithoutMatchLeavesLinesUnchanged) {
    std::vector<std::string> lines    = {"return {", "    \"cmd|" + STROKE_B + "\",", "}"};
    const auto               original = lines;
    EXPECT_EQ(removeEntries(lines, {STROKE_A}), 0u);
    EXPECT_EQ(lines, original);
}

class LuaGestureFileIoTest : public ::testing::Test {
  protected:
    fs::path dir;
    fs::path path;

    void SetUp() override {
        dir  = fs::temp_directory_path() / ("mouse_gestures_lua_test_" + std::to_string(getpid()));
        path = dir / "mouse_gestures.lua";
        fs::create_directories(dir);
    }

    void TearDown() override {
        fs::remove_all(dir);
    }
};

TEST_F(LuaGestureFileIoTest, AppendCreatesFileAndReadsBack) {
    ASSERT_TRUE(appendGesture(path, "hyprctl dispatch workspace r+1|" + STROKE_A));
    ASSERT_TRUE(appendGesture(path, "say \"hi\"|" + STROKE_B));

    const auto lines = readLines(path);
    ASSERT_TRUE(lines.has_value());
    const std::vector<std::string> expected = {
        "return {",
        "    \"hyprctl dispatch workspace r+1|" + STROKE_A + "\",",
        "    \"say \\\"hi\\\"|" + STROKE_B + "\",",
        "}",
    };
    EXPECT_EQ(*lines, expected);
    EXPECT_FALSE(fs::exists(path.string() + ".tmp"));
}

TEST_F(LuaGestureFileIoTest, DeleteRemovesEntryAndKeepsComments) {
    ASSERT_TRUE(writeLinesAtomic(path, {"return {", "    -- \"old|" + STROKE_B + "\",", "    \"cmd|" + STROKE_A + "\",", "}"}));

    ASSERT_TRUE(deleteGestures(path, {STROKE_A}));

    const std::vector<std::string> expected = {"return {", "    -- \"old|" + STROKE_B + "\",", "}"};
    EXPECT_EQ(readLines(path), std::optional<std::vector<std::string>>{expected});
}

TEST_F(LuaGestureFileIoTest, DeleteReportsMissingFileOrEntry) {
    EXPECT_FALSE(deleteGestures(path, {STROKE_A}));
    ASSERT_TRUE(appendGesture(path, "cmd|" + STROKE_B));
    EXPECT_FALSE(deleteGestures(path, {STROKE_A}));
    EXPECT_TRUE(deleteGestures(path, {}));
}
