#pragma once
// Copyright (c) 2014, PG & 2025, WH, All rights reserved.

#include <string>

class Console {
   public:
    static void processCommand(std::string command, bool fromFile = false);
    static void execConfigFile(std::string_view filename_view);
};
