#pragma once
// Copyright (c) 2024, kiwec, All rights reserved.

namespace SettingsImporter {

bool import_from_osu_stable();

// NOTE: Doesn't only import settings, but also enqueues the import of collections.db and scores.db!
bool import_from_mcosu();

}  // namespace SettingsImporter
