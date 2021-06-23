#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "log.h"
#include "memory.h"
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
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNC,
    TYPE_METHOD,
    TYPE_INIT,
    TYPE_SCRIPT,
} FuncType;

typedef struct Compiler {
    struct Compiler *enclosing;
    ObjFunction *function;
    FuncType type;

    Local locals[UINT8_MAX + 1];
    Upvalue upvalues[UINT8_MAX + 1];
    int localCount;
    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler *enclosing;
} ClassCompiler;

Parser parser;
Compiler *current = NULL;
ClassCompiler *current_class = NULL;
Chunk *compiling_chunk = NULL;

static Chunk *current_chunk() { return &current->function->chunk; }

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
static int emit_jump(uint8_t inst);
static void patch_jump(int offset);

static int resolve_local(Compiler *compiler, Token *name);
static int resolve_upvalue(Compiler *compiler, Token *name);

// token parsing functions
static void expression();
static void number(bool);
static void grouping(bool);
static void unary(bool);
static void binary(bool);
static void literal(bool);
static void string(bool);
static void variable(bool);
static void logical_and(bool);
static void logical_or(bool);
static void call(bool);
static void dot(bool);
static void this_(bool);

// statement parsing
static void declaration();
static void varDeclaration();
static void funDeclaration();
static void classDeclaration();
static void statement();
static void printStatement();
static void expressionStatement();
static void block();
static void ifStatement();
static void whileStatement();
static void forStatement();
static void returnStatement();

static void function(FuncType type);

static ParseRule *getRule(TokenType type);
static void parse_precedence(Precedence prec);

static void initCompiler(Compiler *c, FuncType type);
static ObjFunction *endCompiler();

ObjFunction *compile(const char *src) {
    initScanner(src);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadErr = false;
    parser.panicMode = false;

    advance();
    while (!check_advance(TKN_EOF)) {
        declaration();
    }

    ObjFunction *func = endCompiler();
    return parser.hadErr ? NULL : func;
}

void mark_compiler_roots() {
    Compiler *compiler = current;
    while (compiler != NULL) {
        mark_object((Obj *)compiler->function);
        compiler = compiler->enclosing;
    }
}

static void initCompiler(Compiler *c, FuncType type) {
    c->enclosing = current;
    c->function = NULL;
    c->type = type;
    c->localCount = 0;
    c->scopeDepth = 0;
    c->function = newFunction();
    current = c;

    if (type != TYPE_SCRIPT) {
        current->function->name =
            copyString(parser.previous.start, parser.previous.len);
    }

    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNC) {
        local->name.start = "this";
        local->name.len = 4;
    } else {
        local->name.start = "";
        local->name.len = 0;
    }
}

static ObjFunction *endCompiler() {
    if (current->type == TYPE_INIT) {
        emit_bytes(OP_GET_LOCAL, 0);
    } else {
        emit_byte(OP_NIL);
    }

    emit_byte(OP_RETURN);
    ObjFunction *func = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadErr) {
        disassembleChunk(current_chunk(),
                         func->name != NULL ? func->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return func;
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
static void error(const char *msg) { error_at(&parser.previous, msg); }

#define MAX_LEN 1024
static void error_at(Token *token, const char *msg) {
    // suppress errors if we are already panicking
    if (parser.panicMode)
        return;

    parser.panicMode = true;
    char buf[MAX_LEN];
    if (token->type != TKN_Err) {
        snprintf(buf, MAX_LEN, "[Line %d] at '%.*s': %s\n", token->line,
                 token->len, token->start, msg);
    }

    log_error(buf);
    parser.hadErr = true;
}

#undef MAX_LEN

ParseRule rules[] = {
    [TKN_LParen] = {grouping, call, PREC_CALL},
    [TKN_RParen] = {NULL, NULL, PREC_NONE},
    [TKN_LBrace] = {NULL, NULL, PREC_NONE},
    [TKN_RBrace] = {NULL, NULL, PREC_NONE},
    [TKN_Comma] = {NULL, NULL, PREC_NONE},
    [TKN_Dot] = {NULL, dot, PREC_CALL},
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
    [TKN_And] = {NULL, logical_and, PREC_AND},
    [TKN_Class] = {NULL, NULL, PREC_NONE},
    [TKN_Else] = {NULL, NULL, PREC_NONE},
    [TKN_False] = {literal, NULL, PREC_NONE},
    [TKN_For] = {NULL, NULL, PREC_NONE},
    [TKN_Fun] = {NULL, NULL, PREC_NONE},
    [TKN_If] = {NULL, NULL, PREC_NONE},
    [TKN_Nil] = {literal, NULL, PREC_NONE},
    [TKN_Or] = {NULL, logical_or, PREC_OR},
    [TKN_Print] = {NULL, NULL, PREC_NONE},
    [TKN_Return] = {NULL, NULL, PREC_NONE},
    [TKN_Super] = {NULL, NULL, PREC_NONE},
    [TKN_This] = {this_, NULL, PREC_NONE},
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

static void number(bool canAssign __attribute__((unused))) {
    double val = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(val));
}

static void grouping(bool canAssign __attribute__((unused))) {
    expression();
    must_advance(TKN_RParen, "Expect ')' after expression");
}

static void unary(bool canAssign __attribute__((unused))) {
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

static void binary(bool canAssign __attribute__((unused))) {
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

static void literal(bool canAssign __attribute__((unused))) {
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

static void string(bool canAssign __attribute__((unused))) {
    // trim the quotation marks
    emit_constant(OBJ_VAL(
        copyString(parser.previous.start + 1, parser.previous.len - 2)));
}

static void logical_and(bool canAssign __attribute__((unused))) {
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}
static void logical_or(bool canAssign __attribute__((unused))) {
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);
    patch_jump(end_jump);
}

static uint8_t arg_list() {
    uint8_t arg_count = 0;
    if (!check(TKN_RParen)) {
        do {
            expression();
            if (arg_count == 255) {
                error("Cannot have more than 255 arguments");
            }
            arg_count++;
        } while (check_advance(TKN_Comma));
    }

    must_advance(TKN_RParen, "Expect ')' after arguments");
    return arg_count;
}
static void call(bool canAssign __attribute__((unused))) {
    uint8_t arg_count = arg_list();
    emit_bytes(OP_CALL, arg_count);
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
    local->isCaptured = false;
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
    } else if ((arg = resolve_upvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
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

static void this_(bool canAssign __attribute__((unused))) {
    if (current_class == NULL) {
        error("Cannot use 'this' outside of a class");
        return;
    }
    variable(false);
}

// parses get and set expressions on instances
static void dot(bool canAssign) {
    must_advance(TKN_Ident, "Expect property name after '.'");
    uint8_t name = identifier_constant(&parser.previous);

    if (canAssign && check_advance(TKN_Eq)) {
        expression();
        emit_bytes(OP_SET_PROPERTY, name);
    } else {
        emit_bytes(OP_GET_PROPERTY, name);
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
    if (current->scopeDepth == 0)
        return;

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
    } else if (check_advance(TKN_Class)) {
        classDeclaration();
    } else if (check_advance(TKN_Fun)) {
        funDeclaration();
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

static void funDeclaration() {
    uint8_t global = parse_variable("Expect function name");
    mark_init();
    function(TYPE_FUNC);
    define_variable(global);
}

static void method() {
    must_advance(TKN_Ident, "Expect method name");
    uint8_t constant = identifier_constant(&parser.previous);

    FuncType type = TYPE_METHOD;
    if (parser.previous.len == 4 &&
        (memcmp(parser.previous.start, "init", 4) == 0)) {
        type = TYPE_INIT;
    }

    function(type);
    emit_bytes(OP_METHOD, constant);
}

static void classDeclaration() {
    must_advance(TKN_Ident, "Expect class name");
    Token class_name = parser.previous;
    uint8_t nameConstant = identifier_constant(&parser.previous);
    declare_variable();

    emit_bytes(OP_CLASS, nameConstant);
    define_variable(nameConstant);

    ClassCompiler class_compiler;
    class_compiler.enclosing = current_class;
    current_class = &class_compiler;

    named_variable(class_name, false);

    must_advance(TKN_LBrace, "Expect '{' before class body");
    while (!check(TKN_RBrace) && !check(TKN_EOF)) {
        method();
    }

    must_advance(TKN_RBrace, "Expect '}' after class body");
    emit_byte(OP_POP);

    current_class = current_class->enclosing;
}

static void begin_scope() { current->scopeDepth++; }
static void end_scope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
               current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emit_byte(OP_CLOSE_UPVALUE);
        } else {
            emit_byte(OP_POP);
        }
        current->localCount--;
    }
}

static void function(FuncType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    begin_scope();

    must_advance(TKN_LParen, "Expect '(' after function name");
    if (!check(TKN_RParen)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                error("Cannot have more than 255 parameters");
            }

            uint8_t constant = parse_variable("Expect parameter name");
            define_variable(constant);
        } while (check_advance(TKN_Comma));
    }
    must_advance(TKN_RParen, "Expect ')' after parameters");
    must_advance(TKN_LBrace, "Expect '{' before function body");
    block();

    ObjFunction *func = endCompiler();
    emit_bytes(OP_CLOSURE, make_constant(OBJ_VAL(func)));

    for (int i = 0; i < func->upvalueCount; i++) {
        emit_byte(compiler.upvalues[i].isLocal ? 1 : 0);
        emit_byte(compiler.upvalues[i].index);
    }
}

static void statement() {
    if (check_advance(TKN_Print)) {
        printStatement();
    } else if (check_advance(TKN_Return)) {
        returnStatement();
    } else if (check_advance(TKN_While)) {
        whileStatement();
    } else if (check_advance(TKN_For)) {
        forStatement();
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

static void emit_loop(int loopStart) {
    emit_byte(OP_LOOP);

    // calculating jump
    int jump = current_chunk()->len - loopStart + 2;
    if (jump > UINT16_MAX)
        error("Loop body too large");

    emit_bytes((jump >> 8) & 0xff, jump & 0xff);
}
static void whileStatement() {
    int loopStart = current_chunk()->len;
    must_advance(TKN_LParen, "Expect '(' after 'while'");
    expression();
    must_advance(TKN_RParen, "Expect ')' after condition");

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    statement();

    emit_loop(loopStart);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void forStatement() {
    begin_scope();

    must_advance(TKN_LParen, "Expect '(' after 'for'");
    if (check_advance(TKN_Semicolon)) {
        // No initializer
    } else if (check_advance(TKN_Var)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    int loopStart = current_chunk()->len;

    int exit_jump = -1;
    if (!check_advance(TKN_Semicolon)) {
        // optional condition
        expression();
        must_advance(TKN_Semicolon, "Expect ';' after loop condition");

        // jump out of the loop if false
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }

    if (!check_advance(TKN_RParen)) {
        // optional increment
        int body_jump = emit_jump(OP_JUMP);

        int inc_expr_start = current_chunk()->len;
        expression();
        emit_byte(OP_POP);
        must_advance(TKN_RParen, "Expect ')' after for clauses");

        emit_loop(loopStart);
        loopStart = inc_expr_start;
        patch_jump(body_jump);
    }

    statement();

    emit_loop(loopStart);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }

    end_scope();
}

static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Cannot return from top-level code");
    }
    if (check_advance(TKN_Semicolon)) {
        emit_byte(OP_NIL);
        emit_byte(OP_RETURN);
    } else {
        expression();
        must_advance(TKN_Semicolon, "Expect ';' after return value");
        emit_byte(OP_RETURN);
    }
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

static int add_upvalue(Compiler *compiler, uint8_t index, bool isLocal) {
    int upvalue_count = compiler->function->upvalueCount;

    for (int i = 0; i < upvalue_count; i++) {
        Upvalue *upvalue = &compiler->upvalues[i];

        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalue_count == UINT8_MAX + 1) {
        error("Too many closure variables in function");
        return 0;
    }

    compiler->upvalues[upvalue_count].index = index;
    compiler->upvalues[upvalue_count].isLocal = isLocal;

    return compiler->function->upvalueCount++;
}

static int resolve_upvalue(Compiler *compiler, Token *name) {
    if (compiler->enclosing == NULL) {
        // outermost function; not found
        return -1;
    }

    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, upvalue, false);
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
