#pragma once

namespace dynutils {
using lib_obj = struct lib_obj;

namespace detail {
void *load_func_impl(lib_obj *lib, const char *func_name);
}  // namespace detail

// example usage: load_lib("bass"), load_lib("libbass.so"), load_lib("bass.dll"), load_lib("bass", "lib/")
// you get the point
lib_obj *load_lib(const char *c_lib_name, const char *c_search_dir = "");
void unload_lib(lib_obj *&lib);

// usage: auto func = load_func<func_prototype>(lib_obj_here, func_name_here)
template <typename F>
inline F load_func(lib_obj *lib, const char *func_name) {
    return reinterpret_cast<F>(detail::load_func_impl(lib, func_name));
}

const char *get_error();

}  // namespace dynutils
