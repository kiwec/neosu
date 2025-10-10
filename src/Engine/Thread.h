#pragma once
// Copyright (c) 2025, WH, All rights reserved.

namespace McThread {
// WARNING: must be called from within the thread itself! otherwise, the main process name/priority will be changed
bool set_current_thread_name(const char *name);
void set_current_thread_prio(bool high);

const char* get_current_thread_name();

bool is_main_thread();
}
