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
extern EXPORT_NAME_ void handle_existing_window_app(int argc, char *argv[]);
extern EXPORT_NAME_ void *create_app_env_interop(void *envptr);
}
