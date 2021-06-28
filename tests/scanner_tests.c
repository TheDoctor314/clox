#include <string.h>

#include "ctest.h"
#include "scanner.h"

void check_tokens(const Token *expected, size_t len) {
    Token token;

    for (size_t i = 0; i < len; i++) {
        token = scanToken();
        ASSERT_EQUAL(expected[i].type, token.type);
        ASSERT_EQUAL(expected[i].line, token.line);
        ASSERT_EQUAL(expected[i].len, token.len);
        ASSERT_EQUAL(strncmp(expected[i].start, token.start, token.len), 0);
    }
}

CTEST(scanner, keywords) {
    initScanner("and class else false for fun if \n \
    nil or return super this true var while");

    const Token expected[] = {
        {TKN_And, "and", 3, 1},     {TKN_Class, "class", 5, 1},
        {TKN_Else, "else", 4, 1},   {TKN_False, "false", 5, 1},
        {TKN_For, "for", 3, 1},     {TKN_Fun, "fun", 3, 1},
        {TKN_If, "if", 2, 1},       {TKN_Nil, "nil", 3, 2},
        {TKN_Or, "or", 2, 2},       {TKN_Return, "return", 6, 2},
        {TKN_Super, "super", 5, 2}, {TKN_This, "this", 4, 2},
        {TKN_True, "true", 4, 2},   {TKN_Var, "var", 3, 2},
        {TKN_While, "while", 5, 2}, {TKN_EOF, "", 0, 2},
    };

    check_tokens(expected, sizeof(expected) / sizeof(expected[0]));
}

CTEST(scanner, symbols) {
    initScanner("(){};,+-*!===<=>=!/=.");

    const Token expected[] = {
        {TKN_LParen, "(", 1, 1},     {TKN_RParen, ")", 1, 1},
        {TKN_LBrace, "{", 1, 1},     {TKN_RBrace, "}", 1, 1},
        {TKN_Semicolon, ";", 1, 1},  {TKN_Comma, ",", 1, 1},
        {TKN_Plus, "+", 1, 1},       {TKN_Minus, "-", 1, 1},
        {TKN_Star, "*", 1, 1},       {TKN_BangEq, "!=", 2, 1},
        {TKN_EqEq, "==", 2, 1},      {TKN_LessEq, "<=", 2, 1},
        {TKN_GreaterEq, ">=", 2, 1}, {TKN_Bang, "!", 1, 1},
        {TKN_Slash, "/", 1, 1},      {TKN_Eq, "=", 1, 1},
        {TKN_Dot, ".", 1, 1},        {TKN_EOF, "", 0, 1},
    };

    check_tokens(expected, sizeof(expected) / sizeof(expected[0]));
}

CTEST(scanner, whitespace) {
    initScanner("space    tabs\t\t\t\tnewlines\n \
    \n \
    // Should be ignored properly\n \
    \n \
    end");

    const Token expected[] = {
        {TKN_Ident, "space", 5, 1},    {TKN_Ident, "tabs", 4, 1},
        {TKN_Ident, "newlines", 8, 1}, {TKN_Ident, "end", 3, 5},
        {TKN_EOF, "", 0, 5},
    };

    check_tokens(expected, sizeof(expected) / sizeof(expected[0]));
}

CTEST(scanner, strings) {
    initScanner("\"\"\n \
    \"string\" \n \
    ");

    const Token expected[] = {
        {TKN_String, "\"\"", 2, 1},
        {TKN_String, "\"string\"", 8, 2},
        {TKN_EOF, "", 0, 3},
    };

    check_tokens(expected, sizeof(expected) / sizeof(expected[0]));
}
