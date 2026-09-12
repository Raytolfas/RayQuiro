#pragma once

#include <filesystem>
#include <string>
#include <optional>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace LLVMToolchain {

namespace fs = std::filesystem;

static fs::path cacheRoot() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf) == S_OK)
        return fs::path(buf) / "rqio" / "toolchain";
    return fs::temp_directory_path() / "rqio" / "toolchain";
#else
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return fs::path(home) / ".local" / "share" / "rqio" / "toolchain";
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

struct Toolchain {
    std::string clang;
    std::string llvmLink;
    bool valid = false;
};

static Toolchain find() {
    Toolchain t;

    fs::path root = cacheRoot();

#ifdef _WIN32
    fs::path cached = root / "clang.exe";
#else
    fs::path cached = root / "clang";
#endif

    if (fs::exists(cached)) {
        t.clang = cached.string();
        t.valid = true;
        return t;
    }

    if (commandExists("clang")) {
        t.clang = "clang";
        t.valid = true;
        return t;
    }

    if (commandExists("gcc")) {
        t.clang = "gcc";
        t.valid = true;
        return t;
    }

    return t;
}

static bool ensureAvailable(const std::string& statusPrefix) {
    Toolchain t = find();
    if (t.valid) return true;

    (void)statusPrefix;
    return false;
}

static std::string resolveClang() {
    Toolchain t = find();
    return t.valid ? t.clang : "";
}

static std::string targetTriple() {
#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
    return "x86_64-pc-windows-gnu";
#elif defined(_WIN32)
    return "i686-pc-windows-gnu";
#elif defined(__APPLE__) && defined(__arm64__)
    return "aarch64-apple-macosx11.0.0";
#elif defined(__APPLE__)
    return "x86_64-apple-macosx10.15.0";
#elif defined(__aarch64__)
    return "aarch64-unknown-linux-gnu";
#else
    return "x86_64-unknown-linux-gnu";
#endif
}

}

