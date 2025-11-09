#pragma once
// Copyright (c) 2018, PG, All rights reserved.

#include "config.h"

#ifdef MCENGINE_FEATURE_DISCORD
#pragma pack(push, 8)
#include "discord_game_sdk.h"
#pragma pack(pop)
#else
enum DiscordActivityType { DiscordActivityType_Listening, DiscordActivityType_Playing };
struct DiscordActivity {
    struct {
        struct {
            int current_size{0};
            int max_size{0};
        } size{};
    } party{};
    struct {
        int start{0};
        int end{0};
    } timestamps{};
    DiscordActivityType type{};
    char details[512]{};
    char state[512]{};
};
#endif

namespace DiscRPC {
void init();
void tick();
void destroy();
void clear_activity();
void set_activity(struct DiscordActivity *activity);
}  // namespace DiscRPC
