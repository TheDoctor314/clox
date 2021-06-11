#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include "log.h"
#endif

void *mem_reallocate(void *ptr, size_t old_size, size_t new_size) {
    if (new_size > old_size) {
#ifdef DEBUG_LOG_GC
        collectGarbage();
#endif
    }

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void *ret = realloc(ptr, new_size);
    if (!ret) {
        perror("realloc: ");
        exit(1);
    }

    return ret;
}

static void free_object(Obj *object) {
#ifdef DEBUG_LOG_GC
    fprintf(stderr, "%p free type  %d\n", (void *)object, object->type);
#endif

    switch (object->type) {
    case OBJ_STRING: {
        ObjString *str = (ObjString *)object;
        FREE_ARRAY(char, str->chars, str->len + 1);
        FREE(ObjString, object);
        break;
    }
    case OBJ_FUNC: {
        ObjFunction *func = (ObjFunction *)object;
        freeChunk(&func->chunk);
        FREE(ObjFunction, object);
        break;
    }
    case OBJ_CLOSURE: {
        ObjClosure *closure = (ObjClosure *)object;
        FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalueCount);
        FREE(ObjClosure, object);
        break;
    }
    case OBJ_UPVALUE:
        FREE(ObjUpvalue, object);
        break;
    case OBJ_NATIVE:
        FREE(ObjNativeFunc, object);
        break;
    }
}
void freeObjects() {
    Obj *obj = vm.objects;
    while (obj) {
        Obj *next = obj->next;
        free_object(obj);
        obj = next;
    }
}

void mark_object(Obj *obj) {
    if (obj == NULL)
        return;

#ifdef DEBUG_LOG_GC
    fprintf(stderr, "%p mark ", (void *)obj);
    printValue(OBJ_VAL(obj));
    printf("\n");
#endif

    obj->marked = true;
}
void mark_value(Value value) {
    if (IS_OBJ(value))
        mark_object(AS_OBJ(value));
}

void mark_roots() {
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
        mark_value(*slot);
    }

    for (int i = 0; i < vm.frameCount; i++) {
        mark_object((Obj *)vm.frames[i].closure);
    }

    for (ObjUpvalue *upvalue = vm.openUpvalues; upvalue != NULL;
         upvalue = upvalue->next) {
        mark_object((Obj *)upvalue);
    }

    mark_table(&vm.globals);
    mark_compiler_roots();
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    log_info("-- gc begin\n");
#endif

    mark_roots();

#ifdef DEBUG_LOG_GC
    log_info("-- gc end\n");
#endif
}
