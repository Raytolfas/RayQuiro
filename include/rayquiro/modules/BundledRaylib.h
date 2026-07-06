#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class BundledRaylib {
public:
    static std::filesystem::path sourceRoot(const std::filesystem::path& repoRoot) {
        return repoRoot / "third_party" / "raylib" / "src";
    }

    static std::filesystem::path buildRoot(const std::filesystem::path& repoRoot) {
        return repoRoot / ".cache" / "raylib";
    }

    static std::filesystem::path libraryPath(const std::filesystem::path& repoRoot) {
        return buildRoot(repoRoot) / "libraylib.a";
    }

    static bool isAvailable(const std::filesystem::path& repoRoot) {
        return std::filesystem::exists(sourceRoot(repoRoot) / "raylib.h");
    }

    static void ensure(
        const std::filesystem::path& repoRoot,
        const std::string& ccCommand,
        const std::string& arCommand
    ) {
        const std::filesystem::path srcRoot = sourceRoot(repoRoot);
        const std::filesystem::path outRoot = buildRoot(repoRoot);
        const std::filesystem::path libPath = libraryPath(repoRoot);

        if (!isAvailable(repoRoot)) {
            throw std::runtime_error("Bundled raylib was not found in third_party/raylib.");
        }

        std::filesystem::create_directories(outRoot);
        const std::vector<std::filesystem::path> trackedFiles = collectTrackedFiles(srcRoot);
        if (!isOutdated(libPath, trackedFiles)) {
            return;
        }

        const std::vector<std::string> modules = {
            "rcore.c",
            "rshapes.c",
            "rtextures.c",
            "rtext.c",
            "utils.c",
            "rglfw.c",
            "rmodels.c",
            "raudio.c"
        };

        std::vector<std::filesystem::path> objects;
        for (const std::string& module : modules) {
            const std::filesystem::path sourceFile = srcRoot / module;
            const std::filesystem::path objectFile = outRoot / std::filesystem::path(module).replace_extension(".o");
            objects.push_back(objectFile);

            std::ostringstream command;
            command
                << formatCommand(ccCommand)
                << " -Wall"
                << " -D_GNU_SOURCE"
                << " -DPLATFORM_DESKTOP_GLFW"
                << " -DGRAPHICS_API_OPENGL_33"
                << " -Wno-missing-braces"
                << " -Werror=pointer-arith"
                << " -fno-strict-aliasing"
                << " -std=c99"
                << " -O1"
                << " -Werror=implicit-function-declaration"
                << " -I."
                << " -Iexternal/glfw/include"
                << " -c \"" << sourceFile.string() << "\""
                << " -o \"" << objectFile.string() << "\"";

            runCommand(srcRoot, command.str(), "Failed to compile bundled raylib module: " + module);
        }

        std::ostringstream archiveCommand;
        archiveCommand << formatCommand(arCommand) << " rcs \"" << libPath.string() << "\"";
        for (const auto& objectFile : objects) {
            archiveCommand << " \"" << objectFile.string() << "\"";
        }

        runCommand(outRoot, archiveCommand.str(), "Failed to archive bundled raylib.");
    }

private:
    static std::string formatCommand(const std::string& value) {
        if (value.find('\\') == std::string::npos &&
            value.find('/') == std::string::npos &&
            value.find(':') == std::string::npos &&
            value.find(' ') == std::string::npos) {
            return value;
        }
        return "\"" + value + "\"";
    }

    static std::vector<std::filesystem::path> collectTrackedFiles(const std::filesystem::path& root) {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const std::string extension = entry.path().extension().string();
            if (extension == ".c" || extension == ".h") {
                files.push_back(entry.path());
            }
        }
        return files;
    }

    static bool isOutdated(const std::filesystem::path& target, const std::vector<std::filesystem::path>& sources) {
        if (!std::filesystem::exists(target)) {
            return true;
        }

        const auto targetTime = std::filesystem::last_write_time(target);
        for (const auto& source : sources) {
            if (std::filesystem::last_write_time(source) > targetTime) {
                return true;
            }
        }
        return false;
    }

    static void runCommand(const std::filesystem::path& workingDir, const std::string& command, const std::string& errorPrefix) {
        const std::filesystem::path oldDir = std::filesystem::current_path();
        std::filesystem::current_path(workingDir);
        const int exitCode = std::system(command.c_str());
        std::filesystem::current_path(oldDir);

        if (exitCode != 0) {
            throw std::runtime_error(errorPrefix + " Exit code: " + std::to_string(exitCode));
        }
    }
};
