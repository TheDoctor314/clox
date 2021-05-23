#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)
#define IS_STRING(object) is_obj_type(object, OBJ_STRING)

#define AS_STRING(value)  ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj *next;
};

struct ObjString {
    Obj obj;
    int len;
    char *chars;
};

static inline bool is_obj_type(Value val, ObjType type) {
    return IS_OBJ(val) && AS_OBJ(val)->type == type;
}

ObjString *takeString(char *chars, int len);
ObjString *copyString(const char *chars, int len);

void printObject(Value val);

#endif
