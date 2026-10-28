#pragma once

#include <expected>
#include <string>
#include <hyprutils/string/VarList.hpp>

// Fields of a button config value, before color parsing:
// text_color, bg_color, icon_inactive, icon_active, command, [condition]
struct SButtonConfigFields {
    std::string textColor;
    std::string bgColor;
    std::string iconInactive;
    std::string iconActive;
    std::string command;
    std::string condition;
};

inline std::expected<SButtonConfigFields, std::string> splitButtonConfig(const std::string& value) {
    Hyprutils::String::CVarList vars(value, 0, ',', true);

    if (vars.size() < 5)
        return std::unexpected("Invalid button config (need at least 5 args: text_color, bg_color, icon_inactive, icon_active, command)");

    SButtonConfigFields fields;
    fields.textColor    = vars[0];
    fields.bgColor      = vars[1];
    fields.iconInactive = vars[2];
    fields.iconActive   = vars[3];
    fields.command      = vars[4];

    if (vars.size() >= 6)
        fields.condition = vars[5];

    return fields;
}
