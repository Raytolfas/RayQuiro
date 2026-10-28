#pragma once

#include "AST.h"
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct TypeDiagnostic {
    int line = 0;
    int col = 0;
    std::string message;
    bool isError = true;
};

struct FunctionTypeInfo {
    std::string returnType = "any";
    std::vector<std::pair<std::string, std::string>> params;
    bool isVariadic = false;
};

struct StructTypeInfo {
    std::string name;
    std::unordered_map<std::string, std::string> fields;
};

struct EnumTypeInfo {
    std::string name;
    std::vector<std::string> variants;
};

class TypeChecker {
public:
    TypeChecker() {
        initBuiltins();
    }

    void checkProgram(const ProgramNode* program) {
        if (!program) return;
        firstPassCollectTypes(program);
        pushScope();
        for (const auto& stmt : program->statements) {
            checkStmt(stmt.get());
        }
        popScope();
    }

    bool hasErrors() const {
        for (const auto& d : diagnostics_) {
            if (d.isError) return true;
        }
        return false;
    }

    const std::vector<TypeDiagnostic>& diagnostics() const {
        return diagnostics_;
    }

    std::string formatDiagnostics(const std::string& filename) const {
        std::ostringstream out;
        for (const auto& d : diagnostics_) {
            out << filename << ":" << (d.line > 0 ? d.line : 1) << ":" << (d.col > 0 ? d.col : 1)
                << ": " << (d.isError ? "error: " : "warning: ") << d.message << "\n";
        }
        return out.str();
    }

private:
    std::vector<TypeDiagnostic> diagnostics_;
    std::vector<std::unordered_map<std::string, std::string>> scopes_;
    std::unordered_map<std::string, FunctionTypeInfo> functions_;
    std::unordered_map<std::string, StructTypeInfo> structs_;
    std::unordered_map<std::string, EnumTypeInfo> enums_;
    std::string currentFuncReturnType_;
    int currentLine_ = 0;

    void addError(const std::string& message, int line = 0, int col = 0) {
        TypeDiagnostic diag;
        diag.line = line > 0 ? line : currentLine_;
        diag.col = col > 0 ? col : 1;
        diag.message = message;
        diag.isError = true;
        diagnostics_.push_back(diag);
    }

    void addWarning(const std::string& message, int line = 0, int col = 0) {
        TypeDiagnostic diag;
        diag.line = line > 0 ? line : currentLine_;
        diag.col = col > 0 ? col : 1;
        diag.message = message;
        diag.isError = false;
        diagnostics_.push_back(diag);
    }

    void pushScope() {
        scopes_.push_back({});
    }

    void popScope() {
        if (!scopes_.empty()) {
            scopes_.pop_back();
        }
    }

    void setVarType(const std::string& name, const std::string& type) {
        if (scopes_.empty()) pushScope();
        scopes_.back()[name] = type;
    }

    std::string getVarType(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        return "any";
    }

    bool hasVar(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            if (it->find(name) != it->end()) return true;
        }
        return false;
    }

    bool isNumericType(const std::string& t) const {
        return t == "number" || t == "int" || t == "float";
    }

    bool isAssignable(const std::string& target, const std::string& source) const {
        if (target.empty() || source.empty()) return true;
        if (target == "any" || source == "any") return true;
        if (target == source) return true;
        if (isNumericType(target) && isNumericType(source)) return true;
        if (target == "string" && source == "string") return true;
        if (target == "bool" && source == "bool") return true;
        if (target == "array" && (source == "array" || source.find("[]") != std::string::npos)) return true;
        if (target == "object" && (source == "object" || structs_.find(source) != structs_.end())) return true;
        if ((target == "void" || target == "null") && (source == "void" || source == "null")) return true;
        return false;
    }

    void initBuiltins() {
        functions_["print"] = FunctionTypeInfo{"void", {{"value", "any"}}, true};
        functions_["len"] = FunctionTypeInfo{"number", {{"target", "any"}}, false};
        functions_["sqrt"] = FunctionTypeInfo{"number", {{"x", "number"}}, false};
        functions_["pow"] = FunctionTypeInfo{"number", {{"base", "number"}, {"exp", "number"}}, false};
        functions_["abs"] = FunctionTypeInfo{"number", {{"x", "number"}}, false};
        functions_["min"] = FunctionTypeInfo{"number", {{"a", "number"}, {"b", "number"}}, false};
        functions_["max"] = FunctionTypeInfo{"number", {{"a", "number"}, {"b", "number"}}, false};
        functions_["floor"] = FunctionTypeInfo{"number", {{"x", "number"}}, false};
        functions_["ceil"] = FunctionTypeInfo{"number", {{"x", "number"}}, false};
        functions_["round"] = FunctionTypeInfo{"number", {{"x", "number"}}, false};
        functions_["clamp"] = FunctionTypeInfo{"number", {{"val", "number"}, {"min", "number"}, {"max", "number"}}, false};
        functions_["assert"] = FunctionTypeInfo{"void", {{"cond", "bool"}}, false};
        functions_["assert_true"] = FunctionTypeInfo{"void", {{"cond", "bool"}}, false};
        functions_["assert_false"] = FunctionTypeInfo{"void", {{"cond", "bool"}}, false};
        functions_["assert_eq"] = FunctionTypeInfo{"void", {{"actual", "any"}, {"expected", "any"}}, true};
        functions_["vec2"] = FunctionTypeInfo{"object", {{"x", "number"}, {"y", "number"}}, false};
        functions_["vec3"] = FunctionTypeInfo{"object", {{"x", "number"}, {"y", "number"}, {"z", "number"}}, false};
    }

    void firstPassCollectTypes(const ProgramNode* program) {
        for (const auto& s : program->statements) {
            if (auto fs = dynamic_cast<FunctionStmt*>(s.get())) {
                FunctionTypeInfo info;
                info.returnType = fs->returnType.empty() ? "any" : fs->returnType;
                for (const auto& p : fs->params) {
                    info.params.push_back({p.name, p.typeAnnotation.empty() ? "any" : p.typeAnnotation});
                    if (p.isVariadic) info.isVariadic = true;
                }
                functions_[fs->name] = info;
            } else if (auto ss = dynamic_cast<StructStmt*>(s.get())) {
                StructTypeInfo info;
                info.name = ss->name;
                for (const auto& f : ss->fields) {
                    info.fields[f.first] = "any";
                }
                structs_[ss->name] = info;
            } else if (auto es = dynamic_cast<EnumStmt*>(s.get())) {
                EnumTypeInfo info;
                info.name = es->name;
                for (const auto& v : es->variants) {
                    info.variants.push_back(v.name);
                }
                enums_[es->name] = info;
            }
        }
    }

    void checkStmt(Stmt* stmt) {
        if (!stmt) return;
        currentLine_ = stmt->line;

        if (auto vs = dynamic_cast<VarStmt*>(stmt)) {
            std::string inferred = "any";
            if (vs->initializer) {
                inferred = inferExprType(vs->initializer.get());
            }
            if (!vs->typeAnnotation.empty()) {
                if (vs->initializer) {
                    if (!isAssignable(vs->typeAnnotation, inferred)) {
                        addError("Cannot assign value of type '" + inferred + "' to variable '" + vs->name + "' declared as '" + vs->typeAnnotation + "'");
                    }
                }
                setVarType(vs->name, vs->typeAnnotation);
            } else {
                setVarType(vs->name, inferred);
            }
            return;
        }

        if (auto fs = dynamic_cast<FunctionStmt*>(stmt)) {
            std::string prevReturn = currentFuncReturnType_;
            currentFuncReturnType_ = fs->returnType.empty() ? "any" : fs->returnType;
            pushScope();
            for (const auto& p : fs->params) {
                std::string pType = p.typeAnnotation.empty() ? "any" : p.typeAnnotation;
                setVarType(p.name, pType);
            }
            if (fs->body) {
                for (const auto& s : fs->body->statements) {
                    checkStmt(s.get());
                }
            }
            popScope();
            currentFuncReturnType_ = prevReturn;
            return;
        }

        if (auto rs = dynamic_cast<ReturnStmt*>(stmt)) {
            if (!currentFuncReturnType_.empty() && currentFuncReturnType_ != "any") {
                if (rs->values.empty()) {
                    if (currentFuncReturnType_ != "void") {
                        addError("Function requires return value of type '" + currentFuncReturnType_ + "'");
                    }
                } else {
                    std::string retType = inferExprType(rs->values[0].get());
                    if (!isAssignable(currentFuncReturnType_, retType)) {
                        addError("Return type mismatch: expected '" + currentFuncReturnType_ + "', got '" + retType + "'");
                    }
                }
            }
            return;
        }

        if (auto bs = dynamic_cast<BlockStmt*>(stmt)) {
            pushScope();
            for (const auto& s : bs->statements) {
                checkStmt(s.get());
            }
            popScope();
            return;
        }

        if (auto is = dynamic_cast<IfStmt*>(stmt)) {
            inferExprType(is->condition.get());
            checkStmt(is->thenBranch.get());
            if (is->elseBranch) checkStmt(is->elseBranch.get());
            return;
        }

        if (auto ws = dynamic_cast<WhileStmt*>(stmt)) {
            inferExprType(ws->condition.get());
            checkStmt(ws->body.get());
            return;
        }

        if (auto fis = dynamic_cast<ForInStmt*>(stmt)) {
            pushScope();
            setVarType(fis->keyVar, "any");
            if (!fis->valVar.empty()) setVarType(fis->valVar, "any");
            checkStmt(fis->body.get());
            popScope();
            return;
        }

        if (auto es = dynamic_cast<ExprStmt*>(stmt)) {
            inferExprType(es->expr.get());
            return;
        }

        if (auto tcs = dynamic_cast<TryCatchStmt*>(stmt)) {
            if (tcs->tryBody) checkStmt(tcs->tryBody.get());
            if (tcs->catchBody) {
                pushScope();
                if (!tcs->errorParam.empty()) setVarType(tcs->errorParam, "any");
                checkStmt(tcs->catchBody.get());
                popScope();
            }
            if (tcs->finallyBody) checkStmt(tcs->finallyBody.get());
            return;
        }
    }

    std::string inferExprType(Expr* expr) {
        if (!expr) return "any";

        if (auto lit = dynamic_cast<LiteralExpr*>(expr)) {
            switch (lit->kind) {
                case LiteralExpr::Kind::Number: return "number";
                case LiteralExpr::Kind::String: return "string";
                case LiteralExpr::Kind::Bool: return "bool";
                case LiteralExpr::Kind::Null: return "null";
            }
            return "any";
        }

        if (auto id = dynamic_cast<IdentifierExpr*>(expr)) {
            if (enums_.find(id->name) != enums_.end()) return id->name;
            if (structs_.find(id->name) != structs_.end()) return id->name;
            if (hasVar(id->name)) return getVarType(id->name);
            return "any";
        }

        if (auto bin = dynamic_cast<BinaryExpr*>(expr)) {
            std::string leftType = inferExprType(bin->left.get());
            std::string rightType = inferExprType(bin->right.get());

            if (bin->op == "+") {
                if (leftType == "string" || rightType == "string") return "string";
                if (isNumericType(leftType) && isNumericType(rightType)) return "number";
                return "any";
            }

            if (bin->op == "-" || bin->op == "*" || bin->op == "/" || bin->op == "%") {
                if (leftType != "any" && !isNumericType(leftType)) {
                    addError("Operator '" + bin->op + "' cannot be applied to type '" + leftType + "'");
                }
                if (rightType != "any" && !isNumericType(rightType)) {
                    addError("Operator '" + bin->op + "' cannot be applied to type '" + rightType + "'");
                }
                return "number";
            }

            if (bin->op == "<" || bin->op == "<=" || bin->op == ">" || bin->op == ">=") {
                if (leftType != "any" && !isNumericType(leftType)) {
                    addError("Comparison operator '" + bin->op + "' cannot be applied to type '" + leftType + "'");
                }
                if (rightType != "any" && !isNumericType(rightType)) {
                    addError("Comparison operator '" + bin->op + "' cannot be applied to type '" + rightType + "'");
                }
                return "bool";
            }

            if (bin->op == "==" || bin->op == "!=") {
                return "bool";
            }

            if (bin->op == "&&" || bin->op == "||") {
                return "bool";
            }

            return "any";
        }

        if (auto un = dynamic_cast<UnaryExpr*>(expr)) {
            std::string rightType = inferExprType(un->right.get());
            if (un->op == "!") return "bool";
            if (un->op == "-" || un->op == "+") {
                if (rightType != "any" && !isNumericType(rightType)) {
                    addError("Unary operator '" + un->op + "' cannot be applied to type '" + rightType + "'");
                }
                return "number";
            }
            if (un->op == "typeof") return "string";
            return "any";
        }

        if (auto call = dynamic_cast<CallExpr*>(expr)) {
            auto fit = functions_.find(call->callee);
            if (fit != functions_.end()) {
                const auto& finfo = fit->second;
                for (size_t i = 0; i < call->args.size() && i < finfo.params.size(); ++i) {
                    std::string argType = inferExprType(call->args[i].get());
                    const std::string& paramType = finfo.params[i].second;
                    if (!paramType.empty() && paramType != "any") {
                        if (!isAssignable(paramType, argType)) {
                            addError("Argument " + std::to_string(i + 1) + " of '" + call->callee + "' expects type '" + paramType + "', got '" + argType + "'");
                        }
                    }
                }
                return finfo.returnType;
            }
            for (const auto& a : call->args) {
                inferExprType(a.get());
            }
            return "any";
        }

        if (auto dcall = dynamic_cast<DynCallExpr*>(expr)) {
            inferExprType(dcall->callee.get());
            for (const auto& a : dcall->args) {
                inferExprType(a.get());
            }
            return "any";
        }

        if (auto sinst = dynamic_cast<StructInstExpr*>(expr)) {
            for (const auto& f : sinst->fields) {
                inferExprType(f.second.get());
            }
            return sinst->typeName;
        }

        if (dynamic_cast<TemplateLiteralExpr*>(expr)) {
            return "string";
        }

        if (auto nce = dynamic_cast<NullCoalesceExpr*>(expr)) {
            std::string lt = inferExprType(nce->left.get());
            std::string rt = inferExprType(nce->right.get());
            return lt != "null" && lt != "any" ? lt : rt;
        }

        if (auto oce = dynamic_cast<OptionalChainExpr*>(expr)) {
            inferExprType(oce->object.get());
            return "any";
        }

        if (dynamic_cast<RangeExpr*>(expr)) {
            return "array";
        }

        if (dynamic_cast<SliceExpr*>(expr)) {
            return "array";
        }

        return "any";
    }
};
