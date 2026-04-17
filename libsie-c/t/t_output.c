#include "sie_config.h"

#include "my-check.h"
#include "test-utils.h"

static sie_Output *sample_output(void)
{
    sie_Output *out = sie_output_new(ctx, 2);
    sie_output_set_type(out, 0, SIE_OUTPUT_FLOAT64);
    sie_output_set_type(out, 1, SIE_OUTPUT_RAW);
    sie_output_grow(out, 0);
    sie_output_grow(out, 1);
    out->v[0].float64[0] = 1000.0;
    out->v[0].float64[1] = 2000.0;
    out->v[0].float64[2] = 3000.0;
    out->v[0].float64[3] = 4000.0;
    out->v[0].float64[4] = 5000.0;
    sie_output_set_raw(out, 1, 0, "ab", 3);
    sie_output_set_raw(out, 1, 1, "ra", 3);
    sie_output_set_raw(out, 1, 2, "ca", 3);
    sie_output_set_raw(out, 1, 3, "da", 3);
    sie_output_set_raw(out, 1, 4, "bra", 4);
    out->v_guts[0].size = 5;
    out->v_guts[1].size = 5;
    out->num_scans = 5;
    return out;
}

START_TEST (test_output_trim)
{
    sie_Output *out = sample_output();

    fail_if(out == NULL);
    fail_if(out->num_scans != 5);

    sie_output_trim(out, 0, 2);
    
    fail_if(out->num_scans != 2);
    fail_if(out->v[0].float64[0] != 1000.0);
    fail_if(out->v[0].float64[1] != 2000.0);
    fail_if(strcmp(out->v[1].raw[0].ptr, "ab"));
    fail_if(strcmp(out->v[1].raw[1].ptr, "ra"));
    
    sie_release(out);
    out = sample_output();

    sie_output_trim(out, 1, 2);
    
    fail_if(out->num_scans != 2);
    fail_if(out->v[0].float64[0] != 2000.0);
    fail_if(out->v[0].float64[1] != 3000.0);
    fail_if(strcmp(out->v[1].raw[0].ptr, "ra"));
    fail_if(strcmp(out->v[1].raw[1].ptr, "ca"));

    sie_release(out);
}
END_TEST

START_TEST (test_output_accessors)
{
    sie_Output *out = sample_output();

    fail_if(out == NULL);
    
    fail_if(out->num_vs != sie_output_get_num_dims(out));
    fail_if(out->num_scans != sie_output_get_num_rows(out));
    fail_if(out->v[0].type != sie_output_get_type(out, 0));
    fail_if(out->v[1].type != sie_output_get_type(out, 1));
    fail_if(out->v[0].float64 != sie_output_get_float64(out, 0));
    fail_if(out->v[1].raw != sie_output_get_raw(out, 1));

    fail_if(sie_output_get_type(out, 2) != SIE_OUTPUT_NONE);
    fail_if(sie_output_get_float64(out, 2) != NULL);
    fail_if(sie_output_get_raw(out, 2) != NULL);

    sie_release(out);
}
END_TEST

START_TEST (test_output_accessors_null)
{
    sie_Output *out = NULL;

    fail_if(sie_output_get_num_dims(out) != 0);
    fail_if(sie_output_get_num_rows(out) != 0);
    fail_if(sie_output_get_type(out, 0) != SIE_OUTPUT_NONE);
    fail_if(sie_output_get_float64(out, 0) != NULL);
    fail_if(sie_output_get_raw(out, 0) != NULL);

    sie_release(out);
}
END_TEST

START_TEST (test_output_compare)
{
    sie_Output *out = sample_output();
    sie_Output *out2 = sample_output();

    fail_unless(sie_output_compare(out, out2));
    out->v[0].float64[0] = 42;
    fail_if(sie_output_compare(out, out2));

    sie_release(out);
    sie_release(out2);
}
END_TEST

START_TEST (test_output_copy)
{
    sie_Output *out = sample_output();
    sie_Output *out2 = sie_copy(out);

    fail_unless(sie_output_compare(out, out2));
    out->v[0].float64[0] = 42;
    fail_if(sie_output_compare(out, out2));

    sie_release(out);
    sie_release(out2);
}
END_TEST

Suite *output_suite(void)
{
    Suite *s = suite_create("output");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_output_trim);
    tcase_add_test(tc_basics, test_output_accessors);
    tcase_add_test(tc_basics, test_output_accessors_null);
    tcase_add_test(tc_basics, test_output_compare);
    tcase_add_test(tc_basics, test_output_copy);

    return s;
}

