#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <utility>
#include "Token.h"

class Lexer {
    std::string src;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    char peek() const {
        if (pos >= src.size()) return '\0';
        return src[pos];
    }

    char peekNext() const {
        if (pos + 1 >= src.size()) return '\0';
        return src[pos + 1];
    }

    char advance() {
        char c = peek();
        if (pos < src.size()) {
            pos++;
            if (c == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
        }
        return c;
    }

    void addToken(std::vector<Token>& tokens, TokenType type, const std::string& value, int startLine, int startCol) {
        tokens.push_back({type, value, startLine, startCol});
    }

public:
    Lexer(std::string s) : src(std::move(s)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos < src.length()) {
            char c = peek();
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
                continue;
            }

            if (c == '/' && peekNext() == '/') {
                while (pos < src.length() && peek() != '\n') {
                    advance();
                }
                continue;
            }

            if (c == '/' && peekNext() == '*') {
                advance();
                advance();
                while (pos < src.length()) {
                    if (peek() == '*' && peekNext() == '/') {
                        advance();
                        advance();
                        break;
                    }
                    advance();
                }
                continue;
            }

            int startLine = line;
            int startCol = col;

            if (c == '"' && peekNext() == '"' && pos + 2 < src.size() && src[pos + 2] == '"') {
                advance();
                advance();
                advance();
                std::string res;
                bool closed = false;
                while (pos < src.length()) {
                    if (peek() == '"' && peekNext() == '"' && pos + 2 < src.size() && src[pos + 2] == '"') {
                        closed = true;
                        break;
                    }
                    res += advance();
                }
                if (!closed) {
                    throw std::runtime_error("Unterminated triple-quoted string at " + std::to_string(startLine) + ":" + std::to_string(startCol));
                }
                advance();
                advance();
                advance();
                addToken(tokens, TokenType::STRING, res, startLine, startCol);
                continue;
            }

            if (c == '"') {
                advance();
                std::string res;
                bool closed = false;
                while (pos < src.length()) {
                    if (peek() == '"') {
                        closed = true;
                        break;
                    }
                    char ch = advance();
                    if (ch == '\\') {
                        char next = advance();
                        if (next == 'n') res += '\n';
                        else if (next == 't') res += '\t';
                        else if (next == '"') res += '"';
                        else if (next == '\\') res += '\\';
                        else res += next;
                    } else {
                        res += ch;
                    }
                }
                if (!closed) {
                    throw std::runtime_error("Unterminated string at " + std::to_string(startLine) + ":" + std::to_string(startCol));
                }
                advance();
                addToken(tokens, TokenType::STRING, res, startLine, startCol);
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c))) {
                std::string n;
                while (std::isdigit(static_cast<unsigned char>(peek()))) n += advance();
                if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
                    n += advance();
                    while (std::isdigit(static_cast<unsigned char>(peek()))) n += advance();
                }
                addToken(tokens, TokenType::NUMBER, n, startLine, startCol);
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                std::string s;
                while (pos < src.size()) {
                    char ch = peek();
                    if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.') {
                        s += advance();
                    } else {
                        break;
                    }
                }
                if (s == "import") addToken(tokens, TokenType::IMPORT, s, startLine, startCol);
                else if (s == "from") addToken(tokens, TokenType::FROM, s, startLine, startCol);
                else if (s == "as") addToken(tokens, TokenType::AS, s, startLine, startCol);
                else if (s == "var") addToken(tokens, TokenType::VAR, s, startLine, startCol);
                else if (s == "let") addToken(tokens, TokenType::LET, s, startLine, startCol);
                else if (s == "fn") addToken(tokens, TokenType::FN, s, startLine, startCol);
                else if (s == "if") addToken(tokens, TokenType::IF, s, startLine, startCol);
                else if (s == "else") addToken(tokens, TokenType::ELSE, s, startLine, startCol);
                else if (s == "while") addToken(tokens, TokenType::WHILE, s, startLine, startCol);
                else if (s == "for") addToken(tokens, TokenType::FOR, s, startLine, startCol);
                else if (s == "return") addToken(tokens, TokenType::RETURN, s, startLine, startCol);
                else if (s == "break") addToken(tokens, TokenType::BREAK, s, startLine, startCol);
                else if (s == "continue") addToken(tokens, TokenType::CONTINUE, s, startLine, startCol);
                else if (s == "true") addToken(tokens, TokenType::TRUE, s, startLine, startCol);
                else if (s == "false") addToken(tokens, TokenType::FALSE, s, startLine, startCol);
                else if (s == "null") addToken(tokens, TokenType::NULL_T, s, startLine, startCol);
                else addToken(tokens, TokenType::IDENTIFIER, s, startLine, startCol);
                continue;
            }

            if (c == '=' && peekNext() == '>') {
                advance();
                advance();
                addToken(tokens, TokenType::ARROW_LOG, "=>", startLine, startCol);
                continue;
            }

            if (c == '=' && peekNext() == '=') {
                advance();
                advance();
                addToken(tokens, TokenType::EQUAL_EQUAL, "==", startLine, startCol);
                continue;
            }

            if (c == '!' && peekNext() == '=') {
                advance();
                advance();
                addToken(tokens, TokenType::BANG_EQUAL, "!=", startLine, startCol);
                continue;
            }

            if (c == '<' && peekNext() == '=') {
                advance();
                advance();
                addToken(tokens, TokenType::LTE, "<=", startLine, startCol);
                continue;
            }

            if (c == '>' && peekNext() == '=') {
                advance();
                advance();
                addToken(tokens, TokenType::GTE, ">=", startLine, startCol);
                continue;
            }

            if (c == '&' && peekNext() == '&') {
                advance();
                advance();
                addToken(tokens, TokenType::AND_AND, "&&", startLine, startCol);
                continue;
            }

            if (c == '|' && peekNext() == '|') {
                advance();
                advance();
                addToken(tokens, TokenType::OR_OR, "||", startLine, startCol);
                continue;
            }

            if (c == '=') { advance(); addToken(tokens, TokenType::EQUALS, "=", startLine, startCol); continue; }
            if (c == '+') { advance(); addToken(tokens, TokenType::PLUS, "+", startLine, startCol); continue; }
            if (c == '-') { advance(); addToken(tokens, TokenType::MINUS, "-", startLine, startCol); continue; }
            if (c == '*') { advance(); addToken(tokens, TokenType::STAR, "*", startLine, startCol); continue; }
            if (c == '/') { advance(); addToken(tokens, TokenType::SLASH, "/", startLine, startCol); continue; }
            if (c == '%') { advance(); addToken(tokens, TokenType::PERCENT, "%", startLine, startCol); continue; }
            if (c == '!') { advance(); addToken(tokens, TokenType::BANG, "!", startLine, startCol); continue; }
            if (c == '<') { advance(); addToken(tokens, TokenType::LT, "<", startLine, startCol); continue; }
            if (c == '>') { advance(); addToken(tokens, TokenType::GT, ">", startLine, startCol); continue; }
            if (c == '(') { advance(); addToken(tokens, TokenType::LPAREN, "(", startLine, startCol); continue; }
            if (c == ')') { advance(); addToken(tokens, TokenType::RPAREN, ")", startLine, startCol); continue; }
            if (c == '{') { advance(); addToken(tokens, TokenType::LBRACE, "{", startLine, startCol); continue; }
            if (c == '}') { advance(); addToken(tokens, TokenType::RBRACE, "}", startLine, startCol); continue; }
            if (c == '[') { advance(); addToken(tokens, TokenType::LBRACKET, "[", startLine, startCol); continue; }
            if (c == ']') { advance(); addToken(tokens, TokenType::RBRACKET, "]", startLine, startCol); continue; }
            if (c == ',') { advance(); addToken(tokens, TokenType::COMMA, ",", startLine, startCol); continue; }
            if (c == ';') { advance(); addToken(tokens, TokenType::SEMICOLON, ";", startLine, startCol); continue; }
            if (c == ':') { advance(); addToken(tokens, TokenType::COLON, ":", startLine, startCol); continue; }

            throw std::runtime_error("Unexpected character '" + std::string(1, c) + "' at " + std::to_string(startLine) + ":" + std::to_string(startCol));
        }
        tokens.push_back({TokenType::EOF_TYPE, "", line, col});
        return tokens;
    }
};
