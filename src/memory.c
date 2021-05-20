#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

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
