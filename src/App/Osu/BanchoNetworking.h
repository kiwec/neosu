#pragma once
// Copyright (c) 2023, kiwec, All rights reserved.

#include "config.h"

#include <string_view>

struct Packet;

#define NEOSU_DOMAIN "neosu.net"

// NOTE: Full version can be something like "b20200201.2cuttingedge"
#define OSU_VERSION_DATEONLY 20251102
#define OSU_VERSION "b20251102.1"

namespace BANCHO::Net {

// Send a packet to Bancho. Do not free it after calling this.
void send_packet(Packet& packet);

// Process networking logic. Should be called regularly from main thread.
void update_networking();

// Clean up networking. Should be called once when exiting neosu.
void cleanup_networking();

// Callback for complete_oauth command
void complete_oauth(std::string_view code);

}  // namespace BANCHO::Net
