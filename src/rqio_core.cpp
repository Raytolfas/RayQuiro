#include "RqioCoreAPI.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "Compiler.h"
#include "Interpreter.h"
#include "BytecodeCompiler.h"
#include "VM.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
constexpr const char* kRqioCoreVersion = "0.1.1";

std::string make_error_message(const std::exception& error) {
    return std::string("[RayQuiroCore] ") + error.what();
}

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += ch; break;
        }
    }
    return out;
}

char* duplicate_c_string(const std::string& value) {
    char* buffer = static_cast<char*>(std::malloc(value.size() + 1));
    if (buffer == nullptr) {
        return nullptr;
    }
    std::memcpy(buffer, value.c_str(), value.size() + 1);
    return buffer;
}

void assign_error(char** outError, const std::string& value) {
    if (outError == nullptr) {
        return;
    }
    *outError = duplicate_c_string(value);
}

int run_vm_program(const BytecodeProgram& program) {
    VM vm;
    (void)vm.run(program);
    return 0;
}

std::string describe_resolved_program(
    const std::filesystem::path& scriptPath,
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& executablePath,
    ResolvedProgram& resolved
) {
    Interpreter interpreter(
        projectRoot,
        executablePath,
        resolved.builtinNamespaceAliases,
        resolved.builtinSymbolAliases);

    const bool supportsInterpreter = interpreter.supports(*resolved.program);
    const bool supportsVm = BytecodeCompiler::supports(
        *resolved.program,
        resolved.builtinNamespaceAliases,
        resolved.builtinSymbolAliases);

    std::vector<std::string> requiredNativeModules;
    const auto maybe_add_native_module = [&requiredNativeModules](const std::string& name) {
        if (std::find(requiredNativeModules.begin(), requiredNativeModules.end(), name) == requiredNativeModules.end()) {
            requiredNativeModules.push_back(name);
        }
    };
    for (const auto& pair : resolved.builtinNamespaceAliases) {
        if (pair.second == "app" || pair.second == "ui" || pair.second == "web" || pair.second == "engine") {
            maybe_add_native_module("rayquiro." + pair.second);
        }
    }

    std::ostringstream out;
    out << "{";
    out << "\"script_path\":\"" << json_escape(scriptPath.string()) << "\",";
    out << "\"project_root\":\"" << json_escape(projectRoot.string()) << "\",";
    out << "\"executable_path\":\"" << json_escape(executablePath.string()) << "\",";
    out << "\"supports_interpreter\":" << (supportsInterpreter ? "true" : "false") << ",";
    out << "\"supports_vm\":" << (supportsVm ? "true" : "false") << ",";
    out << "\"requires_native_build\":" << ((!supportsInterpreter && !supportsVm) ? "true" : "false") << ",";
    out << "\"requires_native_modules\":" << (!requiredNativeModules.empty() ? "true" : "false") << ",";
    out << "\"required_native_modules\":[";
    for (size_t index = 0; index < requiredNativeModules.size(); ++index) {
        if (index != 0) out << ",";
        out << "\"" << json_escape(requiredNativeModules[index]) << "\"";
    }
    out << "],";
    out << "\"builtin_namespace_aliases\":{";
    bool first = true;
    for (const auto& pair : resolved.builtinNamespaceAliases) {
        if (!first) out << ",";
        first = false;
        out << "\"" << json_escape(pair.first) << "\":\"" << json_escape(pair.second) << "\"";
    }
    out << "},";
    out << "\"builtin_symbol_aliases\":{";
    first = true;
    for (const auto& pair : resolved.builtinSymbolAliases) {
        if (!first) out << ",";
        first = false;
        out << "\"" << json_escape(pair.first) << "\":\"" << json_escape(pair.second) << "\"";
    }
    out << "}";
    out << "}";
    return out.str();
}

int run_resolved_program(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& executablePath,
    ResolvedProgram& resolved,
    int preferVm
) {
    if (preferVm != 0 &&
        BytecodeCompiler::supports(
            *resolved.program,
            resolved.builtinNamespaceAliases,
            resolved.builtinSymbolAliases)) {
        const BytecodeProgram program = BytecodeCompiler::compile(
            *resolved.program,
            resolved.builtinNamespaceAliases,
            resolved.builtinSymbolAliases);
        return run_vm_program(program);
    }

    Interpreter interpreter(
        projectRoot,
        executablePath,
        resolved.builtinNamespaceAliases,
        resolved.builtinSymbolAliases);

    if (interpreter.supports(*resolved.program)) {
        const std::filesystem::path previousPath = std::filesystem::current_path();
        std::filesystem::current_path(projectRoot);
        try {
            const int code = interpreter.run(*resolved.program);
            std::filesystem::current_path(previousPath);
            return code;
        } catch (...) {
            std::filesystem::current_path(previousPath);
            throw;
        }
    }

    throw std::runtime_error(
        "rqio_core.dll currently supports interpreter/VM execution only. "
        "This script still needs native build mode from rqio.exe.");
}

std::filesystem::path normalize_or_default_project_root(const char* projectRootRaw) {
    if (projectRootRaw == nullptr || projectRootRaw[0] == '\0') {
        return std::filesystem::current_path();
    }
    return std::filesystem::absolute(std::filesystem::path(projectRootRaw));
}

std::filesystem::path normalize_or_default_executable_path(const char* executablePathRaw) {
    if (executablePathRaw != nullptr && executablePathRaw[0] != '\0') {
        return std::filesystem::absolute(std::filesystem::path(executablePathRaw));
    }
#ifdef _WIN32
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::filesystem::path(buffer);
    }
#endif
    return std::filesystem::current_path() / "rqio.exe";
}

std::filesystem::path write_temp_source_file(
    const std::filesystem::path& projectRoot,
    const char* virtualFilename,
    const char* sourceCode
) {
    if (sourceCode == nullptr) {
        throw std::runtime_error("source_code is null.");
    }

    const std::string filename = (virtualFilename == nullptr || virtualFilename[0] == '\0')
        ? std::string("__rqio_core_temp__.rq")
        : std::string(virtualFilename);

    const std::filesystem::path tempRoot = projectRoot / ".rq_cache" / "rqio_core";
    std::filesystem::create_directories(tempRoot);
    const std::filesystem::path filePath = tempRoot / filename;

    std::ofstream output(filePath, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot write temporary RayQuiro source file: " + filePath.string());
    }
    output << sourceCode;
    return filePath;
}
}

struct RqioCoreHandle {
    std::filesystem::path projectRoot;
    std::filesystem::path executablePath;
};

RQIO_CORE_EXPORT const char* rqio_core_version(void) {
    return kRqioCoreVersion;
}

RQIO_CORE_EXPORT int rqio_core_create(const char* project_root, const char* executable_path, RqioCoreHandle** out_handle, char** out_error) {
    if (out_handle == nullptr) {
        assign_error(out_error, "[RayQuiroCore] out_handle is null.");
        return RQIO_CORE_ERROR;
    }

    try {
        auto handle = std::make_unique<RqioCoreHandle>();
        handle->projectRoot = normalize_or_default_project_root(project_root);
        handle->executablePath = normalize_or_default_executable_path(executable_path);
        *out_handle = handle.release();
        if (out_error) {
            *out_error = nullptr;
        }
        return RQIO_CORE_OK;
    } catch (const std::exception& error) {
        assign_error(out_error, make_error_message(error));
        return RQIO_CORE_ERROR;
    }
}

RQIO_CORE_EXPORT void rqio_core_destroy(RqioCoreHandle* handle) {
    delete handle;
}

RQIO_CORE_EXPORT int rqio_core_set_project_root(RqioCoreHandle* handle, const char* project_root, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "[RayQuiroCore] handle is null.");
        return RQIO_CORE_ERROR;
    }

    try {
        handle->projectRoot = normalize_or_default_project_root(project_root);
        if (out_error) *out_error = nullptr;
        return RQIO_CORE_OK;
    } catch (const std::exception& error) {
        assign_error(out_error, make_error_message(error));
        return RQIO_CORE_ERROR;
    }
}

RQIO_CORE_EXPORT int rqio_core_set_executable_path(RqioCoreHandle* handle, const char* executable_path, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "[RayQuiroCore] handle is null.");
        return RQIO_CORE_ERROR;
    }

    try {
        handle->executablePath = normalize_or_default_executable_path(executable_path);
        if (out_error) *out_error = nullptr;
        return RQIO_CORE_OK;
    } catch (const std::exception& error) {
        assign_error(out_error, make_error_message(error));
        return RQIO_CORE_ERROR;
    }
}

RQIO_CORE_EXPORT int rqio_core_run_file(RqioCoreHandle* handle, const char* script_path, int prefer_vm, int* out_exit_code, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "[RayQuiroCore] handle is null.");
        return RQIO_CORE_ERROR;
    }
    if (script_path == nullptr || script_path[0] == '\0') {
        assign_error(out_error, "[RayQuiroCore] script_path is empty.");
        return RQIO_CORE_ERROR;
    }

    try {
        const std::filesystem::path scriptPath = std::filesystem::absolute(std::filesystem::path(script_path));
        auto resolved = Compiler::resolveForExecution(scriptPath);
        const int code = run_resolved_program(handle->projectRoot, handle->executablePath, resolved, prefer_vm);
        if (out_exit_code != nullptr) {
            *out_exit_code = code;
        }
        if (out_error) {
            *out_error = nullptr;
        }
        return RQIO_CORE_OK;
    } catch (const std::exception& error) {
        assign_error(out_error, make_error_message(error));
        return RQIO_CORE_ERROR;
    }
}

RQIO_CORE_EXPORT int rqio_core_run_source(RqioCoreHandle* handle, const char* virtual_filename, const char* source_code, int prefer_vm, int* out_exit_code, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "[RayQuiroCore] handle is null.");
        return RQIO_CORE_ERROR;
    }

    try {
        const std::filesystem::path tempFile = write_temp_source_file(handle->projectRoot, virtual_filename, source_code);
        auto resolved = Compiler::resolveForExecution(tempFile);
        const int code = run_resolved_program(handle->projectRoot, handle->executablePath, resolved, prefer_vm);
        if (out_exit_code != nullptr) {
            *out_exit_code = code;
        }
        if (out_error) {
            *out_error = nullptr;
        }
        return RQIO_CORE_OK;
    } catch (const std::exception& error) {
        assign_error(out_error, make_error_message(error));
        return RQIO_CORE_ERROR;
    }
}

RQIO_CORE_EXPORT int rqio_core_describe_file(RqioCoreHandle* handle, const char* script_path, char** out_json, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "[RayQuiroCore] handle is null.");
        return RQIO_CORE_ERROR;
    }
    if (out_json == nullptr) {
        assign_error(out_error, "[RayQuiroCore] out_json is null.");
        return RQIO_CORE_ERROR;
    }
    if (script_path == nullptr || script_path[0] == '\0') {
        assign_error(out_error, "[RayQuiroCore] script_path is empty.");
        return RQIO_CORE_ERROR;
    }

    try {
        const std::filesystem::path scriptPath = std::filesystem::absolute(std::filesystem::path(script_path));
        auto resolved = Compiler::resolveForExecution(scriptPath);
        *out_json = duplicate_c_string(describe_resolved_program(
            scriptPath,
            handle->projectRoot,
            handle->executablePath,
            resolved));
        if (out_error) *out_error = nullptr;
        return RQIO_CORE_OK;
    } catch (const std::exception& error) {
        assign_error(out_error, make_error_message(error));
        return RQIO_CORE_ERROR;
    }
}

RQIO_CORE_EXPORT int rqio_core_describe_source(RqioCoreHandle* handle, const char* virtual_filename, const char* source_code, char** out_json, char** out_error) {
    if (handle == nullptr) {
        assign_error(out_error, "[RayQuiroCore] handle is null.");
        return RQIO_CORE_ERROR;
    }
    if (out_json == nullptr) {
        assign_error(out_error, "[RayQuiroCore] out_json is null.");
        return RQIO_CORE_ERROR;
    }

    try {
        const std::filesystem::path tempFile = write_temp_source_file(handle->projectRoot, virtual_filename, source_code);
        auto resolved = Compiler::resolveForExecution(tempFile);
        *out_json = duplicate_c_string(describe_resolved_program(
            tempFile,
            handle->projectRoot,
            handle->executablePath,
            resolved));
        if (out_error) *out_error = nullptr;
        return RQIO_CORE_OK;
    } catch (const std::exception& error) {
        assign_error(out_error, make_error_message(error));
        return RQIO_CORE_ERROR;
    }
}

RQIO_CORE_EXPORT void rqio_core_free_string(char* value) {
    if (value != nullptr) {
        std::free(value);
    }
}
