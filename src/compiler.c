#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "log.h"
#include "object.h"
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

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    Local locals[UINT8_MAX + 1];
    int localCount;
    int scopeDepth;
} Compiler;

Parser parser;
Compiler *current = NULL;
Chunk *compiling_chunk = NULL;

static Chunk *current_chunk() { return compiling_chunk; }

static void error_at_current(const char *msg);
static void error(const char *msg);
static void error_at(Token *token, const char *msg);
static void synchronize();

static void advance();
static void must_advance(TokenType type, const char *msg);
static bool check(TokenType type) { return parser.current.type == type; }
static bool check_advance(TokenType type);

static inline void emit_byte(uint8_t byte) {
    writeChunk(current_chunk(), byte, parser.previous.line);
}
static inline void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}
static bool identifiers_equal(Token *a, Token *b) {
    if (a->len != b->len)
        return false;

    return memcmp(a->start, b->start, a->len) == 0;
}

static int resolve_local(Compiler *compiler, Token *name);

// token parsing functions
static void expression();
static void number(bool);
static void grouping(bool);
static void unary(bool);
static void binary(bool);
static void literal(bool);
static void string(bool);
static void variable(bool);

// statement parsing
static void declaration();
static void varDeclaration();
static void statement();
static void printStatement();
static void expressionStatement();
static void block();
static void ifStatement();

static ParseRule *getRule(TokenType type);
static void parse_precedence(Precedence prec);

static void initCompiler(Compiler *c);

bool compile(const char *src, Chunk *chunk) {
    initScanner(src);
    Compiler compiler;
    initCompiler(&compiler);
    compiling_chunk = chunk;

    parser.hadErr = false;
    parser.panicMode = false;

    advance();
    while (!check_advance(TKN_EOF)) {
        declaration();
    }

    emit_byte(OP_RETURN);

    if (!parser.hadErr) {
        disassembleChunk(chunk, "code");
    }

    return !parser.hadErr;
}

static void initCompiler(Compiler *c) {
    c->localCount = 0;
    c->scopeDepth = 0;
    current = c;
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
    if (check(type)) {
        advance();
        return;
    }

    error_at_current(msg);
}

static bool check_advance(TokenType type) {
    if (!check(type))
        return false;

    advance();
    return true;
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
    [TKN_Ident] = {variable, NULL, PREC_NONE},
    [TKN_String] = {string, NULL, PREC_NONE},
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

static void number(bool canAssign) {
    double val = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(val));
}

static void grouping(bool canAssign) {
    expression();
    must_advance(TKN_RParen, "Expect ')' after expression");
}

static void unary(bool canAssign) {
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

static void binary(bool canAssign) {
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

static void literal(bool canAssign) {
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

static void string(bool canAssign) {
    // trim the quotation marks
    emit_constant(OBJ_VAL(
        copyString(parser.previous.start + 1, parser.previous.len - 2)));
}

static uint8_t identifier_constant(Token *name) {
    return make_constant(OBJ_VAL(copyString(name->start, name->len)));
}
static void add_local(Token name) {
    if (current->localCount == UINT8_MAX + 1) {
        error("Too many local variables in function");
        return;
    }

    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1; // sentinel for uninitialized state
}
static void declare_variable() {
    if (current->scopeDepth == 0)
        return;

    Token *name = &parser.previous;
    for (int i = current->localCount; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
            break;

        if (identifiers_equal(name, &local->name)) {
            error("Variable already defined with this name in this scope");
        }
    }

    add_local(*name);
}
static void named_variable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolve_local(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifier_constant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && check_advance(TKN_Eq)) {
        expression();
        emit_bytes(setOp, arg);
    } else {
        emit_bytes(getOp, arg);
    }
}
static void variable(bool canAssign) {
    named_variable(parser.previous, canAssign);
}

static ParseRule *getRule(TokenType type) { return &rules[type]; }
static void parse_precedence(Precedence prec) {
    advance();
    ParseFn prefix_rule = getRule(parser.previous.type)->prefix;
    if (!prefix_rule) {
        error("Expect expression");
        return;
    }

    bool canAssign = prec <= PREC_ASSIGNMENT;
    prefix_rule(canAssign);

    while (prec <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = getRule(parser.previous.type)->infix;
        infix_rule(canAssign);
    }

    if (canAssign && check_advance(TKN_Eq)) {
        error("Invalid assignment target");
    }
}

static uint8_t parse_variable(const char *msg) {
    must_advance(TKN_Ident, msg);

    declare_variable();
    if (current->scopeDepth > 0)
        return 0;

    return identifier_constant(&parser.previous);
}

static void mark_init() {
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}
static void define_variable(uint8_t global) {
    if (current->scopeDepth > 0) {
        mark_init();
        return;
    }

    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static void declaration() {
    if (check_advance(TKN_Var)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode)
        synchronize();
}

static void varDeclaration() {
    uint8_t global = parse_variable("Expect variable name");

    if (check_advance(TKN_Eq)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }

    must_advance(TKN_Semicolon, "Expect ';' after variable declaration");
    define_variable(global);
}

static void begin_scope() { current->scopeDepth++; }
static void end_scope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
               current->scopeDepth) {
        emit_byte(OP_POP);
        current->localCount--;
    }
}

static void statement() {
    if (check_advance(TKN_Print)) {
        printStatement();
    } else if (check_advance(TKN_If)) {
        ifStatement();
    } else if (check_advance(TKN_LBrace)) {
        begin_scope();
        block();
        end_scope();
    } else {
        expressionStatement();
    }
}

static void printStatement() {
    expression();
    must_advance(TKN_Semicolon, "Expect ';' after value");
    emit_byte(OP_PRINT);
}

static void expressionStatement() {
    expression();
    must_advance(TKN_Semicolon, "Expect ';' after value");
    emit_byte(OP_POP);
}

static void block() {
    while (!check(TKN_RBrace) && !check(TKN_EOF)) {
        declaration();
    }

    must_advance(TKN_RBrace, "Expect '}' after block");
}

static int emit_jump(uint8_t inst) {
    emit_byte(inst);
    // two byte jump offset
    emit_byte(0xff);
    emit_byte(0xff);

    return current_chunk()->len - 2;
}
static void patch_jump(int offset) {
    // -2 to compensate for the two byte offset itself
    int jump = current_chunk()->len - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over");
    }

    // higher byte stored first
    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}
static void ifStatement() {
    must_advance(TKN_LParen, "Expect '(' after 'if'");
    expression();
    must_advance(TKN_RParen, "EXpect ')' after condition");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (check_advance(TKN_Else))
        statement();

    patch_jump(else_jump);
}

/* if we get a parse error, we skip tokens indiscriminately
 * until we reach the boundary of the next statement.
 * We find the boundary if the previous token was a semicolon
 * or if the current token is one used to start a statement.*/
static void synchronize() {
    parser.panicMode = false;

    while (!check(TKN_EOF)) {
        if (parser.previous.type == TKN_Semicolon)
            return;

        switch (parser.current.type) {
        case TKN_Class:
        case TKN_Fun:
        case TKN_Var:
        case TKN_For:
        case TKN_If:
        case TKN_While:
        case TKN_Print:
        case TKN_Return:
            return;
        default:;
        }

        advance();
    }
}

static int resolve_local(Compiler *compiler, Token *name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error("Cannot read variable in its own initializer");
            }
            return i;
        }
    }

    return -1;
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
