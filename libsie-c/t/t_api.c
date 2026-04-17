#include "sie.h"
#include "my-check.h"

START_TEST(test_null_api)
{
    fail_unless(sie_retain(NULL) == NULL);
    sie_release(NULL);
    fail_unless(sie_context_done(NULL) == 0);
    fail_unless(sie_file_open(NULL, "whatever") == NULL);
    fail_unless(sie_get_tests(NULL) == NULL);
    fail_unless(sie_get_channels(NULL) == NULL);
    fail_unless(sie_get_dimensions(NULL) == NULL);
    fail_unless(sie_get_tags(NULL) == NULL);
    fail_unless(sie_get_test(NULL, 0) == NULL);
    fail_unless(sie_get_channel(NULL, 0) == NULL);
    fail_unless(sie_get_dimension(NULL, 0) == NULL);
    fail_unless(sie_get_tag(NULL, NULL) == NULL);
    fail_unless(sie_get_containing_test(NULL) == NULL);
    fail_unless(sie_get_name(NULL) == NULL);
    fail_unless(sie_get_id(NULL) == SIE_NULL_ID);
    fail_unless(sie_get_index(NULL) == SIE_NULL_ID);
    fail_unless(sie_iterator_next(NULL) == NULL);
    fail_unless(sie_tag_get_id(NULL) == NULL);
    fail_unless(sie_tag_get_value(NULL) == NULL);
    fail_unless(sie_tag_get_value_b(NULL, NULL, NULL) == 0);
    sie_free(NULL);
    sie_system_free(NULL);
    fail_unless(sie_attach_spigot(NULL) == NULL);
    fail_unless(sie_spigot_get(NULL) == NULL);
    sie_spigot_disable_transforms(NULL, 0);
    sie_spigot_transform_output(NULL, NULL);
    fail_unless(sie_spigot_seek(NULL, 0) == SIE_SPIGOT_SEEK_END);
    fail_unless(sie_spigot_tell(NULL) == SIE_SPIGOT_SEEK_END);
    fail_unless(sie_lower_bound(NULL, 0, 0, NULL, NULL) == 0);
    fail_unless(sie_upper_bound(NULL, 0, 0, NULL, NULL) == 0);
    fail_unless(sie_binary_search(NULL, 0, 0, NULL, NULL) == 0);
    fail_unless(sie_output_get_block(NULL) == 0);
    fail_unless(sie_output_get_num_dims(NULL) == 0);
    fail_unless(sie_output_get_num_rows(NULL) == 0);
    fail_unless(sie_output_get_type(NULL, 0) == SIE_OUTPUT_NONE);
    fail_unless(sie_output_get_float64(NULL, 0) == NULL);
    fail_unless(sie_output_get_raw(NULL, 0) == NULL);
    fail_unless(sie_output_get_struct(NULL) == NULL);
    fail_unless(sie_histogram_new(NULL) == NULL);
    fail_unless(sie_histogram_get_num_dims(NULL) == 0);
    fail_unless(sie_histogram_get_num_bins(NULL, 0) == 0);
    sie_histogram_get_bin_bounds(NULL, 0, NULL, NULL);
    fail_unless(sie_histogram_get_bin(NULL, NULL) == 0.0);
    fail_unless(sie_histogram_get_next_nonzero_bin(NULL, NULL, NULL) == 0.0);
    fail_unless(sie_check_exception(NULL) == NULL);
    fail_unless(sie_get_exception(NULL) == NULL);
    fail_unless(sie_report(NULL) == NULL);
    fail_unless(sie_verbose_report(NULL) == NULL);
    sie_ignore_trailing_garbage(NULL, 0);
    sie_set_progress_callbacks(NULL, NULL, NULL, NULL);
    fail_unless(sie_file_is_sie(NULL, NULL) == 0);
    fail_unless(sie_stream_new(NULL) == NULL);
    fail_unless(sie_add_stream_data(NULL, NULL, 0) == 0);
    fail_unless(sie_spigot_done(NULL) == 0);
}
END_TEST

Suite *api_suite(void)
{
    Suite *s = suite_create("api");
    TCase *tc_no_context = tcase_create("no_context");

    suite_add_tcase(s, tc_no_context);
    tcase_add_test(tc_no_context, test_null_api);

    return s;
}

