#include <gtest/gtest.h>
#include "dispatch_args.hpp"

TEST(DispatchArgsTest, LetterArgMapsToLetter) {
    EXPECT_EQ(letterFromDispatchArg("letter-A"), std::optional<std::string>{"A"});
    EXPECT_EQ(letterFromDispatchArg("letter-z"), std::optional<std::string>{"z"});
}

TEST(DispatchArgsTest, ModeArgsAreNotLetters) {
    EXPECT_FALSE(letterFromDispatchArg("move").has_value());
    EXPECT_FALSE(letterFromDispatchArg("select").has_value());
    EXPECT_FALSE(letterFromDispatchArg("").has_value());
}

TEST(DispatchArgsTest, MalformedLetterArgsAreRejected) {
    EXPECT_FALSE(letterFromDispatchArg("letter-").has_value());
    EXPECT_FALSE(letterFromDispatchArg("letter-AB").has_value());
    EXPECT_FALSE(letterFromDispatchArg("no-mouse-letter-A").has_value());
}
