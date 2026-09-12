#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <future>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "AST.h"
#include "UserPaths.h"
#include "rte_api.h"
#include "BuiltinModules.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define DrawText Win32DrawText
#define DrawTextEx Win32DrawTextEx
#define LoadImage Win32LoadImage
#define PlaySound Win32PlaySound
#include <windows.h>
#include <winreg.h>
extern "C" int connect(SOCKET s, const struct sockaddr* name, int namelen);
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound
#include "Framework.h"
#include "ModernUI.h"
#else
#include <dlfcn.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#ifndef RAYQUIRO_SOCKET_DEFINED
#define RAYQUIRO_SOCKET_DEFINED
using SOCKET = int;
static constexpr SOCKET INVALID_SOCKET = -1;
static constexpr int SOCKET_ERROR = -1;
inline int closesocket(SOCKET socket) { return ::close(socket); }
#endif
#endif

class Interpreter {
public:

    struct LambdaValue {
        std::vector<std::string> paramNames;
        std::vector<Expr*>       paramDefaults;
        bool hasVariadic = false;
        BlockStmt* body;
        std::shared_ptr<void> closure;
        bool isAsync = false;
    };

    struct Value {
        using Arr = std::shared_ptr<std::vector<Value>>;
        using Obj = std::shared_ptr<std::unordered_map<std::string, Value>>;
        using Fn  = std::shared_ptr<LambdaValue>;

        std::variant<std::monostate, double, std::string, bool, Arr, Obj, Fn> data;

        Value() : data(std::monostate{}) {}
        Value(double v) : data(v) {}
        Value(const std::string& s) : data(s) {}
        Value(const char* s) : data(std::string(s)) {}
        Value(bool b) : data(b) {}

        static Value array() { return Value(Arr(new std::vector<Value>())); }
        static Value object() { return Value(Obj(new std::unordered_map<std::string, Value>())); }

    private:
        Value(Arr arrayValue) : data(arrayValue) {}
        Value(Obj objectValue) : data(objectValue) {}

    public:
        explicit Value(Fn lambdaValue) : data(lambdaValue) {}

        Arr as_array() const { return std::get<Arr>(data); }
        Obj as_object() const { return std::get<Obj>(data); }
        Fn  as_lambda() const { return std::get<Fn>(data); }
    };

    Interpreter(
        std::filesystem::path projectRoot,
        std::filesystem::path executablePath,
        std::unordered_map<std::string, std::string> builtinNamespaceAliases,
        std::unordered_map<std::string, std::string> builtinSymbolAliases
    )
        : projectRoot_(std::move(projectRoot)),
          executablePath_(std::move(executablePath)),
          builtinNamespaceAliases_(std::move(builtinNamespaceAliases)),
          builtinSymbolAliases_(std::move(builtinSymbolAliases)) {}

    void setProcessArgs(std::vector<std::string> args) {
        processArgs_ = std::move(args);
    }

    ~Interpreter() {
        unloadNativeModules();
    }

    bool supports(const ProgramNode& program) const {
        for (const auto& statement : program.statements) {
            if (!supportsStmt(statement.get())) {
                return false;
            }
        }
        return true;
    }

    int run(ProgramNode& program) {
        globals_ = std::make_shared<Environment>();
        globals_->define("pi", Value(3.14159265358979323846), true);
        functions_.clear();

        for (const auto& statement : program.statements) {
            if (auto functionStmt = dynamic_cast<FunctionStmt*>(statement.get())) {
                functions_[functionStmt->name] = functionStmt;
            }
        }

        for (const auto& statement : program.statements) {
            if (dynamic_cast<FunctionStmt*>(statement.get()) != nullptr) {
                continue;
            }
            execute(statement.get(), globals_);
        }

        return 0;
    }

private:
    struct VariableSlot {
        Value value;
        bool isConst = false;
    };

    struct Environment {
        std::shared_ptr<Environment> parent;
        std::unordered_map<std::string, VariableSlot> values;

        explicit Environment(std::shared_ptr<Environment> parentEnv = nullptr) : parent(std::move(parentEnv)) {}

        void define(const std::string& name, const Value& value, bool isConst) {
            values[name] = VariableSlot{value, isConst};
        }

        Value get(const std::string& name) const {
            const auto found = values.find(name);
            if (found != values.end()) {
                return found->second.value;
            }
            if (parent) {
                return parent->get(name);
            }
            throw std::runtime_error("Undefined variable: " + name);
        }

        void assign(const std::string& name, const Value& value) {
            const auto found = values.find(name);
            if (found != values.end()) {
                if (found->second.isConst) {
                    throw std::runtime_error("Cannot assign to let binding: " + name);
                }
                found->second.value = value;
                return;
            }
            if (parent) {
                parent->assign(name, value);
                return;
            }
            throw std::runtime_error("Undefined variable: " + name);
        }

        void set(const std::string& name, const Value& value) { assign(name, value); }

        const std::unordered_map<std::string, VariableSlot>& vars() const { return values; }
    };

    struct ReturnSignal {
        Value value;
    };

    struct BreakSignal {};
    struct ContinueSignal {};

    struct WebRoute {
        std::string route = "/";
        std::string title = "RayQuiro";
        std::string path;
        std::string head;
        std::string body;
        std::vector<std::string> stack;
    };

    struct WebState {
        std::string defaultTitle = "RayQuiro";
        std::string outputRoot = "build";
        std::string publicDir = "public";
        std::string bindHost = "127.0.0.1";
        std::string css;
        std::unordered_map<std::string, WebRoute> routes;
        std::string currentRoute = "/";
        bool active = false;
        bool liveMode = false;
        int livePort = 5274;
    };

    struct EngineLight {
        RTVec3 direction { -0.4f, -1.0f, -0.25f };
        RTColor color { 255, 245, 222, 255 };
        float intensity = 1.0f;
    };

    struct EngineMaterial {
        RTColor albedo { 255, 255, 255, 255 };
        RTColor emissive { 0, 0, 0, 255 };
        float roughness = 0.5f;
        float metallic = 0.0f;
        std::string texture;
        std::string normalTexture;
    };

    struct EngineMesh {
        std::string primitive = "cube";
        std::string source;
        RTVec3 defaultSize { 1.0f, 1.0f, 1.0f };
        RTVec2 defaultPlaneSize { 4.0f, 4.0f };
        float defaultRadius = 1.0f;
    };

    struct EngineTexture {
        std::string source;
        bool srgb = true;
        bool normalMap = false;
    };

    struct EngineEntity {
        std::string kind = "cube";
        RTVec3 position { 0.0f, 0.5f, 0.0f };
        RTVec3 size { 1.0f, 1.0f, 1.0f };
        RTVec3 scale { 1.0f, 1.0f, 1.0f };
        RTVec2 planeSize { 4.0f, 4.0f };
        float radius = 1.0f;
        RTColor color { 255, 255, 255, 255 };
        bool visible = true;
        std::string material;
        std::string mesh;
        std::string texture;
    };

    struct EngineScene {
        std::unordered_map<std::string, EngineEntity> entities;
        std::unordered_map<std::string, EngineMaterial> materials;
        std::unordered_map<std::string, EngineMesh> meshes;
        std::unordered_map<std::string, EngineTexture> textures;
        RTColor ambient { 30, 38, 54, 255 };
        EngineLight sun;
        bool hasSun = false;
    };

    struct EngineState {
        std::string currentScene = "main";
        std::unordered_map<std::string, EngineScene> scenes;
        std::string assetsRoot = "assets";
    };

    struct TcpSocketState {
        SOCKET socket = INVALID_SOCKET;
    };

    struct PostgresApi {
        bool loaded = false;
        bool available = false;
#ifdef _WIN32
        void* library = nullptr;
#else
        void* library = nullptr;
#endif
        using ConnectDbFn = void* (*)(const char*);
        using StatusFn = int (*)(const void*);
        using ErrorMessageFn = const char* (*)(const void*);
        using FinishFn = void (*)(void*);
        using ExecFn = void* (*)(void*, const char*);
        using ResultStatusFn = int (*)(const void*);
        using NtuplesFn = int (*)(const void*);
        using NfieldsFn = int (*)(const void*);
        using FnameFn = const char* (*)(const void*, int);
        using GetvalueFn = const char* (*)(const void*, int, int);
        using ClearFn = void (*)(void*);
        using CmdTuplesFn = const char* (*)(const void*);

        ConnectDbFn connectDb = nullptr;
        StatusFn status = nullptr;
        ErrorMessageFn errorMessage = nullptr;
        FinishFn finish = nullptr;
        ExecFn exec = nullptr;
        ResultStatusFn resultStatus = nullptr;
        NtuplesFn ntuples = nullptr;
        NfieldsFn nfields = nullptr;
        FnameFn fname = nullptr;
        GetvalueFn getvalue = nullptr;
        ClearFn clear = nullptr;
        CmdTuplesFn cmdTuples = nullptr;
    };

    struct NativeModule {
#ifdef _WIN32
        HMODULE handle = nullptr;
#else
        void* handle = nullptr;
#endif
        using InvokeFn = int (*)(const char*, const char*, char**, char**);
        using FreeFn = void (*)(char*);
        InvokeFn invoke = nullptr;
        FreeFn freeMemory = nullptr;
    };

    std::filesystem::path projectRoot_;
    std::filesystem::path executablePath_;
    std::unordered_map<std::string, std::string> builtinNamespaceAliases_;
    std::unordered_map<std::string, std::string> builtinSymbolAliases_;
    std::unordered_map<std::string, FunctionStmt*> functions_;
    std::unordered_map<std::string, NativeModule> nativeModules_;
    std::unordered_map<int, SOCKET> netSockets_;
    std::unordered_map<int, void*> dbConnections_;
    int nextNetHandle_ = 1;
    int nextDbHandle_ = 1;
    PostgresApi postgres_;
    std::shared_ptr<Environment> globals_;
    WebState webState_;
    EngineState engineState_;

    std::vector<std::string> processArgs_;
    std::string currentSourcePath_;
#if RAYQUIRO_HAS_RAYLIB
    std::vector<Sound>    soundRegistry_;
    std::vector<Music>    musicRegistry_;
    std::vector<Texture2D> textureRegistry_;
#endif
    std::unordered_map<std::string, Value> structDefs_;
    std::unordered_map<std::string, std::vector<std::string>> interfaceRegistry_;

    enum class EngineMode { Editor, Playing, Paused };
    EngineMode engineMode_ = EngineMode::Editor;
    Value sceneSnapshot_;
#ifdef _WIN32
    RayQuiroApp appRuntime_;
    RayQuiroModernUI uiRuntime_;
#endif

    using DebugHook = std::function<void(const std::string& filePath, int line)>;
    DebugHook debugHook_;

public:
    void setDebugHook(std::function<void(const std::string&, int)> hook) { debugHook_ = std::move(hook); }
    const std::filesystem::path& getProjectRoot() const { return projectRoot_; }
    void setSourcePath(const std::string& path) { currentSourcePath_ = path; }

private:

    static inline std::unordered_map<std::string, std::shared_future<Value>> promiseStore_;
    static inline std::mutex promiseMutex_;
    static inline int promiseCounter_ = 0;

    static std::string newPromiseId() {
        std::lock_guard<std::mutex> lock(promiseMutex_);
        return "__p" + std::to_string(++promiseCounter_) + "__";
    }

    static void storePromise(const std::string& id, std::shared_future<Value> fut) {
        std::lock_guard<std::mutex> lock(promiseMutex_);
        promiseStore_[id] = std::move(fut);
    }

    static Value awaitPromise(const Value& promiseObj) {
        if (!is_object(promiseObj)) return promiseObj;
        auto obj = promiseObj.as_object();
        auto it = obj->find("__promise_id__");
        if (it == obj->end()) return promiseObj;
        const std::string id = to_string(it->second);
        std::shared_future<Value> fut;
        {
            std::lock_guard<std::mutex> lock(promiseMutex_);
            auto pit = promiseStore_.find(id);
            if (pit == promiseStore_.end()) return Value();
            fut = pit->second;
        }
        Value result = fut.get();
        {
            std::lock_guard<std::mutex> lock(promiseMutex_);
            promiseStore_.erase(id);
        }
        return result;
    }

    static bool is_null(const Value& value) { return std::holds_alternative<std::monostate>(value.data); }
    static bool is_number(const Value& value) { return std::holds_alternative<double>(value.data); }
    static bool is_string(const Value& value) { return std::holds_alternative<std::string>(value.data); }
    static bool is_bool(const Value& value) { return std::holds_alternative<bool>(value.data); }
    static bool is_array(const Value& value) { return std::holds_alternative<Value::Arr>(value.data); }
    static bool is_object(const Value& value) { return std::holds_alternative<Value::Obj>(value.data); }
    static bool is_lambda(const Value& value) { return std::holds_alternative<Value::Fn>(value.data); }

    static double to_number(const Value& value) {
        if (is_number(value)) return std::get<double>(value.data);
        if (is_bool(value)) return std::get<bool>(value.data) ? 1.0 : 0.0;
        if (is_string(value)) {
            try {
                return std::stod(std::get<std::string>(value.data));
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    }

    static std::string number_to_string(double value) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    }

    static std::string to_string(const Value& value) {
        if (is_null(value)) return "null";
        if (is_number(value)) return number_to_string(std::get<double>(value.data));
        if (is_bool(value)) return std::get<bool>(value.data) ? "true" : "false";
        if (is_string(value)) return std::get<std::string>(value.data);
        if (is_array(value)) {
            auto arrayValue = std::get<Value::Arr>(value.data);
            std::string result = "[";
            for (size_t i = 0; i < arrayValue->size(); ++i) {
                if (i) result += ", ";
                result += to_string((*arrayValue)[i]);
            }
            result += "]";
            return result;
        }
        if (is_object(value)) {
            auto objectValue = std::get<Value::Obj>(value.data);
            std::string result = "{";
            size_t index = 0;
            for (const auto& pair : *objectValue) {
                if (index++) result += ", ";
                result += pair.first + ": " + to_string(pair.second);
            }
            result += "}";
            return result;
        }
        if (is_lambda(value)) return "<fn>";
        return "";
    }

    static bool truthy(const Value& value) {
        if (is_null(value)) return false;
        if (is_bool(value)) return std::get<bool>(value.data);
        if (is_number(value)) return std::get<double>(value.data) != 0.0;
        if (is_string(value)) return !std::get<std::string>(value.data).empty();
        if (is_array(value)) return !std::get<Value::Arr>(value.data)->empty();
        if (is_object(value)) return !std::get<Value::Obj>(value.data)->empty();
        if (is_lambda(value)) return true;
        return false;
    }

    static Value array(const std::vector<Value>& items) {
        Value value = Value::array();
        *value.as_array() = items;
        return value;
    }

    static Value add(const Value& left, const Value& right) {
        if (is_number(left) && is_number(right)) {
            return Value(std::get<double>(left.data) + std::get<double>(right.data));
        }
        return Value(to_string(left) + to_string(right));
    }

    static Value sub(const Value& left, const Value& right) { return Value(to_number(left) - to_number(right)); }
    static Value mul(const Value& left, const Value& right) { return Value(to_number(left) * to_number(right)); }
    static Value div(const Value& left, const Value& right) { return Value(to_number(left) / to_number(right)); }
    static Value mod(const Value& left, const Value& right) { return Value(std::fmod(to_number(left), to_number(right))); }

    static Value eq(const Value& left, const Value& right) {
        if (is_null(left) && is_null(right)) return Value(true);
        if (is_number(left) && is_number(right)) return Value(std::get<double>(left.data) == std::get<double>(right.data));
        if (is_bool(left) && is_bool(right)) return Value(std::get<bool>(left.data) == std::get<bool>(right.data));
        if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) == std::get<std::string>(right.data));
        return Value(false);
    }

    static Value neq(const Value& left, const Value& right) { return Value(!truthy(eq(left, right))); }
    static Value lt(const Value& left, const Value& right) {
        if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) < std::get<std::string>(right.data));
        return Value(to_number(left) < to_number(right));
    }
    static Value lte(const Value& left, const Value& right) {
        if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) <= std::get<std::string>(right.data));
        return Value(to_number(left) <= to_number(right));
    }
    static Value gt(const Value& left, const Value& right) {
        if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) > std::get<std::string>(right.data));
        return Value(to_number(left) > to_number(right));
    }
    static Value gte(const Value& left, const Value& right) {
        if (is_string(left) && is_string(right)) return Value(std::get<std::string>(left.data) >= std::get<std::string>(right.data));
        return Value(to_number(left) >= to_number(right));
    }

    static Value index(const Value& target, const Value& indexValue) {
        if (is_array(target)) {
            auto items = target.as_array();
            const size_t indexNumber = static_cast<size_t>(to_number(indexValue));
            if (indexNumber < items->size()) {
                return (*items)[indexNumber];
            }
            return Value();
        }

        if (is_string(target)) {
            const std::string& text = std::get<std::string>(target.data);
            const size_t indexNumber = static_cast<size_t>(to_number(indexValue));
            if (indexNumber < text.size()) {
                return Value(std::string(1, text[indexNumber]));
            }
            return Value();
        }

        if (is_object(target)) {
            auto objectValue = target.as_object();
            const std::string key = to_string(indexValue);
            const auto found = objectValue->find(key);
            if (found != objectValue->end()) {
                return found->second;
            }
            return Value();
        }

        return Value();
    }

    std::string canonicalizeBuiltinName(const std::string& name) const {
        const auto exact = builtinSymbolAliases_.find(name);
        if (exact != builtinSymbolAliases_.end()) {
            return exact->second;
        }

        const size_t dot = name.find('.');
        if (dot == std::string::npos) {
            return name;
        }

        const std::string prefix = name.substr(0, dot);
        const auto found = builtinNamespaceAliases_.find(prefix);
        if (found == builtinNamespaceAliases_.end()) {
            return name;
        }

        return found->second + name.substr(dot);
    }

    bool supportsStmt(Stmt* stmt) const {
        if (!stmt) return true;
        if (auto varStmt = dynamic_cast<VarStmt*>(stmt)) return supportsExpr(varStmt->initializer.get());
        if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) return supportsExpr(exprStmt->expr.get());
        if (auto logStmt = dynamic_cast<LogStmt*>(stmt)) return supportsExpr(logStmt->message.get());
        if (auto tryCatch = dynamic_cast<TryCatchStmt*>(stmt)) {
            return supportsStmt(tryCatch->tryBody.get()) && supportsStmt(tryCatch->catchBody.get());
        }
        if (auto blockStmt = dynamic_cast<BlockStmt*>(stmt)) {
            for (const auto& child : blockStmt->statements) if (!supportsStmt(child.get())) return false;
            return true;
        }
        if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            return supportsExpr(ifStmt->condition.get()) &&
                supportsStmt(ifStmt->thenBranch.get()) &&
                supportsStmt(ifStmt->elseBranch.get());
        }
        if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            return supportsExpr(whileStmt->condition.get()) && supportsStmt(whileStmt->body.get());
        }
        if (auto functionStmt = dynamic_cast<FunctionStmt*>(stmt)) return supportsStmt(functionStmt->body.get());
        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            for (const auto& v : returnStmt->values)
                if (!supportsExpr(v.get())) return false;
            return true;
        }
        return true;
    }

    bool supportsExpr(Expr* expr) const {
        if (!expr) return true;
        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            const std::string builtin = canonicalizeBuiltinName(call->callee);
            for (const auto& arg : call->args) if (!supportsExpr(arg.get())) return false;
            return true;
        }
        if (auto binary = dynamic_cast<BinaryExpr*>(expr)) return supportsExpr(binary->left.get()) && supportsExpr(binary->right.get());
        if (auto unary = dynamic_cast<UnaryExpr*>(expr)) return supportsExpr(unary->right.get());
        if (auto assign = dynamic_cast<AssignExpr*>(expr)) return supportsExpr(assign->value.get());
        if (auto indexExpr = dynamic_cast<IndexExpr*>(expr)) return supportsExpr(indexExpr->target.get()) && supportsExpr(indexExpr->index.get());
        if (auto setIdx = dynamic_cast<SetIndexExpr*>(expr)) return supportsExpr(setIdx->object.get()) && supportsExpr(setIdx->index.get()) && supportsExpr(setIdx->value.get());
        if (auto awaitExpr = dynamic_cast<AwaitExpr*>(expr)) return supportsExpr(awaitExpr->operand.get());
        if (auto arrayExpr = dynamic_cast<ArrayExpr*>(expr)) {
            for (const auto& element : arrayExpr->elements) if (!supportsExpr(element.get())) return false;
        }
        return true;
    }

    Value evaluate(Expr* expr, const std::shared_ptr<Environment>& env) {
        if (!expr) return Value();

        if (auto literal = dynamic_cast<LiteralExpr*>(expr)) {
            if (literal->kind == LiteralExpr::Kind::Number) return Value(std::stod(literal->value));
            if (literal->kind == LiteralExpr::Kind::String) return Value(literal->value);
            if (literal->kind == LiteralExpr::Kind::Bool) return Value(literal->value == "true");
            return Value();
        }

        if (auto identifier = dynamic_cast<IdentifierExpr*>(expr)) {
            return env->get(identifier->name);
        }

        if (auto binary = dynamic_cast<BinaryExpr*>(expr)) {
            const Value left = evaluate(binary->left.get(), env);
            if (binary->op == "&&") {
                if (!truthy(left)) return Value(false);
                return Value(truthy(evaluate(binary->right.get(), env)));
            }
            if (binary->op == "||") {
                if (truthy(left)) return Value(true);
                return Value(truthy(evaluate(binary->right.get(), env)));
            }

            const Value right = evaluate(binary->right.get(), env);
            if (binary->op == "+") return add(left, right);
            if (binary->op == "-") return sub(left, right);
            if (binary->op == "*") return mul(left, right);
            if (binary->op == "/") return div(left, right);
            if (binary->op == "%") return mod(left, right);
            if (binary->op == "==") return eq(left, right);
            if (binary->op == "!=") return neq(left, right);
            if (binary->op == "<") return lt(left, right);
            if (binary->op == "<=") return lte(left, right);
            if (binary->op == ">") return gt(left, right);
            if (binary->op == ">=") return gte(left, right);
            throw std::runtime_error("Unsupported operator: " + binary->op);
        }

        if (auto unary = dynamic_cast<UnaryExpr*>(expr)) {
            const Value right = evaluate(unary->right.get(), env);
            if (unary->op == "!") return Value(!truthy(right));
            if (unary->op == "-") return Value(-to_number(right));
            throw std::runtime_error("Unsupported unary operator: " + unary->op);
        }

        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            std::vector<Value> args;
            args.reserve(call->args.size());
            for (const auto& arg : call->args) {
                args.push_back(evaluate(arg.get(), env));
            }
            return callFunction(call->callee, args);
        }

        if (auto arrayExpr = dynamic_cast<ArrayExpr*>(expr)) {
            std::vector<Value> values;
            values.reserve(arrayExpr->elements.size());
            for (const auto& element : arrayExpr->elements) {
                values.push_back(evaluate(element.get(), env));
            }
            return array(values);
        }

        if (auto indexExpr = dynamic_cast<IndexExpr*>(expr)) {
            return index(evaluate(indexExpr->target.get(), env), evaluate(indexExpr->index.get(), env));
        }

        if (auto setIdx = dynamic_cast<SetIndexExpr*>(expr)) {
            Value obj = evaluate(setIdx->object.get(), env);
            Value key = evaluate(setIdx->index.get(), env);
            Value val = evaluate(setIdx->value.get(), env);
            if (is_object(obj)) {
                (*obj.as_object())[to_string(key)] = val;
            } else if (is_array(obj)) {
                const int i = static_cast<int>(to_number(key));
                auto arr = obj.as_array();
                if (i >= 0 && i < static_cast<int>(arr->size())) {
                    (*arr)[static_cast<size_t>(i)] = val;
                }
            } else {
                throw std::runtime_error("Cannot set index on a non-object/array value");
            }
            return val;
        }

        if (auto nc = dynamic_cast<NullCoalesceExpr*>(expr)) {
            Value left = evaluate(nc->left.get(), env);
            if (!is_null(left)) return left;
            return evaluate(nc->right.get(), env);
        }

        if (auto oc = dynamic_cast<OptionalChainExpr*>(expr)) {
            Value obj = evaluate(oc->object.get(), env);
            if (is_null(obj)) return Value();
            if (!oc->field.empty()) {
                if (is_object(obj)) {
                    auto it = obj.as_object()->find(oc->field);
                    if (it != obj.as_object()->end()) return it->second;
                    return Value();
                }
                return Value();
            }
            if (oc->index) {
                Value idx = evaluate(oc->index.get(), env);
                if (is_object(obj)) {
                    auto it = obj.as_object()->find(to_string(idx));
                    if (it != obj.as_object()->end()) return it->second;
                } else if (is_array(obj)) {
                    const int i = static_cast<int>(to_number(idx));
                    auto& arr = *obj.as_array();
                    if (i >= 0 && i < static_cast<int>(arr.size())) return arr[i];
                }
                return Value();
            }
            return Value();
        }

        if (auto assignExpr = dynamic_cast<AssignExpr*>(expr)) {
            Value value = evaluate(assignExpr->value.get(), env);
            env->assign(assignExpr->name, value);
            return value;
        }

        if (auto compAssign = dynamic_cast<CompoundAssignExpr*>(expr)) {
            Value cur = env->get(compAssign->name);
            Value rhs = evaluate(compAssign->value.get(), env);
            Value result;
            const std::string& op = compAssign->op;
            if (op == "+") result = add(cur, rhs);
            else if (op == "-") result = Value(to_number(cur) - to_number(rhs));
            else if (op == "*") result = Value(to_number(cur) * to_number(rhs));
            else if (op == "/") result = Value(to_number(rhs) != 0.0 ? to_number(cur) / to_number(rhs) : 0.0);
            else if (op == "%") {
                auto a = static_cast<long long>(to_number(cur));
                auto b = static_cast<long long>(to_number(rhs));
                result = Value(b != 0 ? static_cast<double>(a % b) : 0.0);
            } else result = add(cur, rhs);
            env->assign(compAssign->name, result);
            return result;
        }

        if (auto postfix = dynamic_cast<PostfixExpr*>(expr)) {
            Value cur = env->get(postfix->name);
            Value prev = cur;
            double n = to_number(cur);
            Value next = Value(postfix->op == "++" ? n + 1.0 : n - 1.0);
            env->assign(postfix->name, next);
            return prev;
        }

        if (auto awaitExpr = dynamic_cast<AwaitExpr*>(expr)) {
            Value operand = evaluate(awaitExpr->operand.get(), env);
            return awaitPromise(operand);
        }

        if (auto objExpr = dynamic_cast<ObjectExpr*>(expr)) {
            Value obj = Value::object();
            auto& map = *obj.as_object();
            for (const auto& field : objExpr->fields) {
                map[field.first] = evaluate(field.second.get(), env);
            }
            return obj;
        }

        if (auto lambdaExpr = dynamic_cast<LambdaExpr*>(expr)) {
            auto lv = std::make_shared<LambdaValue>();
            for (const auto& p : lambdaExpr->params) {
                lv->paramNames.push_back(p.name);
                lv->paramDefaults.push_back(p.defaultValue ? p.defaultValue.get() : nullptr);
                if (p.isVariadic) lv->hasVariadic = true;
            }
            lv->body    = lambdaExpr->body.get();
            lv->closure = std::static_pointer_cast<void>(env);
            lv->isAsync = lambdaExpr->isAsync;
            return Value(Value::Fn(lv));
        }

        if (auto dynCall = dynamic_cast<DynCallExpr*>(expr)) {
            Value callee = evaluate(dynCall->callee.get(), env);
            std::vector<Value> args;
            args.reserve(dynCall->args.size());
            for (const auto& arg : dynCall->args) args.push_back(evaluate(arg.get(), env));
            return callLambda(callee, args, env);
        }

        if (auto tmpl = dynamic_cast<TemplateLiteralExpr*>(expr)) {
            std::string result;
            for (size_t i = 0; i < tmpl->parts.size(); ++i) {
                result += tmpl->parts[i];
                if (i < tmpl->exprs.size()) {
                    result += to_string(evaluate(tmpl->exprs[i].get(), env));
                }
            }
            return Value(result);
        }

        if (auto structInst = dynamic_cast<StructInstExpr*>(expr)) {

            Value proto;
            try { proto = env->get(structInst->typeName); } catch (...) {}

            Value obj = Value::object();
            if (is_object(proto)) {
                for (const auto& [k, v] : *proto.as_object()) {
                    if (k == "__struct__") continue;
                    (*obj.as_object())[k] = v;
                }
            }

            for (const auto& field : structInst->fields) {
                (*obj.as_object())[field.first] = evaluate(field.second.get(), env);
            }

            (*obj.as_object())["__type__"] = Value(structInst->typeName);
            return obj;
        }

        if (auto awaitExpr = dynamic_cast<AwaitExpr*>(expr)) {
            Value operand = evaluate(awaitExpr->operand.get(), env);
            return awaitPromise(operand);
        }

        throw std::runtime_error("Unsupported expression encountered in interpreter.");
    }

    void execute(Stmt* stmt, const std::shared_ptr<Environment>& env) {
        if (!stmt) return;

        if (debugHook_ && stmt->line > 0) {
            debugHook_(currentSourcePath_, stmt->line);
        }

        if (auto varStmt = dynamic_cast<VarStmt*>(stmt)) {
            env->define(varStmt->name, evaluate(varStmt->initializer.get(), env), varStmt->isLet);
            return;
        }

        if (dynamic_cast<FunctionStmt*>(stmt) != nullptr) {
            return;
        }

        if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
            (void)evaluate(exprStmt->expr.get(), env);
            return;
        }

        if (auto logStmt = dynamic_cast<LogStmt*>(stmt)) {
            builtin_print({evaluate(logStmt->message.get(), env)});
            return;
        }

        if (auto tryCatch = dynamic_cast<TryCatchStmt*>(stmt)) {
            try {
                execute(tryCatch->tryBody.get(), env);
            } catch (const ReturnSignal&) {
                if (tryCatch->finallyBody) execute(tryCatch->finallyBody.get(), env);
                throw;
            } catch (const BreakSignal&) {
                if (tryCatch->finallyBody) execute(tryCatch->finallyBody.get(), env);
                throw;
            } catch (const ContinueSignal&) {
                if (tryCatch->finallyBody) execute(tryCatch->finallyBody.get(), env);
                throw;
            } catch (const std::exception& e) {
                if (tryCatch->catchBody) {
                    auto catchEnv = std::make_shared<Environment>(env);
                    catchEnv->define(tryCatch->errorParam, Value(std::string(e.what())), false);
                    execute(tryCatch->catchBody.get(), catchEnv);
                }
            } catch (...) {
                if (tryCatch->catchBody) {
                    auto catchEnv = std::make_shared<Environment>(env);
                    catchEnv->define(tryCatch->errorParam, Value(std::string("Unknown error")), false);
                    execute(tryCatch->catchBody.get(), catchEnv);
                }
            }
            if (tryCatch->finallyBody) execute(tryCatch->finallyBody.get(), env);
            return;
        }

        if (auto blockStmt = dynamic_cast<BlockStmt*>(stmt)) {
            auto scope = std::make_shared<Environment>(env);
            for (const auto& child : blockStmt->statements) {
                execute(child.get(), scope);
            }
            return;
        }

        if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            if (truthy(evaluate(ifStmt->condition.get(), env))) execute(ifStmt->thenBranch.get(), env);
            else execute(ifStmt->elseBranch.get(), env);
            return;
        }

        if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            while (truthy(evaluate(whileStmt->condition.get(), env))) {
                try {
                    execute(whileStmt->body.get(), env);
                } catch (const ContinueSignal&) {
                    continue;
                } catch (const BreakSignal&) {
                    break;
                }
            }
            return;
        }

        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            if (returnStmt->values.empty()) {
                throw ReturnSignal{Value()};
            }
            if (returnStmt->values.size() == 1) {
                throw ReturnSignal{evaluate(returnStmt->values[0].get(), env)};
            }
            Value arr = Value::array();
            for (const auto& v : returnStmt->values)
                arr.as_array()->push_back(evaluate(v.get(), env));
            throw ReturnSignal{arr};
        }

        if (dynamic_cast<BreakStmt*>(stmt) != nullptr) {
            throw BreakSignal{};
        }

        if (dynamic_cast<ContinueStmt*>(stmt) != nullptr) {
            throw ContinueSignal{};
        }

        if (auto forIn = dynamic_cast<ForInStmt*>(stmt)) {
            Value iterable = evaluate(forIn->iterable.get(), env);
            if (is_array(iterable)) {
                auto& arr = *iterable.as_array();
                for (size_t i = 0; i < arr.size(); ++i) {
                    auto scope = std::make_shared<Environment>(env);
                    scope->define(forIn->keyVar, arr[i], false);
                    if (!forIn->valVar.empty()) scope->define(forIn->valVar, Value(static_cast<double>(i)), false);
                    try {
                        execute(forIn->body.get(), scope);
                    } catch (const ContinueSignal&) { continue; }
                      catch (const BreakSignal&) { break; }
                }
            } else if (is_object(iterable)) {
                auto& obj = *iterable.as_object();
                for (auto& kv : obj) {
                    auto scope = std::make_shared<Environment>(env);
                    scope->define(forIn->keyVar, Value(kv.first), false);
                    if (!forIn->valVar.empty()) scope->define(forIn->valVar, kv.second, false);
                    try {
                        execute(forIn->body.get(), scope);
                    } catch (const ContinueSignal&) { continue; }
                      catch (const BreakSignal&) { break; }
                }
            }
            return;
        }

        if (auto switchStmt = dynamic_cast<SwitchStmt*>(stmt)) {
            Value subject = evaluate(switchStmt->subject.get(), env);
            bool matched = false;
            for (auto& clause : switchStmt->cases) {
                if (!matched) {
                    if (!clause.value) { matched = true; }
                    else {
                        Value caseVal = evaluate(clause.value.get(), env);
                        matched = (to_string(subject) == to_string(caseVal));
                    }
                }
                if (matched) {
                    try {
                        for (auto& s : clause.body) execute(s.get(), env);
                    } catch (const BreakSignal&) { break; }
                }
            }
            return;
        }

        if (auto throwStmt = dynamic_cast<ThrowStmt*>(stmt)) {
            Value v = evaluate(throwStmt->value.get(), env);
            throw std::runtime_error(to_string(v));
        }

        if (auto structDef = dynamic_cast<StructStmt*>(stmt)) {

            Value proto = Value::object();
            for (const auto& field : structDef->fields) {
                (*proto.as_object())[field.first] = evaluate(field.second.get(), env);
            }

            (*proto.as_object())["__struct__"] = Value(structDef->name);

            if (!structDef->impls.empty()) {
                Value implsArr = Value::array();
                for (const auto& iname : structDef->impls) {
                    implsArr.as_array()->push_back(Value(iname));
                }
                (*proto.as_object())["__impls__"] = implsArr;

                for (const auto& iname : structDef->impls) {
                    auto it = interfaceRegistry_.find(iname);
                    if (it != interfaceRegistry_.end()) {
                        for (const auto& methodName : it->second) {
                            bool found = proto.as_object()->count(methodName) > 0;
                            if (!found) {
                                throw std::runtime_error(
                                    "struct '" + structDef->name + "' implements '" + iname +
                                    "' but is missing method '" + methodName + "'");
                            }
                        }
                    }
                }
            }
            env->define(structDef->name, proto, true);
            return;
        }

        if (auto enumDef = dynamic_cast<EnumStmt*>(stmt)) {
            Value enumObj = Value::object();
            for (const auto& variant : enumDef->variants) {
                Value varVal(static_cast<double>(variant.value));
                (*enumObj.as_object())[variant.name] = varVal;

                env->define(enumDef->name + "." + variant.name, varVal, true);
            }
            (*enumObj.as_object())["__enum__"] = Value(enumDef->name);
            env->define(enumDef->name, enumObj, true);
            return;
        }

        if (auto ifaceDef = dynamic_cast<InterfaceStmt*>(stmt)) {

            std::vector<std::string> required;
            for (const auto& m : ifaceDef->methods) {
                required.push_back(m.name);
            }
            interfaceRegistry_[ifaceDef->name] = required;

            Value ifaceObj = Value::object();
            (*ifaceObj.as_object())["__interface__"] = Value(ifaceDef->name);
            Value methods = Value::array();
            for (const auto& m : ifaceDef->methods) {
                methods.as_array()->push_back(Value(m.name));
            }
            (*ifaceObj.as_object())["methods"] = methods;
            env->define(ifaceDef->name, ifaceObj, true);
            return;
        }

        if (auto ad = dynamic_cast<ArrayDestructureStmt*>(stmt)) {
            Value src = evaluate(ad->init.get(), env);
            if (!is_array(src)) throw std::runtime_error("Array destructure requires an array");
            auto& arr = *src.as_array();
            for (size_t i = 0; i < ad->names.size(); ++i) {
                if (ad->names[i].empty()) continue;
                Value v = (i < arr.size()) ? arr[i] : Value();
                env->define(ad->names[i], v, false);
            }
            return;
        }

        if (auto od = dynamic_cast<ObjectDestructureStmt*>(stmt)) {
            Value src = evaluate(od->init.get(), env);
            if (!is_object(src)) throw std::runtime_error("Object destructure requires an object");
            auto& map = *src.as_object();
            for (const auto& [key, local] : od->bindings) {
                Value v;
                auto it = map.find(key);
                if (it != map.end()) v = it->second;
                env->define(local, v, false);
            }
            return;
        }

        throw std::runtime_error("Unsupported statement encountered in interpreter.");
    }

    Value callLambda(const Value& callee, const std::vector<Value>& args,
                     const std::shared_ptr<Environment>& callSiteEnv) {
        if (!is_lambda(callee)) {
            throw std::runtime_error("Value is not callable");
        }
        auto lv = callee.as_lambda();
        auto closureEnv = std::static_pointer_cast<Environment>(lv->closure);

        auto bindParams = [&](std::shared_ptr<Environment> frame) {
            size_t ni = lv->paramNames.size();
            for (size_t i = 0; i < ni; ++i) {
                if (lv->hasVariadic && i == ni - 1) {

                    Value rest = Value::array();
                    for (size_t j = i; j < args.size(); ++j)
                        rest.as_array()->push_back(args[j]);
                    frame->define(lv->paramNames[i], rest, false);
                    break;
                }
                Value v;
                if (i < args.size()) {
                    v = args[i];
                } else if (lv->paramDefaults[i] != nullptr) {
                    v = evaluate(lv->paramDefaults[i], closureEnv);
                }
                frame->define(lv->paramNames[i], v, false);
            }
        };

        if (lv->isAsync) {
            const std::string promId = newPromiseId();
            auto capturedArgs = args;
            auto fut = std::async(std::launch::async, [this, lv, capturedArgs, closureEnv]() -> Value {
                auto frame = std::make_shared<Environment>(closureEnv);
                size_t ni = lv->paramNames.size();
                for (size_t i = 0; i < ni; ++i) {
                    if (lv->hasVariadic && i == ni - 1) {
                        Value rest = Value::array();
                        for (size_t j = i; j < capturedArgs.size(); ++j)
                            rest.as_array()->push_back(capturedArgs[j]);
                        frame->define(lv->paramNames[i], rest, false);
                        break;
                    }
                    Value v;
                    if (i < capturedArgs.size()) v = capturedArgs[i];
                    else if (lv->paramDefaults[i]) v = evaluate(lv->paramDefaults[i], closureEnv);
                    frame->define(lv->paramNames[i], v, false);
                }
                try {
                    for (const auto& s : lv->body->statements) execute(s.get(), frame);
                } catch (const ReturnSignal& sig) { return sig.value; }
                return Value();
            });
            storePromise(promId, fut.share());
            Value promObj = Value::object();
            (*promObj.as_object())["__promise_id__"] = Value(promId);
            return promObj;
        }

        auto frame = std::make_shared<Environment>(closureEnv);
        bindParams(frame);
        try {
            for (const auto& s : lv->body->statements) execute(s.get(), frame);
        } catch (const ReturnSignal& sig) { return sig.value; }
        return Value();
    }

    std::optional<Value> callFunctionIfExists(const std::string& name, const std::vector<Value>& args) {
        if (functions_.count(name)) return callFunction(name, args);

        try {
            Value v = globals_->get(name);
            if (is_lambda(v)) return callLambda(v, args, globals_);
        } catch (...) {}
        return std::nullopt;
    }

    bool hasFunction(const std::string& name) {
        if (functions_.count(name)) return true;
        try { Value v = globals_->get(name); return is_lambda(v); } catch (...) {}
        return false;
    }

    Value getGlobals() const {
        Value obj = Value::object();
        for (const auto& [k, slot] : globals_->vars()) {
            if (k.size() > 2 && k.front() == '_' && k.back() == '_') continue;
            (*obj.as_object())[k] = slot.value;
        }
        return obj;
    }

    void setGlobal(const std::string& name, Value val) {
        try { globals_->assign(name, val); } catch (...) { globals_->define(name, val, false); }
    }

    Value callFunction(const std::string& callee, const std::vector<Value>& args) {
        const std::string builtin = canonicalizeBuiltinName(callee);
        if (const auto nativeResult = tryNativeModuleCall(builtin, args)) {
            return *nativeResult;
        }
        if (const auto builtinResult = callBuiltin(builtin, args)) {
            return *builtinResult;
        }

        const auto found = functions_.find(callee);
        if (found == functions_.end()) {

            try {
                Value lval = globals_->get(callee);
                if (is_lambda(lval)) return callLambda(lval, args, globals_);
            } catch (...) {}
            throw std::runtime_error("Unknown function: " + callee);
        }

        FunctionStmt* function = found->second;

        auto bindFuncParams = [&](std::shared_ptr<Environment> frame, const std::vector<Value>& callArgs) {
            for (size_t i = 0; i < function->params.size(); ++i) {
                const auto& p = function->params[i];
                if (p.isVariadic) {
                    Value rest = Value::array();
                    for (size_t j = i; j < callArgs.size(); ++j)
                        rest.as_array()->push_back(callArgs[j]);
                    frame->define(p.name, rest, false);
                    break;
                }
                Value v;
                if (i < callArgs.size()) v = callArgs[i];
                else if (p.defaultValue) v = evaluate(p.defaultValue.get(), globals_);
                frame->define(p.name, v, false);
            }
        };

        if (function->isAsync) {
            const std::string promId = newPromiseId();
            auto capturedArgs = args;
            auto fut = std::async(std::launch::async, [this, function, capturedArgs, &bindFuncParams]() -> Value {
                auto frame = std::make_shared<Environment>(globals_);

                for (size_t i = 0; i < function->params.size(); ++i) {
                    const auto& p = function->params[i];
                    if (p.isVariadic) {
                        Value rest = Value::array();
                        for (size_t j = i; j < capturedArgs.size(); ++j)
                            rest.as_array()->push_back(capturedArgs[j]);
                        frame->define(p.name, rest, false);
                        break;
                    }
                    Value v;
                    if (i < capturedArgs.size()) v = capturedArgs[i];
                    else if (p.defaultValue) v = evaluate(p.defaultValue.get(), globals_);
                    frame->define(p.name, v, false);
                }
                try {
                    for (const auto& stmt : function->body->statements) execute(stmt.get(), frame);
                } catch (const ReturnSignal& signal) { return signal.value; }
                return Value();
            });
            storePromise(promId, fut.share());
            Value promObj = Value::object();
            (*promObj.as_object())["__promise_id__"] = Value(promId);
            return promObj;
        }

        auto frame = std::make_shared<Environment>(globals_);
        bindFuncParams(frame, args);

        try {
            for (const auto& statement : function->body->statements) {
                execute(statement.get(), frame);
            }
        } catch (const ReturnSignal& signal) {
            return signal.value;
        }

        return Value();
    }

    std::optional<Value> callBuiltin(const std::string& builtin, const std::vector<Value>& args) {
        if (builtin == "print" || builtin == "log.info") return builtin_print(args);
        if (builtin == "len" || builtin == "length") return builtin_len(args);
        if (builtin == "str" || builtin == "to_string") return builtin_str(args);
        if (builtin == "num") return builtin_num(args);
        if (builtin == "bool") return builtin_bool(args);
        if (builtin == "type") return builtin_type(args);

        if (builtin == "is_impl") {
            if (args.size() < 2) return Value(false);
            const Value& obj = args[0];
            if (!is_object(obj) && !is_null(args[1])) return Value(false);
            std::string ifaceName = to_string(args[1]);

            if (is_object(obj)) {
                auto imp = obj.as_object()->find("__impls__");
                if (imp != obj.as_object()->end() && is_array(imp->second)) {
                    for (const auto& v : *imp->second.as_array()) {
                        if (to_string(v) == ifaceName) return Value(true);
                    }
                }
            }

            auto it = interfaceRegistry_.find(ifaceName);
            if (it == interfaceRegistry_.end()) return Value(false);
            if (!is_object(obj)) return Value(false);
            for (const auto& method : it->second) {
                if (obj.as_object()->find(method) == obj.as_object()->end())
                    return Value(false);
            }
            return Value(true);
        }

        if (builtin == "is_enum") {
            if (args.empty()) return Value(false);
            if (!is_object(args[0])) return Value(false);
            return Value(args[0].as_object()->count("__enum__") > 0);
        }

        if (builtin == "promise.all") {
            if (args.empty() || !is_array(args[0])) return Value::array();
            Value results = Value::array();
            for (const auto& p : *args[0].as_array()) {
                results.as_array()->push_back(awaitPromise(p));
            }
            return results;
        }

        if (builtin == "promise.race") {
            if (args.empty() || !is_array(args[0])) return Value();
            for (const auto& p : *args[0].as_array()) {
                Value result = awaitPromise(p);
                if (!is_null(result)) return result;
            }
            return Value();
        }

        if (builtin == "promise.resolve") {
            const std::string promId = newPromiseId();
            Value val = args.empty() ? Value() : args[0];
            auto fut = std::async(std::launch::deferred, [val]() -> Value { return val; });
            storePromise(promId, fut.share());
            Value promObj = Value::object();
            (*promObj.as_object())["__promise_id__"] = Value(promId);
            return promObj;
        }

        if (builtin == "await") {
            if (args.empty()) return Value();
            return awaitPromise(args[0]);
        }

        if (builtin == "range") return builtin_range(args);
        if (builtin == "push") return builtin_push(args);
        if (builtin == "pop") return builtin_pop(args);
        if (builtin == "join") return builtin_join(args);
        if (builtin == "split") return builtin_split(args);
        if (builtin == "upper") return builtin_upper(args);
        if (builtin == "lower") return builtin_lower(args);
        if (builtin == "contains") return builtin_contains(args);
        if (builtin == "trim") return builtin_trim(args);
        if (builtin == "replace") return builtin_replace(args);
        if (builtin == "slice") return builtin_slice(args);
        if (builtin == "floor") return builtin_floor(args);
        if (builtin == "ceil") return builtin_ceil(args);
        if (builtin == "round") return builtin_round(args);
        if (builtin == "min") return builtin_min(args);
        if (builtin == "max") return builtin_max(args);
        if (builtin == "clamp") return builtin_clamp(args);
        if (builtin == "sqrt") return builtin_sqrt(args);
        if (builtin == "abs") return builtin_abs(args);
        if (builtin == "pow") return builtin_pow(args);
        if (builtin == "sin") return builtin_sin(args);
        if (builtin == "cos") return builtin_cos(args);
        if (builtin == "tan") return builtin_tan(args);
        if (builtin == "log") return builtin_log(args);
        if (builtin == "sleep") return builtin_sleep(args);
        if (builtin == "clock.ms") return builtin_clock_ms(args);
        if (builtin == "time.now_ms") return builtin_clock_ms(args);
        if (builtin == "time.sleep") return builtin_sleep(args);
        if (builtin == "time.unix_ms") return builtin_time_unix_ms(args);
        if (builtin == "json.stringify") return builtin_json_stringify(args);
        if (builtin == "json.parse") return builtin_json_parse(args);
        if (builtin == "random") return builtin_random(args);
        if (builtin == "random.int") return builtin_random_int(args);
        if (builtin == "fs.exists") return builtin_fs_exists(args);
        if (builtin == "fs.mkdir") return builtin_fs_mkdir(args);
        if (builtin == "fs.copy") return builtin_fs_copy(args);
        if (builtin == "fs.copy_tree") return builtin_fs_copy_tree(args);
        if (builtin == "fs.remove") return builtin_fs_remove(args);
        if (builtin == "fs.read") return builtin_fs_read(args);
        if (builtin == "fs.write") return builtin_fs_write(args);
        if (builtin == "process.run") return builtin_process_run(args);
        if (builtin == "process.exe_dir") return builtin_process_exe_dir(args);
        if (builtin == "env.get") return builtin_env_get(args);
        if (builtin == "env.set") return builtin_env_set(args);
        if (builtin == "env.path_add") return builtin_env_path_add(args);
        if (builtin == "os.name") return builtin_os_name(args);
        if (builtin == "os.arch") return builtin_os_arch(args);
        if (builtin == "os.cwd") return builtin_os_cwd(args);
        if (builtin == "os.chdir") return builtin_os_chdir(args);
        if (builtin == "os.home") return builtin_os_home(args);
        if (builtin == "os.temp") return builtin_os_temp(args);
        if (builtin == "os.sep") return builtin_os_sep(args);
        if (builtin == "os.exists") return builtin_os_exists(args);
        if (builtin == "os.is_dir") return builtin_os_is_dir(args);
        if (builtin == "os.is_file") return builtin_os_is_file(args);
        if (builtin == "net.tcp_connect") return builtin_net_tcp_connect(args);
        if (builtin == "net.tcp_send") return builtin_net_tcp_send(args);
        if (builtin == "net.tcp_recv") return builtin_net_tcp_recv(args);
        if (builtin == "net.tcp_close") return builtin_net_tcp_close(args);
        if (builtin == "http.get") return builtin_http_get(args);
        if (builtin == "http.post") return builtin_http_post(args);
        if (builtin == "db.connect") return builtin_db_connect(args);
        if (builtin == "db.query") return builtin_db_query(args);
        if (builtin == "db.exec") return builtin_db_exec(args);
        if (builtin == "db.scalar") return builtin_db_scalar(args);
        if (builtin == "db.close") return builtin_db_close(args);
        if (builtin == "app.init") return builtin_app_init(args);
        if (builtin == "app.run") return builtin_app_run(args);
        if (builtin == "app.button") return builtin_app_button(args);
        if (builtin == "app.text") return builtin_app_text(args);
        if (builtin == "app.msg") return builtin_app_msg(args);
        if (builtin == "ui.init") return builtin_ui_init(args);
        if (builtin == "ui.style") return builtin_ui_style(args);
        if (builtin == "ui.hero") return builtin_ui_hero(args);
        if (builtin == "ui.status") return builtin_ui_status(args);
        if (builtin == "ui.info") return builtin_ui_info(args);
        if (builtin == "ui.text") return builtin_ui_text(args);
        if (builtin == "ui.action" || builtin == "ui.button") return builtin_ui_action(args);
        if (builtin == "ui.run") return builtin_ui_run(args);
        if (builtin == "web.page") return builtin_web_page(args);
        if (builtin == "web.begin") return builtin_web_begin(args);
        if (builtin == "web.route") return builtin_web_route(args);
        if (builtin == "web.head") return builtin_web_head(args);
        if (builtin == "web.public") return builtin_web_public(args);
        if (builtin == "web.live" || builtin == "web.serve") return builtin_web_live(args);
        if (builtin == "web.style") return builtin_web_style(args);
        if (builtin == "web.open") return builtin_web_open(args);
        if (builtin == "web.close") return builtin_web_close(args);
        if (builtin == "web.text") return builtin_web_text(args);
        if (builtin == "web.h1") return builtin_web_h1(args);
        if (builtin == "web.h2") return builtin_web_h2(args);
        if (builtin == "web.p") return builtin_web_p(args);
        if (builtin == "web.button") return builtin_web_button(args);
        if (builtin == "web.raw" || builtin == "web.html") return builtin_web_raw(args);
        if (builtin == "web.end") return builtin_web_end(args);
        if (builtin == "engine.init") return builtin_engine_init(args);
        if (builtin == "engine.shutdown") return builtin_engine_shutdown(args);
        if (builtin == "engine.should_close") return builtin_engine_should_close(args);
        if (builtin == "engine.begin") return builtin_engine_begin(args);
        if (builtin == "engine.end") return builtin_engine_end(args);
        if (builtin == "engine.clear") return builtin_engine_clear(args);
        if (builtin == "engine.set_camera") return builtin_engine_set_camera(args);
        if (builtin == "engine.target_fps") return builtin_engine_target_fps(args);
        if (builtin == "engine.frame_time") return builtin_engine_frame_time(args);
        if (builtin == "engine.backend") return builtin_engine_backend(args);
        if (builtin == "engine.backend_info") return builtin_engine_backend_info(args);
        if (builtin == "engine.backend_name") return builtin_engine_backend_name(args);
        if (builtin == "engine.vsync") return builtin_engine_vsync(args);
        if (builtin == "engine.msaa") return builtin_engine_msaa(args);
        if (builtin == "engine.exposure") return builtin_engine_exposure(args);
        if (builtin == "engine.vignette") return builtin_engine_vignette(args);
        if (builtin == "engine.film_grain") return builtin_engine_film_grain(args);
        if (builtin == "engine.saturation") return builtin_engine_saturation(args);
        if (builtin == "engine.contrast") return builtin_engine_contrast(args);
        if (builtin == "engine.bloom") return builtin_engine_bloom(args);
        if (builtin == "engine.fog") return builtin_engine_fog(args);
        if (builtin == "engine.volumetric") return builtin_engine_volumetric(args);
        if (builtin == "engine.postfx_info") return builtin_engine_postfx_info(args);
        if (builtin == "engine.scene_watch") return builtin_engine_scene_watch(args);
        if (builtin == "engine.scene_reload") return builtin_engine_scene_reload(args);
        if (builtin == "engine.stats") return builtin_engine_stats(args);
        if (builtin == "engine.render_stats") return builtin_engine_stats(args);
        if (builtin == "engine.camera_fov") return builtin_engine_camera_fov(args);
        if (builtin == "engine.camera_orbit") return builtin_engine_camera_orbit(args);
        if (builtin == "engine.key_down") return builtin_engine_key_down(args);
        if (builtin == "engine.key_pressed") return builtin_engine_key_pressed(args);
        if (builtin == "engine.mouse_down") return builtin_engine_mouse_down(args);
        if (builtin == "engine.mouse_pos") return builtin_engine_mouse_pos(args);
        if (builtin == "engine.window") return builtin_engine_window(args);
        if (builtin == "engine.camera") return builtin_engine_camera(args);
        if (builtin == "engine.frame_begin") return builtin_engine_frame_begin(args);
        if (builtin == "engine.frame_end") return builtin_engine_frame_end(args);
        if (builtin == "engine.scene") return builtin_engine_scene(args);
        if (builtin == "engine.scene_clear") return builtin_engine_scene_clear(args);
        if (builtin == "engine.scene_stats") return builtin_engine_scene_stats(args);
        if (builtin == "engine.export_scene") return builtin_engine_export_scene(args);
        if (builtin == "engine.scene_save") return builtin_engine_scene_save(args);
        if (builtin == "engine.scene_load") return builtin_engine_scene_load(args);
        if (builtin == "engine.entity") return builtin_engine_entity(args);
        if (builtin == "engine.entity_exists") return builtin_engine_entity_exists(args);
        if (builtin == "engine.entity_remove") return builtin_engine_entity_remove(args);
        if (builtin == "engine.entity_set_position") return builtin_engine_entity_set_position(args);
        if (builtin == "engine.entity_get_position") return builtin_engine_entity_get_position(args);
        if (builtin == "engine.entity_set_size") return builtin_engine_entity_set_size(args);
        if (builtin == "engine.entity_set_scale") return builtin_engine_entity_set_scale(args);
        if (builtin == "engine.entity_set_radius") return builtin_engine_entity_set_radius(args);
        if (builtin == "engine.entity_set_color") return builtin_engine_entity_set_color(args);
        if (builtin == "engine.entity_set_visible") return builtin_engine_entity_set_visible(args);
        if (builtin == "engine.entity_mesh") return builtin_engine_entity_mesh(args);
        if (builtin == "engine.entity_texture") return builtin_engine_entity_texture(args);
        if (builtin == "engine.mesh") return builtin_engine_mesh(args);
        if (builtin == "engine.mesh_exists") return builtin_engine_mesh_exists(args);
        if (builtin == "engine.texture") return builtin_engine_texture(args);
        if (builtin == "engine.texture_exists") return builtin_engine_texture_exists(args);
        if (builtin == "engine.material") return builtin_engine_material(args);
        if (builtin == "engine.material_exists") return builtin_engine_material_exists(args);
        if (builtin == "engine.material_texture") return builtin_engine_material_texture(args);
        if (builtin == "engine.entity_material") return builtin_engine_entity_material(args);
        if (builtin == "engine.scene_draw") return builtin_engine_scene_draw(args);
        if (builtin == "engine.light_ambient") return builtin_engine_light_ambient(args);
        if (builtin == "engine.light_directional") return builtin_engine_light_directional(args);
        if (builtin == "engine.assets_root") return builtin_engine_assets_root(args);
        if (builtin == "engine.asset_path") return builtin_engine_asset_path(args);
        if (builtin == "engine.asset_exists") return builtin_engine_asset_exists(args);
        if (builtin == "engine.draw_grid") return builtin_engine_draw_grid(args);
        if (builtin == "engine.draw_cube") return builtin_engine_draw_cube(args);
        if (builtin == "engine.draw_plane") return builtin_engine_draw_plane(args);
        if (builtin == "engine.draw_sphere") return builtin_engine_draw_sphere(args);
        if (builtin == "engine.draw_text") return builtin_engine_draw_text(args);
        if (builtin == "engine.draw_fps") return builtin_engine_draw_fps(args);
        if (builtin == "engine.draw_rect") return builtin_engine_draw_rect(args);
        if (builtin == "engine.draw_rect_lines") return builtin_engine_draw_rect_lines(args);
        if (builtin == "engine.draw_circle") return builtin_engine_draw_circle(args);
        if (builtin == "engine.draw_circle_lines") return builtin_engine_draw_circle_lines(args);
        if (builtin == "engine.draw_line") return builtin_engine_draw_line(args);
        if (builtin == "engine.draw_pixel") return builtin_engine_draw_pixel(args);
        if (builtin == "engine.screen_width") return Value(static_cast<double>(rt_screen_width()));
        if (builtin == "engine.screen_height") return Value(static_cast<double>(rt_screen_height()));
        if (builtin == "engine.rect_overlap") return builtin_engine_rect_overlap(args);
        if (builtin == "engine.circle_overlap") return builtin_engine_circle_overlap(args);
        if (builtin == "engine.point_in_rect") return builtin_engine_point_in_rect(args);

        if (builtin == "engine.KEY_A") return Value(65.0);
        if (builtin == "engine.KEY_B") return Value(66.0);
        if (builtin == "engine.KEY_C") return Value(67.0);
        if (builtin == "engine.KEY_D") return Value(68.0);
        if (builtin == "engine.KEY_E") return Value(69.0);
        if (builtin == "engine.KEY_F") return Value(70.0);
        if (builtin == "engine.KEY_G") return Value(71.0);
        if (builtin == "engine.KEY_H") return Value(72.0);
        if (builtin == "engine.KEY_I") return Value(73.0);
        if (builtin == "engine.KEY_J") return Value(74.0);
        if (builtin == "engine.KEY_K") return Value(75.0);
        if (builtin == "engine.KEY_L") return Value(76.0);
        if (builtin == "engine.KEY_M") return Value(77.0);
        if (builtin == "engine.KEY_N") return Value(78.0);
        if (builtin == "engine.KEY_O") return Value(79.0);
        if (builtin == "engine.KEY_P") return Value(80.0);
        if (builtin == "engine.KEY_Q") return Value(81.0);
        if (builtin == "engine.KEY_R") return Value(82.0);
        if (builtin == "engine.KEY_S") return Value(83.0);
        if (builtin == "engine.KEY_T") return Value(84.0);
        if (builtin == "engine.KEY_U") return Value(85.0);
        if (builtin == "engine.KEY_V") return Value(86.0);
        if (builtin == "engine.KEY_W") return Value(87.0);
        if (builtin == "engine.KEY_X") return Value(88.0);
        if (builtin == "engine.KEY_Y") return Value(89.0);
        if (builtin == "engine.KEY_Z") return Value(90.0);
        if (builtin == "engine.KEY_0") return Value(48.0);
        if (builtin == "engine.KEY_1") return Value(49.0);
        if (builtin == "engine.KEY_2") return Value(50.0);
        if (builtin == "engine.KEY_3") return Value(51.0);
        if (builtin == "engine.KEY_4") return Value(52.0);
        if (builtin == "engine.KEY_5") return Value(53.0);
        if (builtin == "engine.KEY_6") return Value(54.0);
        if (builtin == "engine.KEY_7") return Value(55.0);
        if (builtin == "engine.KEY_8") return Value(56.0);
        if (builtin == "engine.KEY_9") return Value(57.0);
        if (builtin == "engine.KEY_SPACE") return Value(32.0);
        if (builtin == "engine.KEY_ENTER") return Value(257.0);
        if (builtin == "engine.KEY_ESCAPE") return Value(256.0);
        if (builtin == "engine.KEY_BACKSPACE") return Value(259.0);
        if (builtin == "engine.KEY_TAB") return Value(258.0);
        if (builtin == "engine.KEY_LEFT") return Value(263.0);
        if (builtin == "engine.KEY_RIGHT") return Value(262.0);
        if (builtin == "engine.KEY_UP") return Value(265.0);
        if (builtin == "engine.KEY_DOWN") return Value(264.0);
        if (builtin == "engine.KEY_SHIFT") return Value(340.0);
        if (builtin == "engine.KEY_CTRL") return Value(341.0);
        if (builtin == "engine.KEY_ALT") return Value(342.0);
        if (builtin == "engine.KEY_F1") return Value(290.0);
        if (builtin == "engine.KEY_F2") return Value(291.0);
        if (builtin == "engine.KEY_F3") return Value(292.0);
        if (builtin == "engine.KEY_F4") return Value(293.0);
        if (builtin == "engine.KEY_F5") return Value(294.0);
        if (builtin == "engine.KEY_F6") return Value(295.0);
        if (builtin == "engine.KEY_F7") return Value(296.0);
        if (builtin == "engine.KEY_F8") return Value(297.0);
        if (builtin == "engine.KEY_F9") return Value(298.0);
        if (builtin == "engine.KEY_F10") return Value(299.0);
        if (builtin == "engine.KEY_F11") return Value(300.0);
        if (builtin == "engine.KEY_F12") return Value(301.0);

        if (builtin == "map") {
            if (args.size() >= 2 && is_array(args[0]) && is_lambda(args[1])) {
                Value result = Value::array();
                for (auto& item : *args[0].as_array())
                    result.as_array()->push_back(callLambda(args[1], {item}, globals_));
                return result;
            }
            return Value::array();
        }
        if (builtin == "filter") {
            if (args.size() >= 2 && is_array(args[0]) && is_lambda(args[1])) {
                Value result = Value::array();
                for (auto& item : *args[0].as_array())
                    if (truthy(callLambda(args[1], {item}, globals_))) result.as_array()->push_back(item);
                return result;
            }
            return Value::array();
        }
        if (builtin == "reduce") {
            if (args.size() >= 2 && is_array(args[0]) && is_lambda(args[1])) {
                Value acc = args.size() >= 3 ? args[2] : Value();
                for (auto& item : *args[0].as_array())
                    acc = callLambda(args[1], {acc, item}, globals_);
                return acc;
            }
            return Value();
        }
        if (builtin == "any") {
            if (args.size() >= 2 && is_array(args[0]) && is_lambda(args[1])) {
                for (auto& item : *args[0].as_array())
                    if (truthy(callLambda(args[1], {item}, globals_))) return Value(true);
                return Value(false);
            }
            return Value(false);
        }
        if (builtin == "all") {
            if (args.size() >= 2 && is_array(args[0]) && is_lambda(args[1])) {
                for (auto& item : *args[0].as_array())
                    if (!truthy(callLambda(args[1], {item}, globals_))) return Value(false);
                return Value(true);
            }
            return Value(true);
        }
        if (builtin == "sorted") {
            if (!args.empty() && is_array(args[0])) {
                Value result = Value::array();
                *result.as_array() = *args[0].as_array();
                if (args.size() >= 2 && is_lambda(args[1])) {
                    auto& fn = args[1];
                    std::sort(result.as_array()->begin(), result.as_array()->end(),
                        [&](const Value& a, const Value& b) {
                            return truthy(callLambda(fn, {a, b}, globals_));
                        });
                } else {
                    std::sort(result.as_array()->begin(), result.as_array()->end(),
                        [](const Value& a, const Value& b) { return to_string(a) < to_string(b); });
                }
                return result;
            }
            return Value::array();
        }

        if (builtin == "startswith") {
            if (args.size() >= 2 && is_string(args[0]) && is_string(args[1])) {
                const std::string& s = std::get<std::string>(args[0].data);
                const std::string& p = std::get<std::string>(args[1].data);
                return Value(s.size() >= p.size() && s.substr(0, p.size()) == p);
            }
            return Value(false);
        }
        if (builtin == "endswith") {
            if (args.size() >= 2 && is_string(args[0]) && is_string(args[1])) {
                const std::string& s = std::get<std::string>(args[0].data);
                const std::string& p = std::get<std::string>(args[1].data);
                return Value(s.size() >= p.size() && s.substr(s.size() - p.size()) == p);
            }
            return Value(false);
        }
        if (builtin == "indexof") {
            if (args.size() >= 2 && is_string(args[0]) && is_string(args[1])) {
                const std::string& s = std::get<std::string>(args[0].data);
                const std::string& needle = std::get<std::string>(args[1].data);
                auto pos = s.find(needle);
                return Value(pos == std::string::npos ? -1.0 : static_cast<double>(pos));
            }
            return Value(-1.0);
        }
        if (builtin == "pad_left") {
            if (args.size() >= 2 && is_string(args[0])) {
                std::string s = to_string(args[0]);
                int n = static_cast<int>(to_number(args[1]));
                std::string ch = args.size() >= 3 ? to_string(args[2]) : " ";
                if (ch.empty()) ch = " ";
                while (static_cast<int>(s.size()) < n) s = ch[0] + s;
                return Value(s);
            }
            return args.empty() ? Value() : args[0];
        }
        if (builtin == "pad_right") {
            if (args.size() >= 2 && is_string(args[0])) {
                std::string s = to_string(args[0]);
                int n = static_cast<int>(to_number(args[1]));
                std::string ch = args.size() >= 3 ? to_string(args[2]) : " ";
                if (ch.empty()) ch = " ";
                while (static_cast<int>(s.size()) < n) s += ch[0];
                return Value(s);
            }
            return args.empty() ? Value() : args[0];
        }
        if (builtin == "repeat") {
            if (args.size() >= 2 && is_string(args[0])) {
                std::string s = to_string(args[0]);
                int n = static_cast<int>(to_number(args[1]));
                std::string result;
                for (int i = 0; i < n; ++i) result += s;
                return Value(result);
            }
            return Value(std::string(""));
        }
        if (builtin == "char_code") {
            if (!args.empty() && is_string(args[0])) {
                const std::string& s = std::get<std::string>(args[0].data);
                return Value(s.empty() ? 0.0 : static_cast<double>(static_cast<unsigned char>(s[0])));
            }
            return Value(0.0);
        }
        if (builtin == "char_from" || builtin == "char") {
            if (!args.empty()) return Value(std::string(1, static_cast<char>(static_cast<int>(to_number(args[0])))));
            return Value(std::string(""));
        }
        if (builtin == "format") {

            if (!args.empty() && is_string(args[0])) {
                std::string fmt = to_string(args[0]);
                std::string out;
                size_t argIdx = 1;
                for (size_t i = 0; i < fmt.size(); ++i) {
                    if (fmt[i] == '%' && i + 1 < fmt.size() && argIdx < args.size()) {
                        char spec = fmt[i+1];
                        if (spec == 's') { out += to_string(args[argIdx++]); i++; }
                        else if (spec == 'd' || spec == 'i') { out += std::to_string(static_cast<long long>(to_number(args[argIdx++]))); i++; }
                        else if (spec == 'f') { out += number_to_string(to_number(args[argIdx++])); i++; }
                        else if (spec == '%') { out += '%'; i++; }
                        else out += fmt[i];
                    } else {
                        out += fmt[i];
                    }
                }
                return Value(out);
            }
            return Value(std::string(""));
        }

        if (builtin == "regexp.test") {
            if (args.size() >= 2 && is_string(args[0]) && is_string(args[1])) {
                try {
                    std::regex re(to_string(args[1]));
                    return Value(std::regex_search(to_string(args[0]), re));
                } catch (...) { return Value(false); }
            }
            return Value(false);
        }
        if (builtin == "regexp.match") {
            if (args.size() >= 2 && is_string(args[0]) && is_string(args[1])) {
                try {
                    std::regex re(to_string(args[1]));
                    std::string text = to_string(args[0]);
                    std::smatch m;
                    Value result = Value::array();
                    auto& arr = *result.as_array();
                    auto begin = std::sregex_iterator(text.begin(), text.end(), re);
                    auto end = std::sregex_iterator();
                    for (auto it = begin; it != end; ++it) arr.push_back(Value((*it)[0].str()));
                    return result;
                } catch (...) { return Value::array(); }
            }
            return Value::array();
        }
        if (builtin == "regexp.replace") {
            if (args.size() >= 3 && is_string(args[0]) && is_string(args[1])) {
                try {
                    std::regex re(to_string(args[1]));
                    return Value(std::regex_replace(to_string(args[0]), re, to_string(args[2])));
                } catch (...) { return args.empty() ? Value() : args[0]; }
            }
            return args.empty() ? Value() : args[0];
        }

        if (builtin == "base64.encode") {
            if (!args.empty() && is_string(args[0])) {
                const std::string& in = std::get<std::string>(args[0].data);
                static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                std::string out;
                int val = 0, valb = -6;
                for (unsigned char c : in) {
                    val = (val << 8) + c; valb += 8;
                    while (valb >= 0) { out.push_back(b64[(val >> valb) & 0x3F]); valb -= 6; }
                }
                if (valb > -6) out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
                while (out.size() % 4) out.push_back('=');
                return Value(out);
            }
            return Value(std::string(""));
        }
        if (builtin == "base64.decode") {
            if (!args.empty() && is_string(args[0])) {
                const std::string& in = std::get<std::string>(args[0].data);
                std::vector<int> T(256, -1);
                for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
                std::string out;
                int val = 0, valb = -8;
                for (unsigned char c : in) {
                    if (T[c] == -1) break;
                    val = (val << 6) + T[c]; valb += 6;
                    if (valb >= 0) { out.push_back(char((val >> valb) & 0xFF)); valb -= 8; }
                }
                return Value(out);
            }
            return Value(std::string(""));
        }

        if (builtin == "url.encode") {
            if (!args.empty() && is_string(args[0])) {
                const std::string& s = std::get<std::string>(args[0].data);
                std::ostringstream enc;
                for (unsigned char c : s) {
                    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') enc << c;
                    else enc << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;
                }
                return Value(enc.str());
            }
            return Value(std::string(""));
        }
        if (builtin == "url.decode") {
            if (!args.empty() && is_string(args[0])) {
                const std::string& s = std::get<std::string>(args[0].data);
                std::string out;
                for (size_t i = 0; i < s.size(); ++i) {
                    if (s[i] == '%' && i + 2 < s.size()) {
                        int val = std::stoi(s.substr(i+1, 2), nullptr, 16);
                        out += static_cast<char>(val); i += 2;
                    } else if (s[i] == '+') out += ' ';
                    else out += s[i];
                }
                return Value(out);
            }
            return Value(std::string(""));
        }

        if (builtin == "uuid") {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<int> dis(0, 15);
            static std::uniform_int_distribution<int> dis2(8, 11);
            std::ostringstream ss;
            ss << std::hex;
            for (int i = 0; i < 8; i++) ss << dis(gen);
            ss << "-"; for (int i = 0; i < 4; i++) ss << dis(gen);
            ss << "-4";  for (int i = 0; i < 3; i++) ss << dis(gen);
            ss << "-"; ss << dis2(gen); for (int i = 0; i < 3; i++) ss << dis(gen);
            ss << "-"; for (int i = 0; i < 12; i++) ss << dis(gen);
            return Value(ss.str());
        }

        if (builtin == "atan2") return Value(std::atan2(to_number(args[0]), to_number(args[1])));
        if (builtin == "hypot") return Value(std::hypot(to_number(args[0]), to_number(args[1])));
        if (builtin == "atan")  return Value(std::atan(to_number(args[0])));
        if (builtin == "asin")  return Value(std::asin(to_number(args[0])));
        if (builtin == "acos")  return Value(std::acos(to_number(args[0])));
        if (builtin == "exp")   return Value(std::exp(to_number(args[0])));
        if (builtin == "log2")  return Value(std::log2(to_number(args[0])));
        if (builtin == "log10") return Value(std::log10(to_number(args[0])));

        if (builtin == "fs.list") {
            if (!args.empty() && is_string(args[0])) {
                Value result = Value::array();
                try {
                    for (const auto& entry : std::filesystem::directory_iterator(to_string(args[0])))
                        result.as_array()->push_back(Value(entry.path().string()));
                } catch (...) {}
                return result;
            }
            return Value::array();
        }
        if (builtin == "fs.rename") {
            if (args.size() >= 2) {
                try { std::filesystem::rename(to_string(args[0]), to_string(args[1])); return Value(true); }
                catch (...) { return Value(false); }
            }
            return Value(false);
        }

        if (builtin == "http.put" || builtin == "http.delete" || builtin == "http.patch") {

            if (args.empty()) return Value::object();
            std::string method = (builtin == "http.put") ? "PUT" : (builtin == "http.delete") ? "DELETE" : "PATCH";
            std::string url = to_string(args[0]);
            std::string body = args.size() >= 2 ? to_string(args[1]) : "";

            Value result = Value::object();
            (*result.as_object())["status"] = Value(0.0);
            (*result.as_object())["body"] = Value(std::string(""));
            return result;
        }

        if (builtin == "http.listen") {
            if (args.size() < 2 || !is_lambda(args[1])) {
                throw std::runtime_error("http.listen(port, handler) requires a lambda handler");
            }
            int port = static_cast<int>(to_number(args[0]));
            Value handler = args[1];

#ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
            SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
            if (serverSock == INVALID_SOCKET) throw std::runtime_error("http.listen: socket() failed");
            int opt = 1;
            setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(static_cast<uint16_t>(port));
            if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
                closesocket(serverSock);
                throw std::runtime_error("http.listen: bind() failed on port " + std::to_string(port));
            }
            listen(serverSock, 16);
            std::cout << "[RayQuiro] HTTP server listening on port " << port << "\n";
            while (true) {
                sockaddr_in clientAddr{};
#ifdef _WIN32
                int clientLen = sizeof(clientAddr);
#else
                socklen_t clientLen = sizeof(clientAddr);
#endif
                SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen);
                if (clientSock == INVALID_SOCKET) continue;

                std::string raw;
                char buf[4096];
                int received;
                while ((received = recv(clientSock, buf, sizeof(buf)-1, 0)) > 0) {
                    buf[received] = 0;
                    raw += buf;
                    if (raw.find("\r\n\r\n") != std::string::npos) break;
                }

                std::string method, path, httpBody, query;
                std::unordered_map<std::string,std::string> headers;
                std::istringstream ss(raw);
                std::string line;
                std::getline(ss, line);
                { std::istringstream ls(line); ls >> method >> path; }

                auto qpos = path.find('?');
                if (qpos != std::string::npos) { query = path.substr(qpos+1); path = path.substr(0,qpos); }
                while (std::getline(ss, line) && line != "\r" && !line.empty()) {
                    auto col = line.find(':');
                    if (col != std::string::npos) headers[line.substr(0,col)] = line.substr(col+2);
                }

                auto bpos = raw.find("\r\n\r\n");
                if (bpos != std::string::npos) httpBody = raw.substr(bpos+4);

                Value req = Value::object();
                (*req.as_object())["method"] = Value(method);
                (*req.as_object())["path"] = Value(path);
                (*req.as_object())["query"] = Value(query);
                (*req.as_object())["body"] = Value(httpBody);
                Value hdrsObj = Value::object();
                for (auto& h : headers) (*hdrsObj.as_object())[h.first] = Value(h.second);
                (*req.as_object())["headers"] = hdrsObj;

                struct ResState { int status = 200; std::string body; std::string contentType = "application/json"; };
                auto resState = std::make_shared<ResState>();
                Value res = Value::object();

                auto sendLambda = std::make_shared<LambdaValue>();
                sendLambda->paramNames = {"body"};

                (*res.as_object())["_resState"] = Value(std::string(""));

                try {
                    callLambda(handler, {req, res}, globals_);
                } catch (...) {}

                std::string responseBody;
                int statusCode = 200;
                std::string ct = "text/plain";
                auto& resMap = *res.as_object();
                if (resMap.count("_body")) responseBody = to_string(resMap["_body"]);
                if (resMap.count("_status")) statusCode = static_cast<int>(to_number(resMap["_status"]));
                if (resMap.count("_ct")) ct = to_string(resMap["_ct"]);

                std::string httpResp = "HTTP/1.1 " + std::to_string(statusCode) + " OK\r\nContent-Type: " + ct +
                    "\r\nContent-Length: " + std::to_string(responseBody.size()) +
                    "\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n" + responseBody;
                send(clientSock, httpResp.c_str(), static_cast<int>(httpResp.size()), 0);
                closesocket(clientSock);
            }
            closesocket(serverSock);
            return Value();
        }

        if (builtin == "env.get") {
            if (args.empty()) return Value(std::string(""));
            const char* v = std::getenv(to_string(args[0]).c_str());
            return Value(v ? std::string(v) : std::string(""));
        }
        if (builtin == "env.set") {
            if (args.size() >= 2) {
#ifdef _WIN32
                _putenv_s(to_string(args[0]).c_str(), to_string(args[1]).c_str());
#else
                setenv(to_string(args[0]).c_str(), to_string(args[1]).c_str(), 1);
#endif
            }
            return Value();
        }
        if (builtin == "env.has") {
            if (args.empty()) return Value(false);
            return Value(std::getenv(to_string(args[0]).c_str()) != nullptr);
        }
        if (builtin == "env.all") {
            Value obj = Value::object();
#ifdef _WIN32
            char* env_block = GetEnvironmentStrings();
            if (env_block) {
                for (char* p = env_block; *p; p += strlen(p) + 1) {
                    std::string entry(p);
                    auto eq = entry.find('=');
                    if (eq != std::string::npos && eq > 0)
                        (*obj.as_object())[entry.substr(0, eq)] = Value(entry.substr(eq + 1));
                }
                FreeEnvironmentStrings(env_block);
            }
#else
            extern char** environ;
            for (char** ep = environ; ep && *ep; ++ep) {
                std::string entry(*ep);
                auto eq = entry.find('=');
                if (eq != std::string::npos && eq > 0)
                    (*obj.as_object())[entry.substr(0, eq)] = Value(entry.substr(eq + 1));
            }
#endif
            return obj;
        }

        if (builtin == "process.exit") {
            int code = args.empty() ? 0 : static_cast<int>(to_number(args[0]));
            std::exit(code);
        }
        if (builtin == "process.pid") {
#ifdef _WIN32
            return Value(static_cast<double>(GetCurrentProcessId()));
#else
            return Value(static_cast<double>(getpid()));
#endif
        }
        if (builtin == "process.cwd") {
            return Value(std::filesystem::current_path().string());
        }
        if (builtin == "process.args") {
            Value arr = Value::array();
            for (const auto& a : processArgs_) arr.as_array()->push_back(Value(a));
            return arr;
        }

        if (builtin == "datetime.now") {
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &t);
#else
            localtime_r(&t, &tm_buf);
#endif
            Value obj = Value::object();
            (*obj.as_object())["year"]   = Value(static_cast<double>(tm_buf.tm_year + 1900));
            (*obj.as_object())["month"]  = Value(static_cast<double>(tm_buf.tm_mon + 1));
            (*obj.as_object())["day"]    = Value(static_cast<double>(tm_buf.tm_mday));
            (*obj.as_object())["hour"]   = Value(static_cast<double>(tm_buf.tm_hour));
            (*obj.as_object())["minute"] = Value(static_cast<double>(tm_buf.tm_min));
            (*obj.as_object())["second"] = Value(static_cast<double>(tm_buf.tm_sec));
            (*obj.as_object())["ms"]     = Value(static_cast<double>(ms.count()));
            (*obj.as_object())["timestamp"] = Value(static_cast<double>(t));
            return obj;
        }
        if (builtin == "datetime.timestamp") {
            return Value(static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
        }
        if (builtin == "datetime.format") {
            if (args.size() < 2) return Value(std::string(""));
            time_t t = static_cast<time_t>(to_number(args[0]));
            std::string fmt = to_string(args[1]);
            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &t);
#else
            localtime_r(&t, &tm_buf);
#endif
            char buf[256];
            std::strftime(buf, sizeof(buf), fmt.c_str(), &tm_buf);
            return Value(std::string(buf));
        }

        if (builtin == "path.join") {
            if (args.empty()) return Value(std::string(""));
            std::filesystem::path p(to_string(args[0]));
            for (size_t i = 1; i < args.size(); ++i) p /= to_string(args[i]);
            return Value(p.string());
        }
        if (builtin == "path.basename") {
            if (args.empty()) return Value(std::string(""));
            return Value(std::filesystem::path(to_string(args[0])).filename().string());
        }
        if (builtin == "path.dirname") {
            if (args.empty()) return Value(std::string(""));
            return Value(std::filesystem::path(to_string(args[0])).parent_path().string());
        }
        if (builtin == "path.ext") {
            if (args.empty()) return Value(std::string(""));
            return Value(std::filesystem::path(to_string(args[0])).extension().string());
        }
        if (builtin == "path.exists") {
            if (args.empty()) return Value(false);
            return Value(std::filesystem::exists(to_string(args[0])));
        }
        if (builtin == "path.abs") {
            if (args.empty()) return Value(std::string(""));
            std::error_code ec;
            auto p = std::filesystem::absolute(to_string(args[0]), ec);
            return Value(ec ? to_string(args[0]) : p.string());
        }
        if (builtin == "path.stem") {
            if (args.empty()) return Value(std::string(""));
            return Value(std::filesystem::path(to_string(args[0])).stem().string());
        }

        if (builtin == "hash.sha256") {
            if (args.empty()) return Value(std::string(""));
            std::string input = to_string(args[0]);
            auto rotr = [](uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); };
            static const uint32_t K[64]={
                0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
                0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
                0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
                0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
                0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
                0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
                0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
                0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
            };
            uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
            std::vector<uint8_t> msg(input.begin(),input.end());
            uint64_t bl=msg.size()*8; msg.push_back(0x80);
            while(msg.size()%64!=56) msg.push_back(0);
            for(int i=7;i>=0;--i) msg.push_back((bl>>(i*8))&0xFF);
            for(size_t chunk=0;chunk<msg.size();chunk+=64){
                uint32_t w[64];
                for(int i=0;i<16;++i) w[i]=(msg[chunk+i*4]<<24)|(msg[chunk+i*4+1]<<16)|(msg[chunk+i*4+2]<<8)|msg[chunk+i*4+3];
                for(int i=16;i<64;++i){auto s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);auto s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
                uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
                for(int i=0;i<64;++i){auto S1=rotr(e,6)^rotr(e,11)^rotr(e,25);auto ch=(e&f)^(~e&g);auto t1=hh+S1+ch+K[i]+w[i];auto S0=rotr(a,2)^rotr(a,13)^rotr(a,22);auto maj=(a&b)^(a&c)^(b&c);hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+S0+maj;}
                h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
            }
            std::ostringstream oss;
            for(auto v:h) oss<<std::hex<<std::setw(8)<<std::setfill('0')<<v;
            return Value(oss.str());
        }
        if (builtin == "hash.sha1") {
            if (args.empty()) return Value(std::string(""));
            std::string input=to_string(args[0]);
            auto rotl=[](uint32_t x,int n){return(x<<n)|(x>>(32-n));};
            uint32_t h0=0x67452301,h1=0xEFCDAB89,h2=0x98BADCFE,h3=0x10325476,h4=0xC3D2E1F0;
            std::vector<uint8_t> msg(input.begin(),input.end());
            uint64_t ml=msg.size()*8; msg.push_back(0x80);
            while(msg.size()%64!=56) msg.push_back(0);
            for(int i=7;i>=0;--i) msg.push_back((ml>>(i*8))&0xFF);
            for(size_t c=0;c<msg.size();c+=64){
                uint32_t w[80];
                for(int i=0;i<16;++i) w[i]=(msg[c+i*4]<<24)|(msg[c+i*4+1]<<16)|(msg[c+i*4+2]<<8)|msg[c+i*4+3];
                for(int i=16;i<80;++i) w[i]=rotl(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
                uint32_t a=h0,b=h1,cc=h2,d=h3,e=h4;
                for(int i=0;i<80;++i){uint32_t f2,k2;if(i<20){f2=(b&cc)|(~b&d);k2=0x5A827999;}else if(i<40){f2=b^cc^d;k2=0x6ED9EBA1;}else if(i<60){f2=(b&cc)|(b&d)|(cc&d);k2=0x8F1BBCDC;}else{f2=b^cc^d;k2=0xCA62C1D6;}uint32_t tmp=rotl(a,5)+f2+e+k2+w[i];e=d;d=cc;cc=rotl(b,30);b=a;a=tmp;}
                h0+=a;h1+=b;h2+=cc;h3+=d;h4+=e;
            }
            std::ostringstream oss;
            for(auto v:{h0,h1,h2,h3,h4}) oss<<std::hex<<std::setw(8)<<std::setfill('0')<<v;
            return Value(oss.str());
        }

        if (builtin == "crypto.sha256") {
            if (args.empty()) return Value(std::string(""));
            return callBuiltin("hash.sha256", args);
        }
        if (builtin == "crypto.md5") {
            if (args.empty()) return Value(std::string(""));
            std::string s = to_string(args[0]);
            const auto msg = [&](){
                std::vector<uint8_t> v(s.begin(), s.end());
                uint64_t origLen = v.size() * 8;
                v.push_back(0x80);
                while ((v.size() % 64) != 56) v.push_back(0);
                for (int i = 0; i < 8; ++i) v.push_back((origLen >> (i*8)) & 0xFF);
                return v;
            }();
            uint32_t a0=0x67452301,b0=0xefcdab89,c0=0x98badcfe,d0=0x10325476;
            static const uint32_t S[]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
            static const uint32_t T[]={0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
            auto lrot=[](uint32_t x,uint32_t n){return (x<<n)|(x>>(32-n));};
            for (size_t c=0;c<msg.size();c+=64){
                uint32_t M[16]; for(int i=0;i<16;++i) M[i]=msg[c+i*4]|(msg[c+i*4+1]<<8)|(msg[c+i*4+2]<<16)|(msg[c+i*4+3]<<24);
                uint32_t A=a0,B=b0,C=c0,D=d0;
                for(int i=0;i<64;++i){uint32_t F2,g;if(i<16){F2=(B&C)|(~B&D);g=i;}else if(i<32){F2=(D&B)|(~D&C);g=(5*i+1)%16;}else if(i<48){F2=B^C^D;g=(3*i+5)%16;}else{F2=C^(B|~D);g=(7*i)%16;}F2=F2+A+T[i]+M[g];A=D;D=C;C=B;B=B+lrot(F2,S[i]);}
                a0+=A;b0+=B;c0+=C;d0+=D;
            }
            std::ostringstream oss;
            auto le=[&](uint32_t v){ for(int i=0;i<4;++i) oss<<std::hex<<std::setw(2)<<std::setfill('0')<<((v>>(i*8))&0xFF); };
            le(a0);le(b0);le(c0);le(d0);
            return Value(oss.str());
        }
        if (builtin == "crypto.sha1") {
            return callBuiltin("hash.sha1", args);
        }
        if (builtin == "crypto.base64_encode") {
            if (args.empty()) return Value(std::string(""));
            const std::string input = to_string(args[0]);
            static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            int val=0, bits=-6;
            for (unsigned char c : input) {
                val = (val << 8) + c; bits += 8;
                while (bits >= 0) { out.push_back(tbl[(val >> bits) & 0x3F]); bits -= 6; }
            }
            if (bits > -6) out.push_back(tbl[((val << 8) >> (bits+8)) & 0x3F]);
            while (out.size() % 4) out.push_back('=');
            return Value(out);
        }
        if (builtin == "crypto.base64_decode") {
            if (args.empty()) return Value(std::string(""));
            const std::string input = to_string(args[0]);
            static const int lookup[256] = {
                -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
                -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
                -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
                -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
            };
            std::string out; int val=0,bits=-8;
            for (unsigned char c : input) {
                int d = lookup[c];
                if (d == -1) break;
                if (d == -2) continue;
                val = (val << 6) + d; bits += 6;
                if (bits >= 0) { out.push_back(static_cast<char>((val >> bits) & 0xFF)); bits -= 8; }
            }
            return Value(out);
        }
        if (builtin == "crypto.uuid") {
            std::random_device rd;
            std::mt19937_64 rng(rd());
            std::uniform_int_distribution<uint64_t> dist;
            uint64_t hi = dist(rng), lo = dist(rng);
            hi = (hi & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull;
            lo = (lo & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull;
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            oss << std::setw(8) << ((hi >> 32) & 0xFFFFFFFF) << "-";
            oss << std::setw(4) << ((hi >> 16) & 0xFFFF) << "-";
            oss << std::setw(4) << (hi & 0xFFFF) << "-";
            oss << std::setw(4) << ((lo >> 48) & 0xFFFF) << "-";
            oss << std::setw(12) << (lo & 0xFFFFFFFFFFFFull);
            return Value(oss.str());
        }
        if (builtin == "crypto.random_bytes") {
            int n = args.empty() ? 16 : static_cast<int>(to_number(args[0]));
            if (n < 1) n = 1; if (n > 1024) n = 1024;
            std::random_device rd;
            std::mt19937 rng(rd());
            std::uniform_int_distribution<int> dist(0, 255);
            std::ostringstream oss;
            for (int i = 0; i < n; ++i)
                oss << std::hex << std::setw(2) << std::setfill('0') << dist(rng);
            return Value(oss.str());
        }
        if (builtin == "crypto.hmac_sha256") {
            if (args.size() < 2) return Value(std::string(""));
            std::string msg2 = to_string(args[0]);
            std::string key2 = to_string(args[1]);
            if (key2.size() > 64) { std::vector<Value> kv{Value(key2)}; key2 = to_string(callBuiltin("hash.sha256", kv).value_or(Value(std::string("")))); }
            while (key2.size() < 64) key2.push_back('\0');
            std::string opad(64, '\x5c'), ipad(64, '\x36');
            for (size_t i = 0; i < 64; ++i) { opad[i] ^= key2[i]; ipad[i] ^= key2[i]; }
            std::vector<Value> inner_args{Value(ipad + msg2)};
            std::string inner = to_string(callBuiltin("hash.sha256", inner_args).value_or(Value(std::string(""))));
            std::string inner_bytes;
            for (size_t i = 0; i < inner.size(); i += 2) {
                inner_bytes.push_back(static_cast<char>(std::stoi(inner.substr(i, 2), nullptr, 16)));
            }
            std::vector<Value> outer_args{Value(opad + inner_bytes)};
            return Value(to_string(callBuiltin("hash.sha256", outer_args).value_or(Value(std::string("")))));
        }

        if (builtin == "regex.test") {
            if (args.size() < 2) return Value(false);
            try {
                std::regex re(to_string(args[0]));
                return Value(std::regex_search(to_string(args[1]), re));
            } catch (...) { return Value(false); }
        }
        if (builtin == "regex.match") {
            if (args.size() < 2) return Value();
            try {
                std::regex re(to_string(args[0]));
                std::string target = to_string(args[1]);
                std::smatch m;
                if (std::regex_search(target, m, re)) {
                    Value arr = Value::array();
                    for (size_t i = 0; i < m.size(); ++i) {
                        arr.as_array()->push_back(Value(m[i].str()));
                    }
                    return arr;
                }
                return Value();
            } catch (...) { return Value(); }
        }
        if (builtin == "regex.replace") {
            if (args.size() < 3) return args.size() > 1 ? args[1] : Value(std::string(""));
            try {
                std::regex re(to_string(args[0]));
                return Value(std::regex_replace(to_string(args[1]), re, to_string(args[2])));
            } catch (...) { return args.size() > 1 ? args[1] : Value(std::string("")); }
        }
        if (builtin == "regex.split") {
            Value arr = Value::array();
            if (args.size() < 2) return arr;
            try {
                std::regex re(to_string(args[0]));
                std::string target = to_string(args[1]);
                std::sregex_token_iterator it(target.begin(), target.end(), re, -1);
                std::sregex_token_iterator end;
                for (; it != end; ++it) {
                    arr.as_array()->push_back(Value(it->str()));
                }
                return arr;
            } catch (...) {
                arr.as_array()->push_back(args[1]);
                return arr;
            }
        }

        if (builtin == "vec2") {
            Value v=Value::object(); double x=args.size()>0?to_number(args[0]):0,y=args.size()>1?to_number(args[1]):0;
            (*v.as_object())["x"]=Value(x);(*v.as_object())["y"]=Value(y);(*v.as_object())["__type__"]=Value(std::string("vec2"));return v;
        }
        if (builtin == "vec3") {
            Value v=Value::object(); double x=args.size()>0?to_number(args[0]):0,y=args.size()>1?to_number(args[1]):0,z=args.size()>2?to_number(args[2]):0;
            (*v.as_object())["x"]=Value(x);(*v.as_object())["y"]=Value(y);(*v.as_object())["z"]=Value(z);(*v.as_object())["__type__"]=Value(std::string("vec3"));return v;
        }
        if (builtin=="vec_add"&&args.size()>=2&&is_object(args[0])&&is_object(args[1])){auto&a=*args[0].as_object();auto&b=*args[1].as_object();bool i3=a.count("z")&&b.count("z");Value r=Value::object();(*r.as_object())["x"]=Value(to_number(a.at("x"))+to_number(b.at("x")));(*r.as_object())["y"]=Value(to_number(a.at("y"))+to_number(b.at("y")));if(i3)(*r.as_object())["z"]=Value(to_number(a.at("z"))+to_number(b.at("z")));return r;}
        if (builtin=="vec_sub"&&args.size()>=2&&is_object(args[0])&&is_object(args[1])){auto&a=*args[0].as_object();auto&b=*args[1].as_object();bool i3=a.count("z")&&b.count("z");Value r=Value::object();(*r.as_object())["x"]=Value(to_number(a.at("x"))-to_number(b.at("x")));(*r.as_object())["y"]=Value(to_number(a.at("y"))-to_number(b.at("y")));if(i3)(*r.as_object())["z"]=Value(to_number(a.at("z"))-to_number(b.at("z")));return r;}
        if (builtin=="vec_mul"&&args.size()>=2&&is_object(args[0])){auto&a=*args[0].as_object();double s=to_number(args[1]);bool i3=a.count("z");Value r=Value::object();(*r.as_object())["x"]=Value(to_number(a.at("x"))*s);(*r.as_object())["y"]=Value(to_number(a.at("y"))*s);if(i3)(*r.as_object())["z"]=Value(to_number(a.at("z"))*s);return r;}
        if (builtin=="vec_dot"&&args.size()>=2&&is_object(args[0])&&is_object(args[1])){auto&a=*args[0].as_object();auto&b=*args[1].as_object();double d=to_number(a.at("x"))*to_number(b.at("x"))+to_number(a.at("y"))*to_number(b.at("y"));if(a.count("z")&&b.count("z"))d+=to_number(a.at("z"))*to_number(b.at("z"));return Value(d);}
        if (builtin=="vec_len"&&!args.empty()&&is_object(args[0])){auto&a=*args[0].as_object();double d=to_number(a.at("x"))*to_number(a.at("x"))+to_number(a.at("y"))*to_number(a.at("y"));if(a.count("z"))d+=to_number(a.at("z"))*to_number(a.at("z"));return Value(std::sqrt(d));}
        if (builtin=="vec_normalize"&&!args.empty()&&is_object(args[0])){auto&a=*args[0].as_object();double d=to_number(a.at("x"))*to_number(a.at("x"))+to_number(a.at("y"))*to_number(a.at("y"));if(a.count("z"))d+=to_number(a.at("z"))*to_number(a.at("z"));double len=std::max(std::sqrt(d),1e-10);bool i3=a.count("z");Value r=Value::object();(*r.as_object())["x"]=Value(to_number(a.at("x"))/len);(*r.as_object())["y"]=Value(to_number(a.at("y"))/len);if(i3)(*r.as_object())["z"]=Value(to_number(a.at("z"))/len);return r;}
        if (builtin=="vec_dist"&&args.size()>=2&&is_object(args[0])&&is_object(args[1])){auto&a=*args[0].as_object();auto&b=*args[1].as_object();double dx=to_number(a.at("x"))-to_number(b.at("x")),dy=to_number(a.at("y"))-to_number(b.at("y")),dz=0;if(a.count("z")&&b.count("z"))dz=to_number(a.at("z"))-to_number(b.at("z"));return Value(std::sqrt(dx*dx+dy*dy+dz*dz));}
        if (builtin=="vec_lerp"&&args.size()>=3&&is_object(args[0])&&is_object(args[1])){auto&a=*args[0].as_object();auto&b=*args[1].as_object();double t=to_number(args[2]);bool i3=a.count("z")&&b.count("z");Value r=Value::object();(*r.as_object())["x"]=Value(to_number(a.at("x"))+(to_number(b.at("x"))-to_number(a.at("x")))*t);(*r.as_object())["y"]=Value(to_number(a.at("y"))+(to_number(b.at("y"))-to_number(a.at("y")))*t);if(i3)(*r.as_object())["z"]=Value(to_number(a.at("z"))+(to_number(b.at("z"))-to_number(a.at("z")))*t);return r;}
        if (builtin=="vec_cross"&&args.size()>=2&&is_object(args[0])&&is_object(args[1])){auto&a=*args[0].as_object();auto&b=*args[1].as_object();double ax=to_number(a.at("x")),ay=to_number(a.at("y")),az=a.count("z")?to_number(a.at("z")):0;double bx=to_number(b.at("x")),by=to_number(b.at("y")),bz=b.count("z")?to_number(b.at("z")):0;Value r=Value::object();(*r.as_object())["x"]=Value(ay*bz-az*by);(*r.as_object())["y"]=Value(az*bx-ax*bz);(*r.as_object())["z"]=Value(ax*by-ay*bx);return r;}

#if RAYQUIRO_HAS_RAYLIB
        if (builtin=="engine.init_audio"){InitAudioDevice();return Value();}
        if (builtin=="engine.load_sound"){
            if(args.empty())return Value();
            Sound snd=LoadSound(to_string(args[0]).c_str());
            size_t idx=soundRegistry_.size(); soundRegistry_.push_back(snd);
            Value obj=Value::object();(*obj.as_object())["__sound_idx__"]=Value(static_cast<double>(idx));return obj;
        }
        if (builtin=="engine.play_sound"){if(!args.empty()&&is_object(args[0])){auto it=args[0].as_object()->find("__sound_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<soundRegistry_.size())PlaySound(soundRegistry_[idx]);}}return Value();}
        if (builtin=="engine.stop_sound"){if(!args.empty()&&is_object(args[0])){auto it=args[0].as_object()->find("__sound_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<soundRegistry_.size())StopSound(soundRegistry_[idx]);}}return Value();}
        if (builtin=="engine.set_sound_volume"){if(args.size()>=2&&is_object(args[0])){auto it=args[0].as_object()->find("__sound_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<soundRegistry_.size())SetSoundVolume(soundRegistry_[idx],static_cast<float>(to_number(args[1])));}}return Value();}
        if (builtin=="engine.unload_sound"){if(!args.empty()&&is_object(args[0])){auto it=args[0].as_object()->find("__sound_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<soundRegistry_.size())UnloadSound(soundRegistry_[idx]);}}return Value();}
        if (builtin=="engine.load_music"){
            if(args.empty())return Value();
            Music mus=LoadMusicStream(to_string(args[0]).c_str());
            size_t idx=musicRegistry_.size(); musicRegistry_.push_back(mus);
            Value obj=Value::object();(*obj.as_object())["__music_idx__"]=Value(static_cast<double>(idx));return obj;
        }
        if (builtin=="engine.play_music"){if(!args.empty()&&is_object(args[0])){auto it=args[0].as_object()->find("__music_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<musicRegistry_.size())PlayMusicStream(musicRegistry_[idx]);}}return Value();}
        if (builtin=="engine.update_music"){if(!args.empty()&&is_object(args[0])){auto it=args[0].as_object()->find("__music_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<musicRegistry_.size())UpdateMusicStream(musicRegistry_[idx]);}}return Value();}
        if (builtin=="engine.stop_music"){if(!args.empty()&&is_object(args[0])){auto it=args[0].as_object()->find("__music_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<musicRegistry_.size())StopMusicStream(musicRegistry_[idx]);}}return Value();}
        if (builtin=="engine.set_music_volume"){if(args.size()>=2&&is_object(args[0])){auto it=args[0].as_object()->find("__music_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<musicRegistry_.size())SetMusicVolume(musicRegistry_[idx],static_cast<float>(to_number(args[1])));}}return Value();}
        if (builtin=="engine.is_music_playing"){if(!args.empty()&&is_object(args[0])){auto it=args[0].as_object()->find("__music_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<musicRegistry_.size())return Value(IsMusicStreamPlaying(musicRegistry_[idx]));}}return Value(false);}

        if (builtin=="engine.load_texture"){
            if(args.empty())return Value();
            Texture2D tex=LoadTexture(to_string(args[0]).c_str());
            size_t idx=textureRegistry_.size(); textureRegistry_.push_back(tex);
            Value obj=Value::object();
            (*obj.as_object())["__tex_idx__"]=Value(static_cast<double>(idx));
            (*obj.as_object())["width"]=Value(static_cast<double>(tex.width));
            (*obj.as_object())["height"]=Value(static_cast<double>(tex.height));
            return obj;
        }
        if (builtin=="engine.draw_texture"){
            if(args.size()>=3&&is_object(args[0])){auto it=args[0].as_object()->find("__tex_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<textureRegistry_.size()){int x=static_cast<int>(to_number(args[1])),y=static_cast<int>(to_number(args[2]));Color tint=WHITE;if(args.size()>=4&&is_object(args[3])){auto&cm=*args[3].as_object();tint={static_cast<unsigned char>(to_number(cm["r"])),static_cast<unsigned char>(to_number(cm["g"])),static_cast<unsigned char>(to_number(cm["b"])),255};}DrawTexture(textureRegistry_[idx],x,y,tint);}}}
            return Value();
        }
        if (builtin=="engine.draw_texture_ex"){
            if(args.size()>=6&&is_object(args[0])){auto it=args[0].as_object()->find("__tex_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<textureRegistry_.size()){auto&tex=textureRegistry_[idx];float x=(float)to_number(args[1]),y=(float)to_number(args[2]),w=(float)to_number(args[3]),h=(float)to_number(args[4]),rot=(float)to_number(args[5]);Color tint=WHITE;if(args.size()>=7&&is_object(args[6])){auto&cm=*args[6].as_object();tint={static_cast<unsigned char>(to_number(cm["r"])),static_cast<unsigned char>(to_number(cm["g"])),static_cast<unsigned char>(to_number(cm["b"])),255};}Rectangle src={0,0,(float)tex.width,(float)tex.height};Rectangle dst={x,y,w,h};DrawTexturePro(tex,src,dst,{w/2,h/2},rot,tint);}}}
            return Value();
        }
        if (builtin=="engine.draw_texture_region"){
            if(args.size()>=9&&is_object(args[0])){auto it=args[0].as_object()->find("__tex_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<textureRegistry_.size()){Rectangle src={(float)to_number(args[1]),(float)to_number(args[2]),(float)to_number(args[3]),(float)to_number(args[4])};Rectangle dst={(float)to_number(args[5]),(float)to_number(args[6]),(float)to_number(args[7]),(float)to_number(args[8])};DrawTexturePro(textureRegistry_[idx],src,dst,{0,0},0,WHITE);}}}
            return Value();
        }
        if (builtin=="engine.unload_texture"){if(!args.empty()&&is_object(args[0])){auto it=args[0].as_object()->find("__tex_idx__");if(it!=args[0].as_object()->end()){size_t idx=static_cast<size_t>(to_number(it->second));if(idx<textureRegistry_.size())UnloadTexture(textureRegistry_[idx]);}}return Value();}
#endif

        if (builtin=="engine.is_editor")  return Value(engineMode_==EngineMode::Editor);
        if (builtin=="engine.is_playing") return Value(engineMode_==EngineMode::Playing);
        if (builtin=="engine.is_paused")  return Value(engineMode_==EngineMode::Paused);
        if (builtin=="engine.mode") {
            if(engineMode_==EngineMode::Editor)  return Value(std::string("editor"));
            if(engineMode_==EngineMode::Playing) return Value(std::string("playing"));
            return Value(std::string("paused"));
        }
        if (builtin=="engine.play") {
            sceneSnapshot_=getGlobals(); engineMode_=EngineMode::Playing;
            callFunctionIfExists("on_init",{}); return Value();
        }
        if (builtin=="engine.pause") {
            if(engineMode_==EngineMode::Playing) engineMode_=EngineMode::Paused;
            return Value();
        }
        if (builtin=="engine.resume") {
            if(engineMode_==EngineMode::Paused) engineMode_=EngineMode::Playing;
            return Value();
        }
        if (builtin=="engine.stop") {
            callFunctionIfExists("on_destroy",{});
            engineMode_=EngineMode::Editor;
            if(is_object(sceneSnapshot_)){
                for(const auto&[k,v]:*sceneSnapshot_.as_object())
                    try{globals_->assign(k,v);}catch(...){globals_->define(k,v,false);}
            }
            return Value();
        }

        if (builtin=="engine.get_script_vars") {
            Value arr=Value::array();
            for(const auto&[k,slot]:globals_->vars()){
                if(k.size()>2&&k.front()=='_'&&k.back()=='_') continue;
                if(functions_.count(k)) continue;
                const Value& v=slot.value;
                Value entry=Value::object();
                (*entry.as_object())["name"]=Value(k);
                (*entry.as_object())["value"]=v;
                std::string t="null";
                if(is_number(v))t="number";
                else if(is_string(v))t="string";
                else if(is_bool(v))t="bool";
                else if(is_array(v))t="array";
                else if(is_object(v)){const auto&o=*v.as_object();t=o.count("__type__")?to_string(o.at("__type__")):"object";}
                else if(is_lambda(v))t="function";
                (*entry.as_object())["type"]=Value(t);
                arr.as_array()->push_back(entry);
            }
            return arr;
        }
        if (builtin=="engine.set_var") {
            if(args.size()>=2) setGlobal(to_string(args[0]),args[1]); return Value();
        }

        if (builtin=="engine.call") {
            if(args.empty()) return Value();
            std::vector<Value> fnArgs(args.begin()+1,args.end());
            return callFunctionIfExists(to_string(args[0]),fnArgs).value_or(Value());
        }
        if (builtin=="engine.has_fn") {
            if(args.empty()) return Value(false);
            return Value(hasFunction(to_string(args[0])));
        }

        if (builtin=="engine.scene_save") {
            if(args.empty()) return Value(false);
            auto res=callBuiltin("json.stringify",{getGlobals()});
            if(res){try{std::ofstream f(to_string(args[0]));f<<to_string(*res);return Value(true);}catch(...){}}
            return Value(false);
        }
        if (builtin=="engine.scene_load") {
            if(args.empty()) return Value(false);
            try{
                std::ifstream f(to_string(args[0]));if(!f.is_open())return Value(false);
                std::string c2((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
                auto parsed=callBuiltin("json.parse",{Value(c2)});
                if(parsed&&is_object(*parsed)){
                    for(const auto&[k,v]:*parsed->as_object())
                        try{globals_->set(k,v);}catch(...){globals_->define(k,v,false);}
                    return Value(true);
                }
            }catch(...){}
            return Value(false);
        }

#if RAYQUIRO_HAS_RAYLIB
        if(builtin=="engine.gizmo_rect"&&engineMode_==EngineMode::Editor&&args.size()>=4){DrawRectangleLinesEx({(float)to_number(args[0]),(float)to_number(args[1]),(float)to_number(args[2]),(float)to_number(args[3])},1.5f,{100,180,255,180});return Value();}
        if(builtin=="engine.gizmo_line"&&engineMode_==EngineMode::Editor&&args.size()>=4){DrawLine((int)to_number(args[0]),(int)to_number(args[1]),(int)to_number(args[2]),(int)to_number(args[3]),{100,180,255,200});return Value();}
        if(builtin=="engine.gizmo_circle"&&engineMode_==EngineMode::Editor&&args.size()>=3){DrawCircleLines((int)to_number(args[0]),(int)to_number(args[1]),(float)to_number(args[2]),{100,255,180,200});return Value();}
        if(builtin=="engine.gizmo_text"&&engineMode_==EngineMode::Editor&&args.size()>=3){DrawText(to_string(args[0]).c_str(),(int)to_number(args[1]),(int)to_number(args[2]),args.size()>=4?(int)to_number(args[3]):12,{100,200,255,220});return Value();}
#endif

        if(builtin=="engine.watch_reload"){bool en=!args.empty()&&is_bool(args[0])&&std::get<bool>(args[0].data);globals_->define("__hot_reload__",Value(en),true);return Value();}

        return std::nullopt;
    }
    static int size_of(const Value& value) {
        if (is_string(value)) return static_cast<int>(std::get<std::string>(value.data).size());
        if (is_array(value))  return static_cast<int>(value.as_array()->size());
        if (is_object(value)) return static_cast<int>(value.as_object()->size());
        return 0;
    }

    static std::string trim_copy(const std::string& value) {
        const auto start = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
        const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
        if (start >= end) return "";
        return std::string(start, end);
    }

    static std::string env_lower_copy(const std::string& value) {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return result;
    }

    static bool env_path_contains_segment(const std::string& pathValue, const std::string& candidate) {
        const std::string wanted = env_lower_copy(candidate);
        size_t start = 0;
        while (start <= pathValue.size()) {
            const size_t end = pathValue.find(';', start);
            const std::string item = pathValue.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!item.empty() && env_lower_copy(item) == wanted) {
                return true;
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return false;
    }

    static const std::unordered_set<std::string>& known_builtin_namespaces() {
        static const std::unordered_set<std::string> namespaces = {
            "app", "ui", "web", "engine", "fs", "env", "process", "time", "json", "os", "net", "http", "db"
        };
        return namespaces;
    }

    static std::string json_escape_text(const std::string& value) {
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

    static std::string json_stringify_value(const Value& value) {
        if (is_null(value)) return "null";
        if (is_number(value)) return number_to_string(std::get<double>(value.data));
        if (is_bool(value)) return std::get<bool>(value.data) ? "true" : "false";
        if (is_string(value)) return "\"" + json_escape_text(std::get<std::string>(value.data)) + "\"";
        if (is_array(value)) {
            std::string result = "[";
            const auto items = value.as_array();
            for (size_t i = 0; i < items->size(); ++i) {
                if (i) result += ",";
                result += json_stringify_value((*items)[i]);
            }
            result += "]";
            return result;
        }
        if (is_object(value)) {
            std::string result = "{";
            bool first = true;
            for (const auto& pair : *value.as_object()) {
                if (!first) result += ",";
                first = false;
                result += "\"" + json_escape_text(pair.first) + "\":" + json_stringify_value(pair.second);
            }
            result += "}";
            return result;
        }
        return "null";
    }

    static void json_skip_ws(const std::string& text, size_t& pos) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
            ++pos;
        }
    }

    static std::string json_parse_string(const std::string& text, size_t& pos) {
        if (pos >= text.size() || text[pos] != '"') {
            throw std::runtime_error("Invalid JSON string.");
        }
        ++pos;
        std::string result;
        while (pos < text.size()) {
            const char ch = text[pos++];
            if (ch == '"') {
                return result;
            }
            if (ch == '\\') {
                if (pos >= text.size()) break;
                const char escaped = text[pos++];
                if (escaped == 'n') result += '\n';
                else if (escaped == 'r') result += '\r';
                else if (escaped == 't') result += '\t';
                else result += escaped;
                continue;
            }
            result += ch;
        }
        throw std::runtime_error("Unterminated JSON string.");
    }

    static Value json_parse_value(const std::string& text, size_t& pos) {
        json_skip_ws(text, pos);
        if (pos >= text.size()) {
            throw std::runtime_error("Unexpected end of JSON input.");
        }

        if (text[pos] == '"') {
            return Value(json_parse_string(text, pos));
        }
        if (text[pos] == '[') {
            ++pos;
            std::vector<Value> values;
            json_skip_ws(text, pos);
            if (pos < text.size() && text[pos] == ']') {
                ++pos;
                return array(values);
            }
            while (pos < text.size()) {
                values.push_back(json_parse_value(text, pos));
                json_skip_ws(text, pos);
                if (pos < text.size() && text[pos] == ',') {
                    ++pos;
                    continue;
                }
                if (pos < text.size() && text[pos] == ']') {
                    ++pos;
                    return array(values);
                }
                throw std::runtime_error("Invalid JSON array.");
            }
            throw std::runtime_error("Unterminated JSON array.");
        }
        if (text[pos] == '{') {
            ++pos;
            Value objectValue = Value::object();
            auto out = objectValue.as_object();
            json_skip_ws(text, pos);
            if (pos < text.size() && text[pos] == '}') {
                ++pos;
                return objectValue;
            }
            while (pos < text.size()) {
                json_skip_ws(text, pos);
                const std::string key = json_parse_string(text, pos);
                json_skip_ws(text, pos);
                if (pos >= text.size() || text[pos] != ':') {
                    throw std::runtime_error("Invalid JSON object.");
                }
                ++pos;
                (*out)[key] = json_parse_value(text, pos);
                json_skip_ws(text, pos);
                if (pos < text.size() && text[pos] == ',') {
                    ++pos;
                    continue;
                }
                if (pos < text.size() && text[pos] == '}') {
                    ++pos;
                    return objectValue;
                }
                throw std::runtime_error("Invalid JSON object.");
            }
            throw std::runtime_error("Unterminated JSON object.");
        }
        if (text.compare(pos, 4, "true") == 0) {
            pos += 4;
            return Value(true);
        }
        if (text.compare(pos, 5, "false") == 0) {
            pos += 5;
            return Value(false);
        }
        if (text.compare(pos, 4, "null") == 0) {
            pos += 4;
            return Value();
        }

        size_t end = pos;
        while (end < text.size()) {
            const char ch = text[end];
            if (!(std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) {
                break;
            }
            ++end;
        }
        if (end == pos) {
            throw std::runtime_error("Invalid JSON value.");
        }
        const double number = std::stod(text.substr(pos, end - pos));
        pos = end;
        return Value(number);
    }

    static Value json_parse_document(const std::string& text) {
        size_t pos = 0;
        Value result = json_parse_value(text, pos);
        json_skip_ws(text, pos);
        if (pos != text.size()) {
            throw std::runtime_error("Unexpected trailing characters in JSON.");
        }
        return result;
    }

    static std::string native_module_extension() {
#ifdef _WIN32
        return ".dll";
#elif defined(__APPLE__)
        return ".dylib";
#else
        return ".so";
#endif
    }

    static bool is_known_builtin_namespace(const std::string& name) {
        return known_builtin_namespaces().count(name) != 0;
    }

    static bool is_overridable_builtin_namespace(const std::string& name) {
        return name == "app" || name == "ui" || name == "web" || name == "engine";
    }

    static bool isNativeFallbackError(const std::string& text) {
        return text.find("does not export") != std::string::npos ||
               text.find("__RQIO_FALLBACK__") != std::string::npos;
    }

    std::vector<std::filesystem::path> native_module_candidates(const std::string& name) const {
        const std::string filename = name + native_module_extension();
        return {
            projectRoot_ / ".rq_modules" / "native" / filename,
            executablePath_.parent_path() / "modules" / filename,
            RayQuiroUserPaths::modulesRoot() / filename,
            RayQuiroUserPaths::systemModulesRoot() / filename
        };
    }

    bool hasNativeModule(const std::string& name) const {
        if (name == "web" || name == "app" || name == "ui" || name == "engine" || name == "raytolfas.engine") {
            return true;
        }
        for (const auto& candidate : native_module_candidates(name)) {
            if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
                return true;
            }
        }
        return false;
    }

    NativeModule& loadNativeModule(const std::string& moduleName) {
        const auto cached = nativeModules_.find(moduleName);
        if (cached != nativeModules_.end()) {
            return cached->second;
        }

        if (moduleName == "web" || moduleName == "app" || moduleName == "ui" || moduleName == "engine" || moduleName == "raytolfas.engine") {
            NativeModule module;
            module.handle = nullptr;
            if (moduleName == "web") module.invoke = rqm_builtin_web_invoke;
            else if (moduleName == "app") module.invoke = rqm_builtin_app_invoke;
            else if (moduleName == "ui") module.invoke = rqm_builtin_ui_invoke;
            else if (moduleName == "engine" || moduleName == "raytolfas.engine") module.invoke = rqm_builtin_engine_invoke;
            module.freeMemory = rqm_builtin_free;
            nativeModules_[moduleName] = module;
            return nativeModules_[moduleName];
        }

        NativeModule module;
        std::filesystem::path loadedFrom;
        for (const auto& candidate : native_module_candidates(moduleName)) {
            if (!std::filesystem::exists(candidate) || !std::filesystem::is_regular_file(candidate)) {
                continue;
            }
#ifdef _WIN32
            module.handle = LoadLibraryA(candidate.string().c_str());
#else
            module.handle = dlopen(candidate.string().c_str(), RTLD_NOW);
#endif
            if (module.handle != nullptr) {
                loadedFrom = candidate;
                break;
            }
        }

        if (
#ifdef _WIN32
            module.handle == nullptr
#else
            module.handle == nullptr
#endif
        ) {
            throw std::runtime_error("Native module was not found: rayquiro." + moduleName);
        }

#ifdef _WIN32
        module.invoke = reinterpret_cast<NativeModule::InvokeFn>(GetProcAddress(module.handle, "rqm_invoke"));
        module.freeMemory = reinterpret_cast<NativeModule::FreeFn>(GetProcAddress(module.handle, "rqm_free"));
#else
        module.invoke = reinterpret_cast<NativeModule::InvokeFn>(dlsym(module.handle, "rqm_invoke"));
        module.freeMemory = reinterpret_cast<NativeModule::FreeFn>(dlsym(module.handle, "rqm_free"));
#endif
        if (module.invoke == nullptr || module.freeMemory == nullptr) {
            throw std::runtime_error("Native module is missing the RayQuiro ABI exports: " + loadedFrom.string());
        }

        nativeModules_[moduleName] = module;
        return nativeModules_[moduleName];
    }

    void unloadNativeModules() {
        for (auto& pair : nativeModules_) {
#ifdef _WIN32
            if (pair.second.handle != nullptr) {
                FreeLibrary(pair.second.handle);
            }
#else
            if (pair.second.handle != nullptr) {
                dlclose(pair.second.handle);
            }
#endif
        }
        nativeModules_.clear();
    }

    std::optional<Value> tryNativeModuleCall(const std::string& builtin, const std::vector<Value>& args) {
        const size_t dot = builtin.find('.');
        if (dot == std::string::npos) {
            return std::nullopt;
        }

        const std::string moduleName = builtin.substr(0, dot);
        const std::string functionName = builtin.substr(dot + 1);
        if (functionName.empty()) {
            return std::nullopt;
        }

        if (is_known_builtin_namespace(moduleName)) {
            if (!is_overridable_builtin_namespace(moduleName) || !hasNativeModule(moduleName)) {
                return std::nullopt;
            }
        } else if (!hasNativeModule(moduleName)) {
            return std::nullopt;
        }

        NativeModule& module = loadNativeModule(moduleName);
        std::vector<Value> argList = args;
        const std::string payload = json_stringify_value(array(argList));
        char* resultJson = nullptr;
        char* errorMessage = nullptr;
        const int code = module.invoke(functionName.c_str(), payload.c_str(), &resultJson, &errorMessage);

        std::string resultText = resultJson ? std::string(resultJson) : "null";
        std::string errorText = errorMessage ? std::string(errorMessage) : "";
        if (resultJson) module.freeMemory(resultJson);
        if (errorMessage) module.freeMemory(errorMessage);

        if (code != 0) {
            if (isNativeFallbackError(errorText)) {
                return std::nullopt;
            }
            throw std::runtime_error(errorText.empty()
                ? ("Native module call failed: " + builtin)
                : ("Native module call failed: " + builtin + " :: " + errorText));
        }

        return json_parse_document(resultText);
    }

    Value builtin_print(const std::vector<Value>& args) {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) std::cout << " ";
            std::cout << to_string(args[i]);
        }
        std::cout << std::endl;
        return Value();
    }

    static Value builtin_len(const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        const Value& value = args[0];
        if (is_string(value)) return Value(static_cast<double>(std::get<std::string>(value.data).size()));
        if (is_array(value)) return Value(static_cast<double>(value.as_array()->size()));
        if (is_object(value)) return Value(static_cast<double>(value.as_object()->size()));
        return Value(0.0);
    }

    static Value builtin_str(const std::vector<Value>& args) { return Value(args.empty() ? "" : to_string(args[0])); }
    static Value builtin_num(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : to_number(args[0])); }
    static Value builtin_bool(const std::vector<Value>& args) { return Value(!args.empty() && truthy(args[0])); }

    static Value builtin_type(const std::vector<Value>& args) {
        if (args.empty()) return Value("null");
        const Value& value = args[0];
        if (is_null(value)) return Value("null");
        if (is_number(value)) return Value("number");
        if (is_string(value)) return Value("string");
        if (is_bool(value)) return Value("bool");
        if (is_array(value)) return Value("array");
        if (is_object(value)) return Value("object");
        if (is_lambda(value)) return Value("function");
        return Value("unknown");
    }

    static Value builtin_range(const std::vector<Value>& args) {
        double start = 0.0;
        double end = 0.0;
        double step = 1.0;
        if (args.size() == 1) end = to_number(args[0]);
        else if (args.size() >= 2) {
            start = to_number(args[0]);
            end = to_number(args[1]);
        }
        if (args.size() >= 3) step = to_number(args[2]);
        if (step == 0.0) return array({});

        std::vector<Value> result;
        if (step > 0.0) for (double value = start; value < end; value += step) result.push_back(Value(value));
        else for (double value = start; value > end; value += step) result.push_back(Value(value));
        return array(result);
    }

    static Value builtin_push(const std::vector<Value>& args) {
        if (args.size() < 2 || !is_array(args[0])) return Value();
        Value arrayValue = args[0];
        arrayValue.as_array()->push_back(args[1]);
        return arrayValue;
    }

    static Value builtin_pop(const std::vector<Value>& args) {
        if (args.empty() || !is_array(args[0])) return Value();
        Value arrayValue = args[0];
        auto items = arrayValue.as_array();
        if (items->empty()) return Value();
        Value result = items->back();
        items->pop_back();
        return result;
    }

    static Value builtin_join(const std::vector<Value>& args) {
        if (args.empty() || !is_array(args[0])) return Value("");
        const std::string separator = args.size() > 1 ? to_string(args[1]) : "";
        auto items = args[0].as_array();
        std::string result;
        for (size_t i = 0; i < items->size(); ++i) {
            if (i) result += separator;
            result += to_string((*items)[i]);
        }
        return Value(result);
    }

    static Value builtin_split(const std::vector<Value>& args) {
        if (args.empty()) return array({});
        const std::string text = to_string(args[0]);
        const std::string separator = args.size() > 1 ? to_string(args[1]) : "";
        std::vector<Value> result;
        if (separator.empty()) {
            for (char c : text) result.push_back(Value(std::string(1, c)));
            return array(result);
        }
        size_t start = 0;
        while (true) {
            const size_t found = text.find(separator, start);
            if (found == std::string::npos) {
                result.push_back(Value(text.substr(start)));
                break;
            }
            result.push_back(Value(text.substr(start, found - start)));
            start = found + separator.size();
        }
        return array(result);
    }

    static Value builtin_upper(const std::vector<Value>& args) {
        std::string result = args.empty() ? "" : to_string(args[0]);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return Value(result);
    }

    static Value builtin_lower(const std::vector<Value>& args) {
        std::string result = args.empty() ? "" : to_string(args[0]);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return Value(result);
    }

    static Value builtin_contains(const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        if (is_string(args[0])) {
            return Value(std::get<std::string>(args[0].data).find(to_string(args[1])) != std::string::npos);
        }
        if (is_array(args[0])) {
            for (const auto& item : *args[0].as_array()) {
                if (truthy(eq(item, args[1]))) return Value(true);
            }
        }
        return Value(false);
    }

    static Value builtin_trim(const std::vector<Value>& args) { return Value(args.empty() ? "" : trim_copy(to_string(args[0]))); }

    static Value builtin_replace(const std::vector<Value>& args) {
        if (args.size() < 3) return Value(args.empty() ? "" : to_string(args[0]));
        std::string text = to_string(args[0]);
        const std::string needle = to_string(args[1]);
        const std::string replacement = to_string(args[2]);
        if (needle.empty()) return Value(text);
        size_t start = 0;
        while ((start = text.find(needle, start)) != std::string::npos) {
            text.replace(start, needle.size(), replacement);
            start += replacement.size();
        }
        return Value(text);
    }

    static Value builtin_slice(const std::vector<Value>& args) {
        if (args.empty()) return Value();
        const Value& source = args[0];
        const int start = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 0;
        const int end = args.size() > 2 ? static_cast<int>(to_number(args[2])) : size_of(source);
        if (is_string(source)) {
            const std::string text = std::get<std::string>(source.data);
            const int safeStart = std::max(0, start);
            const int safeEnd = std::max(safeStart, std::min(static_cast<int>(text.size()), end));
            return Value(text.substr(static_cast<size_t>(safeStart), static_cast<size_t>(safeEnd - safeStart)));
        }
        if (is_array(source)) {
            auto items = source.as_array();
            const int safeStart = std::max(0, start);
            const int safeEnd = std::max(safeStart, std::min(static_cast<int>(items->size()), end));
            std::vector<Value> result;
            for (int i = safeStart; i < safeEnd; ++i) result.push_back((*items)[static_cast<size_t>(i)]);
            return array(result);
        }
        return Value();
    }

    static Value builtin_floor(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::floor(to_number(args[0]))); }
    static Value builtin_ceil(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::ceil(to_number(args[0]))); }
    static Value builtin_round(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::round(to_number(args[0]))); }

    static Value builtin_min(const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        double result = to_number(args[0]);
        for (size_t i = 1; i < args.size(); ++i) result = std::min(result, to_number(args[i]));
        return Value(result);
    }

    static Value builtin_max(const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        double result = to_number(args[0]);
        for (size_t i = 1; i < args.size(); ++i) result = std::max(result, to_number(args[i]));
        return Value(result);
    }

    static Value builtin_clamp(const std::vector<Value>& args) {
        if (args.size() < 3) return Value(args.empty() ? 0.0 : to_number(args[0]));
        return Value(std::clamp(to_number(args[0]), to_number(args[1]), to_number(args[2])));
    }

    static Value builtin_sqrt(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::sqrt(to_number(args[0]))); }
    static Value builtin_abs(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::abs(to_number(args[0]))); }
    static Value builtin_pow(const std::vector<Value>& args) { return Value(args.size() < 2 ? 0.0 : std::pow(to_number(args[0]), to_number(args[1]))); }
    static Value builtin_sin(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::sin(to_number(args[0]))); }
    static Value builtin_cos(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::cos(to_number(args[0]))); }
    static Value builtin_tan(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::tan(to_number(args[0]))); }
    static Value builtin_log(const std::vector<Value>& args) { return Value(args.empty() ? 0.0 : std::log(to_number(args[0]))); }

    static Value builtin_sleep(const std::vector<Value>& args) {
        const int duration = args.empty() ? 0 : static_cast<int>(to_number(args[0]));
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
        return Value();
    }

    static Value builtin_clock_ms(const std::vector<Value>& args) {
        (void)args;
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return Value(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()));
    }

    static Value builtin_time_unix_ms(const std::vector<Value>& args) {
        (void)args;
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return Value(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()));
    }

    static Value builtin_json_stringify(const std::vector<Value>& args) {
        return Value(args.empty() ? std::string("null") : json_stringify_value(args[0]));
    }

    static Value builtin_json_parse(const std::vector<Value>& args) {
        return json_parse_document(args.empty() ? std::string("null") : to_string(args[0]));
    }

    static std::mt19937& random_engine() {
        static std::mt19937 engine(static_cast<unsigned int>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        return engine;
    }

    static Value builtin_random(const std::vector<Value>& args) {
        double min = 0.0;
        double max = 1.0;
        if (args.size() == 1) max = to_number(args[0]);
        else if (args.size() >= 2) {
            min = to_number(args[0]);
            max = to_number(args[1]);
        }
        if (max < min) std::swap(min, max);
        std::uniform_real_distribution<double> distribution(min, max);
        return Value(distribution(random_engine()));
    }

    static Value builtin_random_int(const std::vector<Value>& args) {
        int min = 0;
        int max = 100;
        if (args.size() == 1) max = static_cast<int>(to_number(args[0]));
        else if (args.size() >= 2) {
            min = static_cast<int>(to_number(args[0]));
            max = static_cast<int>(to_number(args[1]));
        }
        if (max < min) std::swap(min, max);
        std::uniform_int_distribution<int> distribution(min, max);
        return Value(static_cast<double>(distribution(random_engine())));
    }

    static Value builtin_fs_exists(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        return Value(std::filesystem::exists(std::filesystem::path(to_string(args[0]))));
    }

    static Value builtin_fs_mkdir(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        try {
            std::filesystem::create_directories(std::filesystem::path(to_string(args[0])));
            return Value(true);
        } catch (...) {
            return Value(false);
        }
    }

    static Value builtin_fs_copy(const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        try {
            const std::filesystem::path source = std::filesystem::path(to_string(args[0]));
            const std::filesystem::path target = std::filesystem::path(to_string(args[1]));
            const auto parent = target.parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing);
            return Value(true);
        } catch (...) {
            return Value(false);
        }
    }

    static Value builtin_fs_copy_tree(const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        try {
            std::filesystem::copy(
                std::filesystem::path(to_string(args[0])),
                std::filesystem::path(to_string(args[1])),
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
            return Value(true);
        } catch (...) {
            return Value(false);
        }
    }

    static Value builtin_fs_remove(const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        try {
            return Value(static_cast<double>(std::filesystem::remove_all(std::filesystem::path(to_string(args[0])))));
        } catch (...) {
            return Value(0.0);
        }
    }

    static Value builtin_fs_read(const std::vector<Value>& args) {
        if (args.empty()) return Value("");
        try {
            std::ifstream input(std::filesystem::path(to_string(args[0])), std::ios::binary);
            if (!input) return Value("");
            std::ostringstream buffer;
            buffer << input.rdbuf();
            return Value(buffer.str());
        } catch (...) {
            return Value("");
        }
    }

    static Value builtin_fs_write(const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        try {
            const std::filesystem::path target = std::filesystem::path(to_string(args[0]));
            const auto parent = target.parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            std::ofstream output(target, std::ios::binary | std::ios::trunc);
            if (!output) return Value(false);
            output << to_string(args[1]);
            return Value(true);
        } catch (...) {
            return Value(false);
        }
    }

    static Value builtin_process_run(const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        return Value(static_cast<double>(std::system(to_string(args[0]).c_str())));
    }

    Value builtin_process_exe_dir(const std::vector<Value>& args) const {
        (void)args;
        return Value(projectRoot_.string());
    }

    static Value builtin_env_get(const std::vector<Value>& args) {
        if (args.empty()) return Value("");
        const char* value = std::getenv(to_string(args[0]).c_str());
        return Value(value ? std::string(value) : std::string());
    }

    static Value builtin_env_set(const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
#ifdef _WIN32
        return Value(_putenv_s(to_string(args[0]).c_str(), to_string(args[1]).c_str()) == 0);
#else
        return Value(setenv(to_string(args[0]).c_str(), to_string(args[1]).c_str(), 1) == 0);
#endif
    }

    static Value builtin_env_path_add(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        const std::string candidate = std::filesystem::path(to_string(args[0])).lexically_normal().string();
#ifdef _WIN32
        HKEY key = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ | KEY_SET_VALUE, &key) != 0L) {
            return Value(false);
        }

        DWORD type = REG_EXPAND_SZ;
        DWORD size = 0;
        std::string currentValue;
        const LONG queryResult = RegQueryValueExA(key, "Path", nullptr, &type, nullptr, &size);
        if (queryResult == 0L && size > 1) {
            std::vector<char> buffer(size + 1, '\0');
            if (RegQueryValueExA(key, "Path", nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size) == 0L) {
                currentValue.assign(buffer.data());
            }
        }

        if (!env_path_contains_segment(currentValue, candidate)) {
            const std::string updatedValue = currentValue.empty() ? candidate : currentValue + ";" + candidate;
            if (RegSetValueExA(
                    key,
                    "Path",
                    0,
                    REG_EXPAND_SZ,
                    reinterpret_cast<const BYTE*>(updatedValue.c_str()),
                    static_cast<DWORD>(updatedValue.size() + 1)) != 0L) {
                RegCloseKey(key);
                return Value(false);
            }
        }
        RegCloseKey(key);

        const char* processPath = std::getenv("PATH");
        std::string currentProcessPath = processPath ? processPath : "";
        if (!env_path_contains_segment(currentProcessPath, candidate)) {
            currentProcessPath = currentProcessPath.empty() ? candidate : currentProcessPath + ";" + candidate;
            _putenv_s("PATH", currentProcessPath.c_str());
        }
        return Value(true);
#else
        return Value(false);
#endif
    }

    static Value builtin_os_name(const std::vector<Value>& args) {
        (void)args;
#ifdef _WIN32
        return Value("windows");
#elif defined(__APPLE__)
        return Value("macos");
#elif defined(__linux__)
        return Value("linux");
#else
        return Value("unknown");
#endif
    }

    static Value builtin_os_arch(const std::vector<Value>& args) {
        (void)args;
#if defined(_M_X64) || defined(__x86_64__)
        return Value("x86_64");
#elif defined(_M_IX86) || defined(__i386__)
        return Value("x86");
#elif defined(_M_ARM64) || defined(__aarch64__)
        return Value("arm64");
#elif defined(_M_ARM) || defined(__arm__)
        return Value("arm");
#else
        return Value("unknown");
#endif
    }

    static Value builtin_os_cwd(const std::vector<Value>& args) {
        (void)args;
        return Value(std::filesystem::current_path().string());
    }

    static Value builtin_os_chdir(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        try {
            std::filesystem::current_path(std::filesystem::path(to_string(args[0])));
            return Value(true);
        } catch (...) {
            return Value(false);
        }
    }

    static Value builtin_os_home(const std::vector<Value>& args) {
        (void)args;
#ifdef _WIN32
        const char* home = std::getenv("USERPROFILE");
        return Value(home ? std::string(home) : std::string());
#else
        const char* home = std::getenv("HOME");
        return Value(home ? std::string(home) : std::string());
#endif
    }

    static Value builtin_os_temp(const std::vector<Value>& args) {
        (void)args;
        return Value(std::filesystem::temp_directory_path().string());
    }

    static Value builtin_os_sep(const std::vector<Value>& args) {
        (void)args;
        return Value(std::string(1, std::filesystem::path::preferred_separator));
    }

    static Value builtin_os_exists(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        return Value(std::filesystem::exists(std::filesystem::path(to_string(args[0]))));
    }

    static Value builtin_os_is_dir(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        const std::filesystem::path path = std::filesystem::path(to_string(args[0]));
        return Value(std::filesystem::exists(path) && std::filesystem::is_directory(path));
    }

    static Value builtin_os_is_file(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        const std::filesystem::path path = std::filesystem::path(to_string(args[0]));
        return Value(std::filesystem::exists(path) && std::filesystem::is_regular_file(path));
    }

    static SOCKET net_open_tcp(const std::string& host, int port) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            return INVALID_SOCKET;
        }

        sockaddr_in address = {};
        address.sin_family = AF_INET;
        if (host == "localhost") {
            address.sin_addr.s_addr = htonl(0x7F000001u);
        } else if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        address.sin_port = htons(static_cast<u_short>(port));

        if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            closesocket(sock);
            return INVALID_SOCKET;
        }

        return sock;
    }

    int net_store_socket(SOCKET socketValue) {
        if (socketValue == INVALID_SOCKET) return 0;
        const int handle = nextNetHandle_++;
        netSockets_[handle] = socketValue;
        return handle;
    }

    SOCKET net_take_socket(int handle) const {
        const auto found = netSockets_.find(handle);
        if (found == netSockets_.end()) return INVALID_SOCKET;
        return found->second;
    }

    static std::string parse_http_path(const std::string& url) {
        const std::string scheme = "http://";
        if (url.rfind(scheme, 0) == 0) {
            const size_t slash = url.find('/', scheme.size());
            return slash == std::string::npos ? std::string("/") : url.substr(slash);
        }
        return "/";
    }

    static bool parse_http_url(const std::string& url, std::string& host, int& port, std::string& path) {
        const std::string scheme = "http://";
        if (url.rfind(scheme, 0) != 0) {
            return false;
        }
        const std::string remainder = url.substr(scheme.size());
        const size_t slash = remainder.find('/');
        const std::string hostPort = slash == std::string::npos ? remainder : remainder.substr(0, slash);
        path = slash == std::string::npos ? "/" : remainder.substr(slash);
        const size_t colon = hostPort.find(':');
        host = colon == std::string::npos ? hostPort : hostPort.substr(0, colon);
        port = colon == std::string::npos ? 80 : std::atoi(hostPort.substr(colon + 1).c_str());
        if (host.empty() || port <= 0) return false;
        return true;
    }

    static std::string decode_http_chunk(const std::string& response) {
        const size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd == std::string::npos) return response;
        return response.substr(headerEnd + 4);
    }

    Value builtin_net_tcp_connect(const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        const std::string host = to_string(args[0]);
        const int port = static_cast<int>(to_number(args[1]));
        return Value(static_cast<double>(net_store_socket(net_open_tcp(host, port))));
    }

    Value builtin_net_tcp_send(const std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        const int handle = static_cast<int>(to_number(args[0]));
        const SOCKET socketValue = net_take_socket(handle);
        if (socketValue == INVALID_SOCKET) return Value(false);
        const std::string text = to_string(args[1]);
        const int sent = send(socketValue, text.c_str(), static_cast<int>(text.size()), 0);
        return Value(sent != SOCKET_ERROR);
    }

    Value builtin_net_tcp_recv(const std::vector<Value>& args) {
        if (args.empty()) return Value("");
        const int handle = static_cast<int>(to_number(args[0]));
        const SOCKET socketValue = net_take_socket(handle);
        if (socketValue == INVALID_SOCKET) return Value("");
        const int maxBytes = args.size() > 1 ? std::max(1, static_cast<int>(to_number(args[1]))) : 65536;
        std::string result;
        result.resize(static_cast<size_t>(maxBytes));
        const int received = recv(socketValue, result.data(), maxBytes, 0);
        if (received <= 0) return Value("");
        result.resize(static_cast<size_t>(received));
        return Value(result);
    }

    Value builtin_net_tcp_close(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        const int handle = static_cast<int>(to_number(args[0]));
        const auto found = netSockets_.find(handle);
        if (found == netSockets_.end()) return Value(false);
        closesocket(found->second);
        netSockets_.erase(found);
        return Value(true);
    }

    Value builtin_http_request(const std::string& method, const std::vector<Value>& args) {
        if (args.empty()) return Value::object();
        std::string host;
        int port = 80;
        std::string path;
        if (!parse_http_url(to_string(args[0]), host, port, path)) {
            return Value::object();
        }
        const std::string body = args.size() > 1 ? to_string(args[1]) : "";
        const SOCKET socketValue = net_open_tcp(host, port);
        if (socketValue == INVALID_SOCKET) {
            return Value::object();
        }

        std::ostringstream request;
        request << method << " " << path << " HTTP/1.1\r\n";
        request << "Host: " << host << "\r\n";
        request << "Connection: close\r\n";
        if (method == "POST") {
            request << "Content-Type: text/plain; charset=utf-8\r\n";
            request << "Content-Length: " << body.size() << "\r\n";
        }
        request << "\r\n";
        if (method == "POST") {
            request << body;
        }
        const std::string payload = request.str();
        if (send(socketValue, payload.c_str(), static_cast<int>(payload.size()), 0) == SOCKET_ERROR) {
            closesocket(socketValue);
            return Value::object();
        }

        std::string response;
        char buffer[4096];
        while (true) {
            const int received = recv(socketValue, buffer, sizeof(buffer), 0);
            if (received <= 0) break;
            response.append(buffer, buffer + received);
        }
        closesocket(socketValue);

        std::istringstream stream(response);
        std::string httpVersion;
        std::string statusText;
        int statusCode = 0;
        stream >> httpVersion >> statusCode;
        std::string line;
        std::getline(stream, line);

        Value result = Value::object();
        (*result.as_object())["status"] = Value(static_cast<double>(statusCode));
        (*result.as_object())["body"] = Value(decode_http_chunk(response));
        return result;
    }

    Value builtin_http_get(const std::vector<Value>& args) {
        return builtin_http_request("GET", args);
    }

    Value builtin_http_post(const std::vector<Value>& args) {
        return builtin_http_request("POST", args);
    }

    bool postgres_load_library() {
        if (postgres_.loaded) return postgres_.available;
#ifdef _WIN32
        for (const char* candidate : {"libpq.dll"}) {
            postgres_.library = LoadLibraryA(candidate);
            if (postgres_.library != nullptr) break;
        }
#else
        for (const char* candidate : {"libpq.so.5", "libpq.so", "libpq.dylib"}) {
            postgres_.library = dlopen(candidate, RTLD_NOW);
            if (postgres_.library != nullptr) break;
        }
#endif
        if (postgres_.library == nullptr) {
            postgres_.loaded = true;
            postgres_.available = false;
            return false;
        }

#ifdef _WIN32
        postgres_.connectDb = reinterpret_cast<PostgresApi::ConnectDbFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQconnectdb"));
        postgres_.status = reinterpret_cast<PostgresApi::StatusFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQstatus"));
        postgres_.errorMessage = reinterpret_cast<PostgresApi::ErrorMessageFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQerrorMessage"));
        postgres_.finish = reinterpret_cast<PostgresApi::FinishFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQfinish"));
        postgres_.exec = reinterpret_cast<PostgresApi::ExecFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQexec"));
        postgres_.resultStatus = reinterpret_cast<PostgresApi::ResultStatusFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQresultStatus"));
        postgres_.ntuples = reinterpret_cast<PostgresApi::NtuplesFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQntuples"));
        postgres_.nfields = reinterpret_cast<PostgresApi::NfieldsFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQnfields"));
        postgres_.fname = reinterpret_cast<PostgresApi::FnameFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQfname"));
        postgres_.getvalue = reinterpret_cast<PostgresApi::GetvalueFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQgetvalue"));
        postgres_.clear = reinterpret_cast<PostgresApi::ClearFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQclear"));
        postgres_.cmdTuples = reinterpret_cast<PostgresApi::CmdTuplesFn>(GetProcAddress(reinterpret_cast<HMODULE>(postgres_.library), "PQcmdTuples"));
#else
        postgres_.connectDb = reinterpret_cast<PostgresApi::ConnectDbFn>(dlsym(postgres_.library, "PQconnectdb"));
        postgres_.status = reinterpret_cast<PostgresApi::StatusFn>(dlsym(postgres_.library, "PQstatus"));
        postgres_.errorMessage = reinterpret_cast<PostgresApi::ErrorMessageFn>(dlsym(postgres_.library, "PQerrorMessage"));
        postgres_.finish = reinterpret_cast<PostgresApi::FinishFn>(dlsym(postgres_.library, "PQfinish"));
        postgres_.exec = reinterpret_cast<PostgresApi::ExecFn>(dlsym(postgres_.library, "PQexec"));
        postgres_.resultStatus = reinterpret_cast<PostgresApi::ResultStatusFn>(dlsym(postgres_.library, "PQresultStatus"));
        postgres_.ntuples = reinterpret_cast<PostgresApi::NtuplesFn>(dlsym(postgres_.library, "PQntuples"));
        postgres_.nfields = reinterpret_cast<PostgresApi::NfieldsFn>(dlsym(postgres_.library, "PQnfields"));
        postgres_.fname = reinterpret_cast<PostgresApi::FnameFn>(dlsym(postgres_.library, "PQfname"));
        postgres_.getvalue = reinterpret_cast<PostgresApi::GetvalueFn>(dlsym(postgres_.library, "PQgetvalue"));
        postgres_.clear = reinterpret_cast<PostgresApi::ClearFn>(dlsym(postgres_.library, "PQclear"));
        postgres_.cmdTuples = reinterpret_cast<PostgresApi::CmdTuplesFn>(dlsym(postgres_.library, "PQcmdTuples"));
#endif
        postgres_.available = postgres_.connectDb && postgres_.status && postgres_.errorMessage && postgres_.finish &&
            postgres_.exec && postgres_.resultStatus && postgres_.ntuples && postgres_.nfields && postgres_.fname &&
            postgres_.getvalue && postgres_.clear && postgres_.cmdTuples;
        postgres_.loaded = true;
        return postgres_.available;
    }

    Value builtin_db_connect(const std::vector<Value>& args) {
        if (args.empty()) return Value(0.0);
        if (!postgres_load_library()) return Value(0.0);
        const std::string conninfo = to_string(args[0]);
        void* connection = postgres_.connectDb(conninfo.c_str());
        if (!connection) return Value(0.0);
        const int handle = nextDbHandle_++;
        dbConnections_[handle] = connection;
        return Value(static_cast<double>(handle));
    }

    void* db_take_connection(int handle) const {
        const auto found = dbConnections_.find(handle);
        return found == dbConnections_.end() ? nullptr : found->second;
    }

    static bool postgres_result_ok(int statusCode) {
        return statusCode == 1 || statusCode == 2;
    }

    Value builtin_db_query(const std::vector<Value>& args) {
        if (args.size() < 2) return array({});
        if (!postgres_load_library()) return array({});
        const int handle = static_cast<int>(to_number(args[0]));
        void* connection = db_take_connection(handle);
        if (!connection) return array({});
        const std::string sql = to_string(args[1]);
        void* result = postgres_.exec(connection, sql.c_str());
        if (!result) return array({});
        const int statusCode = postgres_.resultStatus(result);
        if (!postgres_result_ok(statusCode)) {
            postgres_.clear(result);
            return array({});
        }
        const int rows = postgres_.ntuples(result);
        const int cols = postgres_.nfields(result);
        std::vector<Value> outputRows;
        for (int row = 0; row < rows; ++row) {
            Value objectValue = Value::object();
            auto rowObject = objectValue.as_object();
            for (int col = 0; col < cols; ++col) {
                const char* name = postgres_.fname(result, col);
                const char* cell = postgres_.getvalue(result, row, col);
                (*rowObject)[name ? name : "column"] = Value(cell ? std::string(cell) : std::string());
            }
            outputRows.push_back(objectValue);
        }
        postgres_.clear(result);
        return array(outputRows);
    }

    Value builtin_db_exec(const std::vector<Value>& args) {
        if (args.size() < 2) return Value(0.0);
        if (!postgres_load_library()) return Value(0.0);
        const int handle = static_cast<int>(to_number(args[0]));
        void* connection = db_take_connection(handle);
        if (!connection) return Value(0.0);
        const std::string sql = to_string(args[1]);
        void* result = postgres_.exec(connection, sql.c_str());
        if (!result) return Value(0.0);
        const int statusCode = postgres_.resultStatus(result);
        if (!postgres_result_ok(statusCode)) {
            postgres_.clear(result);
            return Value(0.0);
        }
        const char* affected = postgres_.cmdTuples(result);
        const double count = affected && *affected ? std::atof(affected) : 0.0;
        postgres_.clear(result);
        return Value(count);
    }

    Value builtin_db_scalar(const std::vector<Value>& args) {
        if (args.size() < 2) return Value();
        const Value rows = builtin_db_query(args);
        if (!is_array(rows) || rows.as_array()->empty()) return Value();
        const Value firstRow = (*rows.as_array())[0];
        if (!is_object(firstRow) || firstRow.as_object()->empty()) return Value();
        return firstRow.as_object()->begin()->second;
    }

    Value builtin_db_close(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        if (!postgres_load_library()) return Value(false);
        const int handle = static_cast<int>(to_number(args[0]));
        const auto found = dbConnections_.find(handle);
        if (found == dbConnections_.end()) return Value(false);
        postgres_.finish(found->second);
        dbConnections_.erase(found);
        return Value(true);
    }

    Value builtin_app_init(const std::vector<Value>& args) {
#ifdef _WIN32
        const std::string title = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
        const int width = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 900;
        const int height = args.size() > 2 ? static_cast<int>(to_number(args[2])) : 600;
        appRuntime_.init(title, width, height);
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_app_run(const std::vector<Value>& args) {
#ifdef _WIN32
        (void)args;
        appRuntime_.run();
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_app_button(const std::vector<Value>& args) {
#ifdef _WIN32
        if (args.size() < 5) return Value();
        appRuntime_.add_button(
            to_string(args[0]),
            static_cast<int>(to_number(args[1])),
            static_cast<int>(to_number(args[2])),
            static_cast<int>(to_number(args[3])),
            static_cast<int>(to_number(args[4])));
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_app_text(const std::vector<Value>& args) {
#ifdef _WIN32
        if (args.size() < 5) return Value();
        appRuntime_.add_text(
            to_string(args[0]),
            static_cast<int>(to_number(args[1])),
            static_cast<int>(to_number(args[2])),
            static_cast<int>(to_number(args[3])),
            static_cast<int>(to_number(args[4])));
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    static Value builtin_app_msg(const std::vector<Value>& args) {
#ifdef _WIN32
        const std::string title = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
        const std::string text = args.size() > 1 ? to_string(args[1]) : "";
        RayQuiroApp::show_message(title, text);
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_ui_init(const std::vector<Value>& args) {
#ifdef _WIN32
        const std::string title = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
        const int width = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 920;
        const int height = args.size() > 2 ? static_cast<int>(to_number(args[2])) : 620;
        uiRuntime_.init(title, width, height);
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_ui_style(const std::vector<Value>& args) {
#ifdef _WIN32
        if (!args.empty()) uiRuntime_.style(to_string(args[0]));
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_ui_hero(const std::vector<Value>& args) {
#ifdef _WIN32
        const std::string title = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
        const std::string subtitle = args.size() > 1 ? to_string(args[1]) : "";
        uiRuntime_.hero(title, subtitle);
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_ui_status(const std::vector<Value>& args) {
#ifdef _WIN32
        const std::string title = args.size() > 0 ? to_string(args[0]) : "";
        const std::string body = args.size() > 1 ? to_string(args[1]) : "";
        uiRuntime_.status(title, body);
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_ui_info(const std::vector<Value>& args) {
#ifdef _WIN32
        const std::string label = args.size() > 0 ? to_string(args[0]) : "";
        const std::string value = args.size() > 1 ? to_string(args[1]) : "";
        uiRuntime_.info(label, value);
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_ui_text(const std::vector<Value>& args) {
#ifdef _WIN32
        const std::string text = args.size() > 0 ? to_string(args[0]) : "";
        const std::string role = args.size() > 1 ? to_string(args[1]) : "body";
        uiRuntime_.text(text, role);
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_ui_action(const std::vector<Value>& args) {
#ifdef _WIN32
        if (args.size() < 2) return Value();
        const std::string variant = args.size() > 2 ? to_string(args[2]) : "primary";
        uiRuntime_.action(to_string(args[0]), to_string(args[1]), variant);
        return Value();
#else
        (void)args;
        return Value();
#endif
    }

    Value builtin_ui_run(const std::vector<Value>& args) {
#ifdef _WIN32
        (void)args;
        return Value(uiRuntime_.run());
#else
        (void)args;
        return Value("close");
#endif
    }

    static std::string web_default_css() {
        std::string css;
        css += "body{margin:0;background:linear-gradient(160deg,#edf4ff,#f6fbff);color:#10243a;font-family:'Segoe UI',sans-serif;}";
        css += ".page{max-width:1080px;margin:0 auto;padding:56px 24px 72px;}";
        css += ".hero{font-size:52px;line-height:1.05;margin:0 0 14px;font-weight:800;letter-spacing:-0.03em;}";
        css += ".lead{font-size:20px;line-height:1.7;color:#49617e;margin:0 0 24px;}";
        css += ".panel{background:white;border:1px solid #d8e6f4;border-radius:24px;padding:28px;box-shadow:0 22px 60px rgba(16,36,58,.08);}";
        css += ".button{display:inline-flex;align-items:center;justify-content:center;padding:12px 18px;border-radius:16px;background:#0ea5e9;color:white;text-decoration:none;font-weight:700;border:none;}";
        return css;
    }

    static std::string web_trim(const std::string& value) {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
            ++start;
        }
        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }
        return value.substr(start, end - start);
    }

    static std::string web_class_attr(const std::string& value) {
        return value.empty() ? "" : " class=\"" + value + "\"";
    }

    static std::string web_normalize_route(const std::string& rawValue) {
        std::string route = web_trim(rawValue);
        if (route.empty()) return "/";
        if (route[0] != '/') route = "/" + route;
        while (route.size() > 1 && route.back() == '/') route.pop_back();
        return route;
    }

    static bool web_has_extension_path(const std::string& value) {
        return std::filesystem::path(value).has_extension();
    }

    static std::string web_route_to_output_path(const WebState& state, const std::string& routeValue) {
        const std::string route = web_normalize_route(routeValue);
        const std::filesystem::path root = state.outputRoot.empty() ? std::filesystem::path("build") : std::filesystem::path(state.outputRoot);
        if (route == "/") {
            return (root / "index.html").string();
        }
        const std::string relative = route.substr(1);
        return (root / relative / "index.html").string();
    }

    static std::string web_file_version(const std::filesystem::path& path) {
        try {
            if (!std::filesystem::exists(path)) return "0";
            return std::to_string(static_cast<long long>(std::filesystem::last_write_time(path).time_since_epoch().count()));
        } catch (...) {
            return "0";
        }
    }

    static std::string web_live_script() {
        return R"HTML(<script>
(function () {
  let current = null;
  async function poll() {
    try {
      const response = await fetch('/__rq_version', { cache: 'no-store' });
      const next = await response.text();
      if (current === null) {
        current = next;
      } else if (next !== current) {
        location.reload();
        return;
      }
    } catch (err) {}
    setTimeout(poll, 900);
  }
  poll();
})();
</script>)HTML";
    }

    static std::string web_read_file_text(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    static void web_replace_all(std::string& text, const std::string& needle, const std::string& replacement) {
        if (needle.empty()) return;
        size_t start = 0;
        while ((start = text.find(needle, start)) != std::string::npos) {
            text.replace(start, needle.size(), replacement);
            start += replacement.size();
        }
    }

    std::filesystem::path web_template_path() const {
        const std::filesystem::path candidate = projectRoot_ / webState_.publicDir / "index.html";
        if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
            return candidate;
        }
        return {};
    }

    WebRoute& web_current_route() {
        const std::string key = webState_.currentRoute.empty() ? std::string("/") : webState_.currentRoute;
        auto found = webState_.routes.find(key);
        if (found == webState_.routes.end()) {
            WebRoute route;
            route.route = key;
            route.title = webState_.defaultTitle;
            route.path = web_route_to_output_path(webState_, key);
            found = webState_.routes.insert({key, route}).first;
        }
        return found->second;
    }

    void web_prepare_session() {
        if (webState_.defaultTitle.empty()) {
            webState_.defaultTitle = "RayQuiro";
        }
        if (webState_.outputRoot.empty()) {
            webState_.outputRoot = "build";
        }
        if (webState_.publicDir.empty()) {
            webState_.publicDir = "public";
        }
        if (webState_.css.empty()) {
            webState_.css = web_default_css();
        }
        if (webState_.currentRoute.empty()) {
            webState_.currentRoute = "/";
        }

        webState_.active = true;
        auto& route = web_current_route();
        if (route.route.empty()) route.route = "/";
        if (route.title.empty()) route.title = webState_.defaultTitle;
        if (route.path.empty()) route.path = web_route_to_output_path(webState_, route.route);
    }

    void web_push_tag(const std::string& tag) {
        web_current_route().stack.push_back(tag);
    }

    std::string web_pop_tag(const std::string& fallback) {
        auto& route = web_current_route();
        if (route.stack.empty()) return fallback;
        const std::string tag = route.stack.back();
        route.stack.pop_back();
        return tag;
    }

    static std::string web_extract_tag_name(const std::string& markup) {
        const std::string trimmed = web_trim(markup);
        if (trimmed.size() < 3 || trimmed[0] != '<' || trimmed[1] == '/' || trimmed[1] == '!') return "";
        size_t start = 1;
        while (start < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[start])) != 0) ++start;
        size_t end = start;
        while (end < trimmed.size()) {
            const char ch = trimmed[end];
            if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '>' || ch == '/') break;
            ++end;
        }
        if (end <= start) return "";
        return trimmed.substr(start, end - start);
    }

    static bool web_should_push_markup_tag(const std::string& markup) {
        const std::string trimmed = web_trim(markup);
        if (trimmed.empty() || trimmed[0] != '<') return false;
        if (trimmed.size() >= 2 && trimmed[1] == '/') return false;
        if (trimmed.find("/>") != std::string::npos) return false;
        if (trimmed.find("</") != std::string::npos) return false;
        return web_extract_tag_name(trimmed).size() > 0;
    }

    static void web_copy_public_assets(const WebState& state, const std::filesystem::path& projectRoot) {
        const std::filesystem::path publicRoot = projectRoot / state.publicDir;
        if (!std::filesystem::exists(publicRoot) || !std::filesystem::is_directory(publicRoot)) {
            return;
        }

        const std::filesystem::path outputRoot = projectRoot / state.outputRoot;
        std::filesystem::create_directories(outputRoot);

        for (const auto& entry : std::filesystem::recursive_directory_iterator(publicRoot)) {
            if (!entry.is_regular_file()) continue;
            const std::filesystem::path relative = std::filesystem::relative(entry.path(), publicRoot);
            const std::filesystem::path target = outputRoot / relative;
            const auto parent = target.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }
            std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing);
        }
    }

    std::string web_render_document(const WebRoute& route) const {
        const std::string titleBlock = "<title>" + route.title + "</title>";
        const std::string styleBlock = "<style>" + webState_.css + "</style>";
        const std::string liveBlock = webState_.liveMode ? web_live_script() : "";
        const std::string headBlock = route.head;

        const std::filesystem::path templatePath = web_template_path();
        if (!templatePath.empty()) {
            std::string html = web_read_file_text(templatePath);
            web_replace_all(html, "{{ rq_title }}", route.title);
            web_replace_all(html, "<!-- rq-styles -->", styleBlock);
            web_replace_all(html, "<!-- rq-head -->", titleBlock + styleBlock + headBlock);
            web_replace_all(html, "<!-- rq-body -->", route.body);
            web_replace_all(html, "<!-- rq-live -->", liveBlock);

            if (html.find(titleBlock) == std::string::npos && html.find("</head>") != std::string::npos) {
                html.insert(html.find("</head>"), titleBlock + styleBlock + headBlock);
            }
            if (html.find(route.body) == std::string::npos && html.find("</body>") != std::string::npos) {
                html.insert(html.find("</body>"), route.body + liveBlock);
            }
            return html;
        }

        std::string html = "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
        html += titleBlock;
        html += styleBlock;
        html += headBlock;
        html += "</head><body>";
        html += route.body;
        html += liveBlock;
        html += "</body></html>";
        return html;
    }

    void web_write_output() const {
        web_copy_public_assets(webState_, projectRoot_);
        for (const auto& pair : webState_.routes) {
            const WebRoute& route = pair.second;
            const std::filesystem::path outPath = projectRoot_ / route.path;
            const auto parent = outPath.parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
            std::ofstream file(outPath, std::ios::binary | std::ios::trunc);
            file << web_render_document(route);
        }
    }

#ifdef _WIN32
    static std::string web_content_type_for(const std::filesystem::path& requested) {
        const std::string extension = requested.extension().string();
        if (extension == ".css") return "text/css; charset=utf-8";
        if (extension == ".js" || extension == ".mjs") return "application/javascript; charset=utf-8";
        if (extension == ".json") return "application/json; charset=utf-8";
        if (extension == ".svg") return "image/svg+xml";
        if (extension == ".png") return "image/png";
        if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
        if (extension == ".ico") return "image/x-icon";
        return "text/html; charset=utf-8";
    }

    static void web_send_http(SOCKET client, const std::string& status, const std::string& contentType, const std::string& body) {
        std::ostringstream response;
        response << "HTTP/1.1 " << status << "\r\n";
        response << "Content-Type: " << contentType << "\r\n";
        response << "Cache-Control: no-store, no-cache, must-revalidate\r\n";
        response << "Content-Length: " << body.size() << "\r\n";
        response << "Connection: close\r\n\r\n";
        response << body;
        const std::string payload = response.str();
        send(client, payload.c_str(), static_cast<int>(payload.size()), 0);
    }

    std::optional<std::filesystem::path> web_route_request_target(const std::string& requestPath) const {
        std::string path = requestPath;
        const size_t query = path.find('?');
        if (query != std::string::npos) path = path.substr(0, query);
        const std::string normalized = web_normalize_route(path);
        const auto found = webState_.routes.find(normalized);
        if (found == webState_.routes.end()) return std::nullopt;
        return projectRoot_ / found->second.path;
    }

    std::optional<std::filesystem::path> web_static_request_target(const std::string& requestPath) const {
        std::string path = requestPath;
        const size_t query = path.find('?');
        if (query != std::string::npos) path = path.substr(0, query);
        if (path.empty() || path == "/") return std::nullopt;
        std::string relative = path[0] == '/' ? path.substr(1) : path;
        if (relative.find("..") != std::string::npos) return std::nullopt;
        const std::filesystem::path requested = projectRoot_ / webState_.outputRoot / relative;
        if (std::filesystem::exists(requested) && std::filesystem::is_regular_file(requested)) {
            return requested;
        }
        return std::nullopt;
    }

    void web_handle_client(SOCKET client) const {
        char buffer[4096];
        const int received = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) return;
        buffer[received] = '\0';

        std::istringstream request(std::string(buffer, received));
        std::string method;
        std::string path;
        request >> method >> path;

        const std::filesystem::path versionPath = projectRoot_ / webState_.outputRoot / "index.html";
        if (path == "/__rq_version") {
            web_send_http(client, "200 OK", "text/plain; charset=utf-8", web_file_version(versionPath));
            return;
        }

        if (const auto routeTarget = web_route_request_target(path)) {
            web_send_http(client, "200 OK", "text/html; charset=utf-8", web_read_file_text(*routeTarget));
            return;
        }

        if (const auto staticTarget = web_static_request_target(path)) {
            web_send_http(client, "200 OK", web_content_type_for(*staticTarget), web_read_file_text(*staticTarget));
            return;
        }

        web_send_http(client, "404 Not Found", "text/plain; charset=utf-8", "Not Found");
    }

    void web_serve_forever() const {
#ifdef _WIN32
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            std::cerr << "[RayQuiro] Failed to start Winsock for web.live()." << std::endl;
            return;
        }
        SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server == INVALID_SOCKET) {
            WSACleanup();
            std::cerr << "[RayQuiro] Failed to create web server socket." << std::endl;
            return;
        }

        sockaddr_in address = {};
        address.sin_family = AF_INET;
        if (webState_.bindHost.empty() || webState_.bindHost == "0.0.0.0" || webState_.bindHost == "*") {
            address.sin_addr.s_addr = htonl(INADDR_ANY);
        } else if (inet_pton(AF_INET, webState_.bindHost.c_str(), &address.sin_addr) != 1) {
            std::cerr << "[RayQuiro] Invalid bind host: " << webState_.bindHost << std::endl;
            closesocket(server);
            WSACleanup();
            return;
        }
        address.sin_port = htons(static_cast<u_short>(webState_.livePort));

        if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            std::cerr << "[RayQuiro] Port " << webState_.livePort << " is busy." << std::endl;
            closesocket(server);
            WSACleanup();
            return;
        }

        if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "[RayQuiro] Failed to listen on port " << webState_.livePort << "." << std::endl;
            closesocket(server);
            WSACleanup();
            return;
        }

        std::cout << "[RayQuiro] Live web server: http://127.0.0.1:" << webState_.livePort << std::endl;
        std::cout << "[RayQuiro] Press Ctrl+C to stop." << std::endl;

        while (true) {
            SOCKET client = accept(server, nullptr, nullptr);
            if (client == INVALID_SOCKET) break;
            web_handle_client(client);
            closesocket(client);
        }

        closesocket(server);
        WSACleanup();
#else
        SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
        if (server == INVALID_SOCKET) {
            std::cerr << "[RayQuiro] Failed to create web server socket." << std::endl;
            return;
        }

        const int reuse = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address = {};
        address.sin_family = AF_INET;
        if (webState_.bindHost.empty() || webState_.bindHost == "0.0.0.0" || webState_.bindHost == "*") {
            address.sin_addr.s_addr = htonl(INADDR_ANY);
        } else if (inet_pton(AF_INET, webState_.bindHost.c_str(), &address.sin_addr) != 1) {
            std::cerr << "[RayQuiro] Invalid bind host: " << webState_.bindHost << std::endl;
            closesocket(server);
            return;
        }
        address.sin_port = htons(static_cast<u_short>(webState_.livePort));

        if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            std::cerr << "[RayQuiro] Port " << webState_.livePort << " is busy." << std::endl;
            closesocket(server);
            return;
        }

        if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "[RayQuiro] Failed to listen on port " << webState_.livePort << "." << std::endl;
            closesocket(server);
            return;
        }

        std::cout << "[RayQuiro] Live web server: http://127.0.0.1:" << webState_.livePort << std::endl;
        std::cout << "[RayQuiro] Press Ctrl+C to stop." << std::endl;

        while (true) {
            SOCKET client = accept(server, nullptr, nullptr);
            if (client == INVALID_SOCKET) break;
            web_handle_client(client);
            closesocket(client);
        }

        closesocket(server);
#endif
    }
#endif

    Value builtin_web_begin(const std::vector<Value>& args) {
        webState_ = WebState{};
        webState_.defaultTitle = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
        const std::string requestedPath = args.size() > 1 ? to_string(args[1]) : "build/index.html";
        webState_.css = web_default_css();
        webState_.outputRoot = std::filesystem::path(requestedPath).parent_path().empty()
            ? std::string("build")
            : std::filesystem::path(requestedPath).parent_path().string();
        webState_.currentRoute = "/";
        webState_.active = true;

        WebRoute route;
        route.route = "/";
        route.title = webState_.defaultTitle;
        route.path = requestedPath;
        webState_.routes["/"] = route;
        return Value();
    }

    Value builtin_web_route(const std::vector<Value>& args) {
        web_prepare_session();
        const std::string routeKey = web_normalize_route(args.empty() ? "/" : to_string(args[0]));
        WebRoute& route = webState_.routes[routeKey];
        if (route.route.empty()) route.route = routeKey;
        if (route.title.empty()) route.title = webState_.defaultTitle;
        if (route.path.empty()) route.path = web_route_to_output_path(webState_, routeKey);
        if (args.size() > 1) route.title = to_string(args[1]);
        if (args.size() > 2) route.path = to_string(args[2]);
        webState_.currentRoute = routeKey;
        return Value(route.path);
    }

    Value builtin_web_head(const std::vector<Value>& args) {
        web_prepare_session();
        if (!args.empty()) {
            web_current_route().head += to_string(args[0]);
        }
        return Value();
    }

    Value builtin_web_public(const std::vector<Value>& args) {
        web_prepare_session();
        webState_.publicDir = args.empty() ? "public" : to_string(args[0]);
        return Value(webState_.publicDir);
    }

    Value builtin_web_live(const std::vector<Value>& args) {
        web_prepare_session();
        webState_.liveMode = true;
        webState_.livePort = args.empty() ? 5274 : static_cast<int>(to_number(args[0]));
        if (webState_.livePort <= 0) webState_.livePort = 5274;
        const char* defaultBind = std::getenv("RQIO_WEB_HOST");
        webState_.bindHost = args.size() > 1 ? to_string(args[1]) : (defaultBind ? defaultBind : "127.0.0.1");
        return Value(static_cast<double>(webState_.livePort));
    }

    Value builtin_web_style(const std::vector<Value>& args) {
        web_prepare_session();
        if (!args.empty()) {
            if (!webState_.css.empty()) webState_.css += "\n";
            webState_.css += to_string(args[0]);
        }
        return Value();
    }

    Value builtin_web_open(const std::vector<Value>& args) {
        web_prepare_session();
        auto& route = web_current_route();
        const std::string first = args.size() > 0 ? to_string(args[0]) : "div";
        const std::string trimmed = web_trim(first);
        if (!trimmed.empty() && trimmed[0] == '<') {
            route.body += first;
            if (web_should_push_markup_tag(trimmed)) {
                web_push_tag(web_extract_tag_name(trimmed));
            }
            return Value();
        }

        const std::string className = args.size() > 1 ? to_string(args[1]) : "";
        route.body += "<" + first + web_class_attr(className) + ">";
        web_push_tag(first);
        return Value();
    }

    Value builtin_web_close(const std::vector<Value>& args) {
        web_prepare_session();
        auto& route = web_current_route();
        const std::string fallback = args.size() > 0 ? to_string(args[0]) : "div";
        route.body += "</" + web_pop_tag(fallback) + ">";
        return Value();
    }

    Value builtin_web_text(const std::vector<Value>& args) {
        web_prepare_session();
        auto& route = web_current_route();
        const std::string text = args.size() > 0 ? to_string(args[0]) : "";
        const std::string className = args.size() > 1 ? to_string(args[1]) : "";
        const std::string tag = args.size() > 2 ? to_string(args[2]) : "span";
        route.body += "<" + tag + web_class_attr(className) + ">" + text + "</" + tag + ">";
        return Value();
    }

    Value builtin_web_h1(const std::vector<Value>& args) {
        web_prepare_session();
        auto& route = web_current_route();
        const std::string text = args.size() > 0 ? to_string(args[0]) : "";
        const std::string className = args.size() > 1 ? to_string(args[1]) : "";
        route.body += "<h1" + web_class_attr(className) + ">" + text + "</h1>";
        return Value();
    }

    Value builtin_web_h2(const std::vector<Value>& args) {
        web_prepare_session();
        auto& route = web_current_route();
        const std::string text = args.size() > 0 ? to_string(args[0]) : "";
        const std::string className = args.size() > 1 ? to_string(args[1]) : "";
        route.body += "<h2" + web_class_attr(className) + ">" + text + "</h2>";
        return Value();
    }

    Value builtin_web_p(const std::vector<Value>& args) {
        web_prepare_session();
        auto& route = web_current_route();
        const std::string text = args.size() > 0 ? to_string(args[0]) : "";
        const std::string className = args.size() > 1 ? to_string(args[1]) : "";
        route.body += "<p" + web_class_attr(className) + ">" + text + "</p>";
        return Value();
    }

    Value builtin_web_button(const std::vector<Value>& args) {
        web_prepare_session();
        auto& route = web_current_route();
        const std::string label = args.size() > 0 ? to_string(args[0]) : "Button";
        const std::string className = args.size() > 1 ? to_string(args[1]) : "button";
        const std::string href = args.size() > 2 ? to_string(args[2]) : "#";
        route.body += "<a href=\"" + href + "\"" + web_class_attr(className) + ">" + label + "</a>";
        return Value();
    }

    Value builtin_web_raw(const std::vector<Value>& args) {
        web_prepare_session();
        if (!args.empty()) {
            web_current_route().body += to_string(args[0]);
        }
        return Value();
    }

    Value builtin_web_end(const std::vector<Value>& args) {
        (void)args;
        for (auto& pair : webState_.routes) {
            while (!pair.second.stack.empty()) {
                pair.second.body += "</" + pair.second.stack.back() + ">";
                pair.second.stack.pop_back();
            }
        }
        web_write_output();
        webState_.active = false;
        if (webState_.liveMode) {
#ifdef _WIN32
            web_serve_forever();
#else
            std::cout << "[RayQuiro] web.live() currently falls back to writing into " << webState_.outputRoot << " on this platform." << std::endl;
#endif
        }
        return Value();
    }

    Value builtin_web_page(const std::vector<Value>& args) {
        webState_ = WebState{};
        webState_.defaultTitle = args.size() > 0 ? to_string(args[0]) : "RayQuiro";
        const std::string requestedPath = args.size() > 2 ? to_string(args[2]) : "build/index.html";
        webState_.css = args.size() > 3 ? to_string(args[3]) : web_default_css();
        webState_.outputRoot = std::filesystem::path(requestedPath).parent_path().empty()
            ? std::string("build")
            : std::filesystem::path(requestedPath).parent_path().string();
        webState_.currentRoute = "/";
        WebRoute route;
        route.route = "/";
        route.title = webState_.defaultTitle;
        route.path = requestedPath;
        route.body = args.size() > 1 ? to_string(args[1]) : "";
        webState_.routes["/"] = route;
        web_write_output();
        return Value();
    }

    static std::vector<Value> expect_array_items(const Value& value, const std::string& what) {
        if (!is_array(value)) {
            throw std::runtime_error(what + " expects an array value.");
        }
        return *value.as_array();
    }

    static RTColor value_to_color(const Value& value) {
        const std::vector<Value> items = expect_array_items(value, "Color");
        if (items.size() < 4) {
            throw std::runtime_error("Color expects [r, g, b, a].");
        }
        return RTColor{
            static_cast<unsigned char>(to_number(items[0])),
            static_cast<unsigned char>(to_number(items[1])),
            static_cast<unsigned char>(to_number(items[2])),
            static_cast<unsigned char>(to_number(items[3]))
        };
    }

    static RTVec2 value_to_vec2(const Value& value) {
        const std::vector<Value> items = expect_array_items(value, "Vector2");
        if (items.size() < 2) {
            throw std::runtime_error("Vector2 expects [x, y].");
        }
        return RTVec2{
            static_cast<float>(to_number(items[0])),
            static_cast<float>(to_number(items[1]))
        };
    }

    static RTVec3 value_to_vec3(const Value& value) {
        const std::vector<Value> items = expect_array_items(value, "Vector3");
        if (items.size() < 3) {
            throw std::runtime_error("Vector3 expects [x, y, z].");
        }
        return RTVec3{
            static_cast<float>(to_number(items[0])),
            static_cast<float>(to_number(items[1])),
            static_cast<float>(to_number(items[2]))
        };
    }

    static Value builtin_engine_init(const std::vector<Value>& args) {
        const int width = args.size() > 0 ? static_cast<int>(to_number(args[0])) : 1280;
        const int height = args.size() > 1 ? static_cast<int>(to_number(args[1])) : 720;
        const std::string title = args.size() > 2 ? to_string(args[2]) : "RayQuiro";
        rt_init(width, height, title.c_str());
        return Value();
    }

    static Value builtin_engine_shutdown(const std::vector<Value>& args) {
        (void)args;
        rt_shutdown();
        return Value();
    }

    static Value builtin_engine_should_close(const std::vector<Value>& args) {
        (void)args;
        return Value(rt_should_close() != 0);
    }

    static Value builtin_engine_begin(const std::vector<Value>& args) {
        (void)args;
        rt_begin();
        return Value();
    }

    static Value builtin_engine_end(const std::vector<Value>& args) {
        (void)args;
        rt_end();
        return Value();
    }

    static Value builtin_engine_clear(const std::vector<Value>& args) {
        if (args.empty()) return Value();
        rt_clear(value_to_color(args[0]));
        return Value();
    }

    static Value builtin_engine_set_camera(const std::vector<Value>& args) {
        if (args.size() < 4) return Value();
        const RTVec3 position = value_to_vec3(args[0]);
        const RTVec3 target = value_to_vec3(args[1]);
        const RTVec3 up = value_to_vec3(args[2]);
        const float fov = static_cast<float>(to_number(args[3]));
        rt_set_camera(position.x, position.y, position.z, target.x, target.y, target.z, up.x, up.y, up.z, fov);
        return Value();
    }

    static Value builtin_engine_target_fps(const std::vector<Value>& args) {
        if (args.empty()) return Value();
        rt_set_target_fps(static_cast<int>(to_number(args[0])));
        return Value();
    }

    static Value builtin_engine_frame_time(const std::vector<Value>& args) {
        (void)args;
        return Value(static_cast<double>(rt_get_frame_time()));
    }

    Value builtin_engine_backend(const std::vector<Value>& args) {
        const std::string backend = args.empty() ? "raylib" : to_string(args[0]);
        rt_set_backend(backend.c_str());
        return Value(backend);
    }

    static Value builtin_engine_backend_name(const std::vector<Value>& args) {
        (void)args;
        return Value(std::string(rt_backend_name()));
    }

    static Value builtin_engine_backend_info(const std::vector<Value>& args) {
        (void)args;
        Value result = Value::object();
        auto out = result.as_object();
        (*out)["requested"] = Value(std::string(rt_backend_requested_name()));
        (*out)["active"] = Value(std::string(rt_backend_name()));
        (*out)["supports3d"] = Value(rt_backend_supports_3d() != 0);
        (*out)["available"] = Value(rt_backend_is_available() != 0);
        (*out)["placeholder"] = Value(rt_backend_is_placeholder() != 0);
        (*out)["vulkan_family"] = Value(rt_backend_is_vulkan_family() != 0);
        (*out)["gpu_count"] = Value(static_cast<double>(rt_backend_gpu_count()));
        (*out)["surface_ready"] = Value(rt_backend_surface_ready() != 0);
        (*out)["device_ready"] = Value(rt_backend_device_ready() != 0);
        (*out)["presentation_ready"] = Value(rt_backend_presentation_ready() != 0);
        (*out)["queue_family"] = Value(static_cast<double>(rt_backend_queue_family_index()));
        (*out)["swapchain_ready"] = Value(rt_backend_swapchain_ready() != 0);
        (*out)["swapchain_images"] = Value(static_cast<double>(rt_backend_swapchain_image_count()));
        (*out)["swapchain_width"] = Value(static_cast<double>(rt_backend_swapchain_width()));
        (*out)["swapchain_height"] = Value(static_cast<double>(rt_backend_swapchain_height()));
        (*out)["render_pass_ready"] = Value(rt_backend_render_pass_ready() != 0);
        (*out)["framebuffer_count"] = Value(static_cast<double>(rt_backend_framebuffer_count()));
        (*out)["depth_ready"] = Value(rt_backend_depth_ready() != 0);
        (*out)["geometry_buffers_ready"] = Value(rt_backend_geometry_buffers_ready() != 0);
        (*out)["vertex_buffer_bytes"] = Value(static_cast<double>(rt_backend_vertex_buffer_bytes()));
        (*out)["index_buffer_bytes"] = Value(static_cast<double>(rt_backend_index_buffer_bytes()));
        (*out)["shader_assets_ready"] = Value(rt_backend_shader_assets_ready() != 0);
        (*out)["shader_modules_ready"] = Value(rt_backend_shader_modules_ready() != 0);
        (*out)["pipeline_layout_ready"] = Value(rt_backend_pipeline_layout_ready() != 0);
        (*out)["texture_sampler_ready"] = Value(rt_backend_texture_sampler_ready() != 0);
        (*out)["texture_image_ready"] = Value(rt_backend_texture_image_ready() != 0);
        (*out)["descriptor_set_ready"] = Value(rt_backend_descriptor_set_ready() != 0);
        (*out)["graphics_pipeline_ready"] = Value(rt_backend_graphics_pipeline_ready() != 0);
        (*out)["command_pool_ready"] = Value(rt_backend_command_pool_ready() != 0);
        (*out)["command_buffer_count"] = Value(static_cast<double>(rt_backend_command_buffer_count()));
        (*out)["sync_ready"] = Value(rt_backend_sync_ready() != 0);
        (*out)["frame_path_ready"] = Value(rt_backend_frame_path_ready() != 0);
        (*out)["frame_acquired"] = Value(rt_backend_frame_acquired() != 0);
        (*out)["presented_frames"] = Value(static_cast<double>(rt_backend_presented_frame_count()));
        return result;
    }

    static Value builtin_engine_vsync(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(rt_get_vsync() != 0);
        }
        rt_set_vsync(truthy(args[0]) ? 1 : 0);
        return Value(rt_get_vsync() != 0);
    }

    static Value builtin_engine_msaa(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(static_cast<double>(rt_get_msaa()));
        }
        rt_set_msaa(static_cast<int>(to_number(args[0])));
        return Value(static_cast<double>(rt_get_msaa()));
    }

    Value builtin_engine_exposure(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.exposure", args)) return *overrideResult;
        return args.empty() ? Value(1.0) : Value(to_number(args[0]));
    }

    Value builtin_engine_vignette(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.vignette", args)) return *overrideResult;
        return args.empty() ? Value(0.08) : Value(to_number(args[0]));
    }

    Value builtin_engine_film_grain(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.film_grain", args)) return *overrideResult;
        return args.empty() ? Value(0.02) : Value(to_number(args[0]));
    }

    Value builtin_engine_saturation(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.saturation", args)) return *overrideResult;
        return args.empty() ? Value(1.0) : Value(to_number(args[0]));
    }

    Value builtin_engine_contrast(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.contrast", args)) return *overrideResult;
        return args.empty() ? Value(1.0) : Value(to_number(args[0]));
    }

    Value builtin_engine_bloom(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.bloom", args)) return *overrideResult;
        return args.empty() ? Value(0.0) : Value(to_number(args[0]));
    }

    Value builtin_engine_fog(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.fog", args)) return *overrideResult;
        (void)args;
        return Value();
    }

    Value builtin_engine_volumetric(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.volumetric", args)) return *overrideResult;
        return args.empty() ? Value(0.0) : Value(to_number(args[0]));
    }

    Value builtin_engine_postfx_info(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.postfx_info", args)) return *overrideResult;
        (void)args;
        return Value::object();
    }

    Value builtin_engine_scene_watch(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.scene_watch", args)) return *overrideResult;
        (void)args;
        return Value(false);
    }

    Value builtin_engine_scene_reload(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.scene_reload", args)) return *overrideResult;
        (void)args;
        return Value(false);
    }

    Value builtin_engine_stats(const std::vector<Value>& args) {
        (void)args;
        auto& scenes = engineState_.scenes;
        auto& scene = engine_current_scene();
        Value result = Value::object();
        auto out = result.as_object();
        (*out)["backend"] = Value(std::string(rt_backend_name()));
        (*out)["current_scene"] = Value(engineState_.currentScene);
        (*out)["scene_count"] = Value(static_cast<double>(scenes.size()));
        (*out)["entity_count"] = Value(static_cast<double>(scene.entities.size()));
        (*out)["material_count"] = Value(static_cast<double>(scene.materials.size()));
        (*out)["mesh_count"] = Value(static_cast<double>(scene.meshes.size()));
        (*out)["texture_count"] = Value(static_cast<double>(scene.textures.size()));
        (*out)["target_fps"] = Value(static_cast<double>(rt_get_target_fps()));
        (*out)["vsync"] = Value(rt_get_vsync() != 0);
        (*out)["msaa"] = Value(static_cast<double>(rt_get_msaa()));
        (*out)["camera_fov"] = Value(static_cast<double>(rt_get_camera_fov()));
        (*out)["draw_calls"] = Value(static_cast<double>(rt_get_draw_calls()));
        (*out)["render_items"] = Value(static_cast<double>(rt_get_render_items()));
        (*out)["gpu_count"] = Value(static_cast<double>(rt_backend_gpu_count()));
        return result;
    }

    static Value builtin_engine_camera_fov(const std::vector<Value>& args) {
        if (!args.empty()) {
            rt_set_camera_fov(static_cast<float>(to_number(args[0])));
        }
        return Value(static_cast<double>(rt_get_camera_fov()));
    }

    static Value builtin_engine_camera_orbit(const std::vector<Value>& args) {
        const float yaw = args.size() > 0 ? static_cast<float>(to_number(args[0])) : 0.0f;
        const float pitch = args.size() > 1 ? static_cast<float>(to_number(args[1])) : 0.0f;
        const float radiusDelta = args.size() > 2 ? static_cast<float>(to_number(args[2])) : 0.0f;
        rt_camera_orbit(yaw, pitch, radiusDelta);
        return Value();
    }

    static Value builtin_engine_key_down(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        return Value(rt_key_down(static_cast<int>(to_number(args[0]))) != 0);
    }

    static Value builtin_engine_key_pressed(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        return Value(rt_key_pressed(static_cast<int>(to_number(args[0]))) != 0);
    }

    static Value builtin_engine_mouse_down(const std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        return Value(rt_mouse_down(static_cast<int>(to_number(args[0]))) != 0);
    }

    static Value builtin_engine_mouse_pos(const std::vector<Value>& args) {
        (void)args;
        return array({Value(static_cast<double>(rt_mouse_x())), Value(static_cast<double>(rt_mouse_y()))});
    }

    static Value builtin_engine_draw_grid(const std::vector<Value>& args) {
        if (args.size() < 2) return Value();
        rt_draw_grid(static_cast<int>(to_number(args[0])), static_cast<float>(to_number(args[1])));
        return Value();
    }

    static Value builtin_engine_draw_cube(const std::vector<Value>& args) {
        if (args.size() < 3) return Value();
        rt_draw_cube(value_to_vec3(args[0]), value_to_vec3(args[1]), value_to_color(args[2]));
        return Value();
    }

    static Value builtin_engine_draw_plane(const std::vector<Value>& args) {
        if (args.size() < 3) return Value();
        rt_draw_plane(value_to_vec3(args[0]), value_to_vec2(args[1]), value_to_color(args[2]));
        return Value();
    }

    static Value builtin_engine_draw_sphere(const std::vector<Value>& args) {
        if (args.size() < 3) return Value();
        rt_draw_sphere(value_to_vec3(args[0]), static_cast<float>(to_number(args[1])), value_to_color(args[2]));
        return Value();
    }

    static Value builtin_engine_draw_text(const std::vector<Value>& args) {
        if (args.size() < 5) return Value();
        const std::string text = to_string(args[0]);
        rt_draw_text(
            text.c_str(),
            static_cast<int>(to_number(args[1])),
            static_cast<int>(to_number(args[2])),
            static_cast<int>(to_number(args[3])),
            value_to_color(args[4]));
        return Value();
    }

    static Value builtin_engine_draw_fps(const std::vector<Value>& args) {
        if (args.size() < 2) return Value();
        rt_draw_fps(static_cast<int>(to_number(args[0])), static_cast<int>(to_number(args[1])));
        return Value();
    }

    static Value builtin_engine_draw_rect(const std::vector<Value>& args) {
        if (args.size() < 5) return Value();
        rt_draw_rect(static_cast<int>(to_number(args[0])), static_cast<int>(to_number(args[1])),
                     static_cast<int>(to_number(args[2])), static_cast<int>(to_number(args[3])),
                     value_to_color(args[4]));
        return Value();
    }

    static Value builtin_engine_draw_rect_lines(const std::vector<Value>& args) {
        if (args.size() < 5) return Value();
        rt_draw_rect_lines(static_cast<int>(to_number(args[0])), static_cast<int>(to_number(args[1])),
                           static_cast<int>(to_number(args[2])), static_cast<int>(to_number(args[3])),
                           value_to_color(args[4]));
        return Value();
    }

    static Value builtin_engine_draw_circle(const std::vector<Value>& args) {
        if (args.size() < 4) return Value();
        rt_draw_circle(static_cast<int>(to_number(args[0])), static_cast<int>(to_number(args[1])),
                       static_cast<float>(to_number(args[2])), value_to_color(args[3]));
        return Value();
    }

    static Value builtin_engine_draw_circle_lines(const std::vector<Value>& args) {
        if (args.size() < 4) return Value();
        rt_draw_circle_lines(static_cast<int>(to_number(args[0])), static_cast<int>(to_number(args[1])),
                             static_cast<float>(to_number(args[2])), value_to_color(args[3]));
        return Value();
    }

    static Value builtin_engine_draw_line(const std::vector<Value>& args) {
        if (args.size() < 5) return Value();
        rt_draw_line(static_cast<int>(to_number(args[0])), static_cast<int>(to_number(args[1])),
                     static_cast<int>(to_number(args[2])), static_cast<int>(to_number(args[3])),
                     value_to_color(args[4]));
        return Value();
    }

    static Value builtin_engine_draw_pixel(const std::vector<Value>& args) {
        if (args.size() < 3) return Value();
        rt_draw_pixel(static_cast<int>(to_number(args[0])), static_cast<int>(to_number(args[1])),
                      value_to_color(args[2]));
        return Value();
    }

    static Value builtin_engine_rect_overlap(const std::vector<Value>& args) {
        if (args.size() < 8) return Value(false);
        const double x1 = to_number(args[0]), y1 = to_number(args[1]);
        const double w1 = to_number(args[2]), h1 = to_number(args[3]);
        const double x2 = to_number(args[4]), y2 = to_number(args[5]);
        const double w2 = to_number(args[6]), h2 = to_number(args[7]);
        const bool overlap = (x1 < x2 + w2) && (x1 + w1 > x2) && (y1 < y2 + h2) && (y1 + h1 > y2);
        return Value(overlap);
    }

    static Value builtin_engine_circle_overlap(const std::vector<Value>& args) {
        if (args.size() < 6) return Value(false);
        const double dx = to_number(args[0]) - to_number(args[3]);
        const double dy = to_number(args[1]) - to_number(args[4]);
        const double rSum = to_number(args[2]) + to_number(args[5]);
        return Value((dx * dx + dy * dy) < (rSum * rSum));
    }

    static Value builtin_engine_point_in_rect(const std::vector<Value>& args) {
        if (args.size() < 6) return Value(false);
        const double px = to_number(args[0]), py = to_number(args[1]);
        const double rx = to_number(args[2]), ry = to_number(args[3]);
        const double rw = to_number(args[4]), rh = to_number(args[5]);
        return Value(px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
    }

    static unsigned char engine_clamp_channel(float value) {
        if (value < 0.0f) return 0;
        if (value > 255.0f) return 255;
        return static_cast<unsigned char>(value);
    }

    static RTColor engine_apply_light(RTColor base, RTColor ambient, const std::optional<EngineLight>& light) {
        const float ambientFactor = 0.45f;
        const float lightFactor = light.has_value() ? std::max(0.0f, light->intensity) : 1.0f;
        const float sunMix = light.has_value() ? 0.55f : 0.35f;
        const RTColor lightColor = light.has_value() ? light->color : RTColor{255, 255, 255, 255};

        const float mixedR = base.r * (ambientFactor + sunMix * lightFactor * (lightColor.r / 255.0f));
        const float mixedG = base.g * (ambientFactor + sunMix * lightFactor * (lightColor.g / 255.0f));
        const float mixedB = base.b * (ambientFactor + sunMix * lightFactor * (lightColor.b / 255.0f));

        return RTColor{
            engine_clamp_channel(mixedR * 0.7f + ambient.r * 0.3f),
            engine_clamp_channel(mixedG * 0.7f + ambient.g * 0.3f),
            engine_clamp_channel(mixedB * 0.7f + ambient.b * 0.3f),
            base.a
        };
    }

    static RTColor engine_mix_color(RTColor base, RTColor overlay, float factor) {
        factor = std::clamp(factor, 0.0f, 1.0f);
        return RTColor{
            engine_clamp_channel(base.r * (1.0f - factor) + overlay.r * factor),
            engine_clamp_channel(base.g * (1.0f - factor) + overlay.g * factor),
            engine_clamp_channel(base.b * (1.0f - factor) + overlay.b * factor),
            base.a
        };
    }

    static RTVec3 engine_mul_vec3(RTVec3 value, RTVec3 scale) {
        return RTVec3{value.x * scale.x, value.y * scale.y, value.z * scale.z};
    }

    EngineScene& engine_current_scene() {
        auto found = engineState_.scenes.find(engineState_.currentScene);
        if (found == engineState_.scenes.end()) {
            found = engineState_.scenes.emplace(engineState_.currentScene, EngineScene{}).first;
        }
        return found->second;
    }

    RTVec3 engine_size_vec3(const Value& value, const RTVec3& fallback = RTVec3{1.0f, 1.0f, 1.0f}) const {
        if (!is_array(value)) {
            return fallback;
        }
        const auto items = value.as_array();
        if (items->size() < 3) {
            return fallback;
        }
        return RTVec3{
            static_cast<float>(to_number((*items)[0])),
            static_cast<float>(to_number((*items)[1])),
            static_cast<float>(to_number((*items)[2]))
        };
    }

    RTVec2 engine_size_vec2(const Value& value, const RTVec2& fallback = RTVec2{4.0f, 4.0f}) const {
        if (!is_array(value)) {
            return fallback;
        }
        const auto items = value.as_array();
        if (items->size() < 2) {
            return fallback;
        }
        return RTVec2{
            static_cast<float>(to_number((*items)[0])),
            static_cast<float>(to_number((*items)[1]))
        };
    }

    float engine_size_radius(const Value& value, float fallback = 1.0f) const {
        if (is_array(value)) {
            const auto items = value.as_array();
            if (!items->empty()) {
                return static_cast<float>(to_number((*items)[0]));
            }
        }
        if (is_number(value) || is_string(value) || is_bool(value)) {
            return static_cast<float>(to_number(value));
        }
        return fallback;
    }

    EngineMaterial engine_material_from_args(const std::vector<Value>& args, size_t colorIndex) const {
        EngineMaterial material;
        if (args.size() > colorIndex) {
            material.albedo = value_to_color(args[colorIndex]);
        }
        if (args.size() > colorIndex + 1) {
            material.roughness = static_cast<float>(to_number(args[colorIndex + 1]));
        }
        if (args.size() > colorIndex + 2) {
            material.metallic = static_cast<float>(to_number(args[colorIndex + 2]));
        }
        if (args.size() > colorIndex + 3) {
            material.emissive = value_to_color(args[colorIndex + 3]);
        }
        return material;
    }

    EngineMesh engine_mesh_from_args(const std::vector<Value>& args, size_t startIndex) const {
        EngineMesh mesh;
        if (args.size() > startIndex) {
            mesh.primitive = to_string(args[startIndex]);
        }
        if (args.size() > startIndex + 1) {
            if (is_string(args[startIndex + 1])) {
                mesh.source = to_string(args[startIndex + 1]);
            } else if (is_array(args[startIndex + 1])) {
                mesh.defaultSize = engine_size_vec3(args[startIndex + 1], mesh.defaultSize);
                mesh.defaultPlaneSize = engine_size_vec2(args[startIndex + 1], mesh.defaultPlaneSize);
                mesh.defaultRadius = engine_size_radius(args[startIndex + 1], mesh.defaultRadius);
            }
        }
        if (args.size() > startIndex + 2 && is_array(args[startIndex + 2])) {
            mesh.defaultSize = engine_size_vec3(args[startIndex + 2], mesh.defaultSize);
            mesh.defaultPlaneSize = engine_size_vec2(args[startIndex + 2], mesh.defaultPlaneSize);
            mesh.defaultRadius = engine_size_radius(args[startIndex + 2], mesh.defaultRadius);
        }
        return mesh;
    }

    EngineTexture engine_texture_from_args(const std::vector<Value>& args, size_t startIndex) const {
        EngineTexture texture;
        if (args.size() > startIndex) {
            texture.source = to_string(args[startIndex]);
        }
        if (args.size() > startIndex + 1) {
            texture.srgb = truthy(args[startIndex + 1]);
        }
        if (args.size() > startIndex + 2) {
            texture.normalMap = truthy(args[startIndex + 2]);
        }
        return texture;
    }

    Value builtin_engine_window(const std::vector<Value>& args) {
        return builtin_engine_init(args);
    }

    Value builtin_engine_camera(const std::vector<Value>& args) {
        return builtin_engine_set_camera(args);
    }

    Value builtin_engine_frame_begin(const std::vector<Value>& args) {
        builtin_engine_begin({});
        if (!args.empty()) {
            builtin_engine_clear({args[0]});
        }
        return Value();
    }

    Value builtin_engine_frame_end(const std::vector<Value>& args) {
        (void)args;
        return builtin_engine_end({});
    }

    Value builtin_engine_scene(const std::vector<Value>& args) {
        engineState_.currentScene = args.empty() ? "main" : to_string(args[0]);
        engine_current_scene();
        return Value(engineState_.currentScene);
    }

    Value builtin_engine_scene_clear(const std::vector<Value>& args) {
        (void)args;
        auto& scene = engine_current_scene();
        scene.entities.clear();
        scene.materials.clear();
        scene.meshes.clear();
        scene.textures.clear();
        scene.hasSun = false;
        return Value();
    }

    Value builtin_engine_scene_stats(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.scene_stats", args)) return *overrideResult;
        return Value();
    }

    Value builtin_engine_export_scene(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.export_scene", args)) return *overrideResult;
        return Value();
    }

    Value builtin_engine_scene_save(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.scene_save", args)) return *overrideResult;
        return Value();
    }

    Value builtin_engine_scene_load(const std::vector<Value>& args) {
        if (const auto overrideResult = tryNativeModuleCall("engine.scene_load", args)) return *overrideResult;
        return Value();
    }

    Value builtin_engine_entity(const std::vector<Value>& args) {
        if (args.size() < 5) {
            return Value();
        }

        EngineEntity entity;
        entity.kind = to_string(args[1]);
        entity.position = value_to_vec3(args[2]);
        entity.color = value_to_color(args[4]);

        if (entity.kind == "plane") {
            entity.planeSize = engine_size_vec2(args[3]);
        } else if (entity.kind == "sphere") {
            entity.radius = engine_size_radius(args[3]);
        } else {
            entity.size = engine_size_vec3(args[3]);
        }

        if (args.size() > 5) {
            entity.material = to_string(args[5]);
        }
        if (args.size() > 6) {
            entity.mesh = to_string(args[6]);
        }
        if (args.size() > 7) {
            entity.texture = to_string(args[7]);
        }

        engine_current_scene().entities[to_string(args[0])] = entity;
        return Value();
    }

    Value builtin_engine_entity_remove(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(false);
        }

        auto& scene = engine_current_scene();
        return Value(scene.entities.erase(to_string(args[0])) > 0);
    }

    Value builtin_engine_entity_exists(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        return Value(scene.entities.find(to_string(args[0])) != scene.entities.end());
    }

    Value builtin_engine_entity_set_position(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        found->second.position = value_to_vec3(args[1]);
        return Value(true);
    }

    Value builtin_engine_entity_get_position(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value();
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value();
        }
        return array({
            Value(static_cast<double>(found->second.position.x)),
            Value(static_cast<double>(found->second.position.y)),
            Value(static_cast<double>(found->second.position.z))
        });
    }

    Value builtin_engine_entity_set_size(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        if (found->second.kind == "plane") {
            found->second.planeSize = engine_size_vec2(args[1]);
        } else {
            found->second.size = engine_size_vec3(args[1]);
        }
        return Value(true);
    }

    Value builtin_engine_entity_set_scale(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        found->second.scale = engine_size_vec3(args[1], found->second.scale);
        return Value(true);
    }

    Value builtin_engine_entity_set_radius(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        found->second.radius = engine_size_radius(args[1], found->second.radius);
        return Value(true);
    }

    Value builtin_engine_entity_set_color(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        found->second.color = value_to_color(args[1]);
        return Value(true);
    }

    Value builtin_engine_entity_set_visible(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        found->second.visible = truthy(args[1]);
        return Value(true);
    }

    Value builtin_engine_entity_mesh(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        found->second.mesh = to_string(args[1]);
        return Value(true);
    }

    Value builtin_engine_entity_texture(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        found->second.texture = to_string(args[1]);
        return Value(true);
    }

    Value builtin_engine_mesh(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        scene.meshes[to_string(args[0])] = engine_mesh_from_args(args, 1);
        return Value(true);
    }

    Value builtin_engine_mesh_exists(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        return Value(scene.meshes.find(to_string(args[0])) != scene.meshes.end());
    }

    Value builtin_engine_texture(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        scene.textures[to_string(args[0])] = engine_texture_from_args(args, 1);
        return Value(true);
    }

    Value builtin_engine_texture_exists(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        return Value(scene.textures.find(to_string(args[0])) != scene.textures.end());
    }

    Value builtin_engine_material(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value();
        }
        auto& scene = engine_current_scene();
        scene.materials[to_string(args[0])] = engine_material_from_args(args, 1);
        return Value(true);
    }

    Value builtin_engine_material_exists(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        return Value(scene.materials.find(to_string(args[0])) != scene.materials.end());
    }

    Value builtin_engine_material_texture(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.materials.find(to_string(args[0]));
        if (found == scene.materials.end()) {
            return Value(false);
        }
        found->second.texture = to_string(args[1]);
        if (args.size() > 2) {
            found->second.normalTexture = to_string(args[2]);
        }
        return Value(true);
    }

    Value builtin_engine_entity_material(const std::vector<Value>& args) {
        if (args.size() < 2) {
            return Value(false);
        }
        auto& scene = engine_current_scene();
        const auto found = scene.entities.find(to_string(args[0]));
        if (found == scene.entities.end()) {
            return Value(false);
        }
        found->second.material = to_string(args[1]);
        return Value(true);
    }

    Value builtin_engine_light_ambient(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value();
        }
        engine_current_scene().ambient = value_to_color(args[0]);
        return Value();
    }

    Value builtin_engine_light_directional(const std::vector<Value>& args) {
        if (args.size() < 3) {
            return Value();
        }
        auto& scene = engine_current_scene();
        scene.sun.direction = value_to_vec3(args[0]);
        scene.sun.color = value_to_color(args[1]);
        scene.sun.intensity = static_cast<float>(to_number(args[2]));
        scene.hasSun = true;
        return Value();
    }

    Value builtin_engine_scene_draw(const std::vector<Value>& args) {
        (void)args;
        auto& scene = engine_current_scene();
        const std::optional<EngineLight> light = scene.hasSun ? std::optional<EngineLight>(scene.sun) : std::nullopt;
        int submittedItems = 0;

        for (const auto& pair : scene.entities) {
            const EngineEntity& entity = pair.second;
            if (!entity.visible) {
                continue;
            }

            std::string drawKind = entity.kind;
            RTVec3 drawSize = entity.size;
            RTVec2 drawPlaneSize = entity.planeSize;
            float drawRadius = entity.radius;
            if (!entity.mesh.empty()) {
                const auto mesh = scene.meshes.find(entity.mesh);
                if (mesh != scene.meshes.end()) {
                    if (!mesh->second.primitive.empty()) {
                        drawKind = mesh->second.primitive;
                    }
                    drawSize = mesh->second.defaultSize;
                    drawPlaneSize = mesh->second.defaultPlaneSize;
                    drawRadius = mesh->second.defaultRadius;
                }
            }

            RTColor baseColor = entity.color;
            const auto material = scene.materials.find(entity.material);
            if (material != scene.materials.end()) {
                baseColor = material->second.albedo;
                baseColor = engine_mix_color(baseColor, material->second.emissive, 0.2f);
            }
            submittedItems += 1;

        const RTColor shaded = engine_apply_light(baseColor, scene.ambient, light);
        if (drawKind == "plane") {
            rt_draw_plane(entity.position, RTVec2{drawPlaneSize.x * entity.scale.x, drawPlaneSize.y * entity.scale.z}, shaded);
        } else if (drawKind == "sphere") {
            const float radiusScale = std::max(entity.scale.x, std::max(entity.scale.y, entity.scale.z));
            rt_draw_sphere(entity.position, drawRadius * radiusScale, shaded);
        } else {
            rt_draw_cube(entity.position, engine_mul_vec3(drawSize, entity.scale), shaded);
        }
    }
        rt_set_render_items(submittedItems);
    return Value();
    }

    Value builtin_engine_assets_root(const std::vector<Value>& args) {
        if (!args.empty()) {
            engineState_.assetsRoot = to_string(args[0]);
        }
        return Value(engineState_.assetsRoot);
    }

    Value builtin_engine_asset_path(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(engineState_.assetsRoot);
        }
        const std::filesystem::path assetPath = projectRoot_ / engineState_.assetsRoot / to_string(args[0]);
        return Value(assetPath.lexically_normal().string());
    }

    Value builtin_engine_asset_exists(const std::vector<Value>& args) {
        if (args.empty()) {
            return Value(false);
        }
        const std::filesystem::path assetPath = projectRoot_ / engineState_.assetsRoot / to_string(args[0]);
        return Value(std::filesystem::exists(assetPath));
    }
};

