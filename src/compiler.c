#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "log.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadErr;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMP,       // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compiling_chunk = NULL;

static Chunk *current_chunk() { return compiling_chunk; }

static void error_at_current(const char *msg);
static void error(const char *msg);
static void error_at(Token *token, const char *msg);

static void advance();
static void expression();
static void must_advance(TokenType type, const char *msg);

static inline void emit_byte(uint8_t byte) {
    writeChunk(current_chunk(), byte, parser.previous.line);
}
static inline void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

// token parsing functions
static void number();
static void grouping();
static void unary();
static void binary();
static void literal();

static ParseRule *getRule(TokenType type);
static void parse_precedence(Precedence prec);

bool compile(const char *src, Chunk *chunk) {
    initScanner(src);
    compiling_chunk = chunk;

    parser.hadErr = false;
    parser.panicMode = false;

    advance();
    expression();
    must_advance(TKN_EOF, "Expect end of expression");
    emit_byte(OP_RETURN);

    if (!parser.hadErr) {
        disassembleChunk(chunk, "code");
    }

    return !parser.hadErr;
}

static void advance() {
    parser.previous = parser.current;

    while (true) {
        parser.current = scanToken();
        if (parser.current.type != TKN_Err)
            break;

        error_at_current(parser.current.start);
    }
}

static void must_advance(TokenType type, const char *msg) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    error_at_current(msg);
}

static void error_at_current(const char *msg) {
    error_at(&parser.current, msg);
}
static void error(const char *msg) { error_at(&parser.current, msg); }

#define MAX_LEN 1024
static void error_at(Token *token, const char *msg) {
    // suppress errors if we are already panicking
    if (parser.panicMode)
        return;

    parser.panicMode = true;
    char buf[MAX_LEN];
    if (token->type != TKN_Err) {
        snprintf(buf, MAX_LEN, "[Line %d] at '%.*s': %s", token->line,
                 token->len, token->start, msg);
    }

    log_error(buf);
    parser.hadErr = true;
}

#undef MAX_LEN

ParseRule rules[] = {
    [TKN_LParen] = {grouping, NULL, PREC_NONE},
    [TKN_RParen] = {NULL, NULL, PREC_NONE},
    [TKN_LBrace] = {NULL, NULL, PREC_NONE},
    [TKN_RBrace] = {NULL, NULL, PREC_NONE},
    [TKN_Comma] = {NULL, NULL, PREC_NONE},
    [TKN_Dot] = {NULL, NULL, PREC_NONE},
    [TKN_Minus] = {unary, binary, PREC_TERM},
    [TKN_Plus] = {NULL, binary, PREC_TERM},
    [TKN_Semicolon] = {NULL, NULL, PREC_NONE},
    [TKN_Slash] = {NULL, binary, PREC_FACTOR},
    [TKN_Star] = {NULL, binary, PREC_FACTOR},
    [TKN_Bang] = {unary, NULL, PREC_NONE},
    [TKN_BangEq] = {NULL, binary, PREC_EQUALITY},
    [TKN_Eq] = {NULL, NULL, PREC_NONE},
    [TKN_EqEq] = {NULL, binary, PREC_EQUALITY},
    [TKN_Greater] = {NULL, binary, PREC_COMP},
    [TKN_GreaterEq] = {NULL, binary, PREC_COMP},
    [TKN_Less] = {NULL, binary, PREC_COMP},
    [TKN_LessEq] = {NULL, binary, PREC_COMP},
    [TKN_Ident] = {NULL, NULL, PREC_NONE},
    [TKN_String] = {NULL, NULL, PREC_NONE},
    [TKN_Number] = {number, NULL, PREC_NONE},
    [TKN_And] = {NULL, NULL, PREC_NONE},
    [TKN_Class] = {NULL, NULL, PREC_NONE},
    [TKN_Else] = {NULL, NULL, PREC_NONE},
    [TKN_False] = {literal, NULL, PREC_NONE},
    [TKN_For] = {NULL, NULL, PREC_NONE},
    [TKN_Fun] = {NULL, NULL, PREC_NONE},
    [TKN_If] = {NULL, NULL, PREC_NONE},
    [TKN_Nil] = {literal, NULL, PREC_NONE},
    [TKN_Or] = {NULL, NULL, PREC_NONE},
    [TKN_Print] = {NULL, NULL, PREC_NONE},
    [TKN_Return] = {NULL, NULL, PREC_NONE},
    [TKN_Super] = {NULL, NULL, PREC_NONE},
    [TKN_This] = {NULL, NULL, PREC_NONE},
    [TKN_True] = {literal, NULL, PREC_NONE},
    [TKN_Var] = {NULL, NULL, PREC_NONE},
    [TKN_While] = {NULL, NULL, PREC_NONE},
    [TKN_Err] = {NULL, NULL, PREC_NONE},
    [TKN_EOF] = {NULL, NULL, PREC_NONE},
};

static inline void expression() { parse_precedence(PREC_ASSIGNMENT); }

static uint8_t make_constant(Value val) {
    int constant = addConstant(current_chunk(), val);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk");
        return 0;
    }

    return (uint8_t)constant;
}
static inline void emit_constant(Value val) {
    emit_bytes(OP_CONSTANT, make_constant(val));
}

static void number() {
    double val = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(val));
}

static void grouping() {
    expression();
    must_advance(TKN_RParen, "Expect ')' after expression");
}

static void unary() {
    TokenType op_type = parser.previous.type;

    // Compile the operand
    parse_precedence(
        PREC_UNARY); // same precedence to parse nested unary exprs: (!! false)

    switch (op_type) {
    case TKN_Bang:
        emit_byte(OP_NOT);
        break;
    case TKN_Minus:
        emit_byte(OP_NEGATE);
        break;
    default:
        return;
    }
}

static void binary() {
    TokenType op_type = parser.previous.type;
    ParseRule *rule = getRule(op_type);
    parse_precedence((Precedence)(rule->precedence + 1));

    switch (op_type) {
    case TKN_BangEq:
        emit_bytes(OP_EQUAL, OP_NOT);
        break;
    case TKN_EqEq:
        emit_byte(OP_EQUAL);
        break;
    case TKN_Greater:
        emit_byte(OP_GREATER);
        break;
    case TKN_GreaterEq:
        emit_bytes(OP_LESS, OP_NOT);
        break;
    case TKN_Less:
        emit_byte(OP_LESS);
        break;
    case TKN_LessEq:
        emit_bytes(OP_GREATER, OP_NOT);
        break;
    case TKN_Plus:
        emit_byte(OP_ADD);
        break;
    case TKN_Minus:
        emit_byte(OP_SUBTRACT);
        break;
    case TKN_Slash:
        emit_byte(OP_DIVIDE);
        break;
    case TKN_Star:
        emit_byte(OP_MULTIPLY);
        break;
    default:
        return;
    }
}

static void literal() {
    switch (parser.previous.type) {
    case TKN_False:
        emit_byte(OP_FALSE);
        break;
    case TKN_Nil:
        emit_byte(OP_NIL);
        break;
    case TKN_True:
        emit_byte(OP_TRUE);
        break;
    default:
        return;
    }
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }
static void parse_precedence(Precedence prec) {
    advance();
    ParseFn prefix_rule = getRule(parser.previous.type)->prefix;
    if (!prefix_rule) {
        error("Expect expression");
        return;
    }

    prefix_rule();

    while (prec <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = getRule(parser.previous.type)->infix;
        infix_rule();
    }
}

/* maybe put it in a function to print the tokens?
    int line = -1;
    while (true) {
        Token token = scanToken();
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }

        printf("%2d '%.*s'\n", token.type, token.len, token.start);

        if (token.type == TKN_EOF)
            break;
    }
*/
