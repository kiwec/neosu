#include "MD5Hash.h"
#include "UString.h"
#include "Engine.h"

MD5Hash::MD5Hash(const char *str) {
    assert(str);

    const size_t len = strlen(str);
    if(len == 0) {  // for explicit "empty" construction
        this->clear();
        return;
    }

    if(len != 32) {
        engine->showMessageErrorFatal(u"Programmer Error",
                                      u"Tried to construct an MD5Hash from\na string with length != 32.");
        engine->shutdown();
        return;
    }

    memcpy(this->hash.data(), str, 32);
}

bool MD5Hash::operator==(const UString &other) const { return this->string() == other.utf8View(); }
