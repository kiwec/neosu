#pragma once

#ifdef APP_LIBRARY_BUILD
#ifdef _WIN32
#define EXPORT_NAME_ __declspec(dllexport)
#else
#define EXPORT_NAME_ __attribute__((visibility("default")))
#endif
#else
#define EXPORT_NAME_
#endif

extern "C" {
extern EXPORT_NAME_ void NEOSU_handle_existing_window(int argc, char *argv[]);
extern EXPORT_NAME_ void *NEOSU_create_env_interop(void *envptr);
}
