#pragma once

#include "NativeModuleABI.h"

int rqm_builtin_web_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message);
int rqm_builtin_app_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message);
int rqm_builtin_ui_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message);
int rqm_builtin_engine_invoke(const char* function_name, const char* json_args, char** json_result, char** error_message);

void rqm_builtin_free(char* memory);
