#include <stdio.h>
#include <string.h>

#include "ctest.h"
#include "object.h"
#include "table.h"
#include "value.h"

static int hash_string(const char *key, int len) {
    uint32_t hash = 2166136261; // FNV-offset basis

    for (int i = 0; i < len; i++) {
        hash ^= key[i];
        hash *= 16777619; // FNV-prime
    }

    return hash;
}

CTEST(table, insert_multiple_entries) {
    Table table;
    initTable(&table);

    Obj obj = {.type = OBJ_STRING, .marked = false, .next = NULL};

    const int SIZE = 100;

    ObjString strings[SIZE];
    char chars[SIZE][20];
    for (int i = 0; i < SIZE; i++) {
        char buf[20];
        snprintf(buf, 20, "string%d", i);
        strcpy(chars[i], buf);

        strings[i].chars = chars[i];
        strings[i].len = strlen(chars[i]);
        strings[i].obj = obj;
        strings[i].hash = hash_string(chars[i], strlen(chars[i]));
    }

    for (int i = 0; i < SIZE; i++) {
        Value val = {
            .type = VAL_NUM,
            .as =
                {
                    .number = (double)i,
                },
        };

        tableSet(&table, &strings[i], val);
    }

    Value ret;
    for (int i = 0; i < SIZE; i++) {
        ASSERT_TRUE(tableGet(&table, &strings[i], &ret));
        ASSERT_EQUAL(VAL_NUM, ret.type);

        ASSERT_EQUAL((double)i, ret.as.number);
    }
}

CTEST(table, erase) {
    Table table;
    initTable(&table);

    Obj obj = {.type = OBJ_STRING, .marked = false, .next = NULL};

    const int SIZE = 100;

    ObjString strings[SIZE];
    char chars[SIZE][20];
    for (int i = 0; i < SIZE; i++) {
        char buf[20];
        snprintf(buf, 20, "string%d", i);
        strcpy(chars[i], buf);

        strings[i].chars = chars[i];
        strings[i].len = strlen(chars[i]);
        strings[i].obj = obj;
        strings[i].hash = hash_string(chars[i], strlen(chars[i]));
    }

    for (int i = 0; i < SIZE; i++) {
        Value val = {
            .type = VAL_NUM,
            .as =
                {
                    .number = (double)i,
                },
        };

        tableSet(&table, &strings[i], val);
    }

    for (int i = 0; i < SIZE; i += 2) {
        tableDelete(&table, &strings[i]);
    }

    Value ret;
    for (int i = 1; i < SIZE; i += 2) {
        ASSERT_TRUE(tableGet(&table, &strings[i], &ret));
        ASSERT_EQUAL(VAL_NUM, ret.type);

        ASSERT_EQUAL((double)i, ret.as.number);
    }
}

CTEST(table, copy_table) {
    Table table;
    initTable(&table);

    Obj obj = {.type = OBJ_STRING, .marked = false, .next = NULL};

    const int SIZE = 100;

    ObjString strings[SIZE];
    char chars[SIZE][20];
    for (int i = 0; i < SIZE; i++) {
        char buf[20];
        snprintf(buf, 20, "string%d", i);
        strcpy(chars[i], buf);

        strings[i].chars = chars[i];
        strings[i].len = strlen(chars[i]);
        strings[i].obj = obj;
        strings[i].hash = hash_string(chars[i], strlen(chars[i]));
    }

    for (int i = 0; i < SIZE; i++) {
        Value val = {
            .type = VAL_NUM,
            .as =
                {
                    .number = (double)i,
                },
        };

        tableSet(&table, &strings[i], val);
    }

    for (int i = 0; i < SIZE; i += 2) {
        tableDelete(&table, &strings[i]);
    }

    Table dest;
    initTable(&dest);
    tableAddAll(&table, &dest);

    for (int i = 1; i < SIZE; i += 2) {
        Value ret;

        tableGet(&dest, &strings[i], &ret);
        ASSERT_EQUAL(VAL_NUM, ret.type);
        ASSERT_EQUAL((double)i, ret.as.number);
    }
}
