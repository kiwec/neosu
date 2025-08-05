#include "Collections.h"

#include <algorithm>
#include <utility>

#include "ByteBufferedFile.h"
#include "File.h"  // PREF_PATHSEP
#include "ConVar.h"
#include "Database.h"
#include "Engine.h"

namespace {  // static namespace
bool collections_loaded = false;
}

std::vector<Collection*> collections;

void Collection::delete_collection() {
    for(auto map : this->maps) {
        this->remove_map(map);
    }
}

void Collection::add_map(MD5Hash map_hash) {
    {
        auto it = std::ranges::find(this->deleted_maps, map_hash);
        if(it != this->deleted_maps.end()) {
            this->deleted_maps.erase(it);
        }
    }

    {
        auto it = std::ranges::find(this->neosu_maps, map_hash);
        if(it == this->neosu_maps.end()) {
            this->neosu_maps.push_back(map_hash);
        }
    }

    {
        auto it = std::ranges::find(this->maps, map_hash);
        if(it == this->maps.end()) {
            this->maps.push_back(map_hash);
        }
    }
}

void Collection::remove_map(MD5Hash map_hash) {
    {
        auto it = std::ranges::find(this->maps, map_hash);
        if(it != this->maps.end()) {
            this->maps.erase(it);
        }
    }

    {
        auto it = std::ranges::find(this->neosu_maps, map_hash);
        if(it != this->neosu_maps.end()) {
            this->neosu_maps.erase(it);
        }
    }

    {
        auto it = std::ranges::find(this->peppy_maps, map_hash);
        if(it != this->peppy_maps.end()) {
            this->deleted_maps.push_back(map_hash);
        }
    }
}

void Collection::rename_to(std::string new_name) {
    if(new_name.length() < 1) new_name = "Untitled collection";
    if(this->name == new_name) return;

    auto new_collection = get_or_create_collection(new_name);

    for(auto map : this->maps) {
        this->remove_map(map);
        new_collection->add_map(map);
    }
}

Collection* get_or_create_collection(std::string name) {
    if(name.length() < 1) name = "Untitled collection";

    for(auto collection : collections) {
        if(collection->name == name) {
            return collection;
        }
    }

    auto collection = new Collection();
    collection->name = name;
    collections.push_back(collection);

    return collection;
}

// Should only be called from db loader thread!
bool load_peppy_collections(const UString& peppy_collections_path) {
    ByteBufferedFile::Reader peppy_collections(peppy_collections_path);
    if(peppy_collections.total_size == 0) return false;
    if(!cv::collections_legacy_enabled.getBool()) {
        db->bytes_processed += peppy_collections.total_size;
        return false;
    }

    u32 version = peppy_collections.read<u32>();
    if(version > cv::database_version.getVal<u32>()) {
        debugLog("osu!stable collection.db version is too recent!\n");
        db->bytes_processed += peppy_collections.total_size;
        return false;
    }

    u32 total_maps = 0;
    u32 nb_collections = peppy_collections.read<u32>();
    for(int c = 0; std::cmp_less(c, nb_collections); c++) {
        auto name = peppy_collections.read_string();
        u32 nb_maps = peppy_collections.read<u32>();
        total_maps += nb_maps;

        auto collection = get_or_create_collection(name);
        collection->maps.reserve(nb_maps);
        collection->peppy_maps.reserve(nb_maps);

        for(int m = 0; m < nb_maps; m++) {
            auto map_hash = peppy_collections.read_hash();
            collection->maps.push_back(map_hash);
            collection->peppy_maps.push_back(map_hash);
        }

        u32 progress_bytes = db->bytes_processed + peppy_collections.total_pos;
        f64 progress_float = (f64)progress_bytes / (f64)db->total_bytes;
        db->fLoadingProgress = std::clamp(progress_float, 0.01, 0.99);
    }

    debugLog("Loaded {:d} peppy collections ({:d} maps)\n", nb_collections, total_maps);
    db->bytes_processed += peppy_collections.total_size;
    return true;
}

// Should only be called from db loader thread!
bool load_mcneosu_collections(const UString& neosu_collections_path) {
    ByteBufferedFile::Reader neosu_collections(neosu_collections_path);
    if(neosu_collections.total_size == 0) return false;

    u32 total_maps = 0;

    u32 version = neosu_collections.read<u32>();
    u32 nb_collections = neosu_collections.read<u32>();

    if(version > COLLECTIONS_DB_VERSION) {
        debugLog("neosu collections.db version is too recent! Cannot load it without stuff breaking.\n");
        db->bytes_processed += neosu_collections.total_size;
        return false;
    }

    for(u32 c = 0; std::cmp_less(c, nb_collections); c++) {
        auto name = neosu_collections.read_string();
        auto collection = get_or_create_collection(name);

        u32 nb_deleted_maps = 0;
        if(version >= 20240429) {
            nb_deleted_maps = neosu_collections.read<u32>();
        }

        collection->deleted_maps.reserve(nb_deleted_maps);
        for(int d = 0; std::cmp_less(d, nb_deleted_maps); d++) {
            auto map_hash = neosu_collections.read_hash();

            auto it = std::ranges::find(collection->maps, map_hash);
            if(it != collection->maps.end()) {
                collection->maps.erase(it);
            }

            collection->deleted_maps.push_back(map_hash);
        }

        u32 nb_maps = neosu_collections.read<u32>();
        total_maps += nb_maps;
        collection->maps.reserve(collection->maps.size() + nb_maps);
        collection->neosu_maps.reserve(nb_maps);

        for(int m = 0; std::cmp_less(m, nb_maps); m++) {
            auto map_hash = neosu_collections.read_hash();

            auto it = std::ranges::find(collection->maps, map_hash);
            if(it == collection->maps.end()) {
                collection->maps.push_back(map_hash);
            }

            collection->neosu_maps.push_back(map_hash);
        }

        u32 progress_bytes = db->bytes_processed + neosu_collections.total_pos;
        f64 progress_float = (f64)progress_bytes / (f64)db->total_bytes;
        db->fLoadingProgress = std::clamp(progress_float, 0.01, 0.99);
    }

    debugLog("Loaded {:d} neosu collections ({:d} maps)\n", nb_collections, total_maps);
    db->bytes_processed += neosu_collections.total_size;
    return true;
}

// Should only be called from db loader thread!
bool load_collections() {
    const double startTime = Timing::getTimeReal();

    unload_collections();

    std::string peppy_collections_path = cv::osu_folder.getString();
    peppy_collections_path.append(PREF_PATHSEP "collection.db");
    const auto& peppy_collections = db->database_files[peppy_collections_path];
    load_peppy_collections(peppy_collections);

    const auto& mcneosu_collections = db->database_files["collections.db"];
    load_mcneosu_collections(mcneosu_collections);

    debugLog("peppy+neosu collections: loading took {:f} seconds\n", (Timing::getTimeReal() - startTime));
    collections_loaded = true;
    return true;
}

void unload_collections() {
    collections_loaded = false;

    for(auto collection : collections) {
        delete collection;
    }
    collections.clear();
}

bool save_collections() {
    debugLog("Osu: Saving collections ...\n");
    if(!collections_loaded) {
        debugLog("Cannot save collections since they weren't loaded properly first!\n");
        return false;
    }

    const double startTime = Timing::getTimeReal();

    ByteBufferedFile::Writer db("collections.db");
    db.write<u32>(COLLECTIONS_DB_VERSION);

    u32 nb_collections = collections.size();
    db.write<u32>(nb_collections);

    for(auto collection : collections) {
        db.write_string(collection->name.c_str());

        u32 nb_deleted = collection->deleted_maps.size();
        db.write<u32>(nb_deleted);
        for(auto map : collection->deleted_maps) {
            db.write_string(map.hash.data());
        }

        u32 nb_neosu = collection->neosu_maps.size();
        db.write<u32>(nb_neosu);
        for(auto map : collection->neosu_maps) {
            db.write_string(map.hash.data());
        }
    }

    debugLog("collections.db: saving took {:f} seconds\n", (Timing::getTimeReal() - startTime));
    return true;
}
