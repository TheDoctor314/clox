#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define LOAD_FACTOR_PERCENT 75
void initTable(Table *table) {
    table->len = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table *table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry *lookup(Entry *entries, size_t capacity, ObjString *key);
static void rehash(Table *table, size_t capacity);

bool tableGet(Table *table, ObjString *key, Value *val) {
    if (table->len == 0)
        return false;

    Entry *entry = lookup(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    *val = entry->value;
    return true;
}

bool tableSet(Table *table, ObjString *key, Value val) {
    if ((table->len + 1) * 100 > table->capacity * LOAD_FACTOR_PERCENT) {
        size_t capacity = GROW_CAPACITY(table->capacity);
        rehash(table, capacity);
    }

    Entry *entry = lookup(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->value))
        table->len++;

    entry->key = key;
    entry->value = val;
    return is_new_key;
}

bool tableDelete(Table *table, ObjString *key) {
    if (table->len == 0)
        return true;

    Entry *entry = lookup(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true); // tombstone marker

    return true;
}

ObjString *tableFindString(Table *table, const char *chars, int len,
                           uint32_t hash) {
    if (table->len == 0)
        return NULL;

    uint32_t index = hash % table->capacity;
    while (true) {
        Entry *entry = &table->entries[index];
        if (entry->key == NULL) {
            // stop if we find empty but not dead entry
            if (IS_NIL(entry->value))
                return NULL;
        } else if ((entry->key->len == len) && (entry->key->hash == hash) &&
                   (memcmp(entry->key->chars, chars, len) == 0)) {
            // found it
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

void table_remove_white(Table *table) {
    for (size_t i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        if ((entry->key != NULL) && !(entry->key->obj.marked)) {
            tableDelete(table, entry->key);
        }
    }
}

void mark_table(Table *table) {
    for (size_t i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        mark_object((Obj *)entry->key);
        mark_value(entry->value);
    }
}

// table copy function
void tableAddAll(Table *src, Table *dest) {
    for (size_t i = 0; i < src->capacity; i++) {
        Entry *entry = &src->entries[i];
        if (entry->key != NULL) {
            tableSet(dest, entry->key, entry->value);
        }
    }
}

static Entry *lookup(Entry *entries, size_t capacity, ObjString *key) {
    uint32_t index = key->hash % capacity;
    Entry *tombstone = NULL;

    while (true) {
        Entry *entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // empty entry
                return tombstone != NULL ? tombstone : entry;
            } else {
                // found a tombstone
                if (tombstone == NULL)
                    tombstone = entry;
            }
        } else if (entry->key == key) {
            // key found
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

static void rehash(Table *table, size_t new_capacity) {
    Entry *new_entries =
        (Entry *)mem_reallocate(NULL, 0, sizeof(Entry) * new_capacity);
    for (size_t i = 0; i < new_capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = NIL_VAL;
    }

    size_t old_capacity = table->capacity;
    Entry *old_entries = table->entries;

    table->len = 0;
    for (size_t i = 0; i < old_capacity; i++) {
        Entry *old_entry = &old_entries[i];

        // skip if not used
        if (old_entry->key == NULL)
            continue;

        Entry *dest = lookup(new_entries, new_capacity, old_entry->key);
        dest->key = old_entry->key;
        dest->value = old_entry->value;
        table->len++;
    }

    FREE_ARRAY(Entry, old_entries, old_capacity);

    table->entries = new_entries;
    table->capacity = new_capacity;
}
