#include "sie_config.h"

#include <string.h>
#include "my-check.h"
#include "sie_file.h"
#include "test-utils.h"

START_TEST (test_open_nonexistent_error)
{
    sie_File *file = sie_file_open(ctx, "nonexistent_file.sie");
    test_exception_result(
        file, "SIE file 'nonexistent_file.sie' could not be opened: ");
}
END_TEST

START_TEST (test_open_xml_error)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.xml");
    test_exception_result(file, "No valid block at end of file");
}
END_TEST

START_TEST (test_open_close)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    fail_if(file == NULL, "file didn't open");
    fail_if(sie_get_exception(ctx) != NULL);
    sie_release(file);
    /* We're checking for memory leaks here... */
}
END_TEST

START_TEST (test_open_read_close)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Output *output;

    fail_unless(spigot);

    while ((output = sie_spigot_get(spigot)))
        /* We're checking for memory leaks here... */;

    sie_release(spigot);
    sie_release(channel);
    sie_release(file);

    fail_if(sie_get_exception(ctx) != NULL);
}
END_TEST

START_TEST (test_open_read_close_release_first)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Output *output;

    fail_unless(spigot);

    sie_release(channel);
    sie_release(file);

    while ((output = sie_spigot_get(spigot)))
        /* We're checking for memory leaks or other badness here... */;

    sie_release(spigot);

    fail_if(sie_get_exception(ctx) != NULL);
}
END_TEST

/* KLUDGE - belongs in t_spigot probably! */
START_TEST (test_scan_limit)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Spigot *spigot = sie_attach_spigot(channel);
    sie_Output *output;

    fail_unless(spigot);

    while ((output = sie_spigot_get(spigot)))
        fail_unless(output->num_scans >= 12);

    sie_release(spigot);

    spigot = sie_attach_spigot(channel);
    sie_spigot_set_scan_limit(spigot, 12);
    
    while ((output = sie_spigot_get(spigot)))
        fail_unless(output->num_scans == 12);

    sie_release(spigot);
    sie_release(channel);
    sie_release(file);

    fail_if(sie_get_exception(ctx) != NULL);
}
END_TEST

/* KLUDGE - belongs in t_ref probably! */
START_TEST (test_get_name)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    sie_Channel *channel = sie_get_channel(file, 1);

    fail_unless(channel);

    fail_unless(sie_get_name(channel));
    fail_unless(!strcmp(sie_get_name(channel), "timhis@Tri_10Hz.RN_1"));

    sie_release(channel);
    sie_release(file);

    fail_if(sie_get_exception(ctx) != NULL);
}
END_TEST

static void tagcomp(void *ref, char *id, char *value)
{
    char *out;
    size_t size = 0;;
    out = sie_get_tag_value(ref, id);
    if (value) {
        fail_if(!out);
        fail_unless(!strcmp(out, value));
    } else {
        fail_unless(!out);
    }
    free(out);
    out = NULL;
    if (value) {
        fail_unless(sie_get_tag_value_b(ref, id, &out, &size));
        fail_if(!out);
        fail_if(!size);
        fail_unless(size == strlen(out));
        fail_unless(!strcmp(out, value));
    } else {
        fail_if(sie_get_tag_value_b(ref, id, &out, &size));
        fail_unless(!out);
        fail_unless(!size);
    }
    free(out);
}

/* KLUDGE - belongs in t_ref probably! */
START_TEST (test_get_tag)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    sie_Channel *channel = sie_get_channel(file, 1);

    fail_unless(channel);

    tagcomp(channel, "DataMode", "timhis");
    tagcomp(channel, "Description", "Sim FG Tri_10Hz");
    tagcomp(channel, "nonexistent", NULL);
    tagcomp(file, "SIE:TCE_SetupName", "sie_min_timhis_a");
    tagcomp(file, "nonexistent", NULL);

    sie_release(channel);
    sie_release(file);

    fail_if(sie_get_exception(ctx) != NULL);
}
END_TEST

/* KLUDGE - belongs in t_ref probably! */
START_TEST (test_get_dimension)
{
    sie_File *file =
        sie_file_open(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie");
    sie_Channel *channel = sie_get_channel(file, 1);
    sie_Iterator *iter = sie_get_dimensions(channel);
    sie_Dimension *dim;
    sie_id index = 0;

    fail_unless(channel);
    fail_unless(iter);

    while ((dim = sie_iterator_next(iter))) {
        sie_Dimension *odim = sie_get_dimension(channel, index++);
        fail_unless(odim == dim);
        sie_release(odim);
    }

    fail_unless(sie_get_dimension(channel, index) == NULL);

    sie_release(iter);
    sie_release(channel);
    sie_release(file);

    fail_if(sie_get_exception(ctx) != NULL);
}
END_TEST

START_TEST (test_is_sie_nonexistent_error)
{
    fail_if(sie_file_is_sie(ctx, "nonexistent_file.sie"));
    test_exception_result(
        NULL, "SIE file 'nonexistent_file.sie' could not be opened: ");
}
END_TEST

START_TEST (test_is_sie_xml_error)
{
    fail_if(sie_file_is_sie(ctx, "t/data/sie_min_timhis_a_19EFAA61.xml"));
    test_exception_result(NULL, "No valid block at the beginning of");
}
END_TEST

START_TEST (test_is_sie)
{
    fail_unless(sie_file_is_sie(ctx, "t/data/sie_min_timhis_a_19EFAA61.sie"));
    fail_if(sie_get_exception(ctx) != NULL);
}
END_TEST

Suite *file_suite(void)
{
    Suite *s = suite_create("file");
    TCase *tc_basics = tcase_create("basics");
    tcase_add_checked_fixture(tc_basics, setup_ctx, check_ctx);

    suite_add_tcase(s, tc_basics);
    tcase_add_test(tc_basics, test_open_nonexistent_error);
    tcase_add_test(tc_basics, test_open_xml_error);
    tcase_add_test(tc_basics, test_open_close);
    tcase_add_test(tc_basics, test_open_read_close);
    tcase_add_test(tc_basics, test_open_read_close_release_first);
    tcase_add_test(tc_basics, test_scan_limit);
    tcase_add_test(tc_basics, test_get_name);
    tcase_add_test(tc_basics, test_get_tag);
    tcase_add_test(tc_basics, test_get_dimension);
    tcase_add_test(tc_basics, test_is_sie_nonexistent_error);
    tcase_add_test(tc_basics, test_is_sie_xml_error);
    tcase_add_test(tc_basics, test_is_sie);

    return s;
}

