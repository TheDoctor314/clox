#include "stdio.h"
#include "string.h"

#include "memory.h"
#include "object.h"

static Obj *allocate_object(size_t size, ObjType type) {
    Obj *obj = (Obj *)mem_reallocate(NULL, 0, size);
    obj->type = type;
    return obj;
}

static ObjString *allocate_string(char *chars, int len) {
    ObjString *str =
        (ObjString *)allocate_object(sizeof(ObjString), OBJ_STRING);
    str->len = len;
    str->chars = chars;

    return str;
}

ObjString *copyString(const char *chars, int len) {
    char *heap_chars =
        (char *)mem_reallocate(NULL, 0, sizeof(char) * (len + 1));

    memcpy(heap_chars, chars, len);
    heap_chars[len] = '\0';

    return allocate_string(heap_chars, len);
}

void printObject(Value val) {
    switch (OBJ_TYPE(val)) {
    case OBJ_STRING: {
        ObjString *str = AS_STRING(val);
        printf("%s", str->chars);
        break;
    }
    }
}
