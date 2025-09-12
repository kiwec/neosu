#pragma once
// Copyright (c) 2018, PG, All rights reserved.

// Windows headers shit
#include "cbase.h"

#pragma pack(push, 8)
#include "discord_game_sdk.h"
#pragma pack(pop)

void init_discord_sdk();
void tick_discord_sdk();
void destroy_discord_sdk();
void clear_discord_presence();
void set_discord_presence(struct DiscordActivity *activity);