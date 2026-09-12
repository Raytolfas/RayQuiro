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
        int stmtLine = peek().line;
        std::unique_ptr<Stmt> node;

        if (match({TokenType::IMPORT}))            node = parseImport();
        else if (match({TokenType::FROM}))         node = parseFromImport();
        else if (match({TokenType::VAR, TokenType::LET})) node = parseVar(previous().type == TokenType::LET);
        else if (match({TokenType::FN}))           node = parseFunction();
        else if (match({TokenType::IF}))           node = parseIf();
        else if (match({TokenType::WHILE}))        node = parseWhile();
        else if (match({TokenType::FOR}))          node = parseFor();
        else if (match({TokenType::SWITCH}))       node = parseSwitch();
        else if (match({TokenType::RETURN}))       node = parseReturn();
        else if (match({TokenType::BREAK}))        node = parseBreak();
        else if (match({TokenType::CONTINUE}))     node = parseContinue();
        else if (match({TokenType::TRY}))          node = parseTryCatch();
        else if (match({TokenType::ASYNC})) {
            expect(TokenType::FN, "Expected 'fn' after 'async'");
            node = parseFunction();
            static_cast<FunctionStmt*>(node.get())->isAsync = true;
        }
        else if (check(TokenType::IDENTIFIER) && peek().value == "log.info" &&
                 pos + 1 < tokens.size() && tokens[pos + 1].type == TokenType::ARROW_LOG) {
            node = parseLog();
        }
        else if (match({TokenType::THROW})) {
            auto t = std::make_unique<ThrowStmt>();
            t->value = parseExpression();
            match({TokenType::SEMICOLON});
            node = std::move(t);
        }
        else if (match({TokenType::STRUCT}))    node = parseStruct();
        else if (match({TokenType::ENUM}))      node = parseEnum();
        else if (match({TokenType::INTERFACE})) node = parseInterface();
        else if (match({TokenType::LBRACE}))    node = parseBlock();
        else                                    node = parseExprStmt();

        if (node) node->line = stmtLine;
        return node;
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

        if (match({TokenType::LBRACKET})) {
            auto node = std::make_unique<ArrayDestructureStmt>();
            while (!check(TokenType::RBRACKET) && !isAtEnd()) {
                if (check(TokenType::COMMA)) {
                    node->names.push_back("");
                } else {
                    node->names.push_back(expect(TokenType::IDENTIFIER, "Expected variable name").value);
                }
                if (!match({TokenType::COMMA})) break;
            }
            expect(TokenType::RBRACKET, "Expected ']'");
            expect(TokenType::EQUALS, "Expected '=' after destructure pattern");
            node->init = parseAssignment();
            match({TokenType::SEMICOLON});
            return node;
        }

        if (match({TokenType::LBRACE})) {
            auto node = std::make_unique<ObjectDestructureStmt>();
            while (!check(TokenType::RBRACE) && !isAtEnd()) {
                std::string key = expect(TokenType::IDENTIFIER, "Expected key name").value;
                std::string local = key;
                if (match({TokenType::COLON})) {
                    local = expect(TokenType::IDENTIFIER, "Expected local name").value;
                }
                node->bindings.push_back({key, local});
                if (!match({TokenType::COMMA})) break;
            }
            expect(TokenType::RBRACE, "Expected '}'");
            expect(TokenType::EQUALS, "Expected '=' after destructure pattern");
            node->init = parseAssignment();
            match({TokenType::SEMICOLON});
            return node;
        }

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
        fn->params = parseParamList();
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

        if (!check(TokenType::LPAREN)) {

            std::string keyVar = expect(TokenType::IDENTIFIER, "Expected variable name after 'for'").value;
            std::string valVar;
            if (match({TokenType::COMMA})) {
                valVar = expect(TokenType::IDENTIFIER, "Expected second variable name").value;
            }
            expect(TokenType::IN, "Expected 'in' after for variable");
            auto iterable = parseExpression();
            expect(TokenType::LBRACE, "Expected '{' after for-in iterable");
            auto body = parseBlock();
            auto node = std::make_unique<ForInStmt>();
            node->keyVar = keyVar;
            node->valVar = valVar;
            node->iterable = std::move(iterable);
            node->body = std::move(body);
            return node;
        }

        expect(TokenType::LPAREN, "Expected '(' after 'for'");
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

    std::unique_ptr<Stmt> parseSwitch() {
        expect(TokenType::LPAREN, "Expected '(' after 'switch'");
        auto subject = parseExpression();
        expect(TokenType::RPAREN, "Expected ')' after switch subject");
        expect(TokenType::LBRACE, "Expected '{' after switch(...)");
        auto node = std::make_unique<SwitchStmt>();
        node->subject = std::move(subject);
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            CaseClause clause;
            if (match({TokenType::CASE})) {
                clause.value = parseExpression();
                expect(TokenType::COLON, "Expected ':' after case value");
            } else if (match({TokenType::DEFAULT})) {
                expect(TokenType::COLON, "Expected ':' after default");
                clause.value = nullptr;
            } else {
                throw error("Expected 'case' or 'default'", peek());
            }
            while (!check(TokenType::CASE) && !check(TokenType::DEFAULT) && !check(TokenType::RBRACE) && !isAtEnd()) {
                clause.body.push_back(parseStatement());
            }
            node->cases.push_back(std::move(clause));
        }
        expect(TokenType::RBRACE, "Expected '}' after switch body");
        return node;
    }

    std::unique_ptr<Stmt> parseReturn() {
        auto node = std::make_unique<ReturnStmt>();
        if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE) && !isAtEnd()) {
            node->values.push_back(parseAssignment());
            while (match({TokenType::COMMA})) {
                node->values.push_back(parseAssignment());
            }
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

    std::vector<FuncParam> parseParamList() {
        std::vector<FuncParam> params;
        while (!check(TokenType::RPAREN) && !isAtEnd()) {
            FuncParam p;
            if (match({TokenType::DOT_DOT_DOT})) {
                p.isVariadic = true;
                p.name = expect(TokenType::IDENTIFIER, "Expected parameter name after ...").value;
                params.push_back(std::move(p));
                break;
            }
            p.name = expect(TokenType::IDENTIFIER, "Expected parameter name").value;
            if (match({TokenType::EQUALS})) {
                p.defaultValue = parseAssignment();
            }
            params.push_back(std::move(p));
            if (!match({TokenType::COMMA})) break;
        }
        return params;
    }

    std::unique_ptr<Stmt> parseStruct() {
        std::string name = expect(TokenType::IDENTIFIER, "Expected struct name").value;
        auto node = std::make_unique<StructStmt>();
        node->name = name;

        if (match({TokenType::IMPL})) {
            node->impls.push_back(expect(TokenType::IDENTIFIER, "Expected interface name after 'impl'").value);
            while (match({TokenType::COMMA})) {
                node->impls.push_back(expect(TokenType::IDENTIFIER, "Expected interface name").value);
            }
        }
        expect(TokenType::LBRACE, "Expected '{' after struct name");
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            std::string fieldName = expect(TokenType::IDENTIFIER, "Expected field name or method").value;
            expect(TokenType::COLON, "Expected ':' after field name");
            node->fields.push_back({fieldName, parseAssignment()});
            match({TokenType::COMMA});
        }
        expect(TokenType::RBRACE, "Expected '}' after struct fields");
        return node;
    }

    std::unique_ptr<Stmt> parseEnum() {
        std::string name = expect(TokenType::IDENTIFIER, "Expected enum name").value;
        expect(TokenType::LBRACE, "Expected '{' after enum name");
        auto node = std::make_unique<EnumStmt>();
        node->name = name;
        int autoVal = 0;
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            EnumVariant variant;
            variant.name = expect(TokenType::IDENTIFIER, "Expected enum variant name").value;
            if (match({TokenType::EQUALS})) {
                Token numTok = expect(TokenType::NUMBER, "Expected integer value after '='");
                variant.value = static_cast<int>(std::stod(numTok.value));
                variant.hasExplicit = true;
                autoVal = variant.value + 1;
            } else {
                variant.value = autoVal++;
            }
            node->variants.push_back(std::move(variant));
            match({TokenType::COMMA});
        }
        expect(TokenType::RBRACE, "Expected '}' after enum variants");
        return node;
    }

    std::unique_ptr<Stmt> parseInterface() {
        std::string name = expect(TokenType::IDENTIFIER, "Expected interface name").value;
        expect(TokenType::LBRACE, "Expected '{' after interface name");
        auto node = std::make_unique<InterfaceStmt>();
        node->name = name;
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            expect(TokenType::FN, "Expected 'fn' in interface body");
            InterfaceMethod method;
            method.name = expect(TokenType::IDENTIFIER, "Expected method name").value;
            expect(TokenType::LPAREN, "Expected '(' after method name");
            while (!check(TokenType::RPAREN) && !isAtEnd()) {
                method.paramNames.push_back(expect(TokenType::IDENTIFIER, "Expected param name").value);
                match({TokenType::COMMA});
            }
            expect(TokenType::RPAREN, "Expected ')'");

            if (match({TokenType::MINUS})) {
                if (check(TokenType::GT)) { advance(); }
                if (check(TokenType::IDENTIFIER)) { advance(); }
            }
            match({TokenType::SEMICOLON});
            node->methods.push_back(std::move(method));
        }
        expect(TokenType::RBRACE, "Expected '}' after interface body");
        return node;
    }

    std::unique_ptr<Stmt> parseTryCatch() {
        expect(TokenType::LBRACE, "Expected '{' after try");
        auto tryNode = std::make_unique<TryCatchStmt>();
        tryNode->tryBody = parseBlock();
        if (match({TokenType::CATCH})) {
            expect(TokenType::LPAREN, "Expected '(' after catch");
            Token errTok = expect(TokenType::IDENTIFIER, "Expected error variable name");
            tryNode->errorParam = errTok.value;
            expect(TokenType::RPAREN, "Expected ')' after catch variable");
            expect(TokenType::LBRACE, "Expected '{' after catch(...)");
            tryNode->catchBody = parseBlock();
        }
        if (match({TokenType::FINALLY})) {
            expect(TokenType::LBRACE, "Expected '{' after finally");
            tryNode->finallyBody = parseBlock();
        }
        if (!tryNode->catchBody && !tryNode->finallyBody) {
            throw error("try must have at least catch or finally", previous());
        }
        return tryNode;
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
        auto expr = parseNullCoalesce();

        if (match({TokenType::PLUS_EQUALS, TokenType::MINUS_EQUALS,
                   TokenType::STAR_EQUALS, TokenType::SLASH_EQUALS,
                   TokenType::PERCENT_EQUALS})) {
            std::string op = previous().value;
            auto rhs = parseAssignment();

            if (auto id = dynamic_cast<IdentifierExpr*>(expr.get())) {
                auto node = std::make_unique<CompoundAssignExpr>();
                node->name = id->name;
                node->op = std::string(1, op[0]);
                node->value = std::move(rhs);
                return node;
            }
            throw error("Invalid compound assignment target", previous());
        }
        if (match({TokenType::EQUALS})) {
            auto value = parseAssignment();
            if (auto id = dynamic_cast<IdentifierExpr*>(expr.get())) {
                auto assign = std::make_unique<AssignExpr>();
                assign->name = id->name;
                assign->value = std::move(value);
                return assign;
            }
            if (auto idx = dynamic_cast<IndexExpr*>(expr.get())) {
                auto setIdx = std::make_unique<SetIndexExpr>();
                setIdx->object = std::move(idx->target);
                setIdx->index = std::move(idx->index);
                setIdx->value = std::move(value);
                return setIdx;
            }
            throw error("Invalid assignment target", previous());
        }
        return expr;
    }

    std::unique_ptr<Expr> parseNullCoalesce() {
        auto expr = parseOr();
        while (match({TokenType::QUESTION_QUESTION})) {
            auto right = parseOr();
            auto node = std::make_unique<NullCoalesceExpr>();
            node->left  = std::move(expr);
            node->right = std::move(right);
            expr = std::move(node);
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

        if (match({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
            std::string op = previous().value;
            Token name = expect(TokenType::IDENTIFIER, "Expected variable after '" + op + "'");

            auto node = std::make_unique<CompoundAssignExpr>();
            node->name = name.value;
            node->op = (op == "++") ? "+" : "-";
            auto one = std::make_unique<LiteralExpr>();
            one->kind = LiteralExpr::Kind::Number; one->value = "1";
            node->value = std::move(one);
            return node;
        }
        if (match({TokenType::AWAIT})) {
            auto operand = parseUnary();
            auto awaitExpr = std::make_unique<AwaitExpr>();
            awaitExpr->operand = std::move(operand);
            return awaitExpr;
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

                if (auto id = dynamic_cast<IdentifierExpr*>(expr.get())) {
                    auto call = std::make_unique<CallExpr>();
                    call->callee = id->name;
                    call->args = std::move(args);
                    expr = std::move(call);
                } else {

                    auto call = std::make_unique<DynCallExpr>();
                    call->callee = std::move(expr);
                    call->args = std::move(args);
                    expr = std::move(call);
                }
            } else if (match({TokenType::LBRACKET})) {
                auto index = parseExpression();
                expect(TokenType::RBRACKET, "Expected ']'");
                auto idx = std::make_unique<IndexExpr>();
                idx->target = std::move(expr);
                idx->index = std::move(index);
                expr = std::move(idx);
            } else if (match({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {

                std::string op = previous().value;
                if (auto id = dynamic_cast<IdentifierExpr*>(expr.get())) {
                    auto post = std::make_unique<PostfixExpr>();
                    post->op = op;
                    post->name = id->name;
                    expr = std::move(post);
                } else {
                    throw error("Postfix '" + op + "' requires variable", previous());
                }
            } else if (match({TokenType::QUESTION_DOT})) {

                auto oc = std::make_unique<OptionalChainExpr>();
                oc->object = std::move(expr);
                if (check(TokenType::LBRACKET)) {
                    advance();
                    oc->index = parseExpression();
                    expect(TokenType::RBRACKET, "Expected ']' after ?[");
                } else {
                    Token field = expect(TokenType::IDENTIFIER, "Expected field name after ?.");
                    oc->field = field.value;
                }
                expr = std::move(oc);
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
        if (match({TokenType::TEMPLATE_STRING})) {
            return parseTemplateLiteral(previous().value);
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

        if (match({TokenType::FN})) {
            return parseLambda(false);
        }
        if (match({TokenType::ASYNC})) {
            expect(TokenType::FN, "Expected 'fn' after 'async'");
            return parseLambda(true);
        }

        if (match({TokenType::LBRACE})) {
            return parseObjectLiteral();
        }
        if (match({TokenType::IDENTIFIER})) {
            std::string name = previous().value;

            if (check(TokenType::LBRACE)
                && pos + 1 < tokens.size() && tokens[pos + 1].type == TokenType::IDENTIFIER
                && pos + 2 < tokens.size() && tokens[pos + 2].type == TokenType::COLON) {
                advance();
                auto inst = std::make_unique<StructInstExpr>();
                inst->typeName = name;
                while (!check(TokenType::RBRACE) && !isAtEnd()) {
                    std::string fieldName = expect(TokenType::IDENTIFIER, "Expected field name").value;
                    expect(TokenType::COLON, "Expected ':'");
                    inst->fields.push_back({fieldName, parseAssignment()});
                    match({TokenType::COMMA});
                }
                expect(TokenType::RBRACE, "Expected '}'");
                return inst;
            }
            auto id = std::make_unique<IdentifierExpr>();
            id->name = name;
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

    std::unique_ptr<Expr> parseObjectLiteral() {
        auto obj = std::make_unique<ObjectExpr>();
        if (!check(TokenType::RBRACE)) {
            do {
                std::string key;
                if (check(TokenType::STRING)) {
                    key = advance().value;
                } else if (check(TokenType::IDENTIFIER)) {
                    key = advance().value;
                } else {
                    throw error("Expected string or identifier as object key", peek());
                }
                expect(TokenType::COLON, "Expected ':' after object key");
                auto val = parseExpression();
                obj->fields.emplace_back(key, std::move(val));
            } while (match({TokenType::COMMA}) && !check(TokenType::RBRACE));
        }
        expect(TokenType::RBRACE, "Expected '}' to close object literal");
        return obj;
    }

    std::unique_ptr<Expr> parseLambda(bool isAsync) {
        expect(TokenType::LPAREN, "Expected '(' after 'fn'");
        auto params = parseParamList();
        expect(TokenType::RPAREN, "Expected ')' after parameters");
        expect(TokenType::LBRACE, "Expected '{' after fn parameters");
        auto body = parseBlock();
        auto lambda = std::make_unique<LambdaExpr>();
        lambda->params = std::move(params);
        lambda->body = std::move(body);
        lambda->isAsync = isAsync;
        return lambda;
    }

    std::unique_ptr<Expr> parseTemplateLiteral(const std::string& raw) {
        auto tmpl = std::make_unique<TemplateLiteralExpr>();
        size_t i = 0;
        while (i <= raw.size()) {

            size_t start = i;
            while (i < raw.size() && !(raw[i] == '$' && i + 1 < raw.size() && raw[i+1] == '{')) {
                i++;
            }
            tmpl->parts.push_back(raw.substr(start, i - start));
            if (i >= raw.size()) break;

            i += 2;

            size_t exprStart = i;
            int depth = 1;
            while (i < raw.size() && depth > 0) {
                if (raw[i] == '{') depth++;
                else if (raw[i] == '}') depth--;
                if (depth > 0) i++;
                else i++;
            }
            std::string exprSrc = raw.substr(exprStart, i - exprStart - 1);

            Lexer innerLex(exprSrc);
            auto innerToks = innerLex.tokenize();
            Parser innerParser(innerToks);
            tmpl->exprs.push_back(innerParser.parseExpression());
        }
        return tmpl;
    }
};

