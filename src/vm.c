#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "vm.h"

VM vm;

static void reset_stack() { vm.stackTop = vm.stack; }

void initVM() { reset_stack(); }
void freeVM() {}

static inline uint8_t read_byte() { return *vm.ip++; }

static inline Value read_constant() {
    return vm.chunk->constants.values[read_byte()];
}

#define BINARY_OP(op)                                                          \
    do {                                                                       \
        double b = pop();                                                      \
        double a = pop();                                                      \
        push(a op b);                                                          \
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
        case OP_ADD:
            BINARY_OP(+);
            break;
        case OP_SUBTRACT:
            BINARY_OP(-);
            break;
        case OP_MULTIPLY:
            BINARY_OP(*);
            break;
        case OP_DIVIDE:
            BINARY_OP(/);
            break;
        case OP_NEGATE:
            push(-pop());
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

InterpretResult interpret(Chunk *chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;

    return run();
}

void push(Value val) {
    *vm.stackTop = val;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}
