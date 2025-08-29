#pragma once
// Copyright (c) 2014, PG & 2025, WH, All rights reserved.

#include <string>
#include <vector>

class Console {
   public:
    static void processCommand(std::string command, bool fromFile = false);
    static void execConfigFile(std::string filename);

   public:
    Console();

    static std::vector<std::string> g_commandQueue;
};
