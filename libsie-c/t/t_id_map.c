#include "sie_config.h"

#include "my-check.h"
#include "sie_id_map.h"
#include "test-utils.h"

#define v(x) ((void *)x)

START_TEST (test_creation_set_get)
{
    sie_Id_Map *id_map = sie_id_map_new(ctx, 16);
    fail_if(!id_map, "id_map failed to be created.");
    sie_id_map_set(id_map, 0, v(0));
    sie_id_map_set(id_map, 1, v(1));
    sie_id_map_set(id_map, 2, v(2));
    sie_id_map_set(id_map, 42, v(42));
    fail_unless(sie_id_map_get(id_map, 0) == v(0) &&
                sie_id_map_get(id_map, 1) == v(1) &&
                sie_id_map_get(id_map, 2) == v(2),
                "Failure in id_map direct area.");
    fail_unless(sie_id_map_get(id_map, 42) == v(42),
                "Failure in id_map oflow area.");
    sie_release(id_map);
}
END_TEST

START_TEST (test_grow)
{
    sie_Id_Map *id_map = sie_id_map_new(ctx, 16);
    size_t i;
    sie_id_map_set(id_map, 42, v(42));
    for (i = 0; i < 100; i++) {
        if (i != 42)
            sie_id_map_set(id_map, (sie_id)i, v(i));
    }
    fail_unless(sie_id_map_get(id_map, 42) == v(42),
                "Old entry lost.");
    fail_unless(sie_id_map_get(id_map, 0) == v(0) &&
                sie_id_map_get(id_map, 50) == v(50) &&
                sie_id_map_get(id_map, 99) == v(99),
                "New entries lost.");
    sie_release(id_map);
}
END_TEST

void sum_it(sie_id id, void *value, void *extra)
{
    size_t *sum = extra;
    *sum += id * 42 + (size_t)value;
}

START_TEST (test_foreach)
{
    sie_Id_Map *id_map = sie_id_map_new(ctx, 16);
    size_t value = 0, sum = 0, i;
    sie_id_map_set(id_map, 420, v(19));
    value += 420 * 42 + 19;
    for (i = 0; i < 100; i++) {
        sie_id_map_set(id_map, (sie_id)i, v(i));
        value += i * 42 + i;
    }
    sie_id_map_set(id_map, 900, v(2501));
    value += 900 * 42 + 2501;
    sie_id_map_foreach(id_map, sum_it, &sum);
    fail_unless(sum == value, NULL);
    sie_release(id_map);
}
END_TEST

Suite *id_map_suite(void)
{
    Suite *s = suite_create("id_map");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_creation_set_get);
    tcase_add_test(tc_basics, test_grow);
    tcase_add_test(tc_basics, test_foreach);

    return s;
}

