#pragma once
// Copyright (c) 2014, PG & 2025, WH, All rights reserved.

#include <string_view>

class Console {
   public:
    static void processCommand(std::string_view command, bool fromFile = false);
    static void execConfigFile(std::string_view filename_view);
};
