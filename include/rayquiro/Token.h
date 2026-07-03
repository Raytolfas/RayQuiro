#pragma once
#include <string>

enum class TokenType {
    IMPORT, FROM, AS, VAR, LET, FN, IF, ELSE, WHILE, FOR, RETURN, BREAK, CONTINUE, TRUE, FALSE, NULL_T,
    IDENTIFIER, STRING, NUMBER,
    EQUALS, EQUAL_EQUAL, BANG_EQUAL, LT, LTE, GT, GTE,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    BANG, AND_AND, OR_OR,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    COMMA, SEMICOLON, COLON,
    ARROW_LOG,
    EOF_TYPE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int col;
};
