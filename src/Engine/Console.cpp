// Copyright (c) 2014, PG, All rights reserved.
#include "Console.h"

#include "SString.h"
#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "Logging.h"

Console::Console() {
    // exec autoexec
    Console::execConfigFile("autoexec.cfg");
}

void Console::processCommand(std::string command, bool fromFile) {
    if(command.length() < 1) return;

    // remove whitespace from beginning/end of string
    SString::trim(&command);

    // handle multiple commands separated by semicolons
    if(command.find(';') != std::string::npos && command.find("echo") == std::string::npos) {
        const std::vector<std::string> commands = SString::split(command, ';');
        for(const auto &command : commands) {
            processCommand(command);
        }

        return;
    }

    // separate convar name and value
    const std::vector<std::string> tokens = SString::split(command, ' ');
    std::string commandName;
    std::string commandValue;
    for(size_t i = 0; i < tokens.size(); i++) {
        if(i == 0)
            commandName = tokens[i];
        else {
            commandValue.append(tokens[i]);
            if(i < (tokens.size() - 1)) commandValue.append(" ");
        }
    }

    // get convar
    ConVar *var = cvars->getConVarByName(commandName, false);
    if(!var) {
        debugLog("Unknown command: {:s}", commandName);
        return;
    }

    if(fromFile && var->isFlagSet(cv::NOLOAD)) {
        return;
    }

    // set new value (this handles all callbacks internally)
    if(commandValue.length() > 0) {
        var->setValue(commandValue);
    } else {
        var->exec();
        var->execArgs("");
        var->execFloat(var->getFloat());
    }

    // log
    if(cv::console_logging.getBool() && !var->isFlagSet(cv::HIDDEN)) {
        std::string logMessage;

        bool doLog = false;
        if(commandValue.length() < 1) {
            doLog = var->hasValue();  // assume ConCommands never have helpstrings

            logMessage = commandName;

            if(var->hasValue()) {
                logMessage.append(fmt::format(" = {:s} ( def. \"{:s}\" , ", var->getString(), var->getDefaultString()));
                logMessage.append(ConVar::typeToString(var->getType()));
                logMessage.append(", ");
                logMessage.append(ConVarHandler::flagsToString(var->getFlags()));
                logMessage.append(" )");
            }

            if(var->getHelpstring().length() > 0) {
                logMessage.append(" - ");
                logMessage.append(var->getHelpstring());
            }
        } else if(var->hasValue()) {
            doLog = true;

            logMessage = commandName;
            logMessage.append(" : ");
            logMessage.append(var->getString());
        }

        if(logMessage.length() > 0 && doLog) debugLog("{:s}", logMessage);
    }
}

void Console::execConfigFile(std::string filename) {
    // handle extension
    filename.insert(0, MCENGINE_DATA_DIR "cfg" PREF_PATHSEP "");
    if(filename.find(".cfg", (filename.length() - 4), filename.length()) == std::string::npos) filename.append(".cfg");

    bool needs_write = false;

    std::string rewritten_file;

    {
        File configFile(filename, File::TYPE::READ);
        if(!configFile.canRead()) {
            debugLog("error, file \"{:s}\" not found!", filename);
            return;
        }

        // collect commands first
        std::vector<std::string> cmds;
        while(true) {
            std::string line{configFile.readLine()};

            // if canRead() is false after readLine(), we hit EOF
            if(!configFile.canRead()) break;

            // only process non-empty lines
            if(!line.empty()) {
                // erase comment lines ("//" or "#") and remove everything after
                auto commentIndex = line.find("//");
                if(commentIndex == std::string::npos) {
                    commentIndex = line.find('#');
                }

                if(commentIndex != std::string::npos) {
                    line.erase(commentIndex);

                    // if line now contains only whitespace, clear it entirely
                    if(line.find_first_not_of(" \t") == std::string::npos) {
                        line.clear();
                    }
                }

                // McOsu used to prefix all convars with "osu_". Maybe it made sense when McEngine was
                // a separate thing, but in neosu everything is related to osu anyway, so it's redundant.
                // So, to avoid breaking old configs, we're removing the prefix for (almost) all convars here.
                if(line.starts_with("osu_") && !line.starts_with("osu_folder")) {
                    line.erase(0, 4);
                    needs_write = true;
                }

                // add command (original adds all processed lines, even if they become empty after comment removal)
                cmds.push_back(line);
            }

            rewritten_file.append(line);
            rewritten_file.push_back('\n');
        }

        // process the collected commands
        for(const auto &cmd : cmds) processCommand(cmd, true);
    }

    // if we don't remove prefixed lines, this could prevent users from
    // setting some convars back to their default value
    if(needs_write) {
        File configFile(filename, File::TYPE::WRITE);
        configFile.write((u8 *)rewritten_file.data(), rewritten_file.length());
    }
}
