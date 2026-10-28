#pragma once

#include <optional>
#include <string>

// "letter-A" -> "A", the Lua equivalent of the no-mouse-letter-A dispatcher
inline std::optional<std::string> letterFromDispatchArg(const std::string& arg) {
    static const std::string PREFIX = "letter-";
    if (!arg.starts_with(PREFIX) || arg.size() != PREFIX.size() + 1)
        return std::nullopt;
    return arg.substr(PREFIX.size());
}
