#pragma once

#include <optional>
#include <string>

// A plain `hyprctl dispatch <dispatcher> [args]` shell command, split into its dispatcher and args.
// Under the Lua config, hyprctl dispatch evaluates its argument as Lua, so such commands are run
// in-process through the dispatcher table instead of through the shell.
struct SHyprctlDispatch {
    std::string dispatcher;
    std::string args;
};

inline std::optional<SHyprctlDispatch> parseHyprctlDispatch(const std::string& command) {
    static const std::string PREFIX = "hyprctl dispatch ";

    const auto start = command.find_first_not_of(" \t");
    if (start == std::string::npos || command.compare(start, PREFIX.size(), PREFIX) != 0)
        return std::nullopt;

    // Anything the shell would interpret has to keep going through the shell
    if (command.find_first_of(";&|$`<>()'\"\\\n*?~#{}") != std::string::npos)
        return std::nullopt;

    std::string rest = command.substr(start + PREFIX.size());
    rest.erase(0, rest.find_first_not_of(" \t"));
    rest.erase(rest.find_last_not_of(" \t") + 1);
    if (rest.empty())
        return std::nullopt;

    const auto space = rest.find_first_of(" \t");
    if (space == std::string::npos)
        return SHyprctlDispatch{rest, ""};

    std::string args = rest.substr(space + 1);
    args.erase(0, args.find_first_not_of(" \t"));
    return SHyprctlDispatch{rest.substr(0, space), args};
}
