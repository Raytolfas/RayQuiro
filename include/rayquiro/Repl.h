#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "Lexer.h"
#include "Parser.h"
#include "Interpreter.h"

class Repl {
public:
    static int run(Interpreter& interpreter) {
        std::cout << "\033[1;36mRayQuiro\033[0m 0.2.1 REPL\n";
        std::cout << "Type \"exit\" or press Ctrl+C to quit.\n\n";

        interpreter.initGlobals();
        std::vector<std::unique_ptr<Stmt>> sessionStmts;
        std::string buffer;

        auto countBraceBalance = [](const std::string& str) -> int {
            int balance = 0;
            bool inStr = false;
            char quote = '\0';
            for (size_t i = 0; i < str.size(); ++i) {
                char c = str[i];
                if (inStr) {
                    if (c == '\\' && i + 1 < str.size()) {
                        ++i;
                    } else if (c == quote) {
                        inStr = false;
                    }
                } else {
                    if (c == '"' || c == '\'') {
                        inStr = true;
                        quote = c;
                    } else if (c == '{' || c == '(' || c == '[') {
                        ++balance;
                    } else if (c == '}' || c == ')' || c == ']') {
                        --balance;
                    }
                }
            }
            return balance;
        };

        while (true) {
            if (buffer.empty()) {
                std::cout << "\033[1;32m>>>\033[0m ";
            } else {
                std::cout << "\033[1;33m...\033[0m ";
            }
            std::cout.flush();

            std::string line;
            if (!std::getline(std::cin, line)) {
                std::cout << "\n";
                break;
            }

            if (buffer.empty()) {
                std::string trimmed = line;
                while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) trimmed.erase(trimmed.begin());
                while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' || trimmed.back() == '\r')) trimmed.pop_back();

                if (trimmed == "exit" || trimmed == "quit" || trimmed == "exit()" || trimmed == "quit()") {
                    break;
                }
                if (trimmed.empty()) {
                    continue;
                }
            }

            buffer += line;
            int balance = countBraceBalance(buffer);
            if (balance > 0) {
                buffer += "\n";
                continue;
            }

            std::string input = buffer;
            buffer.clear();

            try {
                Lexer lexer(input);
                auto tokens = lexer.tokenize();
                Parser parser(tokens);
                auto program = parser.parse();

                for (auto& stmt : program->statements) {
                    auto result = interpreter.executeReplStatement(stmt.get());
                    if (result.has_value()) {
                        const auto& val = result.value();
                        if (!Interpreter::isNull(val)) {
                            std::cout << Interpreter::formatReplValue(val) << "\n";
                        }
                    }
                    sessionStmts.push_back(std::move(stmt));
                }
            } catch (const std::exception& e) {
                std::cerr << "\033[1;31merror:\033[0m " << e.what() << "\n";
            }
        }
        return 0;
    }
};