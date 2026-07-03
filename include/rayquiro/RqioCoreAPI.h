#pragma once

#include <stddef.h>

#ifdef _WIN32
#define RQIO_CORE_EXPORT extern "C" __declspec(dllexport)
#define RQIO_CORE_IMPORT extern "C" __declspec(dllimport)
#else
#define RQIO_CORE_EXPORT extern "C"
#define RQIO_CORE_IMPORT extern "C"
#endif

typedef struct RqioCoreHandle RqioCoreHandle;

typedef enum RqioCoreStatus {
    RQIO_CORE_OK = 0,
    RQIO_CORE_ERROR = 1
} RqioCoreStatus;

RQIO_CORE_EXPORT const char* rqio_core_version(void);
RQIO_CORE_EXPORT int rqio_core_create(const char* project_root, const char* executable_path, RqioCoreHandle** out_handle, char** out_error);
RQIO_CORE_EXPORT void rqio_core_destroy(RqioCoreHandle* handle);
RQIO_CORE_EXPORT int rqio_core_set_project_root(RqioCoreHandle* handle, const char* project_root, char** out_error);
RQIO_CORE_EXPORT int rqio_core_set_executable_path(RqioCoreHandle* handle, const char* executable_path, char** out_error);
RQIO_CORE_EXPORT int rqio_core_run_file(RqioCoreHandle* handle, const char* script_path, int prefer_vm, int* out_exit_code, char** out_error);
RQIO_CORE_EXPORT int rqio_core_run_source(RqioCoreHandle* handle, const char* virtual_filename, const char* source_code, int prefer_vm, int* out_exit_code, char** out_error);
RQIO_CORE_EXPORT int rqio_core_describe_file(RqioCoreHandle* handle, const char* script_path, char** out_json, char** out_error);
RQIO_CORE_EXPORT int rqio_core_describe_source(RqioCoreHandle* handle, const char* virtual_filename, const char* source_code, char** out_json, char** out_error);
RQIO_CORE_EXPORT void rqio_core_free_string(char* value);
