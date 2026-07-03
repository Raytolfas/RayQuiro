#pragma once

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace RayQuiroUserPaths {
inline std::optional<std::string> getenvString(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

inline std::filesystem::path userHome() {
    if (const auto profile = getenvString("USERPROFILE")) {
        return std::filesystem::path(profile.value());
    }
    if (const auto home = getenvString("HOME")) {
        return std::filesystem::path(home.value());
    }
    return std::filesystem::current_path();
}

inline std::filesystem::path rqioHome() {
    return userHome() / ".rqio";
}

inline std::filesystem::path frameworksRoot() {
    return rqioHome() / "frameworks";
}

inline std::filesystem::path systemModulesRoot() {
#ifdef _WIN32
    if (const auto programFiles = getenvString("ProgramFiles")) {
        return std::filesystem::path(programFiles.value()) / "RayQuiro" / "modules";
    }
    return std::filesystem::path("C:/Program Files/RayQuiro/modules");
#elif defined(__APPLE__)
    return std::filesystem::path("/Library/Application Support/RayQuiro/modules");
#else
    return std::filesystem::path("/usr/local/lib/rayquiro/modules");
#endif
}

inline std::filesystem::path modulesRoot() {
    return systemModulesRoot();
}

inline std::filesystem::path updatesRoot() {
    return rqioHome() / "updates";
}

inline std::filesystem::path tempRoot() {
    return rqioHome() / "tmp";
}
}
