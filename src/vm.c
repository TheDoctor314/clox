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

static void reset_stack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
}

static void runtime_err(const char *msg, ...) {
    CallFrame *frame = &vm.frames[vm.frameCount - 1];
    size_t inst = frame->ip - frame->function->chunk.code - 1;
    int line = frame->function->chunk.lines[inst];

    char buf[1024] = {0};
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, 1024, msg, args);
    va_end(args);

    log_error("[line %d] - %s", line, buf);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *func = frame->function;
        size_t inst = frame->ip - func->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", func->chunk.lines[inst]);
        if (func->name == NULL) {
            fputs("script\n", stderr);
        } else {
            fprintf(stderr, "%s()\n", func->name->chars);
        }
    }

    reset_stack();
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

static inline Value peek(int dist) { return vm.stackTop[-dist - 1]; }
static bool call(ObjFunction *func, int arg_count);
static bool call_value(Value callee, int arg_count);
static void concatenate();

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT()                                                           \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())

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
    CallFrame *frame = &vm.frames[vm.frameCount - 1];
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
        disassembleInstruction(&frame->function->chunk,
                               (int)(frame->ip - frame->function->chunk.code));
#endif

        switch (inst = READ_BYTE()) {
        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
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
        case OP_GET_LOCAL: {
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
            break;
        }
        case OP_SET_LOCAL: {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            break;
        }
        case OP_GET_GLOBAL: {
            ObjString *name = READ_STRING();
            Value val;
            if (!tableGet(&vm.globals, name, &val)) {
                runtime_err("Undefined variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERR;
            }
            push(val);
            break;
        }
        case OP_SET_GLOBAL: {
            ObjString *name = READ_STRING();
            if (tableSet(&vm.globals, name, peek(0))) {
                tableDelete(&vm.globals, name);
                runtime_err("Undefined variable '%s'", name->chars);
                return INTERPRET_RUNTIME_ERR;
            }
            break;
        }
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
        case OP_JUMP: {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            frame->ip += (is_falsey(peek(0)) * offset);
            break;
        }
        case OP_LOOP: {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_CALL: {
            int arg_count = READ_BYTE();
            if (!call_value(peek(arg_count), arg_count)) {
                return INTERPRET_RUNTIME_ERR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_RETURN: {
            Value ret = pop();
            vm.frameCount--;
            if (vm.frameCount == 0) {
                pop(); // pop the main function
                return INTERPRET_OK;
            }

            vm.stackTop = frame->slots;
            push(ret);
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        }
    }
}

#undef BINARY_OP
#undef READ_STRING
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT

InterpretResult interpret(const char *src) {
    ObjFunction *func = compile(src);
    if (func == NULL)
        return INTERPRET_COMPILE_ERR;

    push(OBJ_VAL(func));
    call(func, 0);

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

static bool call(ObjFunction *func, int arg_count) {
    if (arg_count != func->arity) {
        runtime_err("Expected %d arguments, got %d", func->arity, arg_count);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtime_err("Stack overflow");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->function = func;
    frame->ip = func->chunk.code;
    frame->slots = vm.stackTop - arg_count - 1;

    return true;
}

static bool call_value(Value callee, int arg_count) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
        case OBJ_FUNC:
            return call(AS_FUNC(callee), arg_count);
        case OBJ_NATIVE: {
            NativeFn native = AS_NATIVE(callee);
            Value ret = native(arg_count, vm.stackTop - arg_count);
            vm.stackTop -= (arg_count + 1);
            push(ret);
            return true;
        }
        default:
            break; // do nothing; non-callable
        }
    }

    runtime_err("Can only call functions and classes");
    return false;
}
