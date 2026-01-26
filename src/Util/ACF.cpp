// Copyright (c) 2026, kiwec, All rights reserved.
#include "ACF.h"
#include "SString.h"

// Not the best API, but it gets the job done... and won't be reused anyway
// See https://git.kiwec.net/kiwec/cs2-quake-sounds/src/master/steamfiles.py

namespace ACF {

// Very naive parser, but at the same time, steam is pretty consistent
Section parse(std::string file) {
    Section out;

    std::vector<Section*> stack;
    stack.push_back(&out);

    std::string key = "undefined";

    auto lines = SString::split(file, '\n');
    for(auto line : lines) {
        if(line.find('{') != std::string::npos) {
            // Start of a new section
            stack.back()->map[key] = Section{};
            stack.push_back(std::get_if<Section>(&stack.back()->map[key]));
        } else if(line.find('}') != std::string::npos) {
            // End of current section
            if(stack.size() > 1) {
                stack.pop_back();
            }
        } else {
            auto quote1 = line.find('"');
            if(quote1 == std::string::npos) continue;
            auto quote2 = line.find('"', quote1 + 1);
            if(quote2 == std::string::npos) continue;

            // Can be either key of section, or key of string value
            key = line.substr(quote1 + 1, quote2 - (quote1 + 1));

            // (ignoring malformed lines here!)
            auto quote3 = line.find('"', quote2 + 1);
            if(quote3 == std::string::npos) continue;
            auto quote4 = line.find('"', quote3 + 1);
            if(quote4 == std::string::npos) continue;

            auto value = line.substr(quote3 + 1, quote4 - (quote3 + 1));
            stack.back()->map[key] = std::string(value);
        }
    }

    return out;
}

// Helper to get nested value more easily
// Assumes it's a string, returns "" if it doesn't exist
std::string getValue(const Section* section, const std::vector<std::string>& keys) {
    for(auto i = 0; i < keys.size() && section != nullptr; i++) {
        auto it = section->map.find(keys[i]);
        if(it == section->map.end()) return "";

        if(i == keys.size() - 1) {
            // Last key: try to get value
            auto* str = std::get_if<std::string>(&it->second);
            return str ? *str : "";
        } else {
            // Get nested map
            section = std::get_if<Section>(&it->second);
        }
    }

    return "";
}

}  // namespace ACF
