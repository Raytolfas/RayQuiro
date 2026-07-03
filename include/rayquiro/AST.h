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

struct CallExpr : public Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
};

struct ArrayExpr : public Expr {
    std::vector<std::unique_ptr<Expr>> elements;
};

struct IndexExpr : public Expr {
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> index;
};

struct AssignExpr : public Expr {
    std::string name;
    std::unique_ptr<Expr> value;
};

struct Stmt { virtual ~Stmt() = default; };

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

struct ReturnStmt : public Stmt { std::unique_ptr<Expr> value; };
struct BreakStmt : public Stmt {};
struct ContinueStmt : public Stmt {};

struct FunctionStmt : public Stmt {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockStmt> body;
};

struct LogStmt : public Stmt { std::unique_ptr<Expr> message; };

struct ProgramNode {
    std::vector<std::unique_ptr<Stmt>> statements;
};
