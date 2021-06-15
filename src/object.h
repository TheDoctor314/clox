#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)    (AS_OBJ(value)->type)
#define IS_STRING(object)  is_obj_type(object, OBJ_STRING)
#define IS_FUNC(object)    is_obj_type(object, OBJ_FUNC)
#define IS_CLOSURE(object) is_obj_type(object, OBJ_CLOSURE)
#define IS_NATIVE(object)  is_obj_type(object, OBJ_NATIVE)
#define IS_CLASS(object)   is_obj_type(object, OBJ_CLASS)

#define AS_STRING(value)  ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNC(value)    ((ObjFunction *)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_NATIVE(value)  (((ObjNativeFunc *)AS_OBJ(value))->func)
#define AS_CLASS(value)   ((ObjClass *)AS_OBJ(value))

typedef enum {
    OBJ_STRING,
    OBJ_FUNC,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_NATIVE,
    OBJ_CLASS,
} ObjType;

struct Obj {
    ObjType type;
    bool marked;
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
    int upvalueCount;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

typedef struct ObjUpvalue {
    Obj obj;
    Value *location;
    Value closed;
    struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction *func;
    ObjUpvalue **upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString *name;
} ObjClass;

typedef Value (*NativeFn)(int arg_count, Value *args);

typedef struct {
    Obj obj;
    NativeFn func;
} ObjNativeFunc;

static inline bool is_obj_type(Value val, ObjType type) {
    return IS_OBJ(val) && AS_OBJ(val)->type == type;
}

ObjFunction *newFunction();
ObjClosure *newClosure(ObjFunction *func);
ObjClass *newClass(ObjString *name);

ObjUpvalue *newUpvalue(Value *slot);
ObjNativeFunc *newNative(NativeFn func);

ObjString *takeString(char *chars, int len);
ObjString *copyString(const char *chars, int len);

void printObject(Value val);

#endif
