#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
    const char *start;
    const char *current;
    int line;
} Scanner;

Scanner scanner;

void initScanner(const char *src) {
    scanner.start = src;
    scanner.current = src;
    scanner.line = 1;
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}
static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
static inline bool is_at_end() { return *scanner.current == '\0'; }
static char advance() {
    scanner.current++;
    return scanner.current[-1];
}
static bool check_advance(char expected) {
    if (is_at_end())
        return false;

    if (*scanner.current != expected)
        return false;

    scanner.current++;
    return true;
}
static void skip_whitespace();

static Token err_token(const char *msg);
static Token make_token(TokenType type);
static Token string();
static Token number();
static Token identifier();

Token scanToken() {
    skip_whitespace();
    scanner.start = scanner.current;

    if (is_at_end())
        return make_token(TKN_EOF);

    char ch = advance();
    if (is_alpha(ch))
        return identifier();

    if (is_digit(ch))
        return number();

    switch (ch) {
    case '(':
        return make_token(TKN_LParen);
    case ')':
        return make_token(TKN_RParen);
    case '{':
        return make_token(TKN_LBrace);
    case '}':
        return make_token(TKN_RBrace);
    case ';':
        return make_token(TKN_Semicolon);
    case '.':
        return make_token(TKN_Dot);
    case '-':
        return make_token(TKN_Minus);
    case '+':
        return make_token(TKN_Plus);
    case '*':
        return make_token(TKN_Star);
    case '/':
        return make_token(TKN_Slash);
    case '!':
        return make_token(check_advance('=') ? TKN_BangEq : TKN_Bang);
    case '=':
        return make_token(check_advance('=') ? TKN_EqEq : TKN_Eq);
    case '<':
        return make_token(check_advance('=') ? TKN_LessEq : TKN_Less);
    case '>':
        return make_token(check_advance('=') ? TKN_GreaterEq : TKN_Greater);
    case '"':
        return string();
    }

    return err_token("Unexpected character");
}

static Token err_token(const char *msg) {
    Token token = {.type = TKN_EOF,
                   .start = msg,
                   .len = (int)strlen(msg),
                   .line = scanner.line};
    return token;
}
static Token make_token(TokenType type) {
    Token token = {.type = type,
                   .start = scanner.start,
                   .len = (int)(scanner.current - scanner.start),
                   .line = scanner.line};
    return token;
}

static inline char peek() { return *scanner.current; }
static inline char peek_next() {
    if (is_at_end())
        return '\0';

    return scanner.current[1];
}
static void skip_whitespace() {
    while (true) {
        char ch = peek();
        switch (ch) {
        case ' ':
        case '\t':
        case '\r':
            advance();
            break;
        case '\n':
            scanner.line++;
            advance();
            break;
        case '/':
            if (peek_next() == '/') {
                // a comment goes until the end of the line
                while (peek() != '\n' && !is_at_end())
                    advance();
            } else {
                return;
            }
        default:
            return;
        }
    }
}

static Token string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n')
            scanner.line++;

        advance();
    }

    if (is_at_end())
        return err_token("Unterminated string");

    advance(); // consume the closing quote
    return make_token(TKN_String);
}
static Token number() {
    while (is_digit(peek()))
        advance();

    // look for fractional part
    if (peek() == '.' && is_digit(peek_next())) {
        advance();
        while (is_digit(peek()))
            advance();
    }

    return make_token(TKN_Number);
}

static TokenType check_keyword(int start, int len, const char *rest,
                               TokenType type);
static TokenType ident_type() {
    switch (scanner.start[0]) {
    case 'a':
        return check_keyword(1, 2, "nd", TKN_And);
    case 'c':
        return check_keyword(1, 4, "lass", TKN_Class);
    case 'e':
        return check_keyword(1, 3, "lse", TKN_Else);
    case 'f':
        if ((scanner.current - scanner.start) > 1) {
            switch (scanner.start[1]) {
            case 'a':
                return check_keyword(2, 3, "lse", TKN_False);
            case 'o':
                return check_keyword(2, 1, "r", TKN_For);
            case 'u':
                return check_keyword(2, 1, "n", TKN_Fun);
            }
        }
        break;
    case 'i':
        return check_keyword(1, 1, "f", TKN_If);
    case 'n':
        return check_keyword(1, 2, "il", TKN_Nil);
    case 'o':
        return check_keyword(1, 1, "r", TKN_Or);
    case 'p':
        return check_keyword(1, 4, "rint", TKN_Print);
    case 'r':
        return check_keyword(1, 5, "eturn", TKN_Return);
    case 's':
        return check_keyword(1, 4, "uper", TKN_Super);
    case 't':
        if ((scanner.current - scanner.start) > 1) {
            switch (scanner.start[1]) {
            case 'h':
                return check_keyword(2, 2, "is", TKN_For);
            case 'r':
                return check_keyword(2, 2, "ue", TKN_Fun);
            }
        }
        break;
    case 'v':
        return check_keyword(1, 2, "ar", TKN_Var);
    case 'w':
        return check_keyword(1, 4, "hile", TKN_While);
    }

    return TKN_Ident;
}
static Token identifier() {
    while (is_alpha(peek()) || is_digit(peek()))
        advance();

    return make_token(ident_type());
}

static TokenType check_keyword(int start, int len, const char *rest,
                               TokenType type) {
    if (((scanner.current - scanner.start) == (start + len)) &&
        memcmp(scanner.start + start, rest, len) == 0) {
        return type;
    }

    return TKN_Ident;
}
