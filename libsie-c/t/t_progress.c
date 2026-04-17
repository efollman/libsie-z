#include "sie_config.h"

#include <string.h>
#include "my-check.h"
#include "sie_file.h"
#include "test-utils.h"

static int message_sets = 0;
static int percentages = 0;
static int abort_after = 0;
static int abort_after_type = 0;
void *my_data = (void *)0x12345678;

static int set_message(void *data, const char *msg)
{
    fail_unless(data == my_data);
    ++message_sets;
    return (abort_after_type == 0 && abort_after == message_sets);
}

static int percent(void *data, sie_float64 percent_done)
{
    fail_unless(data == my_data);
    fail_unless(percent_done >= 0.0);
    fail_unless(percent_done <= 100.0);
    ++percentages;
    return (abort_after_type == 1 && abort_after == percentages);
}

START_TEST (test_progress)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    message_sets = 0;
    percentages = 0;
    fail_if(file == NULL, "file didn't open");
    fail_if(sie_get_exception(ctx) != NULL);
    sie_release(file);
    fail_unless(message_sets == 0);
    fail_unless(percentages == 0);

    sie_set_progress_callbacks(ctx, my_data, set_message, percent);

    file = sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    fail_if(file == NULL, "file didn't open");
    fail_if(sie_get_exception(ctx) != NULL);
    sie_release(file);
    fail_unless(message_sets > 0);
    fail_unless(percentages > 0);

    sie_set_progress_callbacks(ctx, NULL, NULL, NULL);
    message_sets = 0;
    percentages = 0;

    file = sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    fail_if(file == NULL, "file didn't open");
    fail_if(sie_get_exception(ctx) != NULL);
    sie_release(file);
    fail_unless(message_sets == 0);
    fail_unless(percentages == 0);
}
END_TEST

START_TEST (test_progress_aborted)
{
    sie_File *file;
    int total_message_sets;
    int total_percentages;

    sie_set_progress_callbacks(ctx, my_data, set_message, percent);
    message_sets = 0;
    percentages = 0;
    file = sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    fail_if(file == NULL, "file didn't open");
    fail_if(sie_get_exception(ctx) != NULL);
    sie_release(file);
    fail_unless(message_sets > 0);
    fail_unless(percentages > 0);

    total_message_sets = message_sets;
    total_percentages = percentages;

    /* Try to abort at all possible points. */
    for (abort_after_type = 0; abort_after_type <= 1; abort_after_type++) {
        int total = abort_after_type ? total_percentages : total_message_sets;
        for (abort_after = 1; abort_after <= total; abort_after++) {
            sie_Exception *e;
            message_sets = 0;
            percentages = 0;
            file = sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
            fail_unless(file == NULL, "file opened instead of aborting, %d:%d",
                        abort_after_type, abort_after);
            e = sie_get_exception(ctx);
            fail_unless(e);
            fail_unless(!strcmp(sie_object_class_name(e),
                                "sie_Operation_Aborted"));
            sie_release(e);
        }
    }
}
END_TEST

Suite *progress_suite(void)
{
    Suite *s = suite_create("progress");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_progress);
    tcase_add_test(tc_basics, test_progress_aborted);

    return s;
}

