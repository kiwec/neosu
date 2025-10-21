// Copyright (c) 2025, kiwec, All rights reserved.
#include "Storyboard.h"

void parse_storyboard_command(StoryboardState* sb, std::string line) {
    // TODO
}

void parse_storyboard_events(StoryboardState* sb, std::string line) {
    // TODO: handle loops and triggers
    if(line[0] == ' ' || line[0] == '_') {
        line.erase(0, 1);
        parse_storyboard_command(sb, line);
        return;
    }

    auto csv = SString::split(line, ',');
    if(csv[0] == "Animation") {
        if(csv.size() != 9) return; // XXX: handle error
        auto [_, layer, origin, filepath, x, y, frame_count, frame_delay, looptype] = csv;
        // TODO
    } else if(csv[0] == "Sample") {
        if(csv.size() != 5) return; // XXX: handle error
        auto [_, time, layer_num, filepath, volume] = csv;
        // TODO
    } else if(csv[0] == "Sprite") {
        if(csv.size() != 6) return; // XXX: handle error
        auto [_, layer, origin, filepath, x, y] = csv;
        // TODO
    } else {
        // XXX: handle error
    }
}

void parse_storyboard(StoryboardState* sb, std::string file, bool is_osb) {

    enum class Block {
        UNKNOWN,
        GENERAL,
        EVENTS,
        VARIABLES,
    };

    Block current_block = Block::UNKNOWN;

    // See https://osu.ppy.sh/wiki/en/Storyboard/Scripting/Variables
    std::unordered_map<std::string, std::string> constants;
    if(is_osb) {
        for(auto line : SString::split(file, '\n')) {
            SString::trim_inplace(line);
            if(line.empty() || line.starts_with("//")) continue;

            if(line[0] == '[') current_block = Block::UNKNOWN;
            if(line == "[Variables]") current_block = Block::VARIABLES;
            if(current_block != Block::VARIABLES) continue;

            auto expr = SString::split(line, '=');
            if(expr.size() != 2) continue;  // NOTE: might be wrong if '=' is ok in values

            auto [name, value] = expr;

            // Lazer forces variable names to start with '$', assuming stable does the same
            if(name.empty()) continue;
            if(name[0] != '$') continue;

            constants[name] = value;
        }
    }

    current_block = is_osb ? Block::EVENTS : Block::UNKNOWN;
    for(auto line : SString::split(file, '\n')) {
        auto trimmed_line = line;
        SString::trim_inplace(trimmed_line);

        if(trimmed_line.empty() || trimmed_line.starts_with("//")) continue;

        if(trimmed_line[0] == '[') {
            current_block = Block::UNKNOWN;
            if(trimmed_line == "[General]") current_block = Block::GENERAL;
            if(trimmed_line == "[Events]") current_block = Block::EVENTS;
        }

        switch(current_block) {
            case Block::GENERAL: {
                Parsing::parse(trimmed_line, "UseSkinSprites", &sb->use_skin_sprites);
                Parsing::parse(trimmed_line, "WidescreenStoryboard", &sb->widescreen);
                break;
            }

            // NOTE: using untrimmed line here, since commands can be either "_X" or " X"
            case Block::EVENTS: {
                for(auto [k, v] : constants) {
                    auto pos = line.find(k);
                    if(pos != std::string::npos) {
                        line.replace(pos, k.length(), v);
                        break;
                    }
                }

                parse_storyboard_events(sb, line);
                break;
            }

            default: {
                // do nothing
                break;
            }
        }
    }
}
