#include "sie_config.h"

#include "my-check.h"

START_TEST (test_simple)
{
    sie_Relation *rel = sie_rel_new(0, 128);
    sie_Relation *clone;
    fail_if(!rel);
    rel = sie_rel_set_value(rel, "foo", "bar");
    rel = sie_rel_set_value(rel, "bar", "baz");
    fail_unless(!strcmp(sie_rel_value(rel, "foo"), "bar"));
    fail_unless(!strcmp(sie_rel_value(rel, "bar"), "baz"));
    clone = sie_rel_clone(rel);
    fail_unless(!strcmp(sie_rel_value(clone, "foo"), "bar"));
    fail_unless(!strcmp(sie_rel_value(clone, "bar"), "baz"));
    sie_rel_free(clone);
    sie_rel_free(rel);
}
END_TEST

START_TEST (test_amd64_stdarg)
{
    sie_Relation *rel = sie_rel_new(0, 128);
    int out;
    rel = sie_rel_set_valuef(rel, "foo", "%d", 42);
    fail_unless(sie_rel_int_value(rel, "foo", &out));
    fail_unless(out == 42, "out is %d (\"%s\") instead of 42",
                out, sie_rel_value(rel, "foo"));
    sie_rel_free(rel);
}
END_TEST

Suite *relation_suite(void)
{
    Suite *s = suite_create("relation");
    TCase *tc_basics = tcase_create("basics");

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_simple);
    tcase_add_test(tc_basics, test_amd64_stdarg);

    return s;
}

