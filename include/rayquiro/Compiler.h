#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Lexer.h"
#include "Parser.h"
#include "Generator.h"
#include "BundledRaylib.h"
#include "UserPaths.h"

struct BuildOptions {
    std::filesystem::path inputPath;
    std::filesystem::path outputCppPath;
    std::filesystem::path outputExePath;
    std::filesystem::path runtimeIncludePath;
};

struct BuildResult {
    std::filesystem::path outputCppPath;
    std::filesystem::path outputExePath;
    std::filesystem::path runtimeIncludePath;
    RuntimeFeatures features;
};

struct ResolvedProgram {
    std::unique_ptr<ProgramNode> program;
    std::unordered_map<std::string, std::string> builtinNamespaceAliases;
    std::unordered_map<std::string, std::string> builtinSymbolAliases;
};

class Compiler {
public:
    static ResolvedProgram resolveForExecution(const std::filesystem::path& inputPath) {
        std::unordered_map<std::string, std::string> builtinNamespaceAliases;
        std::unordered_map<std::string, std::string> builtinSymbolAliases;
        auto program = resolveProgram(
            std::filesystem::absolute(inputPath),
            builtinNamespaceAliases,
            builtinSymbolAliases);

        ResolvedProgram resolved;
        resolved.program = std::move(program);
        resolved.builtinNamespaceAliases = std::move(builtinNamespaceAliases);
        resolved.builtinSymbolAliases = std::move(builtinSymbolAliases);
        return resolved;
    }

    BuildResult compile(const BuildOptions& options) const {
        auto resolved = resolveForExecution(options.inputPath);

        Generator generator;
        const RuntimeFeatures features = generator.generate(
            resolved.program.get(),
            resolved.builtinNamespaceAliases,
            resolved.builtinSymbolAliases,
            options.outputCppPath.string());

        BuildResult result;
        result.outputCppPath = options.outputCppPath;
        result.outputExePath = options.outputExePath;
        result.runtimeIncludePath = options.runtimeIncludePath;
        result.features = features;
        return result;
    }

    int buildExecutable(const BuildResult& result) const {
        std::optional<std::string> raylibInclude = getenvString("RAYQUIRO_RAYLIB_INCLUDE");
        std::optional<std::string> raylibLibDir = getenvString("RAYQUIRO_RAYLIB_LIBDIR");
        const std::filesystem::path runtimeRoot = result.runtimeIncludePath.parent_path().parent_path();
        const std::filesystem::path repoRoot = runtimeRoot;
        const ToolchainCommands toolchain = resolveToolchain(runtimeRoot);

        if (result.features.usesEngine && (!raylibInclude.has_value() || !raylibLibDir.has_value()) && BundledRaylib::isAvailable(repoRoot)) {
            BundledRaylib::ensure(repoRoot, toolchain.cc, toolchain.ar);
            raylibInclude = BundledRaylib::sourceRoot(repoRoot).string();
            raylibLibDir = BundledRaylib::buildRoot(repoRoot).string();
        }

        std::string command = quoteCommand(toolchain.cxx) + " \"" + result.outputCppPath.string() + "\"";
        command += " -std=c++17 -O2 -s";
        command += " -I\"" + std::filesystem::absolute(result.runtimeIncludePath).string() + "\"";
        command += " -o \"" + result.outputExePath.string() + "\"";

        if (result.features.usesEngine) {
            if (raylibInclude.has_value()) {
                command += " -I\"" + raylibInclude.value() + "\"";
            }
            if (raylibLibDir.has_value()) {
                command += " -L\"" + raylibLibDir.value() + "\"";
            }

            command += " -lraylib";
#ifdef _WIN32
            command += " -lopengl32 -lgdi32 -lwinmm";
#elif defined(__APPLE__)
            command += " -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL";
#else
            command += " -lGL -lm -lpthread -ldl -lrt -lX11";
#endif
        }

        if (result.features.usesApp) {
#ifdef _WIN32
            command += " -lgdi32 -luser32";
#else
            throw std::runtime_error("rayquiro.app native build is currently supported on Windows only.");
#endif
        }

        if (result.features.usesUi && !result.features.usesApp) {
#ifdef _WIN32
            command += " -lgdi32 -luser32";
#else
            throw std::runtime_error("rayquiro.ui native build is currently supported on Windows only.");
#endif
        }

        if (result.features.usesEnv) {
#ifdef _WIN32
            if (!result.features.usesApp) {
                command += " -luser32";
            }
            command += " -ladvapi32";
#endif
        }

        if (result.features.usesWeb) {
#ifdef _WIN32
            command += " -lws2_32";
#else
            command += " -ldl";
#endif
        }

        if (result.features.usesNativeModules) {
#ifndef _WIN32
            command += " -ldl";
#endif
        }

        if (result.features.usesApp || result.features.usesUi) {
#ifdef _WIN32
            command += " -mwindows";
#endif
        }

        return std::system(command.c_str());
    }

    static std::string readText(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open source file: " + path.string());
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

private:
    struct ToolchainCommands {
        std::string cxx = "g++";
        std::string cc = "gcc";
        std::string ar = "ar";
    };

    struct ModuleExport {
        bool isFunction = false;
        bool isLet = false;
        std::vector<std::string> params;
    };

    struct ResolveContext {
        std::unordered_set<std::string> loadingFiles;
        std::unordered_set<std::string> loadedModules;
        std::unordered_map<std::string, std::string> builtinNamespaceAliases;
        std::unordered_map<std::string, std::string> builtinSymbolAliases;
        std::unordered_map<std::string, std::string> preferredModuleAliasesByPath;
        int syntheticModuleId = 0;
    };

    static std::string quoteCommand(const std::string& value) {
        if (value.find('\\') == std::string::npos &&
            value.find('/') == std::string::npos &&
            value.find(':') == std::string::npos &&
            value.find(' ') == std::string::npos) {
            return value;
        }
        return "\"" + value + "\"";
    }

    static bool isSyntheticModuleAlias(const std::string& alias) {
        return alias.rfind("__rqmod", 0) == 0;
    }

    static std::filesystem::path pickExistingTool(
        const std::filesystem::path& runtimeRoot,
        const std::vector<std::filesystem::path>& candidates
    ) {
        for (const auto& candidate : candidates) {
            const std::filesystem::path resolved = runtimeRoot / candidate;
            if (std::filesystem::exists(resolved) && std::filesystem::is_regular_file(resolved)) {
                return resolved;
            }
        }
        return {};
    }

    static std::string siblingToolOrDefault(
        const std::string& baseTool,
        const char* envName,
        const std::string& defaultName,
        const std::filesystem::path& runtimeRoot,
        const std::vector<std::filesystem::path>& bundledCandidates
    ) {
        if (const auto envValue = getenvString(envName)) {
            return envValue.value();
        }

        const std::filesystem::path basePath = std::filesystem::path(baseTool);
        if (!basePath.empty() && basePath.has_parent_path()) {
            const std::filesystem::path sibling = basePath.parent_path() / (defaultName + ".exe");
            if (std::filesystem::exists(sibling) && std::filesystem::is_regular_file(sibling)) {
                return sibling.string();
            }
        }

        const std::filesystem::path bundled = pickExistingTool(runtimeRoot, bundledCandidates);
        if (!bundled.empty()) {
            return bundled.string();
        }

        return defaultName;
    }

    static ToolchainCommands resolveToolchain(const std::filesystem::path& runtimeRoot) {
        ToolchainCommands toolchain;

        if (const auto envCxx = getenvString("RAYQUIRO_CXX")) {
            toolchain.cxx = envCxx.value();
        } else {
            const std::filesystem::path bundledCxx = pickExistingTool(runtimeRoot, {
                "toolchain/bin/g++.exe",
                "toolchain/mingw64/bin/g++.exe",
                "mingw64/bin/g++.exe",
                "bin/g++.exe"
            });
            if (!bundledCxx.empty()) {
                toolchain.cxx = bundledCxx.string();
            }
        }

        toolchain.cc = siblingToolOrDefault(
            toolchain.cxx,
            "RAYQUIRO_CC",
            "gcc",
            runtimeRoot,
            {"toolchain/bin/gcc.exe", "toolchain/mingw64/bin/gcc.exe", "mingw64/bin/gcc.exe", "bin/gcc.exe"});
        toolchain.ar = siblingToolOrDefault(
            toolchain.cxx,
            "RAYQUIRO_AR",
            "ar",
            runtimeRoot,
            {"toolchain/bin/ar.exe", "toolchain/mingw64/bin/ar.exe", "mingw64/bin/ar.exe", "bin/ar.exe"});

        return toolchain;
    }

    static std::unique_ptr<ProgramNode> parseProgram(const std::filesystem::path& path) {
        const std::string source = readText(path);
        Lexer lexer(source);
        const auto tokens = lexer.tokenize();
        Parser parser(tokens);
        return parser.parse();
    }

    static std::unique_ptr<ProgramNode> resolveProgram(
        const std::filesystem::path& entryPath,
        std::unordered_map<std::string, std::string>& builtinNamespaceAliases,
        std::unordered_map<std::string, std::string>& builtinSymbolAliases
    ) {
        ResolveContext context;
        auto root = std::make_unique<ProgramNode>();
        auto statements = resolveFile(entryPath, "", context);
        root->statements.reserve(statements.size());
        for (auto& statement : statements) {
            root->statements.push_back(std::move(statement));
        }
        builtinNamespaceAliases = context.builtinNamespaceAliases;
        builtinSymbolAliases = context.builtinSymbolAliases;
        return root;
    }

    static std::vector<std::unique_ptr<Stmt>> resolveFile(
        const std::filesystem::path& filePath,
        const std::string& moduleAlias,
        ResolveContext& context
    ) {
        const std::filesystem::path normalizedPath = std::filesystem::absolute(filePath).lexically_normal();
        const std::string fileKey = normalizedPath.string();
        const std::string importKey = moduleCacheKey(normalizedPath, moduleAlias);

        if (context.loadedModules.count(importKey) != 0) {
            return {};
        }

        if (!moduleAlias.empty() && !isSyntheticModuleAlias(moduleAlias) &&
            context.preferredModuleAliasesByPath.count(fileKey) == 0) {
            context.preferredModuleAliasesByPath[fileKey] = moduleAlias;
        }

        if (context.loadingFiles.count(fileKey) != 0) {
            throw std::runtime_error("Import cycle detected at: " + fileKey);
        }

        context.loadingFiles.insert(fileKey);
        auto program = parseProgram(normalizedPath);
        std::vector<std::unique_ptr<Stmt>> mergedStatements;

        for (auto& statement : program->statements) {
            if (auto importStmt = dynamic_cast<ImportStmt*>(statement.get())) {
                if (isBuiltinModule(importStmt->module) || isNativeRuntimeModule(importStmt->module)) {
                    registerBuiltinImport(*importStmt, context);
                    continue;
                }

                const std::filesystem::path importPath = resolveImportPath(importStmt->module, normalizedPath.parent_path());
                auto importedStatements = resolveFile(importPath, importStmt->alias, context);
                for (auto& importedStatement : importedStatements) {
                    mergedStatements.push_back(std::move(importedStatement));
                }
                continue;
            }

            if (auto fromImportStmt = dynamic_cast<FromImportStmt*>(statement.get())) {
                if (isBuiltinModule(fromImportStmt->module) || isNativeRuntimeModule(fromImportStmt->module)) {
                    registerBuiltinFromImport(*fromImportStmt, context);
                    continue;
                }

                const std::filesystem::path importPath = resolveImportPath(fromImportStmt->module, normalizedPath.parent_path());
                const std::string hiddenAlias = preferredAliasForImportedFile(importPath, context);
                auto importedStatements = resolveFile(importPath, hiddenAlias, context);
                for (auto& importedStatement : importedStatements) {
                    mergedStatements.push_back(std::move(importedStatement));
                }

                const auto exports = collectTopLevelExports(*parseProgram(importPath));
                for (const auto& binding : fromImportStmt->bindings) {
                    const auto found = exports.find(binding.name);
                    if (found == exports.end()) {
                        throw std::runtime_error("Cannot import symbol '" + binding.name + "' from " + importPath.string());
                    }

                    const std::string targetName = binding.alias.empty() ? binding.name : binding.alias;
                    const std::string sourceName = namespacedSymbol(hiddenAlias, binding.name);
                    if (found->second.isFunction) {
                        mergedStatements.push_back(makeFunctionAlias(targetName, sourceName, found->second.params));
                    } else {
                        mergedStatements.push_back(makeVariableAlias(targetName, sourceName, found->second.isLet));
                    }
                }
                continue;
            }

            mergedStatements.push_back(std::move(statement));
        }

        if (!moduleAlias.empty()) {
            auto namespacedProgram = std::make_unique<ProgramNode>();
            namespacedProgram->statements.reserve(mergedStatements.size());
            for (auto& statement : mergedStatements) {
                namespacedProgram->statements.push_back(std::move(statement));
            }
            namespaceProgram(namespacedProgram.get(), moduleAlias);
            mergedStatements.clear();
            for (auto& statement : namespacedProgram->statements) {
                mergedStatements.push_back(std::move(statement));
            }
        }

        context.loadingFiles.erase(fileKey);
        context.loadedModules.insert(importKey);
        return mergedStatements;
    }

    static bool isBuiltinModule(const std::string& moduleName) {
        return moduleName == "rayquiro.app" ||
            moduleName == "rayquiro.ui" ||
            moduleName == "rayquiro.web" ||
            moduleName == "rayquiro.engine" ||
            moduleName == "raytolfas.engine" ||
            moduleName == "rayquiro.json" ||
            moduleName == "rayquiro.time" ||
            moduleName == "rayquiro.fs" ||
            moduleName == "rayquiro.env" ||
            moduleName == "rayquiro.process";
    }

    static bool isNativeRuntimeModule(const std::string& moduleName) {
        if (isBuiltinModule(moduleName)) {
            return false;
        }
        return moduleName.rfind("rayquiro.", 0) == 0;
    }

    static std::string builtinPrefixForModule(const std::string& moduleName) {
        if (moduleName == "rayquiro.app") return "app";
        if (moduleName == "rayquiro.ui") return "ui";
        if (moduleName == "rayquiro.web") return "web";
        if (moduleName == "rayquiro.engine" || moduleName == "raytolfas.engine") return "engine";
        if (moduleName == "rayquiro.json") return "json";
        if (moduleName == "rayquiro.time") return "time";
        if (moduleName == "rayquiro.fs") return "fs";
        if (moduleName == "rayquiro.env") return "env";
        if (moduleName == "rayquiro.process") return "process";
        if (isNativeRuntimeModule(moduleName)) {
            return moduleName.substr(std::string("rayquiro.").size());
        }
        return "";
    }

    static void registerBuiltinImport(const ImportStmt& importStmt, ResolveContext& context) {
        const std::string prefix = builtinPrefixForModule(importStmt.module);
        if (prefix.empty()) {
            return;
        }

        if (!importStmt.alias.empty()) {
            context.builtinNamespaceAliases[importStmt.alias] = prefix;
        }
        context.builtinNamespaceAliases[prefix] = prefix;
    }

    static void registerBuiltinFromImport(const FromImportStmt& fromImportStmt, ResolveContext& context) {
        const std::string prefix = builtinPrefixForModule(fromImportStmt.module);
        if (prefix.empty()) {
            return;
        }

        context.builtinNamespaceAliases[prefix] = prefix;
        for (const auto& binding : fromImportStmt.bindings) {
            const std::string targetName = binding.alias.empty() ? binding.name : binding.alias;
            context.builtinSymbolAliases[targetName] = prefix + "." + binding.name;
        }
    }

    static std::string moduleCacheKey(const std::filesystem::path& filePath, const std::string& alias) {
        return std::filesystem::absolute(filePath).lexically_normal().string() + "|" + alias;
    }

    static std::filesystem::path resolveImportPath(
        const std::string& moduleName,
        const std::filesystem::path& importerDir
    ) {
        std::filesystem::path rawPath = moduleName;
        if (rawPath.is_relative()) {
            rawPath = importerDir / rawPath;
        }

        const std::vector<std::filesystem::path> candidates = {
            rawPath,
            rawPath.has_extension() ? rawPath : std::filesystem::path(rawPath.string() + ".rq"),
            rawPath / "main.rq"
        };

        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
                return std::filesystem::absolute(candidate).lexically_normal();
            }
        }

        for (const auto& candidate : projectModuleCandidates(moduleName, importerDir)) {
            if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
                return std::filesystem::absolute(candidate).lexically_normal();
            }
        }

        for (const auto& candidate : frameworkImportCandidates(moduleName)) {
            if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
                return std::filesystem::absolute(candidate).lexically_normal();
            }
        }

        throw std::runtime_error("Cannot resolve import: " + moduleName + " from " + importerDir.string());
    }

    static std::vector<std::string> splitModuleName(const std::string& value, char separator) {
        std::vector<std::string> parts;
        std::stringstream stream(value);
        std::string token;
        while (std::getline(stream, token, separator)) {
            if (!token.empty()) {
                parts.push_back(token);
            }
        }
        return parts;
    }

    static std::vector<std::filesystem::path> frameworkImportCandidates(const std::string& moduleName) {
        if (moduleName.empty() ||
            moduleName[0] == '.' ||
            moduleName.find('/') != std::string::npos ||
            moduleName.find('\\') != std::string::npos ||
            moduleName.find(':') != std::string::npos) {
            return {};
        }

        const std::vector<std::string> parts = splitModuleName(moduleName, '.');
        if (parts.empty()) {
            return {};
        }

        std::filesystem::path frameworkRoot = RayQuiroUserPaths::frameworksRoot() / parts.front();
        if (parts.size() == 1) {
            return {
                frameworkRoot / "main.rq",
                frameworkRoot / (parts.front() + ".rq")
            };
        }

        std::filesystem::path relativePath;
        for (size_t i = 1; i < parts.size(); ++i) {
            relativePath /= parts[i];
        }

        return {
            frameworkRoot / relativePath,
            std::filesystem::path((frameworkRoot / relativePath).string() + ".rq"),
            frameworkRoot / relativePath / "main.rq"
        };
    }

    static std::vector<std::filesystem::path> projectModuleCandidates(
        const std::string& moduleName,
        const std::filesystem::path& importerDir
    ) {
        if (moduleName.empty() ||
            moduleName[0] == '.' ||
            moduleName.find('/') != std::string::npos ||
            moduleName.find('\\') != std::string::npos ||
            moduleName.find(':') != std::string::npos) {
            return {};
        }

        const std::vector<std::string> parts = splitModuleName(moduleName, '.');
        if (parts.empty()) {
            return {};
        }

        std::vector<std::filesystem::path> roots;
        std::unordered_set<std::string> seenRoots;

        auto addRoot = [&](const std::filesystem::path& candidateRoot) {
            const std::filesystem::path normalized = std::filesystem::absolute(candidateRoot).lexically_normal();
            const std::string key = normalized.string();
            if (seenRoots.insert(key).second) {
                roots.push_back(normalized);
            }
        };

        std::filesystem::path cursor = std::filesystem::absolute(importerDir).lexically_normal();
        while (!cursor.empty()) {
            addRoot(cursor / ".rq_modules");
            if (cursor == cursor.root_path() || cursor.parent_path() == cursor) {
                break;
            }
            cursor = cursor.parent_path();
        }
        addRoot(std::filesystem::current_path() / ".rq_modules");

        std::vector<std::filesystem::path> candidates;
        for (const auto& root : roots) {
            std::filesystem::path frameworkRoot = root / parts.front();
            if (parts.size() == 1) {
                candidates.push_back(frameworkRoot / "main.rq");
                candidates.push_back(frameworkRoot / (parts.front() + ".rq"));
                continue;
            }

            std::filesystem::path relativePath;
            for (size_t i = 1; i < parts.size(); ++i) {
                relativePath /= parts[i];
            }

            candidates.push_back(frameworkRoot / relativePath);
            candidates.push_back(std::filesystem::path((frameworkRoot / relativePath).string() + ".rq"));
            candidates.push_back(frameworkRoot / relativePath / "main.rq");
        }

        return candidates;
    }

    static std::unordered_map<std::string, ModuleExport> collectTopLevelExports(const ProgramNode& program) {
        std::unordered_map<std::string, ModuleExport> exports;
        for (const auto& statement : program.statements) {
            if (auto varStmt = dynamic_cast<VarStmt*>(statement.get())) {
                exports[varStmt->name] = ModuleExport{false, varStmt->isLet, {}};
            } else if (auto functionStmt = dynamic_cast<FunctionStmt*>(statement.get())) {
                exports[functionStmt->name] = ModuleExport{true, false, functionStmt->params};
            }
        }
        return exports;
    }

    static std::string preferredAliasForImportedFile(
        const std::filesystem::path& filePath,
        ResolveContext& context
    ) {
        const std::string fileKey = std::filesystem::absolute(filePath).lexically_normal().string();
        const auto found = context.preferredModuleAliasesByPath.find(fileKey);
        if (found != context.preferredModuleAliasesByPath.end()) {
            return found->second;
        }

        const std::string alias = "__rqmod" + std::to_string(context.syntheticModuleId++);
        context.preferredModuleAliasesByPath[fileKey] = alias;
        return alias;
    }

    static std::unique_ptr<Stmt> makeVariableAlias(
        const std::string& targetName,
        const std::string& sourceName,
        bool isLet
    ) {
        auto node = std::make_unique<VarStmt>();
        node->name = targetName;
        node->isLet = isLet;
        auto identifier = std::make_unique<IdentifierExpr>();
        identifier->name = sourceName;
        node->initializer = std::move(identifier);
        return node;
    }

    static std::unique_ptr<Stmt> makeFunctionAlias(
        const std::string& targetName,
        const std::string& sourceName,
        const std::vector<std::string>& params
    ) {
        auto function = std::make_unique<FunctionStmt>();
        function->name = targetName;
        function->params = params;
        function->body = std::make_unique<BlockStmt>();

        auto returnStmt = std::make_unique<ReturnStmt>();
        auto call = std::make_unique<CallExpr>();
        call->callee = sourceName;
        for (const std::string& param : params) {
            auto identifier = std::make_unique<IdentifierExpr>();
            identifier->name = param;
            call->args.push_back(std::move(identifier));
        }
        returnStmt->value = std::move(call);
        function->body->statements.push_back(std::move(returnStmt));
        return function;
    }

    static std::unordered_set<std::string> collectTopLevelSymbols(const ProgramNode& program) {
        std::unordered_set<std::string> symbols;
        for (const auto& statement : program.statements) {
            if (auto varStmt = dynamic_cast<VarStmt*>(statement.get())) {
                symbols.insert(varStmt->name);
            } else if (auto functionStmt = dynamic_cast<FunctionStmt*>(statement.get())) {
                symbols.insert(functionStmt->name);
            }
        }
        return symbols;
    }

    static std::string namespacedSymbol(const std::string& alias, const std::string& name) {
        return alias + "." + name;
    }

    static std::string maybeNamespaceName(
        const std::string& name,
        const std::string& alias,
        const std::unordered_set<std::string>& topLevelSymbols,
        const std::vector<std::unordered_set<std::string>>& scopes
    ) {
        if (name.find('.') != std::string::npos) {
            return name;
        }
        if (topLevelSymbols.count(name) == 0) {
            return name;
        }
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name) != 0) {
                return name;
            }
        }
        return namespacedSymbol(alias, name);
    }

    static void namespaceExpr(
        Expr* expr,
        const std::string& alias,
        const std::unordered_set<std::string>& topLevelSymbols,
        std::vector<std::unordered_set<std::string>>& scopes
    ) {
        if (!expr) {
            return;
        }

        if (auto identifier = dynamic_cast<IdentifierExpr*>(expr)) {
            identifier->name = maybeNamespaceName(identifier->name, alias, topLevelSymbols, scopes);
            return;
        }

        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            call->callee = maybeNamespaceName(call->callee, alias, topLevelSymbols, scopes);
            for (auto& arg : call->args) {
                namespaceExpr(arg.get(), alias, topLevelSymbols, scopes);
            }
            return;
        }

        if (auto binary = dynamic_cast<BinaryExpr*>(expr)) {
            namespaceExpr(binary->left.get(), alias, topLevelSymbols, scopes);
            namespaceExpr(binary->right.get(), alias, topLevelSymbols, scopes);
            return;
        }

        if (auto unary = dynamic_cast<UnaryExpr*>(expr)) {
            namespaceExpr(unary->right.get(), alias, topLevelSymbols, scopes);
            return;
        }

        if (auto arrayExpr = dynamic_cast<ArrayExpr*>(expr)) {
            for (auto& element : arrayExpr->elements) {
                namespaceExpr(element.get(), alias, topLevelSymbols, scopes);
            }
            return;
        }

        if (auto indexExpr = dynamic_cast<IndexExpr*>(expr)) {
            namespaceExpr(indexExpr->target.get(), alias, topLevelSymbols, scopes);
            namespaceExpr(indexExpr->index.get(), alias, topLevelSymbols, scopes);
            return;
        }

        if (auto assignExpr = dynamic_cast<AssignExpr*>(expr)) {
            assignExpr->name = maybeNamespaceName(assignExpr->name, alias, topLevelSymbols, scopes);
            namespaceExpr(assignExpr->value.get(), alias, topLevelSymbols, scopes);
        }
    }

    static void namespaceStmt(
        Stmt* stmt,
        const std::string& alias,
        const std::unordered_set<std::string>& topLevelSymbols,
        std::vector<std::unordered_set<std::string>>& scopes,
        bool isTopLevel
    ) {
        if (!stmt) {
            return;
        }

        if (auto varStmt = dynamic_cast<VarStmt*>(stmt)) {
            namespaceExpr(varStmt->initializer.get(), alias, topLevelSymbols, scopes);
            if (isTopLevel) {
                varStmt->name = namespacedSymbol(alias, varStmt->name);
            } else if (!scopes.empty()) {
                scopes.back().insert(varStmt->name);
            }
            return;
        }

        if (auto functionStmt = dynamic_cast<FunctionStmt*>(stmt)) {
            if (isTopLevel) {
                functionStmt->name = namespacedSymbol(alias, functionStmt->name);
            }

            scopes.push_back({});
            for (const std::string& param : functionStmt->params) {
                scopes.back().insert(param);
            }
            for (auto& bodyStmt : functionStmt->body->statements) {
                namespaceStmt(bodyStmt.get(), alias, topLevelSymbols, scopes, false);
            }
            scopes.pop_back();
            return;
        }

        if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
            namespaceExpr(exprStmt->expr.get(), alias, topLevelSymbols, scopes);
            return;
        }

        if (auto logStmt = dynamic_cast<LogStmt*>(stmt)) {
            namespaceExpr(logStmt->message.get(), alias, topLevelSymbols, scopes);
            return;
        }

        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            namespaceExpr(returnStmt->value.get(), alias, topLevelSymbols, scopes);
            return;
        }

        if (auto blockStmt = dynamic_cast<BlockStmt*>(stmt)) {
            scopes.push_back({});
            for (auto& child : blockStmt->statements) {
                namespaceStmt(child.get(), alias, topLevelSymbols, scopes, false);
            }
            scopes.pop_back();
            return;
        }

        if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            namespaceExpr(ifStmt->condition.get(), alias, topLevelSymbols, scopes);
            namespaceStmt(ifStmt->thenBranch.get(), alias, topLevelSymbols, scopes, false);
            namespaceStmt(ifStmt->elseBranch.get(), alias, topLevelSymbols, scopes, false);
            return;
        }

        if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            namespaceExpr(whileStmt->condition.get(), alias, topLevelSymbols, scopes);
            namespaceStmt(whileStmt->body.get(), alias, topLevelSymbols, scopes, false);
        }
    }

    static void namespaceProgram(ProgramNode* program, const std::string& alias) {
        if (!program || alias.empty()) {
            return;
        }

        const std::unordered_set<std::string> topLevelSymbols = collectTopLevelSymbols(*program);
        std::vector<std::unordered_set<std::string>> scopes;
        for (auto& statement : program->statements) {
            namespaceStmt(statement.get(), alias, topLevelSymbols, scopes, true);
        }
    }

    static std::optional<std::string> getenvString(const char* name) {
        const char* value = std::getenv(name);
        if (value == nullptr || value[0] == '\0') {
            return std::nullopt;
        }
        return std::string(value);
    }
};
