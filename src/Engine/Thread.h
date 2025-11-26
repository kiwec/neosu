#pragma once
// Copyright (c) 2025, WH, All rights reserved.
class UString;

namespace McThread {
// WARNING: must be called from within the thread itself! otherwise, the main process name/priority will be changed
bool set_current_thread_name(const UString &name);

enum Priority : unsigned char { NORMAL, HIGH, LOW, REALTIME };
void set_current_thread_prio(Priority prio);

const char* get_current_thread_name();

bool is_main_thread();

// returns at least 1
int get_logical_cpu_count();

}  // namespace McThread
