// Copyright (c) 2023, kiwec, All rights reserved.
#include "BanchoProtocol.h"

#include "Bancho.h"
#include "BeatmapInterface.h"
#include "Osu.h"

using namespace BANCHO::Proto;

Room::Room(Packet &packet) {
    this->id = read<u16>(packet);
    this->in_progress = read<u8>(packet);
    this->match_type = read<u8>(packet);
    this->mods = read<u32>(packet);
    this->name = read_string(packet);

    this->has_password = read<u8>(packet) > 0;
    if(this->has_password) {
        // Discard password. It should be an empty string, but just in case, read it properly.
        packet.pos--;
        read_string(packet);
    }

    this->map_name = read_string(packet);
    this->map_id = read<i32>(packet);

    auto hash_str = read_string(packet);
    this->map_md5 = hash_str.toUtf8();

    this->nb_players = 0;
    for(auto &slot : this->slots) {
        slot.status = read<u8>(packet);
    }
    for(auto &slot : this->slots) {
        slot.team = read<u8>(packet);
    }
    for(auto &slot : this->slots) {
        if(!slot.is_locked()) {
            this->nb_open_slots++;
        }

        if(slot.has_player()) {
            slot.player_id = read<i32>(packet);
            this->nb_players++;
        }
    }

    this->host_id = read<i32>(packet);
    this->mode = read<u8>(packet);
    this->win_condition = read<u8>(packet);
    this->team_type = read<u8>(packet);
    this->freemods = read<u8>(packet);
    if(this->freemods) {
        for(auto &slot : this->slots) {
            slot.mods = read<u32>(packet);
        }
    }

    this->seed = read<u32>(packet);
}

void Room::pack(Packet &packet) {
    write<u16>(packet, this->id);
    write<u8>(packet, this->in_progress);
    write<u8>(packet, this->match_type);
    write<u32>(packet, this->mods);
    write_string(packet, this->name.toUtf8());
    write_string(packet, this->password.toUtf8());
    write_string(packet, this->map_name.toUtf8());
    write<i32>(packet, this->map_id);
    write_string(packet, this->map_md5.hash.data());
    for(auto &slot : this->slots) {
        write<u8>(packet, slot.status);
    }
    for(auto &slot : this->slots) {
        write<u8>(packet, slot.team);
    }
    for(auto &slot : this->slots) {
        if(slot.has_player()) {
            write<i32>(packet, slot.player_id);
        }
    }

    write<i32>(packet, this->host_id);
    write<u8>(packet, this->mode);
    write<u8>(packet, this->win_condition);
    write<u8>(packet, this->team_type);
    write<u8>(packet, this->freemods);
    if(this->freemods) {
        for(auto &slot : this->slots) {
            write<u32>(packet, slot.mods);
        }
    }

    write<u32>(packet, this->seed);
}

bool Room::is_host() { return this->host_id == BanchoState::get_uid(); }

ScoreFrame ScoreFrame::get() {
    u8 slot_id = 0;
    for(u8 i = 0; i < 16; i++) {
        if(BanchoState::room.slots[i].player_id == BanchoState::get_uid()) {
            slot_id = i;
            break;
        }
    }

    const auto &score = osu->getScore();
    auto perfect = (score->getNumSliderBreaks() == 0 && score->getNumMisses() == 0 && score->getNum50s() == 0 &&
                    score->getNum100s() == 0);

    return ScoreFrame{
        .time = (i32)osu->getMapInterface()->getCurMusicPos(),  // NOTE: might be incorrect
        .slot_id = slot_id,
        .num300 = (u16)score->getNum300s(),
        .num100 = (u16)score->getNum100s(),
        .num50 = (u16)score->getNum50s(),
        .num_geki = (u16)score->getNum300gs(),
        .num_katu = (u16)score->getNum100ks(),
        .num_miss = (u16)score->getNumMisses(),
        .total_score = (i32)score->getScore(),
        .max_combo = (u16)score->getComboMax(),
        .current_combo = (u16)score->getCombo(),
        .is_perfect = perfect,
        .current_hp = (u8)(osu->getMapInterface()->getHealth() * 200.0),
        .tag = 0,         // tag gamemode currently not supported
        .is_scorev2 = 0,  // scorev2 currently not supported
    };
}
