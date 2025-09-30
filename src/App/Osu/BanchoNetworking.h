#pragma once
// Copyright (c) 2023, kiwec, All rights reserved.

#include "BanchoProtocol.h"

#define NEOSU_DOMAIN "neosu.net"

// NOTE: Full version can be something like "b20200201.2cuttingedge"
#define OSU_VERSION_DATEONLY 20250815
#define OSU_VERSION_DATESTR MC_STRINGIZE(OSU_VERSION_DATEONLY)

#define OSU_VERSION "b" OSU_VERSION_DATESTR

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
