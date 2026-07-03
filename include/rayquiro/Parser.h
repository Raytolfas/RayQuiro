#pragma once
#include "Token.h"
#include "AST.h"
#include <vector>
#include <memory>
#include <stdexcept>
#include <utility>

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;

    bool isAtEnd() const {
        return peek().type == TokenType::EOF_TYPE;
    }

    const Token& peek() const {
        return tokens[pos];
    }

    const Token& previous() const {
        return tokens[pos - 1];
    }

    const Token& advance() {
        if (!isAtEnd()) pos++;
        return previous();
    }

    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    bool match(std::initializer_list<TokenType> types) {
        for (TokenType type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }
        return false;
    }

    Token expect(TokenType type, const std::string& msg) {
        if (check(type)) return advance();
        throw error(msg, peek());
    }

    std::runtime_error error(const std::string& msg, const Token& token) const {
        std::string where = " at " + std::to_string(token.line) + ":" + std::to_string(token.col);
        return std::runtime_error(msg + where);
    }

public:
    Parser(const std::vector<Token>& t) : tokens(t) {}

    std::unique_ptr<ProgramNode> parse() {
        auto program = std::make_unique<ProgramNode>();
        while (!isAtEnd()) {
            program->statements.push_back(parseStatement());
        }
        return program;
    }

private:
    std::unique_ptr<Stmt> parseStatement() {
        if (match({TokenType::IMPORT})) return parseImport();
        if (match({TokenType::FROM})) return parseFromImport();
        if (match({TokenType::VAR, TokenType::LET})) return parseVar(previous().type == TokenType::LET);
        if (match({TokenType::FN})) return parseFunction();
        if (match({TokenType::IF})) return parseIf();
        if (match({TokenType::WHILE})) return parseWhile();
        if (match({TokenType::FOR})) return parseFor();
        if (match({TokenType::RETURN})) return parseReturn();
        if (match({TokenType::BREAK})) return parseBreak();
        if (match({TokenType::CONTINUE})) return parseContinue();
        if (check(TokenType::IDENTIFIER) && peek().value == "log.info") {
            if (pos + 1 < tokens.size() && tokens[pos + 1].type == TokenType::ARROW_LOG) {
                return parseLog();
            }
        }
        if (match({TokenType::LBRACE})) return parseBlock();
        return parseExprStmt();
    }

    std::unique_ptr<Stmt> parseImport() {
        auto node = std::make_unique<ImportStmt>();
        if (check(TokenType::IDENTIFIER) || check(TokenType::STRING)) {
            node->module = advance().value;
        } else {
            throw error("Expected module name after import", peek());
        }
        if (match({TokenType::AS})) {
            node->alias = expect(TokenType::IDENTIFIER, "Expected alias name after 'as'").value;
        }
        match({TokenType::SEMICOLON});
        return node;
    }

    std::unique_ptr<Stmt> parseFromImport() {
        auto node = std::make_unique<FromImportStmt>();
        if (check(TokenType::IDENTIFIER) || check(TokenType::STRING)) {
            node->module = advance().value;
        } else {
            throw error("Expected module name after from", peek());
        }

        expect(TokenType::IMPORT, "Expected 'import' after module name");

        do {
            ImportBinding binding;
            binding.name = expect(TokenType::IDENTIFIER, "Expected imported symbol name").value;
            if (match({TokenType::AS})) {
                binding.alias = expect(TokenType::IDENTIFIER, "Expected alias name after 'as'").value;
            }
            node->bindings.push_back(std::move(binding));
        } while (match({TokenType::COMMA}));

        if (node->bindings.empty()) {
            throw error("Expected at least one symbol after import", peek());
        }

        match({TokenType::SEMICOLON});
        return node;
    }

    std::unique_ptr<Stmt> parseVar(bool isLet) {
        auto node = std::make_unique<VarStmt>();
        node->isLet = isLet;
        Token name = expect(TokenType::IDENTIFIER, "Expected variable name");
        node->name = name.value;
        if (match({TokenType::COLON})) {
            if (check(TokenType::IDENTIFIER)) advance();
        }
        if (match({TokenType::EQUALS})) {
            node->initializer = parseExpression();
        } else {
            auto lit = std::make_unique<LiteralExpr>();
            lit->kind = LiteralExpr::Kind::Null;
            node->initializer = std::move(lit);
        }
        match({TokenType::SEMICOLON});
        return node;
    }

    std::unique_ptr<Stmt> parseFunction() {
        auto fn = std::make_unique<FunctionStmt>();
        Token name = expect(TokenType::IDENTIFIER, "Expected function name");
        fn->name = name.value;
        expect(TokenType::LPAREN, "Expected '(' after function name");
        if (!check(TokenType::RPAREN)) {
            do {
                Token param = expect(TokenType::IDENTIFIER, "Expected parameter name");
                fn->params.push_back(param.value);
            } while (match({TokenType::COMMA}));
        }
        expect(TokenType::RPAREN, "Expected ')' after parameters");
        expect(TokenType::LBRACE, "Expected '{' before function body");
        fn->body = parseBlock();
        return fn;
    }

    std::unique_ptr<Stmt> parseIf() {
        expect(TokenType::LPAREN, "Expected '(' after if");
        auto condition = parseExpression();
        expect(TokenType::RPAREN, "Expected ')' after condition");
        auto thenBranch = parseStatement();
        std::unique_ptr<Stmt> elseBranch;
        if (match({TokenType::ELSE})) {
            elseBranch = parseStatement();
        }
        auto node = std::make_unique<IfStmt>();
        node->condition = std::move(condition);
        node->thenBranch = std::move(thenBranch);
        node->elseBranch = std::move(elseBranch);
        return node;
    }

    std::unique_ptr<Stmt> parseWhile() {
        expect(TokenType::LPAREN, "Expected '(' after while");
        auto condition = parseExpression();
        expect(TokenType::RPAREN, "Expected ')' after condition");
        auto body = parseStatement();
        auto node = std::make_unique<WhileStmt>();
        node->condition = std::move(condition);
        node->body = std::move(body);
        return node;
    }

    std::unique_ptr<Stmt> parseFor() {
        expect(TokenType::LPAREN, "Expected '(' after for");
        std::unique_ptr<Stmt> init;
        if (match({TokenType::SEMICOLON})) {
            init = nullptr;
        } else if (match({TokenType::VAR, TokenType::LET})) {
            init = parseVar(previous().type == TokenType::LET);
        } else {
            auto expr = parseExpression();
            expect(TokenType::SEMICOLON, "Expected ';' after for initializer");
            auto stmt = std::make_unique<ExprStmt>();
            stmt->expr = std::move(expr);
            init = std::move(stmt);
        }

        std::unique_ptr<Expr> condition;
        if (!check(TokenType::SEMICOLON)) {
            condition = parseExpression();
        } else {
            auto lit = std::make_unique<LiteralExpr>();
            lit->kind = LiteralExpr::Kind::Bool;
            lit->value = "true";
            condition = std::move(lit);
        }
        expect(TokenType::SEMICOLON, "Expected ';' after for condition");

        std::unique_ptr<Expr> increment;
        if (!check(TokenType::RPAREN)) {
            increment = parseExpression();
        }
        expect(TokenType::RPAREN, "Expected ')' after for clauses");

        auto body = parseStatement();
        if (increment) {
            auto block = std::make_unique<BlockStmt>();
            block->statements.push_back(std::move(body));
            auto incStmt = std::make_unique<ExprStmt>();
            incStmt->expr = std::move(increment);
            block->statements.push_back(std::move(incStmt));
            body = std::move(block);
        }

        auto loop = std::make_unique<WhileStmt>();
        loop->condition = std::move(condition);
        loop->body = std::move(body);

        if (init) {
            auto wrapper = std::make_unique<BlockStmt>();
            wrapper->statements.push_back(std::move(init));
            wrapper->statements.push_back(std::move(loop));
            return wrapper;
        }
        return loop;
    }

    std::unique_ptr<Stmt> parseReturn() {
        auto node = std::make_unique<ReturnStmt>();
        if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE)) {
            node->value = parseExpression();
        }
        match({TokenType::SEMICOLON});
        return node;
    }

    std::unique_ptr<Stmt> parseBreak() {
        match({TokenType::SEMICOLON});
        return std::make_unique<BreakStmt>();
    }

    std::unique_ptr<Stmt> parseContinue() {
        match({TokenType::SEMICOLON});
        return std::make_unique<ContinueStmt>();
    }

    std::unique_ptr<BlockStmt> parseBlock() {
        auto block = std::make_unique<BlockStmt>();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            block->statements.push_back(parseStatement());
        }
        expect(TokenType::RBRACE, "Expected '}' after block");
        return block;
    }

    std::unique_ptr<Stmt> parseLog() {
        advance();
        expect(TokenType::ARROW_LOG, "Expected '=>' after log.info");
        auto node = std::make_unique<LogStmt>();
        node->message = parseExpression();
        match({TokenType::SEMICOLON});
        return node;
    }

    std::unique_ptr<Stmt> parseExprStmt() {
        auto expr = parseExpression();
        match({TokenType::SEMICOLON});
        auto node = std::make_unique<ExprStmt>();
        node->expr = std::move(expr);
        return node;
    }

    std::unique_ptr<Expr> parseExpression() {
        return parseAssignment();
    }

    std::unique_ptr<Expr> parseAssignment() {
        auto expr = parseOr();
        if (match({TokenType::EQUALS})) {
            auto value = parseAssignment();
            if (auto id = dynamic_cast<IdentifierExpr*>(expr.get())) {
                auto assign = std::make_unique<AssignExpr>();
                assign->name = id->name;
                assign->value = std::move(value);
                return assign;
            }
            throw error("Invalid assignment target", previous());
        }
        return expr;
    }

    std::unique_ptr<Expr> parseOr() {
        auto expr = parseAnd();
        while (match({TokenType::OR_OR})) {
            std::string op = previous().value;
            auto right = parseAnd();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = op;
            bin->left = std::move(expr);
            bin->right = std::move(right);
            expr = std::move(bin);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseAnd() {
        auto expr = parseEquality();
        while (match({TokenType::AND_AND})) {
            std::string op = previous().value;
            auto right = parseEquality();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = op;
            bin->left = std::move(expr);
            bin->right = std::move(right);
            expr = std::move(bin);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseEquality() {
        auto expr = parseComparison();
        while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) {
            std::string op = previous().value;
            auto right = parseComparison();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = op;
            bin->left = std::move(expr);
            bin->right = std::move(right);
            expr = std::move(bin);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseComparison() {
        auto expr = parseTerm();
        while (match({TokenType::LT, TokenType::LTE, TokenType::GT, TokenType::GTE})) {
            std::string op = previous().value;
            auto right = parseTerm();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = op;
            bin->left = std::move(expr);
            bin->right = std::move(right);
            expr = std::move(bin);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseTerm() {
        auto expr = parseFactor();
        while (match({TokenType::PLUS, TokenType::MINUS})) {
            std::string op = previous().value;
            auto right = parseFactor();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = op;
            bin->left = std::move(expr);
            bin->right = std::move(right);
            expr = std::move(bin);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseFactor() {
        auto expr = parseUnary();
        while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
            std::string op = previous().value;
            auto right = parseUnary();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = op;
            bin->left = std::move(expr);
            bin->right = std::move(right);
            expr = std::move(bin);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseUnary() {
        if (match({TokenType::BANG, TokenType::MINUS})) {
            std::string op = previous().value;
            auto right = parseUnary();
            auto un = std::make_unique<UnaryExpr>();
            un->op = op;
            un->right = std::move(right);
            return un;
        }
        return parseCall();
    }

    std::unique_ptr<Expr> parseCall() {
        auto expr = parsePrimary();
        while (true) {
            if (match({TokenType::LPAREN})) {
                std::vector<std::unique_ptr<Expr>> args;
                if (!check(TokenType::RPAREN)) {
                    do {
                        args.push_back(parseExpression());
                    } while (match({TokenType::COMMA}));
                }
                expect(TokenType::RPAREN, "Expected ')' after arguments");
                auto id = dynamic_cast<IdentifierExpr*>(expr.get());
                if (!id) throw error("Can only call functions by name", previous());
                auto call = std::make_unique<CallExpr>();
                call->callee = id->name;
                call->args = std::move(args);
                expr = std::move(call);
            } else if (match({TokenType::LBRACKET})) {
                auto index = parseExpression();
                expect(TokenType::RBRACKET, "Expected ']'");
                auto idx = std::make_unique<IndexExpr>();
                idx->target = std::move(expr);
                idx->index = std::move(index);
                expr = std::move(idx);
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<Expr> parsePrimary() {
        if (match({TokenType::NUMBER})) {
            auto lit = std::make_unique<LiteralExpr>();
            lit->kind = LiteralExpr::Kind::Number;
            lit->value = previous().value;
            return lit;
        }
        if (match({TokenType::STRING})) {
            auto lit = std::make_unique<LiteralExpr>();
            lit->kind = LiteralExpr::Kind::String;
            lit->value = previous().value;
            return lit;
        }
        if (match({TokenType::TRUE})) {
            auto lit = std::make_unique<LiteralExpr>();
            lit->kind = LiteralExpr::Kind::Bool;
            lit->value = "true";
            return lit;
        }
        if (match({TokenType::FALSE})) {
            auto lit = std::make_unique<LiteralExpr>();
            lit->kind = LiteralExpr::Kind::Bool;
            lit->value = "false";
            return lit;
        }
        if (match({TokenType::NULL_T})) {
            auto lit = std::make_unique<LiteralExpr>();
            lit->kind = LiteralExpr::Kind::Null;
            lit->value = "null";
            return lit;
        }
        if (match({TokenType::IDENTIFIER})) {
            auto id = std::make_unique<IdentifierExpr>();
            id->name = previous().value;
            return id;
        }
        if (match({TokenType::LPAREN})) {
            auto expr = parseExpression();
            expect(TokenType::RPAREN, "Expected ')'");
            return expr;
        }
        if (match({TokenType::LBRACKET})) {
            auto arr = std::make_unique<ArrayExpr>();
            if (!check(TokenType::RBRACKET)) {
                do {
                    arr->elements.push_back(parseExpression());
                } while (match({TokenType::COMMA}));
            }
            expect(TokenType::RBRACKET, "Expected ']'");
            return arr;
        }
        throw error("Unexpected token", peek());
    }
};
