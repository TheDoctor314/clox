#include <stdio.h>

#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "log.h"
#include "value.h"
#include "vm.h"

VM vm;

static void reset_stack() { vm.stackTop = vm.stack; }
static void runtime_err(const char *msg) {
    size_t inst = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[inst];
    log_error("[line %d] - %s", line, msg);
}

void initVM() { reset_stack(); }
void freeVM() {}

static inline uint8_t read_byte() { return *vm.ip++; }

static inline Value read_constant() {
    return vm.chunk->constants.values[read_byte()];
}

static inline Value peek(int dist) { return vm.stackTop[-dist - 1]; }

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
        case OP_ADD:
            BINARY_OP(NUMBER_VAL, +);
            break;
        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0))) {
                runtime_err("Operand must be a number");
                return INTERPRET_RUNTIME_ERR;
            }

            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_RETURN: {
            printValue(pop());
            printf("\n");
            return INTERPRET_OK;
        }
        }
    }
}

#undef BINARY_OP

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
