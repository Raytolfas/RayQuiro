#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "AST.h"
#include "UserPaths.h"
#include "rte_api.h"

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
using SOCKET = int;
static constexpr SOCKET INVALID_SOCKET = -1;
static constexpr int SOCKET_ERROR = -1;
inline int closesocket(SOCKET socket) { return ::close(socket); }
#endif

class Interpreter {
public:
    struct Value {
        using Arr = std::shared_ptr<std::vector<Value>>;
        using Obj = std::shared_ptr<std::unordered_map<std::string, Value>>;

        std::variant<std::monostate, double, std::string, bool, Arr, Obj> data;

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
        Arr as_array() const { return std::get<Arr>(data); }
        Obj as_object() const { return std::get<Obj>(data); }
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
#ifdef _WIN32
    RayQuiroApp appRuntime_;
    RayQuiroModernUI uiRuntime_;
#endif

    static bool is_null(const Value& value) { return std::holds_alternative<std::monostate>(value.data); }
    static bool is_number(const Value& value) { return std::holds_alternative<double>(value.data); }
    static bool is_string(const Value& value) { return std::holds_alternative<std::string>(value.data); }
    static bool is_bool(const Value& value) { return std::holds_alternative<bool>(value.data); }
    static bool is_array(const Value& value) { return std::holds_alternative<Value::Arr>(value.data); }
    static bool is_object(const Value& value) { return std::holds_alternative<Value::Obj>(value.data); }

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
        return "";
    }

    static bool truthy(const Value& value) {
        if (is_null(value)) return false;
        if (is_bool(value)) return std::get<bool>(value.data);
        if (is_number(value)) return std::get<double>(value.data) != 0.0;
        if (is_string(value)) return !std::get<std::string>(value.data).empty();
        if (is_array(value)) return !std::get<Value::Arr>(value.data)->empty();
        if (is_object(value)) return !std::get<Value::Obj>(value.data)->empty();
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
        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) return supportsExpr(returnStmt->value.get());
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

        if (auto assignExpr = dynamic_cast<AssignExpr*>(expr)) {
            Value value = evaluate(assignExpr->value.get(), env);
            env->assign(assignExpr->name, value);
            return value;
        }

        throw std::runtime_error("Unsupported expression encountered in interpreter.");
    }

    void execute(Stmt* stmt, const std::shared_ptr<Environment>& env) {
        if (!stmt) return;

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
            throw ReturnSignal{evaluate(returnStmt->value.get(), env)};
        }

        if (dynamic_cast<BreakStmt*>(stmt) != nullptr) {
            throw BreakSignal{};
        }

        if (dynamic_cast<ContinueStmt*>(stmt) != nullptr) {
            throw ContinueSignal{};
        }

        throw std::runtime_error("Unsupported statement encountered in interpreter.");
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
            throw std::runtime_error("Unknown function: " + callee);
        }

        FunctionStmt* function = found->second;
        auto frame = std::make_shared<Environment>(globals_);
        for (size_t i = 0; i < function->params.size(); ++i) {
            frame->define(function->params[i], i < args.size() ? args[i] : Value(), false);
        }

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
        if (builtin == "len") return builtin_len(args);
        if (builtin == "str") return builtin_str(args);
        if (builtin == "num") return builtin_num(args);
        if (builtin == "bool") return builtin_bool(args);
        if (builtin == "type") return builtin_type(args);
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
        return std::nullopt;
    }

    static int size_of(const Value& value) {
        if (is_string(value)) return static_cast<int>(std::get<std::string>(value.data).size());
        if (is_array(value)) return static_cast<int>(value.as_array()->size());
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
