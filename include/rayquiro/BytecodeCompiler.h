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

    std::vector<std::vector<int>> breakStack_;
    std::vector<int>              loopStartStack_;

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
        output.entry.localCount = globals.nextSlot;
        emit(output.entry, OpCode::Null);
        emit(output.entry, OpCode::Return);
        return output;
    }

    BytecodeFunction compileFunction(const FunctionStmt& functionStmt) {
        BytecodeFunction function;
        function.name = functionStmt.name;
        for (const auto& p : functionStmt.params)
            function.params.push_back(p.name);

        LocalScope scope;
        for (const auto& p : functionStmt.params) {
            scope.locals[p.name] = scope.nextSlot++;
        }

        for (const auto& statement : functionStmt.body->statements) {
            compileStmt(statement.get(), function, scope, false);
        }
        emit(function, OpCode::Null);
        emit(function, OpCode::Return);
        function.localCount = scope.nextSlot;
        return function;
    }

    bool supportsStmt(Stmt* stmt) const {
        if (!stmt) return true;
        if (dynamic_cast<ImportStmt*>(stmt) != nullptr) return false;
        if (dynamic_cast<FromImportStmt*>(stmt) != nullptr) return false;

        return true;
    }

    bool supportsExpr(Expr* expr) const {
        if (!expr) return true;

        return true;
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
            name == "random.int" ||
            name == "datetime.now" ||
            name == "datetime.timestamp" ||
            name == "datetime.format" ||
            name == "path.join" ||
            name == "path.basename" ||
            name == "path.dirname" ||
            name == "path.ext" ||
            name == "path.exists" ||
            name == "path.abs" ||
            name == "path.stem" ||
            name == "hash.sha256" ||
            name == "crypto.sha256" ||
            name == "crypto.md5" ||
            name == "crypto.sha1" ||
            name == "crypto.base64_encode" ||
            name == "crypto.base64_decode" ||
            name == "crypto.uuid" ||
            name == "crypto.random_bytes" ||
            name == "crypto.hmac_sha256" ||
            name == "regex.test" ||
            name == "regex.match" ||
            name == "regex.replace" ||
            name == "regex.split" ||
            name == "__optional_get";
    }

    static bool isFrameworkNamespace(const std::string& name) {
        return name == "app" ||
            name == "ui" ||
            name == "web" ||
            name == "engine" ||
            name == "fs" ||
            name == "env" ||
            name == "process" ||
            name == "datetime" ||
            name == "path" ||
            name == "hash" ||
            name == "crypto" ||
            name == "regex";
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
            loopStartStack_.push_back(loopStart);
            breakStack_.push_back({});

            compileExpr(whileStmt->condition.get(), function, scope);
            const int exitJump = emitPlaceholder(function, OpCode::JumpIfFalse);
            emit(function, OpCode::Pop);
            compileStmt(whileStmt->body.get(), function, scope, false);
            emit(function, OpCode::Loop, loopStart);
            patchJump(function, exitJump, static_cast<int>(function.code.size()));
            emit(function, OpCode::Pop);

            int afterLoop = static_cast<int>(function.code.size());
            for (int idx : breakStack_.back()) patchJump(function, idx, afterLoop);
            breakStack_.pop_back();
            loopStartStack_.pop_back();
            return;
        }

        if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            if (returnStmt->values.empty()) {
                emit(function, OpCode::Null);
            } else if (returnStmt->values.size() == 1) {
                compileExpr(returnStmt->values[0].get(), function, scope);
            } else {
                for (const auto& v : returnStmt->values)
                    compileExpr(v.get(), function, scope);
                emit(function, OpCode::BuildArray, static_cast<int>(returnStmt->values.size()));
            }
            emit(function, OpCode::Return);
            return;
        }

        if (dynamic_cast<BreakStmt*>(stmt) != nullptr) {

            int idx = emitPlaceholder(function, OpCode::Jump);
            if (!breakStack_.empty()) breakStack_.back().push_back(idx);
            return;
        }

        if (dynamic_cast<ContinueStmt*>(stmt) != nullptr) {

            int target = loopStartStack_.empty() ? 0 : loopStartStack_.back();
            emit(function, OpCode::Loop, target);
            return;
        }

        if (auto forIn = dynamic_cast<ForInStmt*>(stmt)) {

            int collSlot = scope.nextSlot++;
            int idxSlot  = scope.nextSlot++;
            int keySlot  = scope.nextSlot++;
            scope.locals[forIn->keyVar] = keySlot;
            int valSlot  = -1;
            if (!forIn->valVar.empty()) {
                valSlot = scope.nextSlot++;
                scope.locals[forIn->valVar] = valSlot;
            }

            compileExpr(forIn->iterable.get(), function, scope);
            emit(function, OpCode::SetLocal, collSlot);
            emit(function, OpCode::Pop);

            emit(function, OpCode::Constant, addConstant(function, VMValue(0.0)));
            emit(function, OpCode::SetLocal, idxSlot);
            emit(function, OpCode::Pop);

            const int loopStart = static_cast<int>(function.code.size());
            loopStartStack_.push_back(loopStart);
            breakStack_.push_back({});

            emit(function, OpCode::GetLocal, idxSlot);
            emit(function, OpCode::GetLocal, collSlot);
            emit(function, OpCode::Call,
                addConstant(function, VMValue(std::string("len"))), 1);
            emit(function, OpCode::Less);
            const int exitJump = emitPlaceholder(function, OpCode::JumpIfFalse);
            emit(function, OpCode::Pop);

            emit(function, OpCode::GetLocal, collSlot);
            emit(function, OpCode::GetLocal, idxSlot);
            emit(function, OpCode::GetIndex);
            emit(function, OpCode::SetLocal, keySlot);
            emit(function, OpCode::Pop);

            if (valSlot != -1) {

                emit(function, OpCode::GetLocal, collSlot);
                emit(function, OpCode::GetLocal, keySlot);
                emit(function, OpCode::GetIndex);
                emit(function, OpCode::SetLocal, valSlot);
                emit(function, OpCode::Pop);
            }

            compileStmt(forIn->body.get(), function, scope, false);

            emit(function, OpCode::GetLocal, idxSlot);
            emit(function, OpCode::Constant, addConstant(function, VMValue(1.0)));
            emit(function, OpCode::Add);
            emit(function, OpCode::SetLocal, idxSlot);
            emit(function, OpCode::Pop);

            emit(function, OpCode::Loop, loopStart);
            patchJump(function, exitJump, static_cast<int>(function.code.size()));
            emit(function, OpCode::Pop);

            int afterLoop = static_cast<int>(function.code.size());
            for (int idx : breakStack_.back()) patchJump(function, idx, afterLoop);
            breakStack_.pop_back();
            loopStartStack_.pop_back();
            return;
        }

        if (auto throwStmt = dynamic_cast<ThrowStmt*>(stmt)) {
            compileExpr(throwStmt->value.get(), function, scope);
            emit(function, OpCode::Throw);
            return;
        }

        if (auto tryCatch = dynamic_cast<TryCatchStmt*>(stmt)) {

            int errorNameConst = addConstant(function, VMValue(tryCatch->errorParam));
            int tryBeginIdx = emitPlaceholder(function, OpCode::TryBegin);
            function.code.back().b = errorNameConst;

            compileStmt(tryCatch->tryBody.get(), function, scope, false);

            int tryEndIdx = emitPlaceholder(function, OpCode::TryEnd);

            int catchStart = static_cast<int>(function.code.size());
            patchJump(function, tryBeginIdx, catchStart);

            if (tryCatch->catchBody) {

                compileStmt(tryCatch->catchBody.get(), function, scope, false);
            }

            if (tryCatch->finallyBody) {
                compileStmt(tryCatch->finallyBody.get(), function, scope, false);
            }

            int afterCatch = static_cast<int>(function.code.size());
            patchJump(function, tryEndIdx, afterCatch);
            return;
        }

        if (auto structDef = dynamic_cast<StructStmt*>(stmt)) {
            int count = 0;

            emit(function, OpCode::Constant, addConstant(function, VMValue(std::string("__struct__"))));
            emit(function, OpCode::Constant, addConstant(function, VMValue(structDef->name)));
            count++;
            for (const auto& field : structDef->fields) {
                emit(function, OpCode::Constant, addConstant(function, VMValue(field.first)));
                compileExpr(field.second.get(), function, scope);
                count++;
            }
            emit(function, OpCode::BuildObject, count);
            emit(function, OpCode::DefineGlobal, addConstant(function, VMValue(structDef->name)));
            return;
        }

        if (auto enumStmt = dynamic_cast<EnumStmt*>(stmt)) {

            int count = 0;
            for (const auto& variant : enumStmt->variants) {
                emit(function, OpCode::Constant, addConstant(function, VMValue(variant.name)));
                emit(function, OpCode::Constant, addConstant(function, VMValue(static_cast<double>(variant.value))));
                count++;
            }

            emit(function, OpCode::Constant, addConstant(function, VMValue(std::string("__enum__"))));
            emit(function, OpCode::Constant, addConstant(function, VMValue(enumStmt->name)));
            count++;
            emit(function, OpCode::BuildObject, count);
            emit(function, OpCode::DefineGlobal, addConstant(function, VMValue(enumStmt->name)));

            for (const auto& variant : enumStmt->variants) {
                emit(function, OpCode::Constant,
                    addConstant(function, VMValue(static_cast<double>(variant.value))));
                emit(function, OpCode::DefineGlobal,
                    addConstant(function, VMValue(enumStmt->name + "." + variant.name)));
            }
            return;
        }

        if (dynamic_cast<InterfaceStmt*>(stmt) != nullptr) {
            return;
        }

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

        if (auto objExpr = dynamic_cast<ObjectExpr*>(expr)) {
            for (const auto& [key, valExpr] : objExpr->fields) {
                emit(function, OpCode::Constant, addConstant(function, VMValue(key)));
                compileExpr(valExpr.get(), function, scope);
            }
            emit(function, OpCode::BuildObject, static_cast<int>(objExpr->fields.size()));
            return;
        }

        if (auto structInst = dynamic_cast<StructInstExpr*>(expr)) {

            emit(function, OpCode::Constant, addConstant(function, VMValue(std::string("__type__"))));
            emit(function, OpCode::Constant, addConstant(function, VMValue(structInst->typeName)));
            int count = 1;
            for (const auto& [key, valExpr] : structInst->fields) {
                emit(function, OpCode::Constant, addConstant(function, VMValue(key)));
                compileExpr(valExpr.get(), function, scope);
                count++;
            }
            emit(function, OpCode::BuildObject, count);
            return;
        }

        if (auto indexExpr = dynamic_cast<IndexExpr*>(expr)) {
            compileExpr(indexExpr->target.get(), function, scope);
            compileExpr(indexExpr->index.get(), function, scope);
            emit(function, OpCode::GetIndex);
            return;
        }

        if (auto setIdx = dynamic_cast<SetIndexExpr*>(expr)) {
            if (auto id = dynamic_cast<IdentifierExpr*>(setIdx->object.get())) {

                compileExpr(setIdx->index.get(), function, scope);
                compileExpr(setIdx->value.get(), function, scope);
                if (const auto local = localSlot(scope, id->name)) {
                    emit(function, OpCode::SetLocalIndex, *local);
                } else {
                    emit(function, OpCode::SetGlobalIndex, addConstant(function, VMValue(id->name)));
                }
            } else {

                compileExpr(setIdx->object.get(), function, scope);
                compileExpr(setIdx->index.get(), function, scope);
                compileExpr(setIdx->value.get(), function, scope);
                emit(function, OpCode::SetIndex);
            }
            return;
        }

        if (auto tmpl = dynamic_cast<TemplateLiteralExpr*>(expr)) {
            int n = 0;
            for (std::size_t i = 0; i < tmpl->parts.size(); ++i) {
                if (!tmpl->parts[i].empty()) {
                    emit(function, OpCode::Constant, addConstant(function, VMValue(tmpl->parts[i])));
                    n++;
                }
                if (i < tmpl->exprs.size()) {
                    compileExpr(tmpl->exprs[i].get(), function, scope);

                    emit(function, OpCode::Constant, addConstant(function, VMValue(std::string(""))));
                    emit(function, OpCode::Add);
                    n++;
                }
            }
            if (n == 0) {
                emit(function, OpCode::Constant, addConstant(function, VMValue(std::string(""))));
            } else if (n > 1) {
                emit(function, OpCode::Concat, n);
            }
            return;
        }

        if (auto compAssign = dynamic_cast<CompoundAssignExpr*>(expr)) {

            if (const auto local = localSlot(scope, compAssign->name)) {
                emit(function, OpCode::GetLocal, *local);
            } else {
                emit(function, OpCode::GetGlobal, addConstant(function, VMValue(compAssign->name)));
            }
            compileExpr(compAssign->value.get(), function, scope);
            if (compAssign->op == "+") emit(function, OpCode::Add);
            else if (compAssign->op == "-") emit(function, OpCode::Subtract);
            else if (compAssign->op == "*") emit(function, OpCode::Multiply);
            else if (compAssign->op == "/") emit(function, OpCode::Divide);
            else if (compAssign->op == "%") emit(function, OpCode::Modulo);

            if (const auto local = localSlot(scope, compAssign->name)) {
                emit(function, OpCode::SetLocal, *local);
            } else {
                emit(function, OpCode::SetGlobal, addConstant(function, VMValue(compAssign->name)));
            }
            return;
        }

        if (auto postfix = dynamic_cast<PostfixExpr*>(expr)) {
            if (const auto local = localSlot(scope, postfix->name)) {
                emit(function, OpCode::GetLocal, *local);
                emit(function, OpCode::Constant, addConstant(function, VMValue(1.0)));
                if (postfix->op == "++") emit(function, OpCode::Add);
                else emit(function, OpCode::Subtract);
                emit(function, OpCode::SetLocal, *local);
            } else {
                emit(function, OpCode::GetGlobal, addConstant(function, VMValue(postfix->name)));
                emit(function, OpCode::Constant, addConstant(function, VMValue(1.0)));
                if (postfix->op == "++") emit(function, OpCode::Add);
                else emit(function, OpCode::Subtract);
                emit(function, OpCode::SetGlobal, addConstant(function, VMValue(postfix->name)));
            }
            return;
        }

        if (auto dynCall = dynamic_cast<DynCallExpr*>(expr)) {

            if (auto id = dynamic_cast<IdentifierExpr*>(dynCall->callee.get())) {
                for (const auto& arg : dynCall->args) compileExpr(arg.get(), function, scope);
                emit(function, OpCode::Call,
                    addConstant(function, VMValue(canonicalizeBuiltinName(id->name))),
                    static_cast<int>(dynCall->args.size()));
                return;
            }

            emit(function, OpCode::Null);
            return;
        }

        if (auto awaitExpr = dynamic_cast<AwaitExpr*>(expr)) {
            compileExpr(awaitExpr->operand.get(), function, scope);
            return;
        }

        if (auto call = dynamic_cast<CallExpr*>(expr)) {

            if (call->callee == "push" && call->args.size() == 2) {
                if (auto id = dynamic_cast<IdentifierExpr*>(call->args[0].get())) {

                    compileExpr(call->args[1].get(), function, scope);
                    if (const auto local = localSlot(scope, id->name)) {
                        emit(function, OpCode::AppendLocal, *local);
                    } else {
                        emit(function, OpCode::AppendGlobal, addConstant(function, VMValue(id->name)));
                    }
                    return;
                }
                if (auto indexExpr = dynamic_cast<IndexExpr*>(call->args[0].get())) {
                    if (auto objId = dynamic_cast<IdentifierExpr*>(indexExpr->target.get())) {

                        compileExpr(indexExpr->index.get(), function, scope);
                        compileExpr(call->args[1].get(), function, scope);
                        if (const auto local = localSlot(scope, objId->name)) {
                            emit(function, OpCode::AppendObjLocal, *local);
                        } else {
                            emit(function, OpCode::AppendObjGlobal, addConstant(function, VMValue(objId->name)));
                        }
                        return;
                    }
                }
            }

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

        if (auto nc = dynamic_cast<NullCoalesceExpr*>(expr)) {
            compileExpr(nc->left.get(), function, scope);

            emit(function, OpCode::Dup);
            int jmpIdx = static_cast<int>(function.code.size());
            emit(function, OpCode::JumpIfNotNull, 0);
            emit(function, OpCode::Pop);
            compileExpr(nc->right.get(), function, scope);
            int afterRight = static_cast<int>(function.code.size());
            function.code[jmpIdx].b = afterRight;
            return;
        }

        if (auto oc = dynamic_cast<OptionalChainExpr*>(expr)) {
            compileExpr(oc->object.get(), function, scope);
            if (!oc->field.empty()) {
                emit(function, OpCode::Constant, addConstant(function, VMValue(oc->field)));
            } else if (oc->index) {
                compileExpr(oc->index.get(), function, scope);
            } else {
                emit(function, OpCode::Constant, addConstant(function, VMValue(std::string("__none__"))));
            }
            emit(function, OpCode::Call,
                 addConstant(function, VMValue(std::string("__optional_get"))), 2);
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

