#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"

#define GROW_CAPACITY(cap) ((cap) < 8 ? 8 : (cap)*2)

#define GROW_ARRAY(type, ptr, old_count, new_count)                            \
    (type *)mem_reallocate(ptr, sizeof(type) * old_count,                      \
                           sizeof(type) * new_count)

#define FREE_ARRAY(type, ptr, old_count)                                       \
    mem_reallocate(ptr, sizeof(type) * old_count, 0)

#define FREE(type, ptr) mem_reallocate(ptr, sizeof(type), 0)

void *mem_reallocate(void *pointer, size_t old_size, size_t new_size);
void freeObjects();

void mark_object(Obj *obj);
void mark_value(Value value);
void collectGarbage();

#endif
