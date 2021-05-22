#include <stdio.h>

#include "memory.h"
#include "value.h"

void initValueArray(ValueArray *array) {
    array->len = 0;
    array->capacity = 0;
    array->values = NULL;
}

void freeValueArray(ValueArray *array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void writeValueArray(ValueArray *array, Value val) {
    if (array->capacity < array->len + 1) {
        size_t old_cap = array->capacity;
        array->capacity = GROW_CAPACITY(old_cap);
        array->values =
            GROW_ARRAY(Value, array->values, old_cap, array->capacity);
    }

    array->values[array->len] = val;
    array->len++;
}

void printValue(Value val) {
    switch (val.type) {
    case VAL_BOOL:
        printf(AS_BOOL(val) ? "true" : "false");
        break;
    case VAL_NIL:
        printf("nil");
        break;
    case VAL_NUM:
        printf("%g", AS_NUMBER(val));
        break;
    }
}
