#pragma once
#include <string>
#include <vector>
#include <memory>

struct Expr { virtual ~Expr() = default; };

struct LiteralExpr : public Expr {
    enum class Kind { Number, String, Bool, Null };
    Kind kind;
    std::string value;
};

struct IdentifierExpr : public Expr { std::string name; };

struct BinaryExpr : public Expr {
    std::string op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct UnaryExpr : public Expr {
    std::string op;
    std::unique_ptr<Expr> right;
};

struct PostfixExpr : public Expr {
    std::string op;
    std::string name;
};

struct CallExpr : public Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
};

struct DynCallExpr : public Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
};

struct ArrayExpr : public Expr {
    std::vector<std::unique_ptr<Expr>> elements;
};

struct ObjectExpr : public Expr {
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
};

struct IndexExpr : public Expr {
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> index;
};

struct AssignExpr : public Expr {
    std::string name;
    std::unique_ptr<Expr> value;
};

struct CompoundAssignExpr : public Expr {
    std::string name;
    std::string op;
    std::unique_ptr<Expr> value;
};

struct SetIndexExpr : public Expr {
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> value;
};

struct AwaitExpr : public Expr {
    std::unique_ptr<Expr> operand;
};

struct FuncParam {
    std::string name;
    std::unique_ptr<Expr> defaultValue;
    bool isVariadic = false;
};

struct LambdaExpr : public Expr {
    std::vector<FuncParam> params;
    std::unique_ptr<struct BlockStmt> body;
    bool isAsync = false;
};

struct TemplateLiteralExpr : public Expr {
    std::vector<std::string> parts;
    std::vector<std::unique_ptr<Expr>> exprs;
};

struct StructInstExpr : public Expr {
    std::string typeName;
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
};

struct NullCoalesceExpr : public Expr {
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct OptionalChainExpr : public Expr {
    std::unique_ptr<Expr> object;
    std::string field;
    std::unique_ptr<Expr> index;
};

struct Stmt {
    virtual ~Stmt() = default;
    int line = 0;
};

struct ImportStmt : public Stmt {
    std::string module;
    std::string alias;
};

struct ImportBinding {
    std::string name;
    std::string alias;
};

struct FromImportStmt : public Stmt {
    std::string module;
    std::vector<ImportBinding> bindings;
};

struct VarStmt : public Stmt {
    std::string name;
    bool isLet = false;
    std::unique_ptr<Expr> initializer;
};

struct ArrayDestructureStmt : public Stmt {
    std::vector<std::string> names;
    std::unique_ptr<Expr> init;
};

struct ObjectDestructureStmt : public Stmt {
    std::vector<std::pair<std::string,std::string>> bindings;
    std::unique_ptr<Expr> init;
};

struct ExprStmt : public Stmt { std::unique_ptr<Expr> expr; };

struct BlockStmt : public Stmt { std::vector<std::unique_ptr<Stmt>> statements; };

struct IfStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;
};

struct WhileStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
};

struct ForInStmt : public Stmt {
    std::string keyVar;
    std::string valVar;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<Stmt> body;
};

struct ReturnStmt : public Stmt {
    std::vector<std::unique_ptr<Expr>> values;
    Expr* value() const { return values.empty() ? nullptr : values[0].get(); }
};
struct BreakStmt  : public Stmt {};
struct ContinueStmt : public Stmt {};

struct ThrowStmt : public Stmt {
    std::unique_ptr<Expr> value;
};

struct FunctionStmt : public Stmt {
    std::string name;
    std::vector<FuncParam> params;
    std::unique_ptr<BlockStmt> body;
    bool isAsync = false;
};

struct StructStmt : public Stmt {
    std::string name;
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
    std::vector<std::string> impls;
};

struct EnumVariant {
    std::string name;
    int value = -1;
    bool hasExplicit = false;
};

struct EnumStmt : public Stmt {
    std::string name;
    std::vector<EnumVariant> variants;
};

struct InterfaceMethod {
    std::string name;
    std::vector<std::string> paramNames;
};

struct InterfaceStmt : public Stmt {
    std::string name;
    std::vector<InterfaceMethod> methods;
};

struct TryCatchStmt : public Stmt {
    std::unique_ptr<BlockStmt> tryBody;
    std::string errorParam;
    std::unique_ptr<BlockStmt> catchBody;
    std::unique_ptr<BlockStmt> finallyBody;
};

struct CaseClause {
    std::unique_ptr<Expr> value;
    std::vector<std::unique_ptr<Stmt>> body;
};

struct SwitchStmt : public Stmt {
    std::unique_ptr<Expr> subject;
    std::vector<CaseClause> cases;
};

struct LogStmt : public Stmt { std::unique_ptr<Expr> message; };

struct ProgramNode {
    std::vector<std::unique_ptr<Stmt>> statements;
};

