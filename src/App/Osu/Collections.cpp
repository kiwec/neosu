// Copyright (c) 2024, kiwec, All rights reserved.
#include "Collections.h"

#include "ByteBufferedFile.h"
#include "OsuConVars.h"
#include "Database.h"
#include "Engine.h"
#include "Timing.h"
#include "Logging.h"

#include <algorithm>
#include <utility>

namespace Collections {

namespace {  // static namespace
bool s_collections_loaded{false};
std::vector<Collection> s_collections;

}  // namespace

std::vector<Collection>& get_loaded() { return s_collections; }

bool delete_collection(std::string_view collection_name) {
    if(collection_name.empty() || s_collections.empty()) return false;

    const size_t erased = std::erase_if(
        s_collections, [collection_name](const auto& col) -> bool { return col.name == collection_name; });
    return erased > 0;
}

void Collection::add_map(const MD5Hash& map_hash) {
    // remove from deleted maps
    std::erase_if(this->deleted_maps,
                  [&map_hash](const auto& deleted_hash) -> bool { return map_hash == deleted_hash; });

    // add to neosu maps
    if(!std::ranges::contains(this->neosu_maps, map_hash)) {
        this->neosu_maps.push_back(map_hash);
    }

    // add to maps (TODO: what's the difference...?)
    if(!std::ranges::contains(this->maps, map_hash)) {
        this->maps.push_back(map_hash);
    }
}

void Collection::remove_map(const MD5Hash& map_hash) {
    std::erase_if(this->maps, [&map_hash](const auto& contained_hash) -> bool { return map_hash == contained_hash; });
    std::erase_if(this->neosu_maps,
                  [&map_hash](const auto& contained_hash) -> bool { return map_hash == contained_hash; });

    if(std::ranges::contains(this->peppy_maps, map_hash)) {
        this->deleted_maps.push_back(map_hash);
    }
}

bool Collection::rename_to(std::string_view new_name) {
    if(new_name.empty() || new_name == this->name) return false;

    // don't allow renaming to an existing collection name
    if(std::ranges::contains(s_collections, new_name, [](const auto& col) -> std::string_view { return col.name; })) {
        debugLog("not renaming {} -> {}, conflicting name", this->name, new_name);
        return false;
    }

    this->name = new_name;

    return true;
}

Collection& get_or_create_collection(std::string_view name) {
    if(name.length() < 1) name = "Untitled collection";

    // get
    const auto& it =
        std::ranges::find(s_collections, name, [](const auto& col) -> std::string_view { return col.name; });
    if(it != s_collections.end()) {
        return *it;
    }

    // create
    Collection collection{};
    collection.name = name;

    auto& ret = s_collections.emplace_back(std::move(collection));

    return ret;
}

// Should only be called from db loader thread!
bool load_peppy(std::string_view peppy_collections_path) {
    ByteBufferedFile::Reader peppy_collections(peppy_collections_path);
    if(peppy_collections.total_size == 0) return false;
    if(!cv::collections_legacy_enabled.getBool()) {
        db->bytes_processed += peppy_collections.total_size;
        return false;
    }

    u32 version = peppy_collections.read<u32>();
    if(version > cv::database_version.getVal<u32>() && !cv::database_ignore_version.getBool()) {
        debugLog("osu!stable collection.db (version {}) is newer than latest supported (version {})!", version,
                 cv::database_version.getVal<u32>());
        db->bytes_processed += peppy_collections.total_size;
        return false;
    }

    u32 total_maps = 0;
    u32 nb_collections = peppy_collections.read<u32>();
    for(u32 c = 0; c < nb_collections; c++) {
        auto name = peppy_collections.read_string();
        u32 nb_maps = peppy_collections.read<u32>();
        total_maps += nb_maps;

        auto& collection = get_or_create_collection(name);
        collection.maps.reserve(nb_maps);
        collection.peppy_maps.reserve(nb_maps);

        for(u32 m = 0; m < nb_maps; m++) {
            MD5Hash map_hash;
            (void)peppy_collections.read_hash(map_hash);  // TODO: validate

            collection.maps.push_back(map_hash);
            collection.peppy_maps.push_back(map_hash);
        }

        u32 progress_bytes = db->bytes_processed + peppy_collections.total_pos;
        f64 progress_float = (f64)progress_bytes / (f64)db->total_bytes;
        db->loading_progress = std::clamp(progress_float, 0.01, 0.99);
    }

    debugLog("Loaded {:d} peppy collections ({:d} maps)", nb_collections, total_maps);
    db->bytes_processed += peppy_collections.total_size;
    return true;
}

// Should only be called from db loader thread!
bool load_mcneosu(std::string_view neosu_collections_path) {
    ByteBufferedFile::Reader neosu_collections(neosu_collections_path);
    if(neosu_collections.total_size == 0) return false;

    u32 total_maps = 0;

    u32 version = neosu_collections.read<u32>();
    u32 nb_collections = neosu_collections.read<u32>();

    if(version > COLLECTIONS_DB_VERSION) {
        debugLog("neosu collections.db version is too recent! Cannot load it without stuff breaking.");
        db->bytes_processed += neosu_collections.total_size;
        return false;
    }

    for(u32 c = 0; c < nb_collections; c++) {
        auto name = neosu_collections.read_string();
        auto& collection = get_or_create_collection(name);

        u32 nb_deleted_maps = 0;
        if(version >= 20240429) {
            nb_deleted_maps = neosu_collections.read<u32>();
        }

        collection.deleted_maps.reserve(nb_deleted_maps);
        for(u32 d = 0; d < nb_deleted_maps; d++) {
            MD5Hash map_hash;
            (void)neosu_collections.read_hash(map_hash);  // TODO: validate

            std::erase_if(collection.maps,
                          [&map_hash](const auto& contained_hash) -> bool { return map_hash == contained_hash; });

            collection.deleted_maps.push_back(map_hash);
        }

        u32 nb_maps = neosu_collections.read<u32>();
        total_maps += nb_maps;
        collection.maps.reserve(collection.maps.size() + nb_maps);
        collection.neosu_maps.reserve(nb_maps);

        for(u32 m = 0; m < nb_maps; m++) {
            MD5Hash map_hash;
            (void)neosu_collections.read_hash(map_hash);  // TODO: validate

            if(!std::ranges::contains(collection.maps, map_hash)) {
                collection.maps.push_back(map_hash);
            }

            collection.neosu_maps.push_back(map_hash);
        }

        u32 progress_bytes = db->bytes_processed + neosu_collections.total_pos;
        f64 progress_float = (f64)progress_bytes / (f64)db->total_bytes;
        db->loading_progress = std::clamp(progress_float, 0.01, 0.99);
    }

    debugLog("Loaded {:d} neosu collections ({:d} maps)", nb_collections, total_maps);
    db->bytes_processed += neosu_collections.total_size;
    return true;
}

// Should only be called from db loader thread!
bool load_all() {
    const double startTime = Timing::getTimeReal();

    unload_all();

    const auto& peppy_path = db->database_files[Database::DatabaseType::STABLE_COLLECTIONS];
    load_peppy(peppy_path);

    const auto& mcneosu_path = db->database_files[Database::DatabaseType::MCNEOSU_COLLECTIONS];
    load_mcneosu(mcneosu_path);

    debugLog("peppy+neosu collections: loading took {:f} seconds", (Timing::getTimeReal() - startTime));
    s_collections_loaded = true;
    return true;
}

void unload_all() {
    s_collections_loaded = false;

    s_collections.clear();
}

bool save_collections() {
    debugLog("Osu: Saving collections ...");
    if(!s_collections_loaded) {
        debugLog("Cannot save collections since they weren't loaded properly first!");
        return false;
    }

    const double startTime = Timing::getTimeReal();

    const auto neosu_collections_db = Database::getDBPath(Database::DatabaseType::MCNEOSU_COLLECTIONS);

    ByteBufferedFile::Writer db(neosu_collections_db);
    if(!db.good()) {
        debugLog("Cannot save collections to {}: {}", neosu_collections_db, db.error());
        return false;
    }

    db.write<u32>(COLLECTIONS_DB_VERSION);

    u32 nb_collections = s_collections.size();
    db.write<u32>(nb_collections);

    for(const auto& collection : s_collections) {
        db.write_string(collection.name);

        u32 nb_deleted = collection.deleted_maps.size();
        db.write<u32>(nb_deleted);
        for(const auto& mapmd5 : collection.deleted_maps) {
            db.write_hash(mapmd5);
        }

        u32 nb_neosu = collection.neosu_maps.size();
        db.write<u32>(nb_neosu);
        for(const auto& mapmd5 : collection.neosu_maps) {
            db.write_hash(mapmd5);
        }
    }

    debugLog("collections.db: saving took {:f} seconds", (Timing::getTimeReal() - startTime));
    return true;
}
}  // namespace Collections
