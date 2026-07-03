#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST.h"
#include "Bytecode.h"

class BytecodeCompiler {
public:
    struct CompileContext {
        std::unordered_map<std::string, std::string> builtinNamespaceAliases;
        std::unordered_map<std::string, std::string> builtinSymbolAliases;
    };

    static bool supports(
        const ProgramNode& program,
        const std::unordered_map<std::string, std::string>& namespaceAliases,
        const std::unordered_map<std::string, std::string>& symbolAliases
    ) {
        BytecodeCompiler compiler(namespaceAliases, symbolAliases);
        for (const auto& statement : program.statements) {
            if (!compiler.supportsStmt(statement.get())) {
                return false;
            }
        }
        return true;
    }

    static BytecodeProgram compile(
        const ProgramNode& program,
        const std::unordered_map<std::string, std::string>& namespaceAliases,
        const std::unordered_map<std::string, std::string>& symbolAliases
    ) {
        BytecodeCompiler compiler(namespaceAliases, symbolAliases);
        return compiler.compileProgram(program);
    }

private:
    struct LocalScope {
        std::unordered_map<std::string, int> locals;
        int nextSlot = 0;
    };

    const std::unordered_map<std::string, std::string>& builtinNamespaceAliases_;
    const std::unordered_map<std::string, std::string>& builtinSymbolAliases_;

    BytecodeCompiler(
        const std::unordered_map<std::string, std::string>& namespaceAliases,
        const std::unordered_map<std::string, std::string>& symbolAliases
    )
        : builtinNamespaceAliases_(namespaceAliases),
          builtinSymbolAliases_(symbolAliases) {}

    BytecodeProgram compileProgram(const ProgramNode& program) {
        BytecodeProgram output;
        output.entry.name = "__main__";

        for (const auto& statement : program.statements) {
            if (auto functionStmt = dynamic_cast<FunctionStmt*>(statement.get())) {
                output.functions[functionStmt->name] = compileFunction(*functionStmt);
            }
        }

        LocalScope globals;
        for (const auto& statement : program.statements) {
            if (dynamic_cast<FunctionStmt*>(statement.get()) != nullptr) {
                continue;
            }
            compileStmt(statement.get(), output.entry, globals, true);
        }
        emit(output.entry, OpCode::Null);
        emit(output.entry, OpCode::Return);
        return output;
    }

    BytecodeFunction compileFunction(const FunctionStmt& functionStmt) {
        BytecodeFunction function;
        function.name = functionStmt.name;
        function.params = functionStmt.params;

        LocalScope scope;
        for (const std::string& param : functionStmt.params) {
            scope.locals[param] = scope.nextSlot++;
        }

        for (const auto& statement : functionStmt.body->statements) {
            compileStmt(statement.get(), function, scope, false);
        }
        emit(function, OpCode::Null);
        emit(function, OpCode::Return);
        return function;
    }

    bool supportsStmt(Stmt* stmt) const {
        if (!stmt) return true;
        if (dynamic_cast<ImportStmt*>(stmt) != nullptr) return false;
        if (dynamic_cast<FromImportStmt*>(stmt) != nullptr) return false;
        if (auto varStmt = dynamic_cast<VarStmt*>(stmt)) return supportsExpr(varStmt->initializer.get());
        if (auto functionStmt = dynamic_cast<FunctionStmt*>(stmt)) return supportsStmt(functionStmt->body.get());
        if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) return supportsExpr(exprStmt->expr.get());
        if (auto logStmt = dynamic_cast<LogStmt*>(stmt)) return supportsExpr(logStmt->message.get());
        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) return supportsExpr(returnStmt->value.get());
        if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            return supportsExpr(ifStmt->condition.get()) &&
                supportsStmt(ifStmt->thenBranch.get()) &&
                supportsStmt(ifStmt->elseBranch.get());
        }
        if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            return supportsExpr(whileStmt->condition.get()) && supportsStmt(whileStmt->body.get());
        }
        if (auto blockStmt = dynamic_cast<BlockStmt*>(stmt)) {
            for (const auto& child : blockStmt->statements) if (!supportsStmt(child.get())) return false;
            return true;
        }
        if (dynamic_cast<BreakStmt*>(stmt) != nullptr) return false;
        if (dynamic_cast<ContinueStmt*>(stmt) != nullptr) return false;
        return true;
    }

    bool supportsExpr(Expr* expr) const {
        if (!expr) return true;
        if (dynamic_cast<LiteralExpr*>(expr) != nullptr) return true;
        if (dynamic_cast<IdentifierExpr*>(expr) != nullptr) return true;
        if (auto arrayExpr = dynamic_cast<ArrayExpr*>(expr)) {
            for (const auto& element : arrayExpr->elements) {
                if (!supportsExpr(element.get())) return false;
            }
            return true;
        }
        if (auto indexExpr = dynamic_cast<IndexExpr*>(expr)) {
            return supportsExpr(indexExpr->target.get()) && supportsExpr(indexExpr->index.get());
        }
        if (auto assign = dynamic_cast<AssignExpr*>(expr)) return supportsExpr(assign->value.get());
        if (auto unary = dynamic_cast<UnaryExpr*>(expr)) return supportsExpr(unary->right.get());
        if (auto binary = dynamic_cast<BinaryExpr*>(expr)) return supportsExpr(binary->left.get()) && supportsExpr(binary->right.get());
        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            const std::string callee = canonicalizeBuiltinName(call->callee);
            if (!isSupportedBuiltin(callee) && usesUnsupportedFrameworkBuiltin(callee)) {
                return false;
            }
            for (const auto& arg : call->args) if (!supportsExpr(arg.get())) return false;
            return true;
        }
        return false;
    }

    static bool isSupportedBuiltin(const std::string& name) {
        return name == "print" ||
            name == "str" ||
            name == "num" ||
            name == "bool" ||
            name == "type" ||
            name == "len" ||
            name == "range" ||
            name == "push" ||
            name == "pop" ||
            name == "join" ||
            name == "split" ||
            name == "upper" ||
            name == "lower" ||
            name == "contains" ||
            name == "trim" ||
            name == "replace" ||
            name == "slice" ||
            name == "floor" ||
            name == "ceil" ||
            name == "round" ||
            name == "min" ||
            name == "max" ||
            name == "clamp" ||
            name == "sleep" ||
            name == "clock.ms" ||
            name == "time.now_ms" ||
            name == "time.sleep" ||
            name == "time.unix_ms" ||
            name == "json.stringify" ||
            name == "json.parse" ||
            name == "random" ||
            name == "random.int";
    }

    static bool isFrameworkNamespace(const std::string& name) {
        return name == "app" ||
            name == "ui" ||
            name == "web" ||
            name == "engine" ||
            name == "fs" ||
            name == "env" ||
            name == "process";
    }

    static bool usesUnsupportedFrameworkBuiltin(const std::string& name) {
        const size_t dot = name.find('.');
        if (dot == std::string::npos) {
            return false;
        }

        return isFrameworkNamespace(name.substr(0, dot));
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

    void compileStmt(Stmt* stmt, BytecodeFunction& function, LocalScope& scope, bool topLevel) {
        if (!stmt) return;

        if (auto varStmt = dynamic_cast<VarStmt*>(stmt)) {
            compileExpr(varStmt->initializer.get(), function, scope);
            if (topLevel) {
                emit(function, OpCode::DefineGlobal, addConstant(function, VMValue(varStmt->name)));
            } else {
                const int slot = scope.nextSlot++;
                scope.locals[varStmt->name] = slot;
                emit(function, OpCode::SetLocal, slot);
                emit(function, OpCode::Pop);
            }
            return;
        }

        if (dynamic_cast<FunctionStmt*>(stmt) != nullptr) {
            return;
        }

        if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
            compileExpr(exprStmt->expr.get(), function, scope);
            emit(function, OpCode::Pop);
            return;
        }

        if (auto logStmt = dynamic_cast<LogStmt*>(stmt)) {
            auto call = std::make_unique<CallExpr>();
            call->callee = "print";
            call->args.push_back(cloneExpr(logStmt->message.get()));
            compileExpr(call.get(), function, scope);
            emit(function, OpCode::Pop);
            return;
        }

        if (auto blockStmt = dynamic_cast<BlockStmt*>(stmt)) {
            for (const auto& child : blockStmt->statements) {
                compileStmt(child.get(), function, scope, false);
            }
            return;
        }

        if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            compileExpr(ifStmt->condition.get(), function, scope);
            const int jumpIfFalse = emitPlaceholder(function, OpCode::JumpIfFalse);
            emit(function, OpCode::Pop);
            compileStmt(ifStmt->thenBranch.get(), function, scope, false);
            const int jumpOverElse = emitPlaceholder(function, OpCode::Jump);
            patchJump(function, jumpIfFalse, static_cast<int>(function.code.size()));
            emit(function, OpCode::Pop);
            compileStmt(ifStmt->elseBranch.get(), function, scope, false);
            patchJump(function, jumpOverElse, static_cast<int>(function.code.size()));
            return;
        }

        if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            const int loopStart = static_cast<int>(function.code.size());
            compileExpr(whileStmt->condition.get(), function, scope);
            const int exitJump = emitPlaceholder(function, OpCode::JumpIfFalse);
            emit(function, OpCode::Pop);
            compileStmt(whileStmt->body.get(), function, scope, false);
            emit(function, OpCode::Loop, loopStart);
            patchJump(function, exitJump, static_cast<int>(function.code.size()));
            emit(function, OpCode::Pop);
            return;
        }

        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            compileExpr(returnStmt->value.get(), function, scope);
            emit(function, OpCode::Return);
            return;
        }

        throw std::runtime_error("Unsupported statement for VM compiler.");
    }

    void compileExpr(Expr* expr, BytecodeFunction& function, LocalScope& scope) {
        if (!expr) {
            emit(function, OpCode::Null);
            return;
        }

        if (auto literal = dynamic_cast<LiteralExpr*>(expr)) {
            switch (literal->kind) {
            case LiteralExpr::Kind::Number:
                emit(function, OpCode::Constant, addConstant(function, VMValue(std::stod(literal->value))));
                return;
            case LiteralExpr::Kind::String:
                emit(function, OpCode::Constant, addConstant(function, VMValue(literal->value)));
                return;
            case LiteralExpr::Kind::Bool:
                emit(function, literal->value == "true" ? OpCode::True : OpCode::False);
                return;
            case LiteralExpr::Kind::Null:
                emit(function, OpCode::Null);
                return;
            }
        }

        if (auto identifier = dynamic_cast<IdentifierExpr*>(expr)) {
            if (const auto local = localSlot(scope, identifier->name)) {
                emit(function, OpCode::GetLocal, *local);
            } else {
                emit(function, OpCode::GetGlobal, addConstant(function, VMValue(identifier->name)));
            }
            return;
        }

        if (auto assign = dynamic_cast<AssignExpr*>(expr)) {
            compileExpr(assign->value.get(), function, scope);
            if (const auto local = localSlot(scope, assign->name)) {
                emit(function, OpCode::SetLocal, *local);
            } else {
                emit(function, OpCode::SetGlobal, addConstant(function, VMValue(assign->name)));
            }
            return;
        }

        if (auto unary = dynamic_cast<UnaryExpr*>(expr)) {
            compileExpr(unary->right.get(), function, scope);
            if (unary->op == "-") emit(function, OpCode::Negate);
            else if (unary->op == "!") emit(function, OpCode::Not);
            else throw std::runtime_error("Unsupported unary operator for VM: " + unary->op);
            return;
        }

        if (auto binary = dynamic_cast<BinaryExpr*>(expr)) {
            if (binary->op == "&&") {
                compileExpr(binary->left.get(), function, scope);
                const int jumpIfFalse = emitPlaceholder(function, OpCode::JumpIfFalse);
                emit(function, OpCode::Pop);
                compileExpr(binary->right.get(), function, scope);
                const int jumpOverFalse = emitPlaceholder(function, OpCode::Jump);
                patchJump(function, jumpIfFalse, static_cast<int>(function.code.size()));
                emit(function, OpCode::Pop);
                emit(function, OpCode::False);
                patchJump(function, jumpOverFalse, static_cast<int>(function.code.size()));
                return;
            }

            if (binary->op == "||") {
                compileExpr(binary->left.get(), function, scope);
                const int jumpIfFalse = emitPlaceholder(function, OpCode::JumpIfFalse);
                emit(function, OpCode::Pop);
                emit(function, OpCode::True);
                const int jumpOverRight = emitPlaceholder(function, OpCode::Jump);
                patchJump(function, jumpIfFalse, static_cast<int>(function.code.size()));
                emit(function, OpCode::Pop);
                compileExpr(binary->right.get(), function, scope);
                emit(function, OpCode::Not);
                emit(function, OpCode::Not);
                patchJump(function, jumpOverRight, static_cast<int>(function.code.size()));
                return;
            }

            compileExpr(binary->left.get(), function, scope);
            compileExpr(binary->right.get(), function, scope);
            if (binary->op == "+") emit(function, OpCode::Add);
            else if (binary->op == "-") emit(function, OpCode::Subtract);
            else if (binary->op == "*") emit(function, OpCode::Multiply);
            else if (binary->op == "/") emit(function, OpCode::Divide);
            else if (binary->op == "%") emit(function, OpCode::Modulo);
            else if (binary->op == "==") emit(function, OpCode::Equal);
            else if (binary->op == "!=") emit(function, OpCode::NotEqual);
            else if (binary->op == ">") emit(function, OpCode::Greater);
            else if (binary->op == ">=") emit(function, OpCode::GreaterEqual);
            else if (binary->op == "<") emit(function, OpCode::Less);
            else if (binary->op == "<=") emit(function, OpCode::LessEqual);
            else throw std::runtime_error("Unsupported binary operator for VM: " + binary->op);
            return;
        }

        if (auto arrayExpr = dynamic_cast<ArrayExpr*>(expr)) {
            for (const auto& element : arrayExpr->elements) {
                compileExpr(element.get(), function, scope);
            }
            emit(function, OpCode::BuildArray, static_cast<int>(arrayExpr->elements.size()));
            return;
        }

        if (auto indexExpr = dynamic_cast<IndexExpr*>(expr)) {
            compileExpr(indexExpr->target.get(), function, scope);
            compileExpr(indexExpr->index.get(), function, scope);
            emit(function, OpCode::GetIndex);
            return;
        }

        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            for (const auto& arg : call->args) {
                compileExpr(arg.get(), function, scope);
            }
            emit(
                function,
                OpCode::Call,
                addConstant(function, VMValue(canonicalizeBuiltinName(call->callee))),
                static_cast<int>(call->args.size()));
            return;
        }

        throw std::runtime_error("Unsupported expression for VM compiler.");
    }

    static std::unique_ptr<Expr> cloneExpr(Expr* expr) {
        if (!expr) return nullptr;

        if (auto literal = dynamic_cast<LiteralExpr*>(expr)) {
            auto node = std::make_unique<LiteralExpr>();
            node->kind = literal->kind;
            node->value = literal->value;
            return node;
        }
        if (auto identifier = dynamic_cast<IdentifierExpr*>(expr)) {
            auto node = std::make_unique<IdentifierExpr>();
            node->name = identifier->name;
            return node;
        }
        if (auto binary = dynamic_cast<BinaryExpr*>(expr)) {
            auto node = std::make_unique<BinaryExpr>();
            node->op = binary->op;
            node->left = cloneExpr(binary->left.get());
            node->right = cloneExpr(binary->right.get());
            return node;
        }
        if (auto unary = dynamic_cast<UnaryExpr*>(expr)) {
            auto node = std::make_unique<UnaryExpr>();
            node->op = unary->op;
            node->right = cloneExpr(unary->right.get());
            return node;
        }
        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            auto node = std::make_unique<CallExpr>();
            node->callee = call->callee;
            for (const auto& arg : call->args) {
                node->args.push_back(cloneExpr(arg.get()));
            }
            return node;
        }
        if (auto assign = dynamic_cast<AssignExpr*>(expr)) {
            auto node = std::make_unique<AssignExpr>();
            node->name = assign->name;
            node->value = cloneExpr(assign->value.get());
            return node;
        }
        return nullptr;
    }

    static std::optional<int> localSlot(const LocalScope& scope, const std::string& name) {
        const auto found = scope.locals.find(name);
        if (found == scope.locals.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    static int addConstant(BytecodeFunction& function, const VMValue& value) {
        function.constants.push_back(value);
        return static_cast<int>(function.constants.size() - 1);
    }

    static void emit(BytecodeFunction& function, OpCode op, int a = 0, int b = 0) {
        function.code.push_back(Instruction{op, a, b});
    }

    static int emitPlaceholder(BytecodeFunction& function, OpCode op) {
        function.code.push_back(Instruction{op, -1, 0});
        return static_cast<int>(function.code.size() - 1);
    }

    static void patchJump(BytecodeFunction& function, int instructionIndex, int targetIndex) {
        function.code.at(static_cast<std::size_t>(instructionIndex)).a = targetIndex;
    }
};
