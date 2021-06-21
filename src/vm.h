#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX  (FRAMES_MAX * (UINT8_MAX + 1))

typedef struct {
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value *stackTop;
    Table globals;
    Table strings;
    ObjString *initString;    // = "init", name of the constructor in classes
    ObjUpvalue *openUpvalues; // tracking open upvalues
    Obj *objects;             // linked list of all objects

    size_t bytesAllocated;
    size_t nextGC;

    // tracking all grey objects
    int grayCount;
    int grayCapacity;
    Obj **grayStack;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERR,
    INTERPRET_RUNTIME_ERR,
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();

InterpretResult interpret(const char *src);
void push(Value val);
Value pop();

#endif
