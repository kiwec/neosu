// Copyright (c) 2025, WH, All rights reserved.
#include "Logging.h"

#include "Engine.h"
#include "ConsoleBox.h"
#include "UString.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <io.h>
#else
#include <unistd.h>
#endif

namespace Logger {
bool isaTTY() {
    static int8_t tty_cached = -1;
    if(unlikely(tty_cached == -1)) {
        tty_cached = (isatty(fileno(stdout)) ? 1 : 0);
    }
    return tty_cached;
}

namespace detail {
void logToConsole(std::optional<Color> color, const std::string &message) {
    ConsoleBox *consolebox = likely(!!engine) ? engine->getConsoleBox() : nullptr;
    if(unlikely(!consolebox)) {
        return;
    }

    if(unlikely(color.has_value())) {
        consolebox->log(UString{message}, color.value());
    } else {
        consolebox->log(UString{message});
    }
}
}  // namespace detail
}  // namespace Logger
