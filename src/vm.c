#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "log.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

VM vm;

static void reset_stack() { vm.stackTop = vm.stack; }
static void runtime_err(const char *msg, ...) {
    size_t inst = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[inst];

    char buf[1024] = {0};
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, 1024, msg, args);
    va_end(args);

    log_error("[line %d] - %s", line, buf);
}

void initVM() {
    reset_stack();
    vm.objects = NULL;
    initTable(&vm.strings);
    initTable(&vm.globals);
}
void freeVM() {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globals);
}

static inline uint8_t read_byte() { return *vm.ip++; }

static inline Value read_constant() {
    return vm.chunk->constants.values[read_byte()];
}

static inline Value peek(int dist) { return vm.stackTop[-dist - 1]; }
static void concatenate();

#define READ_STRING() AS_STRING(read_constant())

#define BINARY_OP(valueType, op)                                               \
    do {                                                                       \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                      \
            runtime_err("Operands must be numbers");                           \
            return INTERPRET_RUNTIME_ERR;                                      \
        }                                                                      \
        double b = AS_NUMBER(pop());                                           \
        double a = AS_NUMBER(pop());                                           \
        push(valueType(a op b));                                               \
    } while (false)

static InterpretResult run() {
    uint8_t inst;

    while (true) {
#ifdef DEBUG_TRACE_EXEC
        printf("       ");
        for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        switch (inst = read_byte()) {
        case OP_CONSTANT: {
            Value constant = read_constant();
            push(constant);
            break;
        }
        case OP_NIL:
            push(NIL_VAL);
            break;
        case OP_FALSE:
            push(BOOL_VAL(false));
            break;
        case OP_TRUE:
            push(BOOL_VAL(true));
            break;
        case OP_POP:
            pop();
            break;
        case OP_DEFINE_GLOBAL: {
            ObjString *name = READ_STRING();
            tableSet(&vm.globals, name, peek(0));
            pop();
            break;
        }
        case OP_EQUAL: {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(values_equal(a, b)));
            break;
        }
        case OP_GREATER:
            BINARY_OP(BOOL_VAL, >);
            break;
        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;
        case OP_ADD: {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                concatenate();
            } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            } else {
                runtime_err("Operands must be two numbers or two strings");
                return INTERPRET_RUNTIME_ERR;
            }
            break;
        }
        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;
        case OP_NOT:
            push(BOOL_VAL(is_falsey(pop())));
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0))) {
                runtime_err("Operand must be a number");
                return INTERPRET_RUNTIME_ERR;
            }

            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_PRINT:
            printValue(pop());
            printf("\n");
            break;
        case OP_RETURN: {
            // Exit interpreter
            return INTERPRET_OK;
        }
        }
    }
}

#undef BINARY_OP
#undef READ_STRING

InterpretResult interpret(const char *src) {
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(src, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult ret = run();

    freeChunk(&chunk);
    return ret;
}

void push(Value val) {
    *vm.stackTop = val;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static void concatenate() {
    ObjString *b = AS_STRING(pop());
    ObjString *a = AS_STRING(pop());

    int len = b->len + a->len;
    char *chars = mem_reallocate(NULL, 0, sizeof(char) * (len + 1));
    memcpy(chars, a->chars, a->len);
    memcpy(chars + a->len, b->chars, b->len);
    chars[len] = '\0';

    ObjString *ret = takeString(chars, len);
    push(OBJ_VAL(ret));
}
