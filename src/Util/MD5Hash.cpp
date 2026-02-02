#include "MD5Hash.h"
#include "UString.h"
#include "Engine.h"

MD5String::MD5String(const char *str) {
    assert(str);

    const size_t len = strnlen(str, 33);
    if(len == 0) {  // for explicit "empty" construction
        this->clear();
        return;
    }

    if(len != 32) {
        engine->showMessageErrorFatal(
            u"Programmer Error",
            fmt::format("Tried to construct an MD5String from\na string with length != 32.\n{} length {}", str, len));
        fubar_abort();
        return;
    }

    memcpy(this->data(), str, 32);
}

bool MD5Hash::operator==(const UString &other) const { return this->to_chars().string() == other.utf8View(); }
