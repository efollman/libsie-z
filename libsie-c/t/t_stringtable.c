#include "sie_config.h"

#include <string.h>
#include "my-check.h"
#include "test-utils.h"

START_TEST (test_stringtable)
{
    sie_String *null_string = sie_string_get(ctx, "", 0);
    sie_String *test_string = sie_string_get(ctx, "test", 4);
    sie_String *ch_string = sie_string_get(ctx, "ch", 2);

    sie_String *test2_string = sie_string_get(ctx, "test", 4);
    sie_String *null2_string = sie_string_get(ctx, "", 0);
    sie_String *ch2_string = sie_string_get(ctx, "ch", 2);

    fail_unless(null_string);
    fail_unless(test_string);
    fail_unless(ch_string);
    fail_unless(null2_string);
    fail_unless(test2_string);
    fail_unless(ch2_string);

    fail_unless(null_string != test_string);
    fail_unless(null_string != ch_string);

    fail_unless(null_string == null2_string);
    fail_unless(test_string == test2_string);
    fail_unless(ch_string == ch2_string);
    
    sie_release(null_string);
    sie_release(test_string);
    sie_release(ch_string);
    sie_release(null2_string);
    sie_release(test2_string);
    sie_release(ch2_string);
}
END_TEST

START_TEST (test_stringtable_binary)
{
    sie_String *test1_string = sie_string_get(ctx, "te\0st", 5);
    sie_String *other_string = sie_string_get(ctx, "te\0sT", 5);
    sie_String *test2_string = sie_string_get(ctx, "te\0st", 5);

    fail_unless(test1_string);
    fail_unless(other_string);
    fail_unless(test2_string);

    fail_unless(test1_string == test2_string);
    fail_unless(test1_string != other_string);

    sie_release(test1_string);
    sie_release(other_string);
    sie_release(test2_string);
}
END_TEST

START_TEST (test_stringtable_literals)
{
    sie_String *test_lit = sie_literal(ctx, test);
    sie_String *ch_lit = sie_literal(ctx, ch);

    sie_String *test_string = sie_string_get(ctx, "test", 4);
    sie_String *ch_string = sie_string_get(ctx, "ch", 2);

    fail_unless(test_string);
    fail_unless(ch_string);
    fail_unless(test_lit);
    fail_unless(ch_lit);

    fail_unless(test_string == test_lit);
    fail_unless(ch_string == ch_lit);

    fail_unless(test_lit != ch_lit);
    
    sie_release(test_string);
    sie_release(ch_string);
}
END_TEST

Suite *stringtable_suite(void)
{
    Suite *s = suite_create("stringtable");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_stringtable);
    tcase_add_test(tc_basics, test_stringtable_binary);
    tcase_add_test(tc_basics, test_stringtable_literals);

    return s;
}

