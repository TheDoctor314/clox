#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "common.h"
#include "value.h"

typedef struct {
    ObjString *key;
    Value value;
} Entry;

typedef struct {
    size_t len;
    size_t capacity;
    Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);

bool tableGet(Table *table, ObjString *key, Value *val);
bool tableSet(Table *table, ObjString *key, Value val);
bool tableDelete(Table *table, ObjString *key);
void tableAddAll(Table *src, Table *dest);
ObjString *tableFindString(Table *table, const char *chars, int len,
                           uint32_t hash);

// gc methods
// if there is a white string in the table, then we delete it from the hash
// table and resolve the dangling pointers
void table_remove_white(Table *table);
void mark_table(Table *table);

#endif
