#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUM,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj *obj;
    } as; // extracting raw type reads like a cast
} Value;

// factory functions to define Value types
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = (value)}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUM, {.number = (value)}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj *)object}})

// check value types
#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUM)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)

// convert Lox dynamic type to raw C type
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value)    ((value).as.obj)

typedef struct {
    size_t len;
    size_t capacity;
    Value *values;
} ValueArray;

// like Ruby: nil and false are falsey, rest are truthy
static inline bool is_falsey(Value val) {
    return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}
bool values_equal(Value a, Value b);

void initValueArray(ValueArray *array);
void freeValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value val);
void printValue(Value val);

#endif
