#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

#include "MD5Hash.h"

#include <vector>

#define COLLECTIONS_DB_VERSION 20240429

struct Collection {
    std::string name;
    std::vector<MD5Hash> maps;

    std::vector<MD5Hash> neosu_maps;
    std::vector<MD5Hash> peppy_maps;
    std::vector<MD5Hash> deleted_maps;

    void delete_collection();
    void add_map(const MD5Hash &map_hash);
    void remove_map(const MD5Hash &map_hash);
    void rename_to(std::string_view new_name);
};

extern std::vector<Collection*> collections;

Collection* get_or_create_collection(std::string_view name);

bool load_collections();
bool load_peppy_collections(std::string_view peppy_collections_path);
bool load_mcneosu_collections(std::string_view neosu_collections_path);
void unload_collections();
bool save_collections();
