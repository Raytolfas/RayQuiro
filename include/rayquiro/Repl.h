#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "Lexer.h"
#include "Parser.h"
#include "Interpreter.h"

class Repl {
private:
    static std::string readLineWithHistory(const std::string& prompt, std::vector<std::string>& history) {
        std::cout << prompt;
        std::cout.flush();

#ifdef _WIN32
        std::string current;
        int historyIndex = static_cast<int>(history.size());
        size_t cursor = 0;

        auto redraw = [&](const std::string& line, size_t pos) {
            std::cout << "\r\033[K" << prompt << line;
            if (line.size() > pos) {
                std::cout << "\033[" << (line.size() - pos) << "D";
            }
            std::cout.flush();
        };

        while (true) {
            int ch = _getch();
            if (ch == 0 || ch == 224) {
                int code = _getch();
                if (code == 72) {
                    if (!history.empty() && historyIndex > 0) {
                        historyIndex--;
                        current = history[historyIndex];
                        cursor = current.size();
                        redraw(current, cursor);
                    }
                } else if (code == 80) {
                    if (historyIndex + 1 < static_cast<int>(history.size())) {
                        historyIndex++;
                        current = history[historyIndex];
                        cursor = current.size();
                        redraw(current, cursor);
                    } else if (historyIndex + 1 == static_cast<int>(history.size())) {
                        historyIndex++;
                        current.clear();
                        cursor = 0;
                        redraw(current, cursor);
                    }
                } else if (code == 75) {
                    if (cursor > 0) {
                        cursor--;
                        std::cout << "\033[1D";
                        std::cout.flush();
                    }
                } else if (code == 77) {
                    if (cursor < current.size()) {
                        cursor++;
                        std::cout << "\033[1C";
                        std::cout.flush();
                    }
                }
            } else if (ch == '\r' || ch == '\n') {
                std::cout << "\n";
                if (!current.empty() && (history.empty() || history.back() != current)) {
                    history.push_back(current);
                }
                return current;
            } else if (ch == '\b') {
                if (cursor > 0) {
                    current.erase(cursor - 1, 1);
                    cursor--;
                    redraw(current, cursor);
                }
            } else if (ch == 3) {
                std::cout << "^C\n";
                return "";
            } else if (ch >= 32) {
                current.insert(cursor, 1, static_cast<char>(ch));
                cursor++;
                redraw(current, cursor);
            }
        }
#else
        std::string line;
        if (!std::getline(std::cin, line)) {
            return "exit";
        }
        if (!line.empty() && (history.empty() || history.back() != line)) {
            history.push_back(line);
        }
        return line;
#endif
    }

public:
    static int run(Interpreter& interpreter) {
        std::cout << "\033[1;36mRayQuiro\033[0m 1.0 REPL\n";
        std::cout << "Type \"exit\" or press Ctrl+C to quit.\n\n";

        interpreter.initGlobals();
        std::vector<std::unique_ptr<Stmt>> sessionStmts;
        std::vector<std::string> history;
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
            std::string prompt = buffer.empty() ? "\033[1;32m>>>\033[0m " : "\033[1;33m...\033[0m ";
            std::string line = readLineWithHistory(prompt, history);

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