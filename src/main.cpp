#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "Compiler.h"
#include "Interpreter.h"
#include "BytecodeCompiler.h"
#include "BytecodePackage.h"
#include "CliServices.h"
#include "Formatter.h"
#include "VM.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
const char* kRayQuiroVersion = "0.1.1";

std::string rebuildCommandLine(int argc, char* argv[]) {
    std::string cmd;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find(' ') != std::string::npos || arg.empty()) {
            cmd += "\"" + arg + "\"";
        } else {
            cmd += arg;
        }
        if (i + 1 < argc) {
            cmd += " ";
        }
    }
    return cmd;
}


enum class CliMode {
    Run,
    Pack,
    Bundle,
    Build,
    Format,
    Init,
    FrameworkInstall,
    SelfUpdate,
    Help,
    Version
};

struct CliOptions {
    CliMode mode = CliMode::Run;
    std::filesystem::path inputPath;
    std::filesystem::path outExePath;
    std::filesystem::path outBundlePath;
    std::filesystem::path initPath;
    std::string frameworkSpec;
    bool approvedRegistryOnly = false;
    bool checkOnly = false;
    bool localInstall = false;
    bool preferVm = false;
};

struct ProjectConfig {
    bool found = false;
    std::filesystem::path projectRoot;
    std::filesystem::path entryPath;
    std::string buildDir = "build";
};

void printHelp() {
    std::cout << "RayQuiro CLI\n";
    std::cout << "Usage:\n";
    std::cout << "  rqio <script.rq>\n";
    std::cout << "  rqio run <script.rq>\n";
    std::cout << "  rqio run --vm <script.rq>\n";
    std::cout << "  rqio pack <script.rq> [-o out.rqb]\n";
    std::cout << "  rqio bundle <script.rq> [-o out-dir]\n";
    std::cout << "  rqio build-vm <script.rq> [-o out-dir]\n";
    std::cout << "  rqio <bundle.rqb>\n";
    std::cout << "  rqio fmt <script.rq>\n";
    std::cout << "  rqio init [project-folder]\n";
    std::cout << "  rqio framework install <owner/repo[@branch]> [--local]\n";
    std::cout << "  rqio framework install <approved-name> [--local]\n";
    std::cout << "  rqio install <approved-name> [--local]\n";
    std::cout << "  rqio install rayquiro.<module> [--local]\n";
    std::cout << "  rqio self-update [check]\n";
    std::cout << "  rqio version\n";
    std::cout << "  rqio help\n";
    std::cout << "  rqio build <script.rq> [-o out.exe]\n";
}

std::optional<std::filesystem::path> resolveScriptPath(const std::string& rawValue) {
    std::filesystem::path rawPath = rawValue;

    if (std::filesystem::exists(rawPath) && std::filesystem::is_regular_file(rawPath)) {
        return std::filesystem::absolute(rawPath);
    }

    if (!rawPath.has_extension()) {
        for (const char* extension : {".rq", ".rqio"}) {
            const std::filesystem::path withExtension = rawPath.string() + extension;
            if (std::filesystem::exists(withExtension) && std::filesystem::is_regular_file(withExtension)) {
                return std::filesystem::absolute(withExtension);
            }
        }
        for (const char* extension : {".rqb"}) {
            const std::filesystem::path withExtension = rawPath.string() + extension;
            if (std::filesystem::exists(withExtension) && std::filesystem::is_regular_file(withExtension)) {
                return std::filesystem::absolute(withExtension);
            }
        }
    }

    if (std::filesystem::exists(rawPath) && std::filesystem::is_directory(rawPath)) {
        for (const char* entryName : {"main.rq", "main.rqio"}) {
            const std::filesystem::path mainFile = rawPath / entryName;
            if (std::filesystem::exists(mainFile) && std::filesystem::is_regular_file(mainFile)) {
                return std::filesystem::absolute(mainFile);
            }
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> resolveSiblingBundlePath(const std::filesystem::path& exePath) {
    if (exePath.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path candidate = exePath.parent_path() / (exePath.stem().string() + ".rqb");
    if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
        return std::filesystem::absolute(candidate);
    }

    return std::nullopt;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return "";
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<std::string> findJsonStringValue(const std::string& json, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() >= 2) {
        return match[1].str();
    }
    return std::nullopt;
}

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    for (char ch : value) {
        if (ch == '\\') escaped += "\\\\";
        else if (ch == '"') escaped += "\\\"";
        else escaped += ch;
    }
    return escaped;
}

void writeTextFile(const std::filesystem::path& path, const std::string& content) {
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot write file: " + path.string());
    }
    output << content;
}

void copyBinaryFile(const std::filesystem::path& from, const std::filesystem::path& to) {
    const auto parent = to.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing);
}

void createStarterProject(const std::filesystem::path& requestedRoot) {
    const std::filesystem::path projectRoot = requestedRoot.empty()
        ? std::filesystem::current_path()
        : std::filesystem::absolute(requestedRoot);
    const std::string projectName = projectRoot.filename().empty()
        ? std::string("rayquiro-app")
        : projectRoot.filename().string();

    const std::filesystem::path projectFile = projectRoot / "rqproject.json";
    const std::filesystem::path mainFile = projectRoot / "main.rq";
    const std::filesystem::path gitignoreFile = projectRoot / ".gitignore";

    if (std::filesystem::exists(projectFile) || std::filesystem::exists(mainFile)) {
        throw std::runtime_error("The target already contains rqproject.json or main.rq. Choose an empty folder.");
    }

    std::filesystem::create_directories(projectRoot / ".rq_modules");
    std::filesystem::create_directories(projectRoot / "build");

    writeTextFile(
        projectFile,
        "{\n"
        "  \"name\": \"" + jsonEscape(projectName) + "\",\n"
        "  \"version\": \"0.1.1\",\n"
        "  \"entry\": \"main.rq\",\n"
        "  \"build_dir\": \"build\"\n"
        "}\n");

    writeTextFile(
        mainFile,
        "import rayquiro.web as web;\n"
        "\n"
        "var project = \"" + projectName + "\";\n"
        "\n"
        "web.begin(project, \"build/index.html\");\n"
        "web.style(\"\n"
        ".page { max-width: 1080px; margin: 0 auto; padding: 64px 24px 96px; }\n"
        ".hero { padding: 48px; border-radius: 32px; background: linear-gradient(145deg, #08111f, #13213a); color: #f5f8ff; box-shadow: 0 28px 80px rgba(8, 17, 31, .35); }\n"
        ".eyebrow { display: inline-flex; padding: 8px 14px; border-radius: 999px; background: rgba(92, 200, 255, .16); color: #5cc8ff; font-size: 13px; letter-spacing: .08em; text-transform: uppercase; }\n"
        ".title { font-size: 56px; line-height: 1.02; margin: 18px 0 14px; }\n"
        ".lead { font-size: 19px; line-height: 1.8; color: #b4c6df; max-width: 680px; }\n"
        ".primary { display: inline-flex; margin-top: 18px; background: #5cc8ff; color: #07111d; }\n"
        "\");\n"
        "web.open(\"main\", \"page\");\n"
        "web.open(\"section\", \"hero\");\n"
        "web.text(\"RayQuiro Starter\", \"eyebrow\");\n"
        "web.h1(\"Build apps, installers and websites with one language.\", \"title\");\n"
        "web.p(\"This starter already uses rqproject.json, build/ and live web mode on port 5274.\", \"lead\");\n"
        "web.button(\"Open RayQuiro GitHub\", \"primary\", \"https://github.com/\");\n"
        "web.close(\"section\");\n"
        "web.close(\"main\");\n"
        "web.live(5274);\n"
        "web.end();\n");

    writeTextFile(
        gitignoreFile,
        ".rq_modules/\n"
        "build/\n"
        "*.generated.cpp\n"
        "*.exe\n"
        "*.html\n");

    std::cout << "[RayQuiro] Project created at " << projectRoot.string() << std::endl;
    std::cout << "[RayQuiro] Next: cd " << projectRoot.filename().string() << " && rqio" << std::endl;
}

ProjectConfig loadProjectConfig(const std::filesystem::path& startPath) {
    std::filesystem::path cursor = std::filesystem::absolute(startPath);
    if (!std::filesystem::is_directory(cursor)) {
        cursor = cursor.parent_path();
    }

    while (!cursor.empty()) {
        const std::filesystem::path configPath = cursor / "rqproject.json";
        if (std::filesystem::exists(configPath) && std::filesystem::is_regular_file(configPath)) {
            ProjectConfig config;
            config.found = true;
            config.projectRoot = cursor;
            const std::string json = readTextFile(configPath);
            config.buildDir = findJsonStringValue(json, "build_dir").value_or("build");
            const std::string entry = findJsonStringValue(json, "entry").value_or("main.rq");
            config.entryPath = std::filesystem::absolute(cursor / entry);
            return config;
        }

        if (cursor == cursor.root_path() || cursor.parent_path() == cursor) {
            break;
        }
        cursor = cursor.parent_path();
    }

    ProjectConfig fallback;
    fallback.projectRoot = std::filesystem::is_directory(startPath)
        ? std::filesystem::absolute(startPath)
        : std::filesystem::absolute(startPath).parent_path();
    fallback.entryPath = fallback.projectRoot / "main.rq";
    return fallback;
}

CliOptions parseArguments(int argc, char* argv[]) {
    if (argc < 2) {
        return CliOptions{};
    }

    CliOptions options;
    int index = 1;
    const std::string first = argv[index];

    if (first == "help" || first == "--help" || first == "-h") {
        options.mode = CliMode::Help;
        return options;
    }
    if (first == "version" || first == "--version" || first == "-v") {
        options.mode = CliMode::Version;
        return options;
    }
    if (first == "init") {
        options.mode = CliMode::Init;
        if (argc >= 3) {
            options.initPath = argv[2];
        }
        if (argc > 3) {
            throw std::runtime_error("Usage: rqio init [project-folder]");
        }
        return options;
    }
    if (first == "self-update") {
        options.mode = CliMode::SelfUpdate;
        ++index;
        if (index < argc && std::string(argv[index]) == "check") {
            options.checkOnly = true;
        }
        return options;
    }
    if (first == "install") {
        options.mode = CliMode::FrameworkInstall;
        options.approvedRegistryOnly = true;
        ++index;
        for (; index < argc; ++index) {
            const std::string arg = argv[index];
            if (arg == "--local") {
                options.localInstall = true;
                continue;
            }
            if (options.frameworkSpec.empty()) {
                options.frameworkSpec = arg;
                continue;
            }
            throw std::runtime_error("Too many arguments for 'install'.");
        }
        if (options.frameworkSpec.empty()) {
            throw std::runtime_error("Expected a framework name after 'install'.");
        }
        return options;
    }
    if (first == "framework") {
        options.mode = CliMode::FrameworkInstall;
        index += 1;
        if (index >= argc || std::string(argv[index]) != "install") {
            throw std::runtime_error("Supported framework command: rqio framework install <owner/repo|name>");
        }
        ++index;
        for (; index < argc; ++index) {
            const std::string arg = argv[index];
            if (arg == "--local") {
                options.localInstall = true;
                continue;
            }
            if (options.frameworkSpec.empty()) {
                options.frameworkSpec = arg;
                continue;
            }
            throw std::runtime_error("Too many arguments for 'framework install'.");
        }
        if (options.frameworkSpec.empty()) {
            throw std::runtime_error("Expected a framework name or repo after 'framework install'.");
        }
        return options;
    }
    if (first == "run") {
        options.mode = CliMode::Run;
        ++index;
    } else if (first == "bundle" || first == "build-vm") {
        options.mode = CliMode::Bundle;
        ++index;
    } else if (first == "build") {
        options.mode = CliMode::Build;
        ++index;
    } else if (first == "pack") {
        options.mode = CliMode::Pack;
        ++index;
    } else if (first == "fmt" || first == "format") {
        options.mode = CliMode::Format;
        ++index;
    }

    for (int i = index; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            const std::filesystem::path outPath = argv[++i];
            if (options.mode == CliMode::Pack || options.mode == CliMode::Bundle) {
                options.outBundlePath = outPath;
            } else {
                options.outExePath = outPath;
            }
            continue;
        }
        if (arg == "--vm") {
            options.preferVm = true;
            continue;
        }

        const auto resolved = resolveScriptPath(arg);
        if (!resolved.has_value()) {
            throw std::runtime_error("Cannot find script: " + arg);
        }
        options.inputPath = resolved.value();
    }

    return options;
}

BuildOptions makeBuildOptions(
    const CliOptions& cliOptions,
    const std::filesystem::path& repoRoot,
    const ProjectConfig& projectConfig
) {
    auto makeTransientCppPath = [&](const std::filesystem::path& inputPath) {
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
#ifdef _WIN32
        const auto processId = static_cast<unsigned long long>(GetCurrentProcessId());
#else
        const auto processId = static_cast<unsigned long long>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "rayquiro";
        std::filesystem::create_directories(tempDir);
        return tempDir / (inputPath.stem().string() + "-" + std::to_string(processId) + "-" + std::to_string(timestamp) + ".generated.cpp");
    };

    auto resolveRuntimeIncludePath = [&](const std::filesystem::path& exeRoot) {
        const std::vector<std::filesystem::path> candidates = {
            exeRoot / "include" / "rayquiro",
            exeRoot.parent_path() / "include" / "rayquiro",
            std::filesystem::current_path() / "include" / "rayquiro",
            std::filesystem::current_path().parent_path() / "include" / "rayquiro",
            exeRoot / "rayquiro",
            exeRoot.parent_path() / "rayquiro"
        };

        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate / "rte_api.h") &&
                std::filesystem::exists(candidate / "RuntimeEmitter.h")) {
                return std::filesystem::absolute(candidate);
            }
        }

        return std::filesystem::absolute(exeRoot / "include" / "rayquiro");
    };

    BuildOptions options;
    options.inputPath = cliOptions.inputPath;
    const std::filesystem::path buildDir = projectConfig.projectRoot / projectConfig.buildDir;
    std::filesystem::create_directories(buildDir);
    options.outputCppPath = makeTransientCppPath(cliOptions.inputPath);
    options.outputExePath = cliOptions.outExePath.empty()
        ? buildDir / (cliOptions.inputPath.stem().string() + ".exe")
        : std::filesystem::absolute(cliOptions.outExePath);
    if (!options.outputCppPath.parent_path().empty()) {
        std::filesystem::create_directories(options.outputCppPath.parent_path());
    }
    if (!options.outputExePath.parent_path().empty()) {
        std::filesystem::create_directories(options.outputExePath.parent_path());
    }
    options.runtimeIncludePath = resolveRuntimeIncludePath(repoRoot);
    return options;
}

void cleanupGeneratedCppArtifacts(const std::filesystem::path& buildDir) {
    if (!std::filesystem::exists(buildDir) || !std::filesystem::is_directory(buildDir)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(buildDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto& path = entry.path();
        if (path.extension() == ".cpp" && path.filename().string().find(".generated.cpp") != std::string::npos) {
            std::error_code ignore;
            std::filesystem::remove(path, ignore);
        }
    }
}

std::filesystem::path makeBundlePath(
    const CliOptions& cliOptions,
    const ProjectConfig& projectConfig
) {
    if (!cliOptions.outBundlePath.empty()) {
        const std::filesystem::path outPath = std::filesystem::absolute(cliOptions.outBundlePath);
        if (!outPath.parent_path().empty()) {
            std::filesystem::create_directories(outPath.parent_path());
        }
        return outPath;
    }

    const std::filesystem::path buildDir = projectConfig.projectRoot / projectConfig.buildDir;
    std::filesystem::create_directories(buildDir);
    return buildDir / (cliOptions.inputPath.stem().string() + ".rqb");
}

std::filesystem::path makeBundleDir(
    const CliOptions& cliOptions,
    const ProjectConfig& projectConfig
) {
    if (!cliOptions.outBundlePath.empty()) {
        const std::filesystem::path outPath = std::filesystem::absolute(cliOptions.outBundlePath);
        if (!outPath.parent_path().empty()) {
            std::filesystem::create_directories(outPath.parent_path());
        }
        return outPath;
    }

    const std::filesystem::path buildDir = projectConfig.projectRoot / projectConfig.buildDir;
    std::filesystem::create_directories(buildDir);
    return buildDir / (cliOptions.inputPath.stem().string() + "-bundle");
}

int runVmProgram(const BytecodeProgram& program) {
    VM vm;
    vm.setBuiltinHandler([](const std::string& name, const std::vector<VMValue>& args) {
        return VM::callDefaultBuiltin(name, args);
    });
    (void)vm.run(program);
    return 0;
}

void buildVmBundle(
    const std::filesystem::path& cliExePath,
    const CliOptions& cliOptions,
    const ProjectConfig& projectConfig
) {
    auto resolved = Compiler::resolveForExecution(cliOptions.inputPath);
    if (!BytecodeCompiler::supports(
        *resolved.program,
        resolved.builtinNamespaceAliases,
        resolved.builtinSymbolAliases)) {
        throw std::runtime_error("This script uses features that are not yet supported by the VM bundle builder.");
    }

    const BytecodeProgram program = BytecodeCompiler::compile(
        *resolved.program,
        resolved.builtinNamespaceAliases,
        resolved.builtinSymbolAliases);

    const std::filesystem::path bundleDir = makeBundleDir(cliOptions, projectConfig);
    std::filesystem::create_directories(bundleDir);

    const std::string appName = cliOptions.inputPath.stem().string();
    const std::filesystem::path bundledExePath = bundleDir / (appName + ".exe");
    const std::filesystem::path bundledBytecodePath = bundleDir / (appName + ".rqb");
    const std::filesystem::path launcherPath = bundleDir / "run.bat";
    const std::filesystem::path readmePath = bundleDir / "README.txt";

    BytecodePackage::writeToFile(program, bundledBytecodePath);
    copyBinaryFile(cliExePath, bundledExePath);

    writeTextFile(
        launcherPath,
        "@echo off\r\n"
        "setlocal\r\n"
        "\"%~dp0" + appName + ".exe\"\r\n");

    writeTextFile(
        readmePath,
        "RayQuiro VM Bundle\r\n"
        "===================\r\n\r\n"
        "Run `" + appName + ".exe` or `run.bat`.\r\n"
        "This bundle contains obfuscated RayQuiro bytecode in `" + appName + ".rqb`.\r\n"
        "The original `.rq` source file is not required to run this app.\r\n");

    std::cout << "[RayQuiro] Bundled " << bundleDir.string() << std::endl;
}

int runExecutable(const std::filesystem::path& exePath, const std::filesystem::path& workingDir) {
#ifdef _WIN32
    STARTUPINFOA startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};

    std::string commandLine = "\"" + exePath.string() + "\"";
    std::vector<char> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back('\0');

    const BOOL created = CreateProcessA(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workingDir.string().c_str(),
        &startupInfo,
        &processInfo);
    if (!created) {
        return 1;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return static_cast<int>(exitCode);
#else
    const std::string command = "\"" + exePath.string() + "\"";
    const std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::current_path(workingDir);
    const int code = std::system(command.c_str());
    std::filesystem::current_path(previous);
    return code;
#endif
}
}

int main(int argc, char* argv[]) {
    try {
        const std::filesystem::path exePath = std::filesystem::absolute(std::filesystem::path(argv[0]));
        const std::filesystem::path exeRoot = exePath.parent_path();
        CliOptions cliOptions = parseArguments(argc, argv);
        if (argc < 2 && cliOptions.inputPath.empty()) {
            if (const auto siblingBundle = resolveSiblingBundlePath(exePath)) {
                cliOptions.mode = CliMode::Run;
                cliOptions.inputPath = *siblingBundle;
            }
        }
        ProjectConfig projectConfig = loadProjectConfig(
            cliOptions.inputPath.empty() ? std::filesystem::current_path() : cliOptions.inputPath);

        if ((cliOptions.mode == CliMode::Run || cliOptions.mode == CliMode::Pack || cliOptions.mode == CliMode::Bundle || cliOptions.mode == CliMode::Build || cliOptions.mode == CliMode::Format) &&
            cliOptions.inputPath.empty()) {
            if (std::filesystem::exists(projectConfig.entryPath) && std::filesystem::is_regular_file(projectConfig.entryPath)) {
                cliOptions.inputPath = std::filesystem::absolute(projectConfig.entryPath);
            } else {
                throw std::runtime_error("No input script was provided and rqproject.json entry was not found.");
            }
        }

        if (!cliOptions.inputPath.empty()) {
            projectConfig = loadProjectConfig(cliOptions.inputPath);
        }

        if (argc < 2 && cliOptions.inputPath.empty() && !std::filesystem::exists(projectConfig.entryPath)) {
            printHelp();
            return 0;
        }
        if (cliOptions.mode == CliMode::Help) {
            printHelp();
            return 0;
        }
        if (cliOptions.mode == CliMode::Version) {
            std::cout << "RayQuiro " << kRayQuiroVersion << std::endl;
            return 0;
        }
        if (cliOptions.mode == CliMode::Init) {
            createStarterProject(cliOptions.initPath);
            return 0;
        }

        // Automatic update check & prompt
        if (cliOptions.mode != CliMode::Help &&
            cliOptions.mode != CliMode::Version &&
            cliOptions.mode != CliMode::Init &&
            cliOptions.mode != CliMode::SelfUpdate) {
            if (RayQuiroCliServices::shouldCheckForUpdates()) {
                const auto updateRes = RayQuiroCliServices::checkForUpdatesQuietly(kRayQuiroVersion);
                if (updateRes.hasUpdate) {
                    std::cout << "[RayQuiro] A new version is available: " << kRayQuiroVersion << " -> " << updateRes.remoteVersion << std::endl;
                    std::cout << "Would you like to install the update now? [y/N]: ";
                    std::string response;
                    if (std::getline(std::cin, response)) {
                        // Trim response
                        response.erase(0, response.find_first_not_of(" \t\r\n"));
                        response.erase(response.find_last_not_of(" \t\r\n") + 1);
                        if (response == "y" || response == "Y") {
                            std::cout << "[RayQuiro] Downloading update..." << std::endl;
                            const std::string relaunchCmd = rebuildCommandLine(argc, argv);
                            int code = RayQuiroCliServices::selfUpdate(exePath, kRayQuiroVersion, false, relaunchCmd);
                            if (code == 0) {
                                std::cout << "[RayQuiro] Update scheduled. Restarting..." << std::endl;
                                return 0;
                            }
                        }
                    }
                }
            }
        }

        if (cliOptions.mode == CliMode::FrameworkInstall) {
            if (RayQuiroCliServices::isNativeModuleSpec(cliOptions.frameworkSpec)) {
                const std::filesystem::path installRoot = cliOptions.localInstall
                    ? (projectConfig.projectRoot / ".rq_modules" / "native")
                    : RayQuiroUserPaths::systemModulesRoot();
                return RayQuiroCliServices::installNativeModule(cliOptions.frameworkSpec, installRoot);
            }

            const std::filesystem::path installRoot = cliOptions.localInstall
                ? (projectConfig.projectRoot / ".rq_modules")
                : RayQuiroUserPaths::frameworksRoot();
            if (cliOptions.approvedRegistryOnly) {
                return RayQuiroCliServices::installApprovedFramework(cliOptions.frameworkSpec, installRoot);
            }
            return RayQuiroCliServices::installFramework(cliOptions.frameworkSpec, installRoot);
        }
        if (cliOptions.mode == CliMode::SelfUpdate) {
            return RayQuiroCliServices::selfUpdate(
                std::filesystem::absolute(std::filesystem::path(argv[0])),
                kRayQuiroVersion,
                cliOptions.checkOnly);
        }
        if (cliOptions.mode == CliMode::Format) {
            Formatter::formatFile(cliOptions.inputPath);
            std::cout << "[RayQuiro] Formatted " << cliOptions.inputPath.string() << std::endl;
            return 0;
        }

        if (cliOptions.mode == CliMode::Run && cliOptions.inputPath.extension() == ".rqb") {
            return runVmProgram(BytecodePackage::readFromFile(cliOptions.inputPath));
        }

        if (cliOptions.mode == CliMode::Pack) {
            auto resolved = Compiler::resolveForExecution(cliOptions.inputPath);
            if (!BytecodeCompiler::supports(
                *resolved.program,
                resolved.builtinNamespaceAliases,
                resolved.builtinSymbolAliases)) {
                throw std::runtime_error("This script uses features that are not yet supported by the VM packer.");
            }

            const BytecodeProgram program = BytecodeCompiler::compile(
                *resolved.program,
                resolved.builtinNamespaceAliases,
                resolved.builtinSymbolAliases);
            const std::filesystem::path bundlePath = makeBundlePath(cliOptions, projectConfig);
            BytecodePackage::writeToFile(program, bundlePath);
            std::cout << "[RayQuiro] Packed " << bundlePath.string() << std::endl;
            return 0;
        }

        if (cliOptions.mode == CliMode::Bundle) {
            buildVmBundle(exePath, cliOptions, projectConfig);
            return 0;
        }

        if (cliOptions.mode == CliMode::Run) {
            auto resolved = Compiler::resolveForExecution(cliOptions.inputPath);
            if (cliOptions.preferVm &&
                BytecodeCompiler::supports(
                    *resolved.program,
                    resolved.builtinNamespaceAliases,
                    resolved.builtinSymbolAliases)) {
                const auto program = BytecodeCompiler::compile(
                    *resolved.program,
                    resolved.builtinNamespaceAliases,
                    resolved.builtinSymbolAliases);
                return runVmProgram(program);
            }
            Interpreter interpreter(
                projectConfig.projectRoot,
                exePath,
                resolved.builtinNamespaceAliases,
                resolved.builtinSymbolAliases);

            if (interpreter.supports(*resolved.program)) {
                const std::filesystem::path previousPath = std::filesystem::current_path();
                std::filesystem::current_path(projectConfig.projectRoot);
                try {
                    const int code = interpreter.run(*resolved.program);
                    std::filesystem::current_path(previousPath);
                    return code;
                } catch (...) {
                    std::filesystem::current_path(previousPath);
                    throw;
                }
            }
        }

        cleanupGeneratedCppArtifacts(projectConfig.projectRoot / projectConfig.buildDir);

        const BuildOptions buildOptions = makeBuildOptions(cliOptions, exeRoot, projectConfig);
        Compiler compiler;
        const BuildResult result = compiler.compile(buildOptions);
        const auto cleanupGeneratedCpp = [&]() {
            std::error_code ignore;
            std::filesystem::remove(result.outputCppPath, ignore);
            cleanupGeneratedCppArtifacts(projectConfig.projectRoot / projectConfig.buildDir);
        };

        const int buildCode = compiler.buildExecutable(result);
        if (buildCode != 0) {
            cleanupGeneratedCpp();
            std::cerr << "[RayQuiro] Compile failed with code " << buildCode << std::endl;
            return 1;
        }

        cleanupGeneratedCpp();

        std::cout << "[RayQuiro] Built " << result.outputExePath.string() << " from " << cliOptions.inputPath.string() << std::endl;

        if (cliOptions.mode == CliMode::Run) {
            return runExecutable(result.outputExePath, projectConfig.projectRoot);
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[RayQuiro] Error: " << e.what() << std::endl;
        return 1;
    }
}
