#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)      (AS_OBJ(value)->type)
#define IS_STRING(obj)       is_obj_type(obj, OBJ_STRING)
#define IS_FUNC(obj)         is_obj_type(obj, OBJ_FUNC)
#define IS_CLOSURE(obj)      is_obj_type(obj, OBJ_CLOSURE)
#define IS_NATIVE(obj)       is_obj_type(obj, OBJ_NATIVE)
#define IS_CLASS(obj)        is_obj_type(obj, OBJ_CLASS)
#define IS_INSTANCE(obj)     is_obj_type(obj, OBJ_INSTANCE)
#define IS_BOUND_METHOD(obj) is_obj_type(obj, OBJ_BOUND_METHOD)

#define AS_STRING(val)       ((ObjString *)AS_OBJ(val))
#define AS_CSTRING(val)      (((ObjString *)AS_OBJ(val))->chars)
#define AS_FUNC(val)         ((ObjFunction *)AS_OBJ(val))
#define AS_CLOSURE(val)      ((ObjClosure *)AS_OBJ(val))
#define AS_NATIVE(val)       (((ObjNativeFunc *)AS_OBJ(val))->func)
#define AS_CLASS(val)        ((ObjClass *)AS_OBJ(val))
#define AS_INSTANCE(val)     ((ObjInstance *)AS_OBJ(val))
#define AS_BOUND_METHOD(val) ((ObjBoundMethod *)AS_OBJ(val))

typedef enum {
    OBJ_STRING,
    OBJ_FUNC,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_NATIVE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
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
    Table methods;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass *klass;
    Table fields;
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure *method;
} ObjBoundMethod;

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
ObjInstance *newInstance(ObjClass *klass);
ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method);

ObjUpvalue *newUpvalue(Value *slot);
ObjNativeFunc *newNative(NativeFn func);

ObjString *takeString(char *chars, int len);
ObjString *copyString(const char *chars, int len);

void printObject(Value val);

#endif
