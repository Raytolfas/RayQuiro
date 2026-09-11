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
#include "DAPServer.h"
#include "Formatter.h"
#include "VM.h"
#include "CEmitter.h"
#include "Log.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
const char* kRayQuiroVersion = "0.2.0";

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
    Debug,
    Pack,
    Bundle,
    Build,
    Format,
    Init,
    FrameworkInstall,
    PackageInit,
    PackageAdd,
    PackageInstall,
    PackageRemove,
    PackageList,
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
    bool legacyMode = false;
    bool releaseMode = false;   // --release: -O3 -flto
    bool debugBuild  = false;   // --debug:   -O0 -g
    int  dapPort = 4711;
    std::vector<std::string> scriptArgs;
    std::string packageSpec;
    std::string packageName;
};

struct ProjectConfig {
    bool found = false;
    std::filesystem::path projectRoot;
    std::filesystem::path entryPath;
    std::string buildDir = "build";
};

void printHelp() {
    std::cout << "RayQuiro " << kRayQuiroVersion << "\n";
    std::cout << "\nUsage:\n";
    std::cout << "  rqio run <script.rq> [--legacy]     Run a script (VM by default)\n";
    std::cout << "  rqio build <script.rq> [-o out.exe] Compile to native binary\n";
    std::cout << "  rqio build <script.rq> --release    Native binary with -O3 -flto\n";
    std::cout << "  rqio build <script.rq> --debug      Native binary with -O0 -g\n";
    std::cout << "  rqio fmt <script.rq>                Format source file\n";
    std::cout << "  rqio debug <script.rq> [--port N]   Start DAP debug server\n";
    std::cout << "\nPackages:\n";
    std::cout << "  rqio init [folder]                  Initialize rqio.json\n";
    std::cout << "  rqio add <name>                     Install package globally\n";
    std::cout << "  rqio add <owner/repo[@branch]>      Install from GitHub globally\n";
    std::cout << "  rqio add <name> --local             Install locally (.rqio/packages/)\n";
    std::cout << "  rqio install                        Install all from rqio.json\n";
    std::cout << "  rqio install <name>                 Alias for rqio add\n";
    std::cout << "  rqio remove <name>                  Remove package\n";
    std::cout << "  rqio list                           List installed packages\n";
    std::cout << "\nOther:\n";
    std::cout << "  rqio self-update [check]            Update rqio\n";
    std::cout << "  rqio version                        Show version\n";
    std::cout << "  rqio help                           Show this help\n";
    std::cout << "\nLanguage features:\n";
    std::cout << "  ??  null coalescing:    x ?? \"default\"\n";
    std::cout << "  ?.  optional chaining:  obj?.[\"key\"]\n";
    std::cout << "  Multiple return:        return a, b, c\n";
    std::cout << "  Built-in modules:       json, process, datetime, path, fs, env, hash\n";
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
        "  \"version\": \"0.2.0\",\n"
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

    Log::status("Created", "project at " + projectRoot.string());
    Log::info("next: cd " + projectRoot.filename().string() + " && rqio");
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
        if (argc >= 3) {
            // rqio init <path>  →  old project scaffold
            options.mode = CliMode::Init;
            options.initPath = argv[2];
            if (argc > 3) throw std::runtime_error("Usage: rqio init [project-folder]");
        } else {
            // rqio init  →  create rqio.json in current directory
            options.mode = CliMode::PackageInit;
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
        ++index;
        // collect remaining args
        std::string spec;
        for (; index < argc; ++index) {
            const std::string arg = argv[index];
            if (arg == "--local") { options.localInstall = true; continue; }
            if (spec.empty()) { spec = arg; continue; }
            throw std::runtime_error("Too many arguments for 'install'.");
        }
        if (spec.empty()) {
            // rqio install  →  install all from rqio.json
            options.mode = CliMode::PackageInstall;
        } else {
            // rqio install <name>  →  install from approved registry (old behavior)
            options.mode = CliMode::FrameworkInstall;
            options.approvedRegistryOnly = true;
            options.frameworkSpec = spec;
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
    if (first == "add") {
        options.mode = CliMode::PackageAdd;
        for (int i = 2; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--local") { options.localInstall = true; continue; }
            if (options.packageSpec.empty()) { options.packageSpec = a; continue; }
        }
        if (options.packageSpec.empty())
            throw std::runtime_error("Usage: rqio add <name|user/repo[@branch]> [--local]");
        return options;
    }
    if (first == "remove" || first == "rm") {
        options.mode = CliMode::PackageRemove;
        for (int i = 2; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--local") { options.localInstall = true; continue; }
            if (options.packageName.empty()) { options.packageName = a; continue; }
        }
        if (options.packageName.empty())
            throw std::runtime_error("Usage: rqio remove <package-name> [--local]");
        return options;
    }
    if (first == "list" || first == "ls") {
        options.mode = CliMode::PackageList;
        return options;
    }
    if (first == "run") {
        options.mode = CliMode::Run;
        ++index;
    } else if (first == "debug" || first == "dap") {
        options.mode = CliMode::Debug;
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
    } else if (first == "add") {
        options.mode = CliMode::PackageAdd;
        ++index;
        if (index < argc) {
            options.packageSpec = argv[index++];
        }
    } else if (first == "install") {
        options.mode = CliMode::PackageInstall;
        ++index;
    } else if (first == "remove" || first == "rm") {
        options.mode = CliMode::PackageRemove;
        ++index;
        if (index < argc) {
            options.packageName = argv[index++];
        }
    } else if (first == "list" || first == "ls") {
        options.mode = CliMode::PackageList;
        ++index;
    } else if (first == "init") {
        // "rqio init" without a path → project init (package manager)
        // "rqio init <path>" → project scaffold (existing Init mode)
        if (index + 1 < argc && std::string(argv[index + 1]).rfind("--", 0) != 0
                              && std::string(argv[index + 1]).find('=') == std::string::npos) {
            // next arg is a path, treat as old Init
        } else {
            options.mode = CliMode::PackageInit;
            ++index;
        }
    }

    for (int i = index; i < argc; ++i) {
        const std::string arg = argv[i];

        // -- separator: everything after this goes to the script
        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                options.scriptArgs.push_back(argv[j]);
            }
            break;
        }

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
            options.preferVm = true; // no-op, VM is now default
            continue;
        }
        if (arg == "--legacy") {
            options.legacyMode = true;
            continue;
        }
        if (arg == "--port" && i + 1 < argc) {
            options.dapPort = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "--release") {
            options.releaseMode = true;
            continue;
        }
        if (arg == "--debug") {
            options.debugBuild = true;
            continue;
        }

        // If script file already resolved, remaining non-flag args are script args
        if (!options.inputPath.empty() && arg.rfind("--", 0) != 0 && arg.rfind("-", 0) != 0) {
            options.scriptArgs.push_back(arg);
            continue;
        }

        const auto resolved = resolveScriptPath(arg);
        if (!resolved.has_value()) {
            // Unknown flag after script — treat as script arg
            if (!options.inputPath.empty()) {
                options.scriptArgs.push_back(arg);
                continue;
            }
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
    options.releaseMode = cliOptions.releaseMode;
    options.debugBuild  = cliOptions.debugBuild;
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

// Global script args for VM's process.args() support
static std::vector<std::string> g_scriptArgs;

int runVmProgram(const BytecodeProgram& program) {
    VM vm;
    vm.setBuiltinHandler([](const std::string& name, const std::vector<VMValue>& args) -> std::optional<VMValue> {
        if (name == "process.args") {
            VMValue::Array items;
            for (const auto& a : g_scriptArgs) {
                items.push_back(VMValue(a));
            }
            return VMValue(items);
        }
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

    Log::status("Bundled", bundleDir.string());
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
    Log::init();
    CliOptions cliOptions;
    try {
        const std::filesystem::path exePath = std::filesystem::absolute(std::filesystem::path(argv[0]));
        const std::filesystem::path exeRoot = exePath.parent_path();
        cliOptions = parseArguments(argc, argv);
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

        // ── Package manager commands ──────────────────────────────────────────
        const std::filesystem::path pkgRoot = projectConfig.found
            ? projectConfig.projectRoot
            : std::filesystem::current_path();

        if (cliOptions.mode == CliMode::PackageInit) {
            RayQuiroCliServices::initProject(pkgRoot);
            return 0;
        }
        if (cliOptions.mode == CliMode::PackageAdd) {
            if (cliOptions.packageSpec.empty()) {
                throw std::runtime_error("Usage: rqio add <name|user/repo[@branch]> [--local]");
            }
            RayQuiroCliServices::addPackage(cliOptions.packageSpec, pkgRoot,
                                            cliOptions.localInstall);
            return 0;
        }
        if (cliOptions.mode == CliMode::PackageInstall) {
            RayQuiroCliServices::installAllPackages(pkgRoot);
            return 0;
        }
        if (cliOptions.mode == CliMode::PackageRemove) {
            if (cliOptions.packageName.empty()) {
                throw std::runtime_error("Usage: rqio remove <package-name> [--local]");
            }
            RayQuiroCliServices::removePackage(cliOptions.packageName, pkgRoot,
                                               cliOptions.localInstall);
            return 0;
        }
        if (cliOptions.mode == CliMode::PackageList) {
            RayQuiroCliServices::listInstalledPackages(pkgRoot);
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
                    Log::info(std::string("A new version is available: ") + kRayQuiroVersion + " -> " + updateRes.remoteVersion);
                    std::cout << "Would you like to install the update now? [y/N]: ";
                    std::string response;
                    if (std::getline(std::cin, response)) {
                        // Trim response
                        response.erase(0, response.find_first_not_of(" \t\r\n"));
                        response.erase(response.find_last_not_of(" \t\r\n") + 1);
                        if (response == "y" || response == "Y") {
                            Log::status("Updating", "downloading update...");
                            const std::string relaunchCmd = rebuildCommandLine(argc, argv);
                            int code = RayQuiroCliServices::selfUpdate(exePath, kRayQuiroVersion, false, relaunchCmd);
                            if (code == 0) {
                                Log::status("Updating", "update scheduled. Restarting...");
                                return 0;
                            }
                        }
                    }
                }
            }
        }

        if (cliOptions.mode == CliMode::FrameworkInstall) {
            // Native modules still use old path
            if (RayQuiroCliServices::isNativeModuleSpec(cliOptions.frameworkSpec)) {
                const std::filesystem::path installRoot = cliOptions.localInstall
                    ? (projectConfig.projectRoot / ".rq_modules" / "native")
                    : RayQuiroUserPaths::systemModulesRoot();
                return RayQuiroCliServices::installNativeModule(cliOptions.frameworkSpec, installRoot);
            }
            // rqio framework install → alias for rqio add (global)
            RayQuiroCliServices::addPackage(cliOptions.frameworkSpec, pkgRoot,
                                            cliOptions.localInstall);
            return 0;
        }
        if (cliOptions.mode == CliMode::SelfUpdate) {
            return RayQuiroCliServices::selfUpdate(
                std::filesystem::absolute(std::filesystem::path(argv[0])),
                kRayQuiroVersion,
                cliOptions.checkOnly);
        }
        if (cliOptions.mode == CliMode::Format) {
            Formatter::formatFile(cliOptions.inputPath);
            Log::status("Formatted", cliOptions.inputPath.string());
            return 0;
        }

        // ── Native build (C transpiler backend) ───────────────────────────────
        if (cliOptions.mode == CliMode::Build) {
            auto resolved = Compiler::resolveForExecution(cliOptions.inputPath);
            if (!BytecodeCompiler::supports(
                    *resolved.program,
                    resolved.builtinNamespaceAliases,
                    resolved.builtinSymbolAliases)) {
                throw std::runtime_error(
                    "rqio build: script uses import statements which are not yet "
                    "supported by the native C backend (MVP). Use --legacy run for now.");
            }
            const BytecodeProgram program = BytecodeCompiler::compile(
                *resolved.program,
                resolved.builtinNamespaceAliases,
                resolved.builtinSymbolAliases);

            // Emit C code
            std::string cSrc = CEmitter::emit(program);

            // Write to temp file next to the rqio executable
            std::filesystem::path tmpDir = std::filesystem::temp_directory_path();
            std::filesystem::path cFile  = tmpDir / "_rqio_build_tmp.c";
            std::filesystem::path rtFile = exePath.parent_path() / "rq_runtime.h";

            {
                std::ofstream f(cFile);
                if (!f) throw std::runtime_error("rqio build: cannot write temp C file: " + cFile.string());
                f << cSrc;
            }

            // Determine output binary name
            std::filesystem::path outBin = cliOptions.outExePath;
            if (outBin.empty()) {
                outBin = cliOptions.inputPath.parent_path() /
                         cliOptions.inputPath.stem();
#ifdef _WIN32
                outBin.replace_extension(".exe");
#endif
            }

            // Build flags
            std::string optFlags = cliOptions.releaseMode ? "-O2 -flto" :
                                   cliOptions.debugBuild  ? "-O0 -g"    : "-O2";

            // Pick compiler: on Windows prefer gcc (handles MinGW linking cleanly)
            std::string cc;
#ifdef _WIN32
            // Try gcc first (MSYS2/MinGW), then clang
            if (std::system("where gcc >nul 2>&1") == 0) cc = "gcc";
            else if (std::system("where clang >nul 2>&1") == 0) cc = "clang";
            else throw std::runtime_error("rqio build: no C compiler found (install gcc or clang via MSYS2)");
#else
            // Linux/macOS: prefer clang, fallback to gcc
            if (std::system("which clang >/dev/null 2>&1") == 0) cc = "clang";
            else cc = "gcc";
#endif
            // Runtime header: copy rq_runtime.h next to temp .c if not already there
            std::filesystem::path rtInTmp = tmpDir / "rq_runtime.h";
            if (std::filesystem::exists(rtFile) && !std::filesystem::exists(rtInTmp)) {
                std::filesystem::copy_file(rtFile, rtInTmp,
                    std::filesystem::copy_options::overwrite_existing);
            }

            // Compile command
            std::string tmpInclude = tmpDir.string();
            std::replace(tmpInclude.begin(), tmpInclude.end(), '\\', '/');
            std::string cFilePath = cFile.string();
            std::replace(cFilePath.begin(), cFilePath.end(), '\\', '/');
            std::string outBinPath = outBin.string();
            std::replace(outBinPath.begin(), outBinPath.end(), '\\', '/');

            std::string platformFlags;
#ifdef _WIN32
            platformFlags = " -mconsole";
#endif
            std::string cmd = cc + " " + optFlags + platformFlags + " -lm"
                + " -I\"" + tmpInclude + "\""
                + " \"" + cFilePath + "\""
                + " -o \"" + outBinPath + "\"";

            Log::status("Compiling", cliOptions.inputPath.filename().string() + " (" + (cliOptions.releaseMode ? "release" : cliOptions.debugBuild ? "debug" : "optimized") + ")");
            int ret = std::system(cmd.c_str());
            if (ret != 0) {
                throw std::runtime_error("C compiler exited with code " + std::to_string(ret));
            }
            Log::status("Finished", outBin.string());
            // Cleanup temp
            std::filesystem::remove(cFile);
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
            Log::status("Packed", bundlePath.string());
            return 0;
        }

        if (cliOptions.mode == CliMode::Bundle) {
            buildVmBundle(exePath, cliOptions, projectConfig);
            return 0;
        }

        if (cliOptions.mode == CliMode::Run) {
            auto resolved = Compiler::resolveForExecution(cliOptions.inputPath);
            g_scriptArgs = cliOptions.scriptArgs;  // for VM process.args()

            // ── Bytecode VM (default since 0.2.0) ────────────────────────────
            // Skip VM if --legacy flag used or program has imports (not yet supported)
            if (!cliOptions.legacyMode &&
                BytecodeCompiler::supports(
                    *resolved.program,
                    resolved.builtinNamespaceAliases,
                    resolved.builtinSymbolAliases)) {
                try {
                    const auto program = BytecodeCompiler::compile(
                        *resolved.program,
                        resolved.builtinNamespaceAliases,
                        resolved.builtinSymbolAliases);
                    return runVmProgram(program);
                } catch (const std::exception& vmErr) {
                    // VM compilation failed, fall through to tree-walk
                    Log::warn(std::string("VM: ") + vmErr.what() + " - falling back to interpreter");
                }
            }

            // ── Tree-walk interpreter (legacy / import-using programs) ────────
            Interpreter interpreter(
                projectConfig.projectRoot,
                exePath,
                resolved.builtinNamespaceAliases,
                resolved.builtinSymbolAliases);
            interpreter.setProcessArgs(cliOptions.scriptArgs);

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

        // ── Debug mode (DAP) ──────────────────────────────────────────────────
        if (cliOptions.mode == CliMode::Debug) {
            auto resolved = Compiler::resolveForExecution(cliOptions.inputPath);
            const std::string absPath =
                std::filesystem::absolute(cliOptions.inputPath).string();

            // Create DAP server and wait for debugger to connect
            auto dap = std::make_shared<DAPServer>(cliOptions.dapPort);
            if (!dap->start()) {
                std::cerr << "[DAP] Failed to start server on port "
                          << cliOptions.dapPort << "\n";
                return 1;
            }

            Interpreter interpreter(
                projectConfig.projectRoot, exePath,
                resolved.builtinNamespaceAliases,
                resolved.builtinSymbolAliases);
            interpreter.setSourcePath(absPath);

            // Attach debug hook
            interpreter.setDebugHook(
                [&dap, &absPath]
                (const std::string& filePath, int line) {
                    if (!dap->isConnected()) return;
                    const std::string& path = filePath.empty() ? absPath : filePath;
                    if (!dap->shouldPause(path, line)) return;

                    // Stack frame
                    dap->setStackFrames({{"<script>", path, line}});

                    // Block until VS Code sends continue/next/stepIn
                    dap->waitForResume();
                });

            // Notify VS Code that we're ready, then run
            dap->sendEvent("process", {{"name", absPath}, {"isLocalProcess", true}});

            const std::filesystem::path previousPath = std::filesystem::current_path();
            std::filesystem::current_path(projectConfig.projectRoot);
            try {
                interpreter.run(*resolved.program);
            } catch (const std::exception& e) {
                dap->sendOutput(std::string("Runtime error: ") + e.what(), "stderr");
            }
            std::filesystem::current_path(previousPath);

            dap->sendEvent("exited",    {{"exitCode", 0}});
            dap->sendEvent("terminated", {});
            dap->stop();
            return 0;
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
            Log::error("Compile failed with code " + std::to_string(buildCode));
            return 1;
        }

        cleanupGeneratedCpp();

        Log::status("Finished", result.outputExePath.string());

        if (cliOptions.mode == CliMode::Run) {
            return runExecutable(result.outputExePath, projectConfig.projectRoot);
        }

        return 0;
    } catch (const std::exception& e) {
        Log::reportException(e, cliOptions.inputPath);
        return 1;
    }
}
