#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace TCCToolchain {

namespace fs = std::filesystem;

static fs::path cacheRoot() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf) == S_OK)
        return fs::path(buf) / "rqio" / "toolchain" / "tcc";
    return fs::temp_directory_path() / "rqio" / "toolchain" / "tcc";
#else
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return fs::path(home) / ".local" / "share" / "rqio" / "toolchain" / "tcc";
#endif
}

static bool commandExists(const std::string& cmd) {
#ifdef _WIN32
    std::string check = "where " + cmd + " >nul 2>&1";
#else
    std::string check = "which " + cmd + " >/dev/null 2>&1";
#endif
    return std::system(check.c_str()) == 0;
}

static std::string resolveTcc(const fs::path& exePath) {
    fs::path exeDir = exePath.parent_path();
    std::vector<fs::path> candidates;

#ifdef _WIN32
    candidates.push_back(exeDir / "tools" / "tcc" / "tcc.exe");
    candidates.push_back(exeDir / "tcc.exe");
    candidates.push_back(fs::current_path() / "tools" / "tcc" / "tcc.exe");
    candidates.push_back(cacheRoot() / "tcc.exe");
#else
    candidates.push_back(exeDir / "tools" / "tcc" / "tcc");
    candidates.push_back(exeDir / "tcc");
    candidates.push_back(fs::current_path() / "tools" / "tcc" / "tcc");
    candidates.push_back(cacheRoot() / "tcc");
#endif

    for (const auto& p : candidates) {
        std::error_code ec;
        if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
            return p.string();
        }
    }

    if (commandExists("tcc")) {
        return "tcc";
    }

    return "";
}

}
