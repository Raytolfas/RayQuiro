#pragma once

#include <cstddef>

#ifdef _WIN32
#define RQM_EXPORT extern "C" __declspec(dllexport)
#else
#define RQM_EXPORT extern "C"
#endif

using RayQuiroModuleInvoke = int (*)(const char* function_name, const char* json_args, char** json_result, char** error_message);
using RayQuiroModuleFree = void (*)(char*);

RQM_EXPORT int rqm_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message);
RQM_EXPORT void rqm_free(char* memory);
