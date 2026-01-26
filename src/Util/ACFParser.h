#pragma once
// Copyright (c) 2026, kiwec, All rights reserved.
#include <map>
#include <string>
#include <variant>
#include <vector>

// Parser for Steam file format, used to locate McOsu's install dir
namespace Parsing::ACF {

struct Section {
    using Value = std::variant<std::string, Section>;
    std::map<std::string, Value> map;
};

Section parse(std::string_view file);

std::string getValue(const Section* section, const std::vector<std::string>& keys);

}  // namespace Parsing::ACF
