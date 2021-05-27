#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

void *mem_reallocate(void *ptr, size_t old_size, size_t new_size) {
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
    switch (object->type) {
    case OBJ_STRING: {
        ObjString *str = (ObjString *)object;
        FREE_ARRAY(char, str->chars, str->len + 1);
        FREE(ObjString, object);
        break;
    }
    case OBJ_FUNC: {
        ObjFunction *func = (ObjFunction *)object;
        freeChunk(func->chunk);
        free_object((Obj *)func->name);
        FREE(ObjFunction, object);
    }
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
