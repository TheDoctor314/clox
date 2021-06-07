#include "chunk.h"
#include "stdio.h"
#include "string.h"

#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"

static Obj *allocate_object(size_t size, ObjType type) {
    Obj *obj = (Obj *)mem_reallocate(NULL, 0, size);
    obj->type = type;

    obj->next = vm.objects;
    vm.objects = obj;

    return obj;
}

static ObjString *allocate_string(char *chars, int len, uint32_t hash) {
    ObjString *str =
        (ObjString *)allocate_object(sizeof(ObjString), OBJ_STRING);
    str->len = len;
    str->chars = chars;
    str->hash = hash;

    // We intern all strings to deduplicate them
    tableSet(&vm.strings, str, NIL_VAL); // using a hash set

    return str;
}

// we use the hash function FNV-1a
static uint32_t hash_string(const char *key, int len) {
    uint32_t hash = 2166136261; // FNV-offset basis

    for (int i = 0; i < len; i++) {
        hash ^= key[i];
        hash *= 16777619; // FNV-prime
    }

    return hash;
}

ObjFunction *newFunction() {
    ObjFunction *func =
        (ObjFunction *)allocate_object(sizeof(ObjFunction), OBJ_FUNC);

    func->arity = 0;
    func->upvalueCount = 0;
    func->name = NULL;
    initChunk(&func->chunk);

    return func;
}

ObjClosure *newClosure(ObjFunction *func) {
    ObjUpvalue **upvalues = (ObjUpvalue **)mem_reallocate(
        NULL, 0, func->upvalueCount * sizeof(ObjUpvalue *));

    for (int i = 0; i < func->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure *closure =
        (ObjClosure *)allocate_object(sizeof(ObjClosure), OBJ_CLOSURE);

    closure->func = func;
    closure->upvalues = upvalues;
    closure->upvalueCount = func->upvalueCount;

    return closure;
}

ObjUpvalue *newUpvalue(Value *slot) {
    ObjUpvalue *upvalue =
        (ObjUpvalue *)allocate_object(sizeof(ObjUpvalue), OBJ_UPVALUE);

    upvalue->location = slot;
    return upvalue;
}

ObjNativeFunc *newNative(NativeFn func) {
    ObjNativeFunc *native =
        (ObjNativeFunc *)allocate_object(sizeof(ObjNativeFunc), OBJ_NATIVE);

    native->func = func;

    return native;
}

ObjString *takeString(char *chars, int len) {
    uint32_t hash = hash_string(chars, len);
    ObjString *interned = tableFindString(&vm.strings, chars, len, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, len + 1);
        return interned;
    }

    return allocate_string(chars, len, hash);
}

ObjString *copyString(const char *chars, int len) {
    uint32_t hash = hash_string(chars, len);
    ObjString *interned = tableFindString(&vm.strings, chars, len, hash);
    if (interned != NULL)
        return interned;

    char *heap_chars =
        (char *)mem_reallocate(NULL, 0, sizeof(char) * (len + 1));

    memcpy(heap_chars, chars, len);
    heap_chars[len] = '\0';

    return allocate_string(heap_chars, len, hash);
}

static void print_func(ObjFunction *func) {
    if (func->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", func->name->chars);
}

void printObject(Value val) {
    switch (OBJ_TYPE(val)) {
    case OBJ_STRING: {
        ObjString *str = AS_STRING(val);
        printf("%s", str->chars);
        break;
    }
    case OBJ_FUNC:
        print_func(AS_FUNC(val));
        break;
    case OBJ_CLOSURE:
        print_func(AS_CLOSURE(val)->func);
        break;
    case OBJ_UPVALUE:
        fputs("<upvalue>", stdout);
        break;
    case OBJ_NATIVE:
        fputs("<native fn>", stdout);
    }
}
