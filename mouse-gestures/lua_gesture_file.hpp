#pragma once

// Persistence of recorded gestures for the Lua config. The file is a Lua module of the form
//
//   return {
//       "command|x,y;x,y;...",
//       -- "commented out command|x,y;...",
//   }
//
// with one double-quoted entry per line, which the config passes to
// hl.plugin.mouse_gestures.add_gesture.

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace LuaGestureFile {

// Normalize stroke data by replacing -0.000000 with 0.000000
inline std::string normalizeStrokeData(const std::string& stroke) {
    std::string normalized = stroke;
    size_t pos = 0;
    while ((pos = normalized.find("-0.000000", pos)) != std::string::npos) {
        normalized.replace(pos, 9, "0.000000");
        pos += 8;
    }
    return normalized;
}

inline std::string trim(std::string_view s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
        return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return std::string{s.substr(start, end - start + 1)};
}

inline std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20 || c == 0x7f) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\%03d", c);
                    out += buf;
                } else
                    out += static_cast<char>(c);
        }
    }
    out += "\"";
    return out;
}

// Parses a double-quoted Lua string literal at the start of s. On success, consumed is set to the
// number of characters of s that the literal spans.
inline std::optional<std::string> unquote(std::string_view s, size_t* consumed = nullptr) {
    if (s.empty() || s[0] != '"')
        return std::nullopt;

    std::string out;
    for (size_t i = 1; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '"') {
            if (consumed)
                *consumed = i + 1;
            return out;
        }
        if (c == '\n')
            return std::nullopt;
        if (c != '\\') {
            out += c;
            continue;
        }

        if (++i >= s.size())
            return std::nullopt;

        const char e = s[i];
        switch (e) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'a': out += '\a'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'v': out += '\v'; break;
            case '\\': out += '\\'; break;
            case '"': out += '"'; break;
            case '\'': out += '\''; break;
            case 'x': {
                if (i + 2 >= s.size())
                    return std::nullopt;
                const std::string hex{s.substr(i + 1, 2)};
                if (hex.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
                    return std::nullopt;
                out += static_cast<char>(std::stoi(hex, nullptr, 16));
                i += 2;
                break;
            }
            default: {
                if (e < '0' || e > '9')
                    return std::nullopt;
                int    value  = 0;
                size_t digits = 0;
                while (digits < 3 && i < s.size() && s[i] >= '0' && s[i] <= '9') {
                    value = value * 10 + (s[i] - '0');
                    ++i;
                    ++digits;
                }
                --i;
                if (value > 255)
                    return std::nullopt;
                out += static_cast<char>(value);
            }
        }
    }

    return std::nullopt;
}

// The gesture string on an active entry line ("...",), or nullopt for any other line.
inline std::optional<std::string> entryOnLine(const std::string& line) {
    const std::string trimmed = trim(line);
    size_t            consumed = 0;
    auto              value    = unquote(trimmed, &consumed);
    if (!value)
        return std::nullopt;

    std::string rest = trim(std::string_view{trimmed}.substr(consumed));
    if (rest.starts_with(","))
        rest = trim(std::string_view{rest}.substr(1));
    if (!rest.empty() && !rest.starts_with("--"))
        return std::nullopt;

    return value;
}

// A "-- ..." comment line that is not a commented-out entry.
inline bool isPlainComment(const std::string& line) {
    const std::string trimmed = trim(line);
    if (!trimmed.starts_with("--"))
        return false;
    return !entryOnLine(trimmed.substr(2)).has_value();
}

// ASCII art lines are rendered as "# ..." for the legacy config.
inline std::string toLuaComment(const std::string& line) {
    return line.starts_with("#") ? "--" + line.substr(1) : "-- " + line;
}

inline std::string strokeOf(const std::string& entry) {
    const auto pipePos = entry.rfind('|');
    if (pipePos == std::string::npos)
        return "";
    return trim(std::string_view{entry}.substr(pipePos + 1));
}

inline std::vector<std::string> emptyFileLines() {
    return {"return {", "}"};
}

// Inserts the entry, preceded by the comment lines, before the closing brace of the returned table.
inline bool insertEntry(std::vector<std::string>& lines, const std::string& entry, const std::vector<std::string>& commentLines = {}) {
    for (size_t i = lines.size(); i-- > 0;) {
        if (!trim(lines[i]).starts_with("}"))
            continue;

        std::vector<std::string> block;
        for (const auto& comment : commentLines)
            block.push_back("    " + comment);
        block.push_back("    " + quote(entry) + ",");
        lines.insert(lines.begin() + i, block.begin(), block.end());
        return true;
    }
    return false;
}

// Removes active entries whose stroke matches one of strokesToDelete, together with the plain
// comment lines directly above them. Commented-out entries are kept. Returns the number of entries removed.
inline size_t removeEntries(std::vector<std::string>& lines, const std::vector<std::string>& strokesToDelete) {
    std::vector<std::string> normalizedToDelete;
    for (const auto& s : strokesToDelete)
        normalizedToDelete.push_back(normalizeStrokeData(s));

    std::vector<bool> drop(lines.size(), false);
    size_t            removed = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto entry = entryOnLine(lines[i]);
        if (!entry)
            continue;

        const auto stroke = normalizeStrokeData(strokeOf(*entry));
        if (stroke.empty() || std::find(normalizedToDelete.begin(), normalizedToDelete.end(), stroke) == normalizedToDelete.end())
            continue;

        drop[i] = true;
        ++removed;
        for (size_t j = i; j-- > 0 && isPlainComment(lines[j]);)
            drop[j] = true;
    }

    std::vector<std::string> kept;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!drop[i])
            kept.push_back(lines[i]);
    }
    lines = std::move(kept);
    return removed;
}

inline std::optional<std::vector<std::string>> readLines(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open())
        return std::nullopt;

    std::vector<std::string> lines;
    std::string              line;
    while (std::getline(in, line))
        lines.push_back(line);
    return lines;
}

// Writes to a temporary file and renames it over path so readers never see a partial file.
inline bool writeLinesAtomic(const std::string& path, const std::vector<std::string>& lines) {
    const std::string tempPath = path + ".tmp";
    {
        std::ofstream out(tempPath);
        if (!out.is_open())
            return false;
        for (const auto& l : lines)
            out << l << "\n";
        if (!out.good()) {
            out.close();
            std::remove(tempPath.c_str());
            return false;
        }
    }

    if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
        std::remove(tempPath.c_str());
        return false;
    }
    return true;
}

// Adds the entry to the file, creating the file if it does not exist.
inline bool appendGesture(const std::string& path, const std::string& entry, const std::vector<std::string>& commentLines = {}) {
    auto lines = readLines(path).value_or(emptyFileLines());
    if (!insertEntry(lines, entry, commentLines))
        return false;
    return writeLinesAtomic(path, lines);
}

inline bool deleteGestures(const std::string& path, const std::vector<std::string>& strokesToDelete) {
    if (strokesToDelete.empty())
        return true;

    auto lines = readLines(path);
    if (!lines)
        return false;

    if (removeEntries(*lines, strokesToDelete) == 0)
        return false;
    return writeLinesAtomic(path, *lines);
}

}
