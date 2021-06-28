#include <stddef.h>

#include "ctest.h"
#include "object.h"
#include "value.h"

CTEST(value, tagged_uinon) {
    {
        Value val = NIL_VAL;
        ASSERT_TRUE(IS_NIL(val));
    }

    {
        Value val = BOOL_VAL(false);
        ASSERT_TRUE(IS_BOOL(val));
        ASSERT_FALSE(AS_BOOL(val));

        val = BOOL_VAL(true);
        ASSERT_TRUE(IS_BOOL(val));
        ASSERT_TRUE(AS_BOOL(val));
    }

    {
        Value val = NUMBER_VAL(1.0);
        ASSERT_TRUE(IS_NUMBER(val));
        ASSERT_EQUAL(AS_NUMBER(val), 1.0);

        val = NUMBER_VAL(2.0);
        ASSERT_TRUE(IS_NUMBER(val));
        ASSERT_EQUAL(AS_NUMBER(val), 2.0);
    }

    {
        Obj obj = {.type = OBJ_STRING, .marked = false, .next = NULL};
        ObjString hello = {.obj = obj, .chars = "hello", .len = 5, .hash = 5};

        Value val = OBJ_VAL(&hello);
        ASSERT_TRUE(IS_OBJ(val));
        ASSERT_TRUE(AS_STRING(val) == &hello);

        ObjString world = {.obj = obj, .chars = "world", .len = 5, .hash = 10};
        val = OBJ_VAL(&world);
        ASSERT_TRUE(IS_OBJ(val));
        ASSERT_TRUE(AS_STRING(val) == &world);
    }
}

CTEST(value, falsey) {
    ASSERT_TRUE(is_falsey(NIL_VAL));
    ASSERT_TRUE(is_falsey(BOOL_VAL(false)));

    ASSERT_FALSE(is_falsey(BOOL_VAL(true)));
    ASSERT_FALSE(is_falsey(NUMBER_VAL(5.0)));
    ASSERT_FALSE(is_falsey(OBJ_VAL(NULL)));
}
