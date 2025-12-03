// Copyright (c) 2025, WH, All rights reserved.
#include "App.h"

#include <cassert>

#ifdef APP_LIBRARY_BUILD
#include "Logging.h"
#include "dynutils.h"
#endif

App *App::create(bool dummy) {
    if(dummy) return new App();

    App *ret = nullptr;
#ifndef APP_LIBRARY_BUILD
    ret = NEOSU_create_app_real();
#else
    auto *selfHandle = dynutils::load_lib(nullptr);
    assert(selfHandle);

    using NEOSU_create_app_real_t = App *(void);
    auto *pCreate = dynutils::load_func<NEOSU_create_app_real_t>(selfHandle, "NEOSU_create_app_real");
    if(pCreate) {
        debugLog("got app creation function at {:p}", (void *)pCreate);
        ret = pCreate();
    } else {
        debugLog("could not resolve dynamic app creation function");
        debugLog(dynutils::get_error());
        ret = new App();  // fallback
    }
    dynutils::unload_lib(selfHandle);

#endif
    assert(ret);
    return ret;
}
