#ifndef CLOX_SCANNER_H
#define CLOX_SCANNER_H

typedef enum {
    TKN_LParen,
    TKN_RParen,
    TKN_LBrace,
    TKN_RBrace,
    TKN_Comma,
    TKN_Dot,
    TKN_Minus,
    TKN_Plus,
    TKN_Semicolon,
    TKN_Slash,
    TKN_Star,

    TKN_Bang,
    TKN_BangEq,
    TKN_Eq,
    TKN_EqEq,
    TKN_Greater,
    TKN_GreaterEq,
    TKN_Less,
    TKN_LessEq,

    TKN_Ident,
    TKN_String,
    TKN_Number,

    TKN_And,
    TKN_Class,
    TKN_Else,
    TKN_False,
    TKN_For,
    TKN_Fun,
    TKN_If,
    TKN_Nil,
    TKN_Or,
    TKN_Print,
    TKN_Return,
    TKN_Super,
    TKN_This,
    TKN_True,
    TKN_Var,
    TKN_While,

    TKN_Err,
    TKN_EOF,
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int len;
    int line;
} Token;

void initScanner(const char *src);
Token scanToken();

#endif
