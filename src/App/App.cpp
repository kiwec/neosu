// Copyright (c) 2025, WH, All rights reserved.
#include "App.h"

#include <cassert>

#ifdef APP_LIBRARY_BUILD
#include "Logging.h"
#include "dynutils.h"
#endif

App *App::create(bool dummy) {
    App *ret = nullptr;
#ifndef APP_LIBRARY_BUILD
    if(dummy) {
        ret = new App();
    } else {
        ret = create_app_real();
    }

#else
    auto *selfHandle = dynutils::load_lib(nullptr);
    assert(selfHandle);

    using create_app_real_t = App *(void);
    auto *pCreate = dynutils::load_func<create_app_real_t>(selfHandle, "create_app_real");
    if(pCreate) {
        debugLog("got app creation function at {:p}", (void *)pCreate);
        ret = pCreate();
    } else {
        debugLog("could not resolve dynamic app creation function");
        ret = new App();  // fallback
    }
    dynutils::unload_lib(selfHandle);

#endif
    assert(ret);
    return ret;
}
