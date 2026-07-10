#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "UserPaths.h"

namespace RayQuiroCliServices {
struct FrameworkPackage {
    std::string name;
    std::string repo;
    std::string branch = "main";
    std::string archiveUrl;
    std::string description;
};

struct NativeModulePackage {
    std::string name;
    std::string file;
    std::string source;
};

struct UpdateManifest {
    std::string version;
    std::string downloadUrl;
    std::string notesUrl;
};

inline std::string trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

inline std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::string sanitizeName(std::string value) {
    value = toLower(trim(value));
    for (char& ch : value) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.')) {
            ch = '-';
        }
    }
    return value;
}

inline std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

inline void writeText(const std::filesystem::path& path, const std::string& contents) {
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Cannot write file: " + path.string());
    }
    output << contents;
}

inline std::string jsonEscape(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += ch; break;
        }
    }
    return result;
}

inline std::string nowIsoUtc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm utcTime{};
#ifdef _WIN32
    gmtime_s(&utcTime, &nowTime);
#else
    gmtime_r(&nowTime, &utcTime);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime);
    return buffer;
}

inline std::string frameworksRegistryUrl() {
    if (const auto custom = RayQuiroUserPaths::getenvString("RAYQUIRO_FRAMEWORKS_REGISTRY_URL")) {
        return custom.value();
    }
    return "https://rq.raytolfas.cc/framework";
}

inline std::string updateManifestUrl() {
    if (const auto custom = RayQuiroUserPaths::getenvString("RAYQUIRO_UPDATE_URL")) {
        return custom.value();
    }
    return "https://rq.raytolfas.cc/update";
}

inline std::string nativeModulesBaseUrl() {
    if (const auto custom = RayQuiroUserPaths::getenvString("RAYQUIRO_MODULES_BASE_URL")) {
        return custom.value();
    }
    return "https://rq.raytolfas.cc/modules/";
}

inline std::string nativeModulesRegistryUrl() {
    if (const auto custom = RayQuiroUserPaths::getenvString("RAYQUIRO_MODULES_REGISTRY_URL")) {
        return custom.value();
    }
    return "https://rq.raytolfas.cc/modules";
}

inline bool isNativeModuleSpec(const std::string& spec) {
    return spec.rfind("rayquiro.", 0) == 0 && spec.find('/') == std::string::npos;
}

inline std::string nativeModuleShortName(const std::string& spec) {
    return trim(spec.substr(std::string("rayquiro.").size()));
}

inline std::string nativeModuleExtension() {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

inline std::string repoBasename(const std::string& repo) {
    const size_t slash = repo.find_last_of('/');
    if (slash == std::string::npos) {
        return repo;
    }
    return repo.substr(slash + 1);
}

inline std::optional<std::string> findJsonStringValue(const std::string& json, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() >= 2) {
        return match[1].str();
    }
    return std::nullopt;
}

inline std::vector<std::string> extractJsonObjectsFromArray(const std::string& json, const std::string& key) {
    size_t arrayStart = 0;
    if (!key.empty()) {
        const std::string keyToken = "\"" + key + "\"";
        const size_t keyPos = json.find(keyToken);
        if (keyPos == std::string::npos) {
            return {};
        }
        arrayStart = json.find('[', keyPos);
        if (arrayStart == std::string::npos) {
            return {};
        }
    } else {
        arrayStart = json.find('[');
        if (arrayStart == std::string::npos) {
            return {};
        }
    }

    std::vector<std::string> objects;
    int arrayDepth = 0;
    int objectDepth = 0;
    size_t objectStart = std::string::npos;
    bool inString = false;
    bool escaped = false;

    for (size_t i = arrayStart; i < json.size(); ++i) {
        const char ch = json[i];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '[') {
            ++arrayDepth;
            continue;
        }
        if (ch == ']') {
            --arrayDepth;
            if (arrayDepth <= 0) {
                break;
            }
            continue;
        }
        if (ch == '{') {
            if (objectDepth == 0) {
                objectStart = i;
            }
            ++objectDepth;
            continue;
        }
        if (ch == '}') {
            --objectDepth;
            if (objectDepth == 0 && objectStart != std::string::npos) {
                objects.push_back(json.substr(objectStart, i - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }

    return objects;
}

inline std::vector<FrameworkPackage> parseFrameworkRegistry(const std::string& json) {
    std::vector<FrameworkPackage> packages;
    std::vector<std::string> objects = extractJsonObjectsFromArray(json, "frameworks");
    if (objects.empty()) {
        objects = extractJsonObjectsFromArray(json, "");
    }

    for (const auto& object : objects) {
        const auto name = findJsonStringValue(object, "name");
        const auto repo = findJsonStringValue(object, "repo");
        if (!name.has_value() || !repo.has_value()) {
            continue;
        }

        FrameworkPackage package;
        package.name = name.value();
        package.repo = repo.value();
        if (const auto branch = findJsonStringValue(object, "branch")) {
            package.branch = branch.value();
        }
        if (const auto archiveUrl = findJsonStringValue(object, "archive_url")) {
            package.archiveUrl = archiveUrl.value();
        }
        if (const auto description = findJsonStringValue(object, "description")) {
            package.description = description.value();
        }
        packages.push_back(package);
    }

    return packages;
}

inline std::vector<NativeModulePackage> parseNativeModuleRegistry(const std::string& json) {
    std::vector<NativeModulePackage> packages;
    std::vector<std::string> objects = extractJsonObjectsFromArray(json, "modules");
    if (objects.empty()) {
        objects = extractJsonObjectsFromArray(json, "");
    }

    for (const auto& object : objects) {
        const auto name = findJsonStringValue(object, "name");
        if (!name.has_value()) {
            continue;
        }

        NativeModulePackage package;
        package.name = name.value();
        package.file = findJsonStringValue(object, "file").value_or("");
        package.source = findJsonStringValue(object, "source").value_or("");
        if (package.source.empty()) {
            package.source = findJsonStringValue(object, "download_url").value_or("");
        }
        if (package.source.empty()) {
            package.source = findJsonStringValue(object, "url").value_or("");
        }
        packages.push_back(package);
    }

    return packages;
}

inline UpdateManifest parseUpdateManifest(const std::string& json) {
    UpdateManifest manifest;
    manifest.version = findJsonStringValue(json, "version").value_or("");
    manifest.downloadUrl = findJsonStringValue(json, "download_url").value_or("");
    if (manifest.downloadUrl.empty()) {
        manifest.downloadUrl = findJsonStringValue(json, "asset_url").value_or("");
    }
    if (manifest.downloadUrl.empty()) {
        manifest.downloadUrl = findJsonStringValue(json, "url").value_or("");
    }
    manifest.notesUrl = findJsonStringValue(json, "notes_url").value_or("");
    return manifest;
}

inline void copyRecursively(const std::filesystem::path& source, const std::filesystem::path& target) {
    std::filesystem::create_directories(target);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
        const auto relative = std::filesystem::relative(entry.path(), source);
        const auto destination = target / relative;
        if (entry.is_directory()) {
            std::filesystem::create_directories(destination);
        } else if (entry.is_regular_file()) {
            const auto parent = destination.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }
            std::filesystem::copy_file(entry.path(), destination, std::filesystem::copy_options::overwrite_existing);
        }
    }
}

inline std::filesystem::path makeTempPath(const std::string& prefix, const std::string& extension = "") {
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::filesystem::create_directories(RayQuiroUserPaths::tempRoot());
    return RayQuiroUserPaths::tempRoot() / (prefix + "_" + std::to_string(stamp) + extension);
}

inline int runPowerShellScript(const std::string& script, const std::string& prefix) {
#ifndef _WIN32
    (void)script;
    (void)prefix;
    throw std::runtime_error("This feature is currently supported on Windows only.");
#else
    const std::filesystem::path scriptPath = makeTempPath(prefix, ".ps1");
    writeText(scriptPath, script);
    const std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath.string() + "\"";
    const int code = std::system(command.c_str());
    std::error_code ignored;
    std::filesystem::remove(scriptPath, ignored);
    return code;
#endif
}

inline std::string psQuote(const std::string& value) {
    std::string result = "'";
    for (char ch : value) {
        if (ch == '\'') {
            result += "''";
        } else {
            result += ch;
        }
    }
    result += "'";
    return result;
}

inline void fetchResource(const std::string& source, const std::filesystem::path& outPath) {
    const std::filesystem::path localPath = std::filesystem::path(source);
    if ((source.rfind("http://", 0) != 0 && source.rfind("https://", 0) != 0) &&
        std::filesystem::exists(localPath)) {
        std::filesystem::create_directories(outPath.parent_path());
        std::filesystem::copy_file(localPath, outPath, std::filesystem::copy_options::overwrite_existing);
        return;
    }

#ifndef _WIN32
    std::filesystem::create_directories(outPath.parent_path());
    std::string command = "curl -L -s -o \"" + outPath.string() + "\" \"" + source + "\"";
    int code = std::system(command.c_str());
    if (code != 0) {
        command = "wget -q -O \"" + outPath.string() + "\" \"" + source + "\"";
        code = std::system(command.c_str());
    }
    if (code != 0) {
        throw std::runtime_error("Failed to download: " + source);
    }
#else
    std::ostringstream script;
    script << "$ErrorActionPreference = 'Stop'\n";
    script << "$ProgressPreference = 'SilentlyContinue'\n";
    script << "New-Item -ItemType Directory -Force -Path " << psQuote(outPath.parent_path().string()) << " | Out-Null\n";
    script << "Invoke-WebRequest -Uri " << psQuote(source) << " -OutFile " << psQuote(outPath.string()) << "\n";
    if (runPowerShellScript(script.str(), "rqio_fetch") != 0) {
        throw std::runtime_error("Failed to download: " + source);
    }
#endif
}

inline void expandZipArchive(const std::filesystem::path& zipPath, const std::filesystem::path& outDir) {
#ifndef _WIN32
    std::filesystem::create_directories(outDir);
    std::string command = "unzip -q -o \"" + zipPath.string() + "\" -d \"" + outDir.string() + "\"";
    int code = std::system(command.c_str());
    if (code != 0) {
        throw std::runtime_error("Failed to extract archive: " + zipPath.string());
    }
#else
    std::ostringstream script;
    script << "$ErrorActionPreference = 'Stop'\n";
    script << "if (Test-Path " << psQuote(outDir.string()) << ") { Remove-Item -Recurse -Force " << psQuote(outDir.string()) << " }\n";
    script << "New-Item -ItemType Directory -Force -Path " << psQuote(outDir.string()) << " | Out-Null\n";
    script << "Expand-Archive -Path " << psQuote(zipPath.string()) << " -DestinationPath " << psQuote(outDir.string()) << " -Force\n";
    if (runPowerShellScript(script.str(), "rqio_expand") != 0) {
        throw std::runtime_error("Failed to extract archive: " + zipPath.string());
    }
#endif
}


inline std::filesystem::path detectExtractedRoot(const std::filesystem::path& extractDir) {
    std::vector<std::filesystem::path> directories;
    for (const auto& entry : std::filesystem::directory_iterator(extractDir)) {
        if (entry.is_directory()) {
            directories.push_back(entry.path());
        }
    }
    if (directories.size() == 1) {
        return directories.front();
    }
    return extractDir;
}

inline std::vector<int> parseVersionParts(const std::string& version) {
    std::vector<int> parts;
    std::stringstream stream(version);
    std::string token;
    while (std::getline(stream, token, '.')) {
        token = trim(token);
        std::string digits;
        for (char ch : token) {
            if (std::isdigit(static_cast<unsigned char>(ch))) {
                digits += ch;
            } else {
                break;
            }
        }
        parts.push_back(digits.empty() ? 0 : std::stoi(digits));
    }
    return parts;
}

inline int compareVersions(const std::string& left, const std::string& right) {
    const auto leftParts = parseVersionParts(left);
    const auto rightParts = parseVersionParts(right);
    const size_t count = std::max(leftParts.size(), rightParts.size());
    for (size_t i = 0; i < count; ++i) {
        const int a = i < leftParts.size() ? leftParts[i] : 0;
        const int b = i < rightParts.size() ? rightParts[i] : 0;
        if (a < b) return -1;
        if (a > b) return 1;
    }
    return 0;
}

inline void writeUpdateState(
    const std::string& currentVersion,
    const std::string& remoteVersion,
    const std::string& status,
    const std::string& manifestUrl,
    const std::string& downloadUrl
) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"current_version\": \"" << jsonEscape(currentVersion) << "\",\n";
    json << "  \"remote_version\": \"" << jsonEscape(remoteVersion) << "\",\n";
    json << "  \"last_check_utc\": \"" << jsonEscape(nowIsoUtc()) << "\",\n";
    json << "  \"status\": \"" << jsonEscape(status) << "\",\n";
    json << "  \"manifest_url\": \"" << jsonEscape(manifestUrl) << "\",\n";
    json << "  \"download_url\": \"" << jsonEscape(downloadUrl) << "\"\n";
    json << "}\n";
    writeText(RayQuiroUserPaths::rqioHome() / "update.json", json.str());
}

inline FrameworkPackage parseRepoSpec(const std::string& spec) {
    FrameworkPackage package;
    const std::string trimmed = trim(spec);
    const size_t atPos = trimmed.find('@');
    const std::string repoPart = atPos == std::string::npos ? trimmed : trimmed.substr(0, atPos);
    if (atPos != std::string::npos) {
        package.branch = trim(trimmed.substr(atPos + 1));
    }
    if (repoPart.find('/') == std::string::npos) {
        throw std::runtime_error("Framework repo must look like owner/repo");
    }
    package.repo = repoPart;
    package.name = repoBasename(repoPart);
    if (package.branch.empty()) {
        package.branch = "main";
    }
    return package;
}

inline FrameworkPackage findApprovedFramework(const std::string& name) {
    const std::filesystem::path registryPath = makeTempPath("rqio_frameworks", ".json");
    fetchResource(frameworksRegistryUrl(), registryPath);
    const auto packages = parseFrameworkRegistry(readText(registryPath));
    std::error_code ignored;
    std::filesystem::remove(registryPath, ignored);

    const std::string wanted = toLower(trim(name));
    for (const auto& package : packages) {
        if (toLower(package.name) == wanted) {
            return package;
        }
    }

    throw std::runtime_error("Framework '" + name + "' was not found in the approved registry.");
}

inline std::string frameworkArchiveUrl(const FrameworkPackage& package) {
    if (!package.archiveUrl.empty()) {
        return package.archiveUrl;
    }
    return "https://codeload.github.com/" + package.repo + "/zip/refs/heads/" + package.branch;
}

inline std::filesystem::path frameworkInstallPath(const FrameworkPackage& package) {
    return RayQuiroUserPaths::frameworksRoot() / sanitizeName(package.name);
}

inline std::filesystem::path frameworkInstallPath(
    const FrameworkPackage& package,
    const std::filesystem::path& installRoot
) {
    return installRoot / sanitizeName(package.name);
}

inline void writeFrameworkMetadata(const FrameworkPackage& package, const std::filesystem::path& installPath) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"name\": \"" << jsonEscape(package.name) << "\",\n";
    json << "  \"repo\": \"" << jsonEscape(package.repo) << "\",\n";
    json << "  \"branch\": \"" << jsonEscape(package.branch) << "\",\n";
    json << "  \"installed_at_utc\": \"" << jsonEscape(nowIsoUtc()) << "\"\n";
    json << "}\n";
    writeText(installPath / ".rqio-framework.json", json.str());
}

inline int installFrameworkPackage(
    const FrameworkPackage& package,
    const std::filesystem::path& installRoot = RayQuiroUserPaths::frameworksRoot()
) {
    const std::filesystem::path workDir = makeTempPath("rqio_framework_pkg");
    const std::filesystem::path archivePath = workDir / "package.zip";
    const std::filesystem::path extractDir = workDir / "extract";
    const std::filesystem::path installPath = frameworkInstallPath(package, installRoot);

    std::filesystem::create_directories(workDir);
    fetchResource(frameworkArchiveUrl(package), archivePath);
    expandZipArchive(archivePath, extractDir);

    const std::filesystem::path extractedRoot = detectExtractedRoot(extractDir);
    std::filesystem::create_directories(installPath.parent_path());
    std::error_code ignored;
    std::filesystem::remove_all(installPath, ignored);

    try {
        std::filesystem::rename(extractedRoot, installPath);
    } catch (...) {
        copyRecursively(extractedRoot, installPath);
    }

    writeFrameworkMetadata(package, installPath);
    std::filesystem::remove_all(workDir, ignored);

    std::cout << "[RayQuiro] Installed framework '" << package.name << "' into " << installPath.string() << std::endl;
    return 0;
}


inline int installFramework(
    const std::string& spec,
    const std::filesystem::path& installRoot = RayQuiroUserPaths::frameworksRoot()
) {
    if (spec.find('/') != std::string::npos) {
        return installFrameworkPackage(parseRepoSpec(spec), installRoot);
    }
    return installFrameworkPackage(findApprovedFramework(spec), installRoot);
}

inline int installApprovedFramework(
    const std::string& name,
    const std::filesystem::path& installRoot = RayQuiroUserPaths::frameworksRoot()
) {
    return installFrameworkPackage(findApprovedFramework(name), installRoot);
}

inline int installNativeModule(
    const std::string& spec,
    const std::filesystem::path& installRoot = RayQuiroUserPaths::systemModulesRoot()
) {
    if (!isNativeModuleSpec(spec)) {
        throw std::runtime_error("Native module names must look like rayquiro.<module>");
    }

    const std::string shortName = sanitizeName(nativeModuleShortName(spec));
    std::string fileName = shortName + nativeModuleExtension();
    std::string source = nativeModulesBaseUrl() + fileName;

    try {
        const std::filesystem::path registryPath = makeTempPath("rqio_modules", ".json");
        fetchResource(nativeModulesRegistryUrl(), registryPath);
        const auto packages = parseNativeModuleRegistry(readText(registryPath));
        std::error_code ignored;
        std::filesystem::remove(registryPath, ignored);

        const std::string wantedName = toLower(trim(spec));
        const std::string wantedFile = toLower(fileName);
        for (const auto& package : packages) {
            const std::string packageName = toLower(trim(package.name));
            const std::string packageFile = toLower(trim(package.file));
            if (packageName == wantedName || (!packageFile.empty() && packageFile == wantedFile)) {
                if (!package.file.empty()) {
                     fileName = package.file;
                }
                if (!package.source.empty()) {
                     source = package.source;
                } else {
                     source = nativeModulesBaseUrl() + fileName;
                }
                break;
            }
        }
    } catch (...) {
        // Fall back to the default module location when registry lookup fails.
    }

    const std::filesystem::path targetPath = installRoot / fileName;
    fetchResource(source, targetPath);

    std::ostringstream metadata;
    metadata << "{\n";
    metadata << "  \"name\": \"" << jsonEscape(spec) << "\",\n";
    metadata << "  \"file\": \"" << jsonEscape(fileName) << "\",\n";
    metadata << "  \"source\": \"" << jsonEscape(source) << "\",\n";
    metadata << "  \"installed_at_utc\": \"" << jsonEscape(nowIsoUtc()) << "\"\n";
    metadata << "}\n";
    writeText(installRoot / (shortName + ".rqio-module.json"), metadata.str());

    std::cout << "[RayQuiro] Installed native module '" << spec << "' into " << targetPath.string() << std::endl;
    return 0;
}


inline void launchHiddenBatch(const std::filesystem::path& batchPath) {
#ifndef _WIN32
    (void)batchPath;
    throw std::runtime_error("Self-update is currently supported on Windows only.");
#else
    std::ostringstream script;
    script << "$batch = " << psQuote(batchPath.string()) << "\n";
    script << "Start-Process -WindowStyle Hidden -FilePath 'cmd.exe' -ArgumentList '/c', $batch | Out-Null\n";
    if (runPowerShellScript(script.str(), "rqio_launch_update") != 0) {
        throw std::runtime_error("Failed to launch updater batch.");
    }
#endif
}

inline void createSelfUpdateScripts(
    const std::filesystem::path& exePath,
    const std::filesystem::path& assetPath,
    const std::string& currentVersion,
    const std::string& remoteVersion,
    const std::string& relaunchCmd = ""
) {
    const std::filesystem::path updatesRoot = RayQuiroUserPaths::updatesRoot();
    const std::filesystem::path applyScriptPath = updatesRoot / "apply_update.ps1";
    const std::filesystem::path batchPath = updatesRoot / "apply_update.bat";
    const std::filesystem::path extractDir = updatesRoot / "extract";
    const std::filesystem::path statePath = RayQuiroUserPaths::rqioHome() / "update.json";
    const std::filesystem::path targetDir = exePath.parent_path();

    std::ostringstream ps;
    ps << "$ErrorActionPreference = 'Stop'\n";
    ps << "$asset = " << psQuote(assetPath.string()) << "\n";
    ps << "$targetExe = " << psQuote(exePath.string()) << "\n";
    ps << "$targetDir = " << psQuote(targetDir.string()) << "\n";
    ps << "$extractDir = " << psQuote(extractDir.string()) << "\n";
    ps << "$statePath = " << psQuote(statePath.string()) << "\n";
    ps << "New-Item -ItemType Directory -Force -Path $targetDir | Out-Null\n";
    ps << "if (Test-Path $extractDir) { Remove-Item -Recurse -Force $extractDir }\n";
    ps << "if ($asset.ToLower().EndsWith('.zip')) {\n";
    ps << "  Expand-Archive -Path $asset -DestinationPath $extractDir -Force\n";
    ps << "  $rqio = Get-ChildItem -Path $extractDir -Filter 'rqio.exe' -Recurse | Select-Object -First 1\n";
    ps << "  if ($null -eq $rqio) { throw 'rqio.exe was not found in the update package.' }\n";
    ps << "  $packageRoot = Split-Path -Parent $rqio.FullName\n";
    ps << "  Copy-Item -Path (Join-Path $packageRoot '*') -Destination $targetDir -Recurse -Force\n";
    ps << "} else {\n";
    ps << "  Copy-Item -Path $asset -Destination $targetExe -Force\n";
    ps << "}\n";
    ps << "$state = @\"\n";
    ps << "{\n";
    ps << "  \"current_version\": \"" << jsonEscape(remoteVersion) << "\",\n";
    ps << "  \"remote_version\": \"" << jsonEscape(remoteVersion) << "\",\n";
    ps << "  \"last_check_utc\": \"" << jsonEscape(nowIsoUtc()) << "\",\n";
    ps << "  \"status\": \"applied\",\n";
    ps << "  \"manifest_url\": \"" << jsonEscape(updateManifestUrl()) << "\",\n";
    ps << "  \"download_url\": \"" << jsonEscape(assetPath.string()) << "\"\n";
    ps << "}\n";
    ps << "\"@\n";
    ps << "Set-Content -Path $statePath -Value $state -Encoding UTF8\n";
    writeText(applyScriptPath, ps.str());

    std::ostringstream bat;
    bat << "@echo off\r\n";
    bat << "setlocal\r\n";
#ifdef _WIN32
    bat << "set \"RQIO_PID=" << GetCurrentProcessId() << "\"\r\n";
#else
    bat << "set \"RQIO_PID=0\"\r\n";
#endif
    bat << ":wait_loop\r\n";
    bat << "tasklist /FI \"PID eq %RQIO_PID%\" 2>NUL | find \"%RQIO_PID%\" >NUL\r\n";
    bat << "if not errorlevel 1 (\r\n";
    bat << "  timeout /t 1 /nobreak >NUL\r\n";
    bat << "  goto wait_loop\r\n";
    bat << ")\r\n";
    bat << "powershell -NoProfile -ExecutionPolicy Bypass -File \"" << applyScriptPath.string() << "\" > \"" << (updatesRoot / "apply_update.log").string() << "\" 2>&1\r\n";
    if (!relaunchCmd.empty()) {
        bat << "echo [RayQuiro] Relaunching in new version...\r\n";
        bat << "\"" << exePath.string() << "\" " << relaunchCmd << "\r\n";
    }
    bat << "del \"%~f0\"\r\n";
    writeText(batchPath, bat.str());

    writeUpdateState(currentVersion, remoteVersion, "scheduled", updateManifestUrl(), assetPath.string());
    launchHiddenBatch(batchPath);
}



inline std::time_t parseIsoUtc(const std::string& s) {
    if (s.size() < 19) return 0;
    try {
        std::tm t{};
        t.tm_year = std::stoi(s.substr(0, 4)) - 1900;
        t.tm_mon = std::stoi(s.substr(5, 2)) - 1;
        t.tm_mday = std::stoi(s.substr(8, 2));
        t.tm_hour = std::stoi(s.substr(11, 2));
        t.tm_min = std::stoi(s.substr(14, 2));
        t.tm_sec = std::stoi(s.substr(17, 2));
        t.tm_isdst = 0;
#ifdef _WIN32
        return _mkgmtime(&t);
#else
        return timegm(&t);
#endif
    } catch (...) {
        return 0;
    }
}

inline bool shouldCheckForUpdates() {
    const std::filesystem::path statePath = RayQuiroUserPaths::rqioHome() / "update.json";
    if (!std::filesystem::exists(statePath)) {
        return true;
    }
    try {
        const std::string content = readText(statePath);
        const auto lastCheckOpt = findJsonStringValue(content, "last_check_utc");
        if (!lastCheckOpt) return true;
        const std::time_t lastCheck = parseIsoUtc(*lastCheckOpt);
        if (lastCheck == 0) return true;
        const std::time_t now = std::time(nullptr);
        if (now - lastCheck >= 86400) {
            return true;
        }
    } catch (...) {
        return true;
    }
    return false;
}

struct UpdateCheckResult {
    bool hasUpdate = false;
    std::string currentVersion;
    std::string remoteVersion;
    std::string downloadUrl;
};

inline UpdateCheckResult checkForUpdatesQuietly(const std::string& currentVersion) {
    UpdateCheckResult res;
    res.currentVersion = currentVersion;
#ifndef _WIN32
    (void)currentVersion;
    return res;
#else
    try {
        const std::filesystem::path manifestPath = RayQuiroUserPaths::updatesRoot() / "remote_update.json";
        fetchResource(updateManifestUrl(), manifestPath);
        const UpdateManifest manifest = parseUpdateManifest(readText(manifestPath));
        if (!manifest.version.empty() && !manifest.downloadUrl.empty()) {
            const int comparison = compareVersions(currentVersion, manifest.version);
            if (comparison < 0) {
                res.hasUpdate = true;
                res.remoteVersion = manifest.version;
                res.downloadUrl = manifest.downloadUrl;
                writeUpdateState(currentVersion, manifest.version, "available", updateManifestUrl(), manifest.downloadUrl);
            } else {
                writeUpdateState(currentVersion, manifest.version, "up-to-date", updateManifestUrl(), manifest.downloadUrl);
            }
        }
    } catch (...) {
        // quiet fail
    }
    return res;
#endif
}

inline int selfUpdate(
    const std::filesystem::path& exePath,
    const std::string& currentVersion,
    bool checkOnly,
    const std::string& relaunchCmd = ""
) {
#ifndef _WIN32
    (void)exePath;
    (void)currentVersion;
    (void)checkOnly;
    (void)relaunchCmd;
    throw std::runtime_error("Self-update is currently supported on Windows only.");
#else
    const std::filesystem::path manifestPath = RayQuiroUserPaths::updatesRoot() / "remote_update.json";
    fetchResource(updateManifestUrl(), manifestPath);
    const UpdateManifest manifest = parseUpdateManifest(readText(manifestPath));

    if (manifest.version.empty() || manifest.downloadUrl.empty()) {
        writeUpdateState(currentVersion, manifest.version, "invalid-manifest", updateManifestUrl(), manifest.downloadUrl);
        throw std::runtime_error("Update manifest is missing 'version' or 'download_url'.");
    }

    const int comparison = compareVersions(currentVersion, manifest.version);
    if (comparison >= 0) {
        writeUpdateState(currentVersion, manifest.version, "up-to-date", updateManifestUrl(), manifest.downloadUrl);
        std::cout << "[RayQuiro] Already up to date: " << currentVersion << std::endl;
        return 0;
    }

    writeUpdateState(currentVersion, manifest.version, "available", updateManifestUrl(), manifest.downloadUrl);
    std::cout << "[RayQuiro] Update available: " << currentVersion << " -> " << manifest.version << std::endl;

    if (checkOnly) {
        return 0;
    }

    std::filesystem::create_directories(RayQuiroUserPaths::updatesRoot());
    const std::filesystem::path assetPath = RayQuiroUserPaths::updatesRoot() /
        ("rqio_update" + std::filesystem::path(manifest.downloadUrl).extension().string());
    fetchResource(manifest.downloadUrl, assetPath);
    createSelfUpdateScripts(exePath, assetPath, currentVersion, manifest.version, relaunchCmd);

    std::cout << "[RayQuiro] Update downloaded. The updater is replacing files in " << exePath.parent_path().string() << std::endl;
    return 0;
#endif
}
}
