#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)
#define IS_STRING(object) is_obj_type(object, OBJ_STRING)
#define IS_FUNC(object)   is_obj_type(object, OBJ_FUNC)

#define AS_STRING(value)  ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNC(value)    ((ObjFunction *)AS_OBJ(value))

typedef enum {
    OBJ_STRING,
    OBJ_FUNC,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj *next;
};

struct ObjString {
    Obj obj;
    int len;
    char *chars;
    uint32_t hash;
};

typedef struct {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

static inline bool is_obj_type(Value val, ObjType type) {
    return IS_OBJ(val) && AS_OBJ(val)->type == type;
}

ObjFunction *newFunction();
ObjString *takeString(char *chars, int len);
ObjString *copyString(const char *chars, int len);

void printObject(Value val);

#endif
