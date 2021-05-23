#include "string.h"
#include <stdio.h>

#include "memory.h"
#include "object.h"
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
    case VAL_OBJ:
        printObject(val);
        break;
    }
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type)
        return false;

    switch (a.type) {
    case VAL_BOOL:
        return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:
        return true;
    case VAL_NUM:
        return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ: {
        // currently only strings supported
        ObjString *a_str = AS_STRING(a);
        ObjString *b_str = AS_STRING(b);
        return (a_str->len == b_str->len) &&
               (memcmp(a_str->chars, b_str->chars, a_str->len));
    }
    default:
        return false;
    }
}
