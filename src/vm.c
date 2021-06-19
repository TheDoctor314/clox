#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
    vm.openUpvalues = NULL;
}

static void runtime_err(const char *msg, ...) {
    CallFrame *frame = &vm.frames[vm.frameCount - 1];
    ObjFunction *function = frame->closure->func;
    size_t inst = frame->ip - function->chunk.code - 1;
    int line = function->chunk.lines[inst];

    char buf[1024] = {0};
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, 1024, msg, args);
    va_end(args);

    log_error("[line %d] - %s\n", line, buf);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame *frame = &vm.frames[i];
        ObjFunction *func = frame->closure->func;
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

static void define_native(const char *name, NativeFn func) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(func)));

    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);

    pop();
    pop();
}

static Value clockNative(int arg_count, Value *args);

void initVM() {
    reset_stack();
    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024; // arbitrary
    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    initTable(&vm.strings);
    initTable(&vm.globals);

    define_native("clock", clockNative);
}
void freeVM() {
    freeObjects();
    freeTable(&vm.strings);
    freeTable(&vm.globals);
}

static inline Value peek(int dist) { return vm.stackTop[-dist - 1]; }
static bool call(ObjClosure *closure, int arg_count);
static bool call_value(Value callee, int arg_count);
static ObjUpvalue *capture_upvalue(Value *local);
static void close_upvalues(Value *last);
static void concatenate();
static void define_method(ObjString *name);

#define READ_BYTE() (*frame->ip++)

#define READ_SHORT()                                                           \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_CONSTANT()                                                        \
    (frame->closure->func->chunk.constants.values[READ_BYTE()])

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
        disassembleInstruction(
            &frame->closure->func->chunk,
            (int)(frame->ip - frame->closure->func->chunk.code));
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
        case OP_GET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }
        case OP_SET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
            break;
        }
        case OP_GET_PROPERTY: {
            if (!IS_INSTANCE(peek(0))) {
                runtime_err("Only instances have properties");
                return INTERPRET_RUNTIME_ERR;
            }

            ObjInstance *inst = AS_INSTANCE(peek(0));
            ObjString *name = READ_STRING();

            Value val;
            if (tableGet(&inst->fields, name, &val)) {
                pop(); // Instance
                push(val);
                break;
            }

            runtime_err("Undefined property: '%s'", name->chars);
            return INTERPRET_RUNTIME_ERR;
        }
        case OP_SET_PROPERTY: {
            if (!IS_INSTANCE(peek(1))) {
                runtime_err("Only instances have fields");
                return INTERPRET_RUNTIME_ERR;
            }

            ObjInstance *inst = AS_INSTANCE(peek(1));
            tableSet(&inst->fields, READ_STRING(), peek(0));
            Value val = pop();
            pop();
            push(val);
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
        case OP_CLOSURE: {
            ObjFunction *func = AS_FUNC(READ_CONSTANT());
            ObjClosure *closure = newClosure(func);
            push(OBJ_VAL(closure));

            for (int i = 0; i < closure->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();

                if (isLocal) {
                    closure->upvalues[i] =
                        capture_upvalue(frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
            break;
        }
        case OP_CLOSE_UPVALUE:
            close_upvalues(vm.stackTop - 1);
            pop();
            break;
        case OP_CLASS:
            push(OBJ_VAL(newClass(READ_STRING())));
            break;
        case OP_METHOD:
            define_method(READ_STRING());
            break;
        case OP_RETURN: {
            Value ret = pop();
            close_upvalues(frame->slots);
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
    ObjClosure *closure = newClosure(func);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

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
    ObjString *b = AS_STRING(peek(0));
    ObjString *a = AS_STRING(peek(1));

    int len = b->len + a->len;
    char *chars = mem_reallocate(NULL, 0, sizeof(char) * (len + 1));
    memcpy(chars, a->chars, a->len);
    memcpy(chars + a->len, b->chars, b->len);
    chars[len] = '\0';

    ObjString *ret = takeString(chars, len);
    pop();
    pop();
    push(OBJ_VAL(ret));
}

static bool call(ObjClosure *closure, int arg_count) {
    if (arg_count != closure->func->arity) {
        runtime_err("Expected %d arguments, got %d", closure->func->arity,
                    arg_count);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtime_err("Stack overflow");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->func->chunk.code;
    frame->slots = vm.stackTop - arg_count - 1;

    return true;
}

static bool call_value(Value callee, int arg_count) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
        case OBJ_CLOSURE:
            return call(AS_CLOSURE(callee), arg_count);
        case OBJ_NATIVE: {
            NativeFn native = AS_NATIVE(callee);
            Value ret = native(arg_count, vm.stackTop - arg_count);
            vm.stackTop -= (arg_count + 1);
            push(ret);
            return true;
        }
        case OBJ_CLASS: {
            ObjClass *klass = AS_CLASS(callee);
            vm.stackTop[-arg_count - 1] = OBJ_VAL(newInstance(klass));
            return true;
        }
        default:
            break; // do nothing; non-callable
        }
    }

    runtime_err("Can only call functions and classes");
    return false;
}

static ObjUpvalue *capture_upvalue(Value *local) {
    // we look for a previously created upvalue referring to the same local
    ObjUpvalue *prev = NULL;
    ObjUpvalue *upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prev = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue *created_upval = newUpvalue(local);
    created_upval->next = upvalue;

    if (prev == NULL) {
        vm.openUpvalues = created_upval;
    } else {
        prev->next = created_upval;
    }

    return created_upval;
}

static void close_upvalues(Value *last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue *upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static void define_method(ObjString *name) {
    Value method = peek(0);
    ObjClass *klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

static Value clockNative(int arg_count __attribute__((unused)),
                         Value *args __attribute__((unused))) {
    return NUMBER_VAL(((double)clock() / CLOCKS_PER_SEC));
}
