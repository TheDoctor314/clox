#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"
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

    free(vm.grayStack);
}

static void mark_roots();
static void trace_references();
static void sweep();

void collectGarbage() {
#ifdef DEBUG_LOG_GC
    log_info("-- gc begin\n");
#endif

    mark_roots();
    trace_references();
    sweep();

#ifdef DEBUG_LOG_GC
    log_info("-- gc end\n");
#endif
}

void mark_object(Obj *obj) {
    if (obj == NULL)
        return;
    if (obj->marked)
        return;

#ifdef DEBUG_LOG_GC
    fprintf(stderr, "%p mark ", (void *)obj);
    printValue(OBJ_VAL(obj));
    printf("\n");
#endif

    obj->marked = true;

    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack =
            (Obj **)realloc(vm.grayStack, sizeof(Obj *) * vm.grayCapacity);

        if (vm.grayStack == NULL)
            exit(1);
    }

    vm.grayStack[vm.grayCount++] = obj;
}
void mark_value(Value value) {
    if (IS_OBJ(value))
        mark_object(AS_OBJ(value));
}

static void mark_array(ValueArray *arr) {
    for (size_t i = 0; i < arr->len; i++) {
        mark_value(arr->values[i]);
    }
}
static void blacken_object(Obj *obj) {
#ifdef DEBUG_LOG_GC
    fprintf(stderr, "%p blacken ", (void *)obj);
    printValue(OBJ_VAL(obj));
    printf("\n");
#endif

    switch (obj->type) {
    case OBJ_STRING:
    case OBJ_NATIVE:
        break;
    case OBJ_UPVALUE:
        mark_value(((ObjUpvalue *)obj)->closed);
        break;
    case OBJ_FUNC: {
        ObjFunction *func = (ObjFunction *)obj;
        mark_object((Obj *)func->name);
        mark_array(&func->chunk.constants);
        break;
    }
    case OBJ_CLOSURE: {
        ObjClosure *closure = (ObjClosure *)obj;
        mark_object((Obj *)closure->func);

        for (int i = 0; i < closure->upvalueCount; i++) {
            mark_object((Obj *)closure->upvalues[i]);
        }
        break;
    }
    }
}

static void mark_roots() {
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

static void trace_references() {
    while (vm.grayCount > 0) {
        Obj *obj = vm.grayStack[--vm.grayCount];
        blacken_object(obj);
    }
}

static void sweep() {
    // walk the objects list freeing the white objects
    Obj *prev = NULL;
    Obj *obj = vm.objects;

    while (obj != NULL) {
        if (obj->marked) {
            obj->marked = false;
            prev = obj;
            obj = obj->next;
        } else {
            Obj *unreached = obj;
            obj = obj->next;

            if (prev != NULL) {
                prev->next = obj;
            } else {
                vm.objects = obj;
            }

            free_object(unreached);
        }
    }
}
